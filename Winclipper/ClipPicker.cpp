#include "stdafx.h"
#include "ClipPicker.h"
#include "ClipsManager.h"
#include <Uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Msimg32.lib")

namespace
{
	const wchar_t * const MENU_CLASS = L"WinclipperClipMenu";
	const wchar_t * const PICKER_CLASS = L"WinclipperClipPicker";
	const int ID_SEARCH = 1;
	const int ID_LIST = 2;
	const int VISIBLE_ROWS = 20;
	const UINT_PTR TIMER_OPEN_PICKER = 1;
	const UINT_PTR TIMER_CLOSE_PICKER = 2;

	enum ItemKind { ITEM_CLIP, ITEM_COMMAND, ITEM_MORE, ITEM_SEPARATOR, ITEM_DISABLED };

	struct MenuItem
	{
		ItemKind kind;
		int id;				// clip index + 1, or a *_SELECT command
		std::wstring text;	// commands may contain an & mnemonic
		int top = 0;		// in content coordinates
		int height = 0;
	};

	struct State
	{
		ClipsManager * manager = nullptr;
		const ClipMenuOptions * options = nullptr;
		int result = CANCELED_SELECTION;
		bool done = false;

		// Shared look, taken from the system menu theme.
		HTHEME theme = nullptr;
		HFONT font = nullptr;
		HBRUSH background = nullptr;
		COLORREF backgroundColor = RGB(255, 255, 255);
		COLORREF textColor = RGB(0, 0, 0);
		UINT dpi = 96;
		int margin = 0;			// between the window edge and the items
		int padding = 0;		// right of the text
		int gutter = 0;			// left of the text, room for thumbnails
		int textHeight = 16;
		int itemHeight = 24;
		int separatorHeight = 9;
		int arrowWidth = 16;
		bool showCues = false;	// underline mnemonics

		// Menu window.
		HWND menu = nullptr;
		std::vector<MenuItem> items;
		int hot = -1;
		int scroll = 0;
		int contentHeight = 0;
		int viewHeight = 0;
		int menuWheelRemainder = 0;
		POINT lastMenuMouse = { LONG_MIN, LONG_MIN };
		bool trackingLeave = false;

		// Picker window ("More..." / "Search...").
		HWND picker = nullptr;
		HWND search = nullptr;
		HWND list = nullptr;
		int pickerItem = -1;	// menu item the picker belongs to
		std::vector<unsigned int> shown;	// clip index of each list row
		size_t pinnedRows = 0;
		POINT lastListMouse = { LONG_MIN, LONG_MIN };
		bool thumbDragging = false;	// position bar grabbed with the mouse
		bool thumbHot = false;		// cursor over the position bar: drawn wider
		int thumbGrabOffset = 0;
		int thumbDragPos = -1;		// bar offset while dragging: follows the mouse pixel by pixel
		int wheelRemainder = 0;
		bool previewSuppressed = false;	// no preview for the item highlighted on open
	};

	int Scale(const State & state, int value)
	{
		return MulDiv(value, static_cast<int>(state.dpi), 96);
	}

	bool IsOurs(const State & state, HWND window)
	{
		return window != nullptr && (window == state.menu || window == state.picker
			|| (state.picker != nullptr && IsChild(state.picker, window)));
	}

	void HidePreview(State & state)
	{
		if (state.options->preview)
		{
			state.options->preview(-1, nullptr);
		}
	}

	void ShowPreview(State & state, int clipIndex, const RECT & screenRect)
	{
		if (state.previewSuppressed)
		{
			HidePreview(state);
			return;
		}
		if (state.options->preview && UserSettings::ShowPreview())
		{
			state.options->preview(clipIndex, &screenRect);
		}
	}

	bool Contains(const wchar_t * text, int textLength, const std::wstring & filter)
	{
		if (filter.empty())
		{
			return true;
		}
		if (text == nullptr || textLength <= 0)
		{
			return false;
		}
		return FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, FIND_FROMSTART | LINGUISTIC_IGNORECASE,
			text, textLength, filter.c_str(), static_cast<int>(filter.length()),
			nullptr, nullptr, nullptr, 0) >= 0;
	}

	// Same text the Win32 clips menu showed for the clip.
	std::wstring RowText(const std::shared_ptr<Clip> & clip)
	{
		if (clip->ContainsFormat(CF_UNICODETEXT) && clip->UnicodeText() != nullptr)
		{
			// Clips loaded from clips.dat carry a trailing NUL (the serializer
			// encodes length + 1); theme text APIs count it and add an ellipsis.
			std::wstring text = clip->GetUnicodeMenuText(UserSettings::MenuDisplayChars());
			const size_t end = text.find(L'\0');
			if (end != std::wstring::npos)
			{
				text.resize(end);
			}
			return text;
		}
		if (clip->ContainsFormat(CF_DIB))
		{
			return L"[IMAGE] " + std::to_wstring(clip->DibHeight()) + L" x " + std::to_wstring(clip->DibWidth());
		}
		return std::wstring();
	}

	std::wstring StripMnemonic(const std::wstring & text)
	{
		std::wstring plain;
		for (size_t i = 0; i < text.length(); i++)
		{
			if (text[i] == L'&' && i + 1 < text.length())
			{
				i++;
			}
			plain += text[i];
		}
		return plain;
	}

	wchar_t Mnemonic(const std::wstring & text)
	{
		const size_t amp = text.find(L'&');
		if (amp == std::wstring::npos || amp + 1 >= text.length())
		{
			return 0;
		}
		return static_cast<wchar_t>(towupper(text[amp + 1]));
	}

	// --- Shared drawing -------------------------------------------------------

	// Width of a row text exactly as DrawItem draws it (same theme font and
	// flags), so the window is not wider than its longest row.
	int TextWidth(const State & state, HDC dc, const std::wstring & text)
	{
		RECT rect = { 0, 0, 0, 0 };
		const DWORD flags = DT_SINGLELINE | DT_LEFT | DT_NOPREFIX | DT_CALCRECT;
		if (state.theme != nullptr)
		{
			if (SUCCEEDED(GetThemeTextExtent(state.theme, dc, MENU_POPUPITEM, MPI_NORMAL, text.c_str(),
				static_cast<int>(text.length()), DT_SINGLELINE | DT_LEFT | DT_NOPREFIX, nullptr, &rect)))
			{
				return rect.right - rect.left;
			}
		}
		DrawTextW(dc, text.c_str(), static_cast<int>(text.length()), &rect, flags);
		return rect.right - rect.left;
	}

	void LoadLook(State & state)
	{
		state.theme = OpenThemeData(nullptr, L"MENU");
		state.backgroundColor = GetSysColor(COLOR_MENU);
		state.textColor = GetSysColor(COLOR_MENUTEXT);
		if (state.theme != nullptr)
		{
			COLORREF color;
			if (SUCCEEDED(GetThemeColor(state.theme, MENU_POPUPITEM, MPI_NORMAL, TMT_TEXTCOLOR, &color)))
			{
				state.textColor = color;
			}
			// Render the background part once to read its color for solid brushes.
			HDC screen = GetDC(nullptr);
			HDC memory = screen != nullptr ? CreateCompatibleDC(screen) : nullptr;
			HBITMAP pixel = screen != nullptr ? CreateCompatibleBitmap(screen, 16, 16) : nullptr;
			if (memory != nullptr && pixel != nullptr)
			{
				HGDIOBJ old = SelectObject(memory, pixel);
				RECT rect = { 0, 0, 16, 16 };
				DrawThemeBackground(state.theme, memory, MENU_POPUPBACKGROUND, 0, &rect, nullptr);
				state.backgroundColor = GetPixel(memory, 8, 8);
				SelectObject(memory, old);
			}
			if (pixel != nullptr) DeleteObject(pixel);
			if (memory != nullptr) DeleteDC(memory);
			if (screen != nullptr) ReleaseDC(nullptr, screen);
		}
		state.background = CreateSolidBrush(state.backgroundColor);

		// System DPI, like the rest of the app (targetver.h predates per-monitor APIs).
		if (HDC screen = GetDC(nullptr))
		{
			state.dpi = static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSY));
			ReleaseDC(nullptr, screen);
		}
		NONCLIENTMETRICSW metrics = { sizeof(metrics) };
		if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
		{
			state.font = CreateFontIndirectW(&metrics.lfMenuFont);
		}
		BOOL cues = FALSE;
		SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &cues, 0);
		state.showCues = cues != FALSE;

		if (HDC dc = GetDC(nullptr))
		{
			HGDIOBJ old = state.font != nullptr ? SelectObject(dc, state.font) : nullptr;
			TEXTMETRICW tm = {};
			if (GetTextMetricsW(dc, &tm))
			{
				state.textHeight = tm.tmHeight;
			}
			if (old != nullptr)
			{
				SelectObject(dc, old);
			}
			ReleaseDC(nullptr, dc);
		}

		// Row metrics like a menu item: text height plus padding, and a gutter
		// on the left wide enough for the thumbnails.
		state.margin = Scale(state, 4);
		state.padding = Scale(state, 8);
		// Compact like a classic Win32 menu item (about 22 px at 100 %).
		state.itemHeight = state.textHeight + Scale(state, 6);
		state.gutter = Scale(state, 28);
		state.arrowWidth = Scale(state, 20);
		state.separatorHeight = Scale(state, 7);
	}

	void FreeLook(State & state)
	{
		if (state.font != nullptr) DeleteObject(state.font);
		if (state.theme != nullptr) CloseThemeData(state.theme);
		if (state.background != nullptr) DeleteObject(state.background);
	}

	void DrawSeparatorLine(const State & state, HDC dc, RECT line)
	{
		if (state.theme != nullptr)
		{
			SIZE size = {};
			if (SUCCEEDED(GetThemePartSize(state.theme, dc, MENU_POPUPSEPARATOR, 0, nullptr, TS_TRUE, &size)) && size.cy > 0)
			{
				line.bottom = line.top + size.cy;
			}
			DrawThemeBackground(state.theme, dc, MENU_POPUPSEPARATOR, 0, &line, nullptr);
		}
		else
		{
			line.bottom = line.top + 1;
			FillRect(dc, &line, GetSysColorBrush(COLOR_3DSHADOW));
		}
	}

	void DrawThumbnail(HDC dc, HBITMAP bitmap, const RECT & cell)
	{
		BITMAP info = {};
		if (bitmap == nullptr || GetObjectW(bitmap, sizeof(info), &info) == 0 || info.bmWidth <= 0 || info.bmHeight <= 0)
		{
			return;
		}
		HDC memory = CreateCompatibleDC(dc);
		if (memory == nullptr)
		{
			return;
		}
		HGDIOBJ old = SelectObject(memory, bitmap);
		const int x = cell.left + (cell.right - cell.left - info.bmWidth) / 2;
		const int y = cell.top + (cell.bottom - cell.top - info.bmHeight) / 2;
		if (info.bmBitsPixel == 32)
		{
			BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
			AlphaBlend(dc, x, y, info.bmWidth, info.bmHeight, memory, 0, 0, info.bmWidth, info.bmHeight, blend);
		}
		else
		{
			BitBlt(dc, x, y, info.bmWidth, info.bmHeight, memory, 0, 0, SRCCOPY);
		}
		SelectObject(memory, old);
		DeleteDC(memory);
	}

	// One menu row: highlight, optional thumbnail, text, optional submenu arrow.
	void DrawItem(const State & state, HDC dc, const RECT & row, const std::wstring & text, HBITMAP thumbnail,
		bool hot, bool disabled, bool arrow, DWORD textFlags)
	{
		if (hot)
		{
			// A frame around the item, as in the old Winclipper menu, not a filled bar.
			const int inset = Scale(state, 2);
			const int radius = Scale(state, 4);
			HPEN pen = CreatePen(PS_SOLID, Scale(state, 1), GetSysColor(COLOR_HIGHLIGHT));
			HGDIOBJ oldPen = SelectObject(dc, pen);
			HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
			RoundRect(dc, row.left + inset, row.top + inset / 2, row.right - inset, row.bottom - inset / 2, radius, radius);
			SelectObject(dc, oldBrush);
			SelectObject(dc, oldPen);
			DeleteObject(pen);
		}
		if (thumbnail != nullptr)
		{
			const RECT cell = { row.left, row.top, row.left + state.gutter, row.bottom };
			DrawThumbnail(dc, thumbnail, cell);
		}
		RECT textRect = { row.left + state.gutter, row.top, row.right - state.padding - (arrow ? state.arrowWidth : 0), row.bottom };
		const DWORD flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | textFlags;
		const int part = disabled ? MPI_DISABLED : MPI_NORMAL;
		if (state.theme != nullptr)
		{
			DrawThemeText(state.theme, dc, MENU_POPUPITEM, part, text.c_str(), static_cast<int>(text.length()), flags, 0, &textRect);
		}
		else
		{
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, disabled ? GetSysColor(COLOR_GRAYTEXT) : state.textColor);
			DrawTextW(dc, text.c_str(), static_cast<int>(text.length()), &textRect, flags);
		}
		if (arrow)
		{
			RECT arrowRect = { row.right - state.arrowWidth, row.top, row.right, row.bottom };
			if (state.theme != nullptr)
			{
				// Native glyph size, centered; stretching it to the cell made it huge.
				SIZE glyph = {};
				if (FAILED(GetThemePartSize(state.theme, dc, MENU_POPUPSUBMENU, MSM_NORMAL, nullptr, TS_TRUE, &glyph)) || glyph.cx <= 0 || glyph.cy <= 0)
				{
					glyph.cx = glyph.cy = Scale(state, 9);
				}
				const int gx = arrowRect.left + (arrowRect.right - arrowRect.left - glyph.cx) / 2;
				const int gy = arrowRect.top + (arrowRect.bottom - arrowRect.top - glyph.cy) / 2;
				RECT glyphRect = { gx, gy, gx + glyph.cx, gy + glyph.cy };
				DrawThemeBackground(state.theme, dc, MENU_POPUPSUBMENU, MSM_NORMAL, &glyphRect, nullptr);
			}
			else
			{
				DrawTextW(dc, L"›", 1, &arrowRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
			}
		}
	}

	// Clamps a window rect into the work area of the monitor that holds pt.
	void FitOnMonitor(POINT pt, int & x, int & y, int width, int height)
	{
		MONITORINFO monitor = { sizeof(monitor) };
		if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &monitor))
		{
			const RECT & work = monitor.rcWork;
			if (x + width > work.right) x = work.right - width;
			if (y + height > work.bottom) y = work.bottom - height;
			if (x < work.left) x = work.left;
			if (y < work.top) y = work.top;
		}
	}

	void SetupPopup(HWND window)
	{
		// Rounded corners like a Windows 11 menu; ignored on older systems.
		const DWORD corners = 3; // DWMWCP_ROUNDSMALL
		DwmSetWindowAttribute(window, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corners, sizeof(corners));
	}

	void ClipContextMenu(State & state, HWND owner, unsigned int clipIndex);

	// --- Picker ---------------------------------------------------------------

	void PickerPreview(State & state)
	{
		const LRESULT row = SendMessageW(state.list, LB_GETCURSEL, 0, 0);
		RECT rect = {};
		RECT client = {};
		GetClientRect(state.list, &client);
		if (row == LB_ERR || row < 0 || static_cast<size_t>(row) >= state.shown.size()
			|| SendMessageW(state.list, LB_GETITEMRECT, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&rect)) == LB_ERR
			|| rect.top < client.top || rect.bottom > client.bottom)
		{
			HidePreview(state);
			return;
		}
		MapWindowPoints(state.list, nullptr, reinterpret_cast<LPPOINT>(&rect), 2);
		ShowPreview(state, static_cast<int>(state.shown[static_cast<size_t>(row)]), rect);
	}

	void Fill(State & state, int selectClip)
	{
		const int length = GetWindowTextLengthW(state.search);
		std::wstring filter(static_cast<size_t>(length) + 1, L'\0');
		GetWindowTextW(state.search, &filter[0], length + 1);
		filter.resize(static_cast<size_t>(length));

		SendMessageW(state.list, WM_SETREDRAW, FALSE, 0);
		SendMessageW(state.list, LB_RESETCONTENT, 0, 0);
		state.shown.clear();
		state.pinnedRows = 0;

		int selectRow = 0;
		const unsigned int size = state.manager->GetSize();
		for (unsigned int i = 0; i < size; i++)
		{
			std::shared_ptr<Clip> clip = state.manager->GetClipAt(i);
			if (clip == nullptr)
			{
				continue;
			}
			const bool hasText = clip->ContainsFormat(CF_UNICODETEXT) && clip->UnicodeText() != nullptr;
			// Images have no text; search their row label ("[IMAGE] h x w") instead.
			const std::wstring label = hasText ? std::wstring() : RowText(clip);
			const wchar_t * text = hasText ? clip->UnicodeText() : (label.empty() ? nullptr : label.c_str());
			const size_t textLength = hasText ? wcslen(text) : label.length();
			if (!Contains(text, textLength > INT_MAX ? INT_MAX : static_cast<int>(textLength), filter))
			{
				continue;
			}
			if (static_cast<int>(i) == selectClip)
			{
				selectRow = static_cast<int>(state.shown.size());
			}
			if (state.manager->IsClipPinnedAt(i))
			{
				state.pinnedRows++;
			}
			state.shown.push_back(i);
			SendMessageW(state.list, LB_ADDSTRING, 0, static_cast<LPARAM>(i));
		}

		if (!state.shown.empty())
		{
			SendMessageW(state.list, LB_SETCURSEL, selectRow, 0);
			SendMessageW(state.list, LB_SETTOPINDEX, selectRow, 0);
		}
		SendMessageW(state.list, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(state.list, nullptr, TRUE);
		if (IsWindowVisible(state.picker))
		{
			PickerPreview(state);
		}
	}

	void Accept(State & state)
	{
		const LRESULT row = SendMessageW(state.list, LB_GETCURSEL, 0, 0);
		if (row != LB_ERR && row >= 0 && static_cast<size_t>(row) < state.shown.size())
		{
			state.result = static_cast<int>(state.shown[static_cast<size_t>(row)]) + 1;
			state.done = true;
		}
	}

	void MoveSelection(State & state, int delta)
	{
		const int count = static_cast<int>(state.shown.size());
		if (count == 0)
		{
			return;
		}
		int row = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
		row = row < 0 ? 0 : row + delta;
		row = row < 0 ? 0 : (row >= count ? count - 1 : row);
		SendMessageW(state.list, LB_SETCURSEL, row, 0);
		// The list may have scrolled; repaint so the scroll bar moves along.
		InvalidateRect(state.list, nullptr, FALSE);
		PickerPreview(state);
	}

	// One line per wheel notch; the system setting (default 3) is too fast.
	void Wheel(State & state, int delta)
	{
		state.wheelRemainder += delta;
		const int lines = state.wheelRemainder / WHEEL_DELTA;
		if (lines == 0)
		{
			return;
		}
		state.wheelRemainder -= lines * WHEEL_DELTA;
		int top = static_cast<int>(SendMessageW(state.list, LB_GETTOPINDEX, 0, 0)) - lines;
		SendMessageW(state.list, LB_SETTOPINDEX, top < 0 ? 0 : top, 0);
		InvalidateRect(state.list, nullptr, FALSE);
		PickerPreview(state);
	}

	void ClosePicker(State & state)
	{
		if (state.picker == nullptr)
		{
			return;
		}
		HidePreview(state);
		HWND picker = state.picker;
		// Hand the activation back to the menu first, so destroying the picker
		// does not activate some other window and close everything.
		if (state.menu != nullptr && GetActiveWindow() == picker)
		{
			SetActiveWindow(state.menu);
			SetFocus(state.menu);
		}
		SetWindowLongPtrW(picker, AGNOSTIC_USERDATA, 0);
		state.picker = nullptr;
		state.search = nullptr;
		state.list = nullptr;
		state.pickerItem = -1;
		DestroyWindow(picker);
		if (state.menu != nullptr)
		{
			InvalidateRect(state.menu, nullptr, FALSE);
		}
	}

	LRESULT CALLBACK SearchProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
	{
		State & state = *reinterpret_cast<State *>(data);
		switch (message)
		{
		case WM_KEYDOWN:
			switch (wParam)
			{
			case VK_UP:    MoveSelection(state, -1); return 0;
			case VK_DOWN:  MoveSelection(state, 1); return 0;
			case VK_PRIOR: MoveSelection(state, -(VISIBLE_ROWS - 1)); return 0;
			case VK_NEXT:  MoveSelection(state, VISIBLE_ROWS - 1); return 0;
			case VK_RETURN: Accept(state); return 0;
			case VK_ESCAPE:
				// Like a submenu: Esc goes back to the menu.
				if (state.menu != nullptr) ClosePicker(state); else state.done = true;
				return 0;
			case VK_LEFT:
				if (state.menu != nullptr && GetWindowTextLengthW(hWnd) == 0)
				{
					ClosePicker(state);
					return 0;
				}
				break;
			}
			break;
		case WM_CHAR:
			// Swallow Enter/Esc so the single-line edit does not beep.
			if (wParam == L'\r' || wParam == 27)
			{
				return 0;
			}
			break;
		case WM_MOUSEWHEEL:
			Wheel(state, GET_WHEEL_DELTA_WPARAM(wParam));
			return 0;
		}
		return DefSubclassProc(hWnd, message, wParam, lParam);
	}

	RECT ScrollThumb(const State & state);
	void DrawScrollThumb(const State & state, HDC dc, const RECT & clip);

	// Hit area of the position bar: wider than the bar so it is easy to grab.
	bool OnThumb(const State & state, POINT pt)
	{
		RECT thumb = ScrollThumb(state);
		if (IsRectEmpty(&thumb))
		{
			return false;
		}
		RECT client;
		GetClientRect(state.list, &client);
		RECT hit = { client.right - Scale(state, 12), client.top, client.right, client.bottom };
		return PtInRect(&hit, pt) != FALSE;
	}

	void DragThumb(State & state, int y)
	{
		RECT client;
		GetClientRect(state.list, &client);
		const int count = static_cast<int>(state.shown.size());
		const int visible = state.itemHeight > 0 ? (client.bottom - client.top) / state.itemHeight : 0;
		const RECT thumb = ScrollThumb(state);
		const int length = thumb.bottom - thumb.top;
		const int track = client.bottom - client.top - 2 * Scale(state, 2) - length;
		const int maxTop = count - visible;
		if (track <= 0 || maxTop <= 0)
		{
			return;
		}
		int pos = y - state.thumbGrabOffset - client.top - Scale(state, 2);
		pos = pos < 0 ? 0 : (pos > track ? track : pos);
		if (pos == state.thumbDragPos)
		{
			return;
		}
		state.thumbDragPos = pos;
		const int top = MulDiv(pos, maxTop, track);
		// No redraw while scrolling: the list would scroll the old pixels
		// (bar included) and repaint row by row, which flickers.
		SendMessageW(state.list, WM_SETREDRAW, FALSE, 0);
		SendMessageW(state.list, LB_SETTOPINDEX, top, 0);
		SendMessageW(state.list, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(state.list, nullptr, FALSE);
		UpdateWindow(state.list);
	}

	void SetThumbHot(State & state, bool hot)
	{
		if (state.thumbHot != hot)
		{
			state.thumbHot = hot;
			InvalidateRect(state.list, nullptr, FALSE);
		}
		if (hot)
		{
			TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, state.list, 0 };
			TrackMouseEvent(&track);
		}
	}

	LRESULT CALLBACK ListProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
	{
		State & state = *reinterpret_cast<State *>(data);
		switch (message)
		{
		case WM_MOUSEMOVE:
			{
				if (state.menu != nullptr)
				{
					KillTimer(state.menu, TIMER_CLOSE_PICKER);
				}
				// Only a real cursor move counts, and the highlight only changes
				// once the cursor is over a different row.
				const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				if (pt.x == state.lastListMouse.x && pt.y == state.lastListMouse.y)
				{
					return 0;
				}
				state.lastListMouse = pt;
				if (state.thumbDragging)
				{
					DragThumb(state, pt.y);
					return 0;
				}
				SetThumbHot(state, OnThumb(state, pt));
				if (state.thumbHot)
				{
					return 0;
				}
				const LRESULT hit = SendMessageW(hWnd, LB_ITEMFROMPOINT, 0, lParam);
				if (HIWORD(hit) == 0)
				{
					const int row = LOWORD(hit);
					if (row != static_cast<int>(SendMessageW(hWnd, LB_GETCURSEL, 0, 0)))
					{
						SendMessageW(hWnd, LB_SETCURSEL, row, 0);
						PickerPreview(state);
					}
				}
			}
			return 0;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			{
				const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				if (OnThumb(state, pt))
				{
					// Grab the bar; a click beside it centers the bar there.
					const RECT thumb = ScrollThumb(state);
					state.thumbGrabOffset = (pt.y >= thumb.top && pt.y < thumb.bottom) ? pt.y - thumb.top : (thumb.bottom - thumb.top) / 2;
					state.thumbDragging = true;
					state.thumbDragPos = -1;
					SetCapture(hWnd);
					DragThumb(state, pt.y);
				}
			}
			SetFocus(state.search);
			return 0;
		case WM_CAPTURECHANGED:
			if (state.thumbDragging)
			{
				state.thumbDragging = false;
				state.thumbDragPos = -1;
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			break;
		case WM_MOUSELEAVE:
			if (!state.thumbDragging)
			{
				SetThumbHot(state, false);
			}
			break;
		case WM_RBUTTONDOWN:
			// Keep the keyboard focus in the search box.
			SetFocus(state.search);
			return 0;
		case WM_LBUTTONUP:
			{
				if (state.thumbDragging || state.thumbHot)
				{
					// End of a drag on the position bar; no clip is chosen.
					if (GetCapture() == hWnd)
					{
						ReleaseCapture();
					}
					return 0;
				}
				const LRESULT hit = SendMessageW(hWnd, LB_ITEMFROMPOINT, 0, lParam);
				if (HIWORD(hit) == 0)
				{
					SendMessageW(hWnd, LB_SETCURSEL, LOWORD(hit), 0);
					Accept(state);
				}
			}
			return 0;
		case WM_RBUTTONUP:
			{
				const LRESULT hit = SendMessageW(hWnd, LB_ITEMFROMPOINT, 0, lParam);
				const size_t row = LOWORD(hit);
				if (HIWORD(hit) == 0 && row < state.shown.size())
				{
					SendMessageW(hWnd, LB_SETCURSEL, row, 0);
					ClipContextMenu(state, state.picker, state.shown[row]);
				}
			}
			return 0;
		case WM_MOUSEWHEEL:
			Wheel(state, GET_WHEEL_DELTA_WPARAM(wParam));
			return 0;
		case WM_SETFOCUS:
			SetFocus(state.search);
			return 0;
		case WM_ERASEBKGND:
			// WM_PAINT fills the background in its buffer.
			return 1;
		case WM_PAINT:
			{
				// Double-buffered: rows and the position bar reach the screen in one blit.
				PAINTSTRUCT ps;
				HDC dc = BeginPaint(hWnd, &ps);
				RECT client;
				GetClientRect(hWnd, &client);
				HDC memory = CreateCompatibleDC(dc);
				HBITMAP bitmap = memory != nullptr ? CreateCompatibleBitmap(dc, client.right, client.bottom) : nullptr;
				if (bitmap != nullptr)
				{
					HGDIOBJ old = SelectObject(memory, bitmap);
					FillRect(memory, &client, state.background);
					DefSubclassProc(hWnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT);
					DrawScrollThumb(state, memory, client);
					BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top, memory, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
					SelectObject(memory, old);
					DeleteObject(bitmap);
				}
				else
				{
					FillRect(dc, &client, state.background);
					DefSubclassProc(hWnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
				}
				if (memory != nullptr)
				{
					DeleteDC(memory);
				}
				EndPaint(hWnd, &ps);
			}
			return 0;
		}
		return DefSubclassProc(hWnd, message, wParam, lParam);
	}

	// Thin scroll position bar like Windows 11 overlay scrollbars; the list has
	// no real scrollbar. Empty when all rows fit.
	RECT ScrollThumb(const State & state)
	{
		RECT thumb = {};
		RECT client;
		const int count = static_cast<int>(state.shown.size());
		if (state.list == nullptr || !GetClientRect(state.list, &client) || state.itemHeight <= 0)
		{
			return thumb;
		}
		const int visible = (client.bottom - client.top) / state.itemHeight;
		if (visible <= 0 || count <= visible)
		{
			return thumb;
		}
		const int top = static_cast<int>(SendMessageW(state.list, LB_GETTOPINDEX, 0, 0));
		const int track = client.bottom - client.top - 2 * Scale(state, 2);
		int length = MulDiv(track, visible, count);
		if (length < Scale(state, 16)) length = Scale(state, 16);
		const int maxTop = count - visible;
		int offset = maxTop > 0 ? MulDiv(track - length, top < maxTop ? top : maxTop, maxTop) : 0;
		if (state.thumbDragging && state.thumbDragPos >= 0)
		{
			offset = state.thumbDragPos > track - length ? track - length : state.thumbDragPos;
		}
		const int width = Scale(state, state.thumbHot || state.thumbDragging ? 6 : 3);
		thumb.right = client.right - Scale(state, 3);
		thumb.left = thumb.right - width;
		thumb.top = client.top + Scale(state, 2) + offset;
		thumb.bottom = thumb.top + length;
		return thumb;
	}

	void DrawScrollThumb(const State & state, HDC dc, const RECT & clip)
	{
		const RECT thumb = ScrollThumb(state);
		if (IsRectEmpty(&thumb))
		{
			return;
		}
		const int saved = SaveDC(dc);
		IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
		HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_GRAYTEXT));
		HGDIOBJ oldBrush = SelectObject(dc, brush);
		HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
		const int r = thumb.right - thumb.left;
		RoundRect(dc, thumb.left, thumb.top, thumb.right + 1, thumb.bottom + 1, r, r);
		SelectObject(dc, oldPen);
		SelectObject(dc, oldBrush);
		DeleteObject(brush);
		RestoreDC(dc, saved);
	}

	void DrawRow(State & state, const DRAWITEMSTRUCT & item)
	{
		if (item.itemID == static_cast<UINT>(-1) || item.itemID >= state.shown.size())
		{
			return;
		}
		// ODA_FOCUS only toggles the dotted focus frame, which menus do not show.
		if (item.itemAction == ODA_FOCUS)
		{
			return;
		}
		HDC dc = item.hDC;
		RECT row = item.rcItem;
		FillRect(dc, &row, state.background);

		std::shared_ptr<Clip> clip = state.manager->GetClipAt(state.shown[item.itemID]);
		if (clip != nullptr)
		{
			DrawItem(state, dc, row, RowText(clip), clip->ContainsFormat(CF_DIB) ? clip->GetThumbnail() : nullptr,
				(item.itemState & ODS_SELECTED) != 0, false, false, DT_NOPREFIX);
		}
		// Separator between the pinned and the other clips, as in the menu.
		if (item.itemID == state.pinnedRows && item.itemID > 0)
		{
			DrawSeparatorLine(state, dc, { row.left + state.gutter, row.top, row.right - state.padding, row.top + 1 });
		}
		DrawScrollThumb(state, dc, row);
	}

	LRESULT CALLBACK PickerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		State * state = reinterpret_cast<State *>(GetWindowLongPtrW(hWnd, AGNOSTIC_USERDATA));
		if (state == nullptr)
		{
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
		switch (message)
		{
		case WM_COMMAND:
			if (LOWORD(wParam) == ID_SEARCH && HIWORD(wParam) == EN_CHANGE)
			{
				Fill(*state, -1);
				return 0;
			}
			break;
		case WM_DRAWITEM:
			if (wParam == ID_LIST)
			{
				DrawRow(*state, *reinterpret_cast<const DRAWITEMSTRUCT *>(lParam));
				return TRUE;
			}
			break;
		case WM_CTLCOLOREDIT:
			{
				HDC dc = reinterpret_cast<HDC>(wParam);
				SetTextColor(dc, state->textColor);
				SetBkColor(dc, state->backgroundColor);
				return reinterpret_cast<LRESULT>(state->background);
			}
		case WM_ERASEBKGND:
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				FillRect(reinterpret_cast<HDC>(wParam), &rect, state->background);
				return 1;
			}
		case WM_PAINT:
			{
				// Separator line between the search box and the list.
				PAINTSTRUCT ps;
				HDC dc = BeginPaint(hWnd, &ps);
				RECT search;
				GetWindowRect(state->list, &search);
				MapWindowPoints(nullptr, hWnd, reinterpret_cast<LPPOINT>(&search), 2);
				RECT client;
				GetClientRect(hWnd, &client);
				// Centered in the gap above the list, like a menu separator.
				const int lineY = search.top - state->separatorHeight / 2 - 1;
				DrawSeparatorLine(*state, dc, { client.left + state->padding, lineY,
					client.right - state->padding, lineY + 1 });
				EndPaint(hWnd, &ps);
				return 0;
			}
		case WM_SETFOCUS:
			SetFocus(state->search);
			return 0;
		case WM_MOUSEACTIVATE:
			// Clicking the picker gives it the keyboard, like a real window.
			return MA_ACTIVATE;
		case WM_ACTIVATE:
			// Clicking anywhere outside the menu and the picker closes both, like a menu.
			if (LOWORD(wParam) == WA_INACTIVE && !IsOurs(*state, reinterpret_cast<HWND>(lParam)))
			{
				state->done = true;
			}
			break;
		case WM_CLOSE:
			state->done = true;
			return 0;
		}
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	// Opens the picker. anchor: screen rect of the menu row it belongs to; the
	// picker then opens beside the menu like a submenu. Otherwise at pos.
	bool OpenPicker(State & state, POINT pos, const RECT * anchor, unsigned int initialIndex, bool takeFocus)
	{
		HINSTANCE instance = GetModuleHandleW(nullptr);
		HWND owner = state.menu != nullptr ? state.menu : state.options->owner;
		HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, PICKER_CLASS, L"Winclipper",
			WS_POPUP, pos.x, pos.y, 100, 100, owner, nullptr, instance, nullptr);
		if (window == nullptr)
		{
			return false;
		}
		SetupPopup(window);
		state.picker = window;
		state.search = CreateWindowExW(0, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
			0, 0, 10, 10, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SEARCH)), instance, nullptr);
		state.list = CreateWindowExW(0, WC_LISTBOXW, L"",
			WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED,
			0, 0, 10, 10, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LIST)), instance, nullptr);
		if (state.search == nullptr || state.list == nullptr)
		{
			DestroyWindow(window);
			state.picker = state.search = state.list = nullptr;
			return false;
		}
		SetWindowTheme(state.list, L"Explorer", nullptr);
		if (state.font != nullptr)
		{
			SendMessageW(state.search, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), FALSE);
			SendMessageW(state.list, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), FALSE);
		}
		SendMessageW(state.search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search clips"));
		SetWindowSubclass(state.search, SearchProc, 1, reinterpret_cast<DWORD_PTR>(&state));
		SetWindowSubclass(state.list, ListProc, 1, reinterpret_cast<DWORD_PTR>(&state));
		SendMessageW(state.list, LB_SETITEMHEIGHT, 0, state.itemHeight);
		state.lastListMouse = { LONG_MIN, LONG_MIN };
		state.thumbDragging = state.thumbHot = false;
		state.wheelRemainder = 0;

		// As wide as the longest row, like a menu, within sensible limits.
		int widest = 0;
		if (HDC dc = GetDC(window))
		{
			HGDIOBJ old = state.font != nullptr ? SelectObject(dc, state.font) : nullptr;
			const unsigned int size = state.manager->GetSize();
			for (unsigned int i = 0; i < size; i++)
			{
				std::shared_ptr<Clip> clip = state.manager->GetClipAt(i);
				if (clip == nullptr)
				{
					continue;
				}
				const std::wstring text = RowText(clip);
				const int w = TextWidth(state, dc, text);
				if (w > widest)
				{
					widest = w;
				}
			}
			if (old != nullptr)
			{
				SelectObject(dc, old);
			}
			ReleaseDC(window, dc);
		}
		const int margin = state.margin;
		int width = 2 * margin + state.gutter + widest + state.padding;
		if (width < Scale(state, 320)) width = Scale(state, 320);
		if (width > Scale(state, 720)) width = Scale(state, 720);

		// Layout: search box on top, a separator, then the list.
		// The search box takes one menu row; the edit is text-high and centered in it.
		// No scroll bar: the list scrolls with the wheel and the keyboard.
		const int searchHeight = state.textHeight;
		const int searchTop = margin + (state.itemHeight - searchHeight) / 2;
		const int listTop = margin + state.itemHeight + state.separatorHeight;
		// Like the old submenu: as tall as the monitor's work area allows (so taller
		// than the main menu), but no taller than needed for all clips.
		int rows = VISIBLE_ROWS;
		{
			MONITORINFO work = { sizeof(work) };
			const POINT at = anchor != nullptr ? POINT{ anchor->left, anchor->top } : pos;
			if (GetMonitorInfoW(MonitorFromPoint(at, MONITOR_DEFAULTTONEAREST), &work) && state.itemHeight > 0)
			{
				const int fit = (work.rcWork.bottom - work.rcWork.top - listTop - margin) / state.itemHeight;
				if (fit > 0) rows = fit;
			}
			const int clipCount = static_cast<int>(state.manager->GetSize());
			if (clipCount > 0 && clipCount < rows) rows = clipCount;
		}
		const int listHeight = state.itemHeight * rows;
		MoveWindow(state.search, margin + state.gutter, searchTop, width - 2 * margin - state.gutter - state.padding, searchHeight, FALSE);
		MoveWindow(state.list, margin, listTop, width - 2 * margin, listHeight, FALSE);
		const int height = listTop + listHeight + margin;

		int x = pos.x - Scale(state, 5);
		int y = pos.y - Scale(state, 5);
		POINT monitorPoint = pos;
		if (anchor != nullptr && state.menu != nullptr)
		{
			// Beside the menu, top edges aligned with the row, like a submenu.
			RECT menuRect;
			GetWindowRect(state.menu, &menuRect);
			x = menuRect.right;
			y = anchor->top - margin;
			monitorPoint = { anchor->left, anchor->top };
			MONITORINFO monitor = { sizeof(monitor) };
			// No room on the right: open to the left of the menu, as submenus do.
			if (GetMonitorInfoW(MonitorFromPoint(monitorPoint, MONITOR_DEFAULTTONEAREST), &monitor)
				&& x + width > monitor.rcWork.right && menuRect.left - width >= monitor.rcWork.left)
			{
				x = menuRect.left - width;
			}
		}
		FitOnMonitor(monitorPoint, x, y, width, height);

		SetWindowLongPtrW(window, AGNOSTIC_USERDATA, reinterpret_cast<LONG_PTR>(&state));
		SetWindowPos(window, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
		Fill(state, static_cast<int>(initialIndex));
		if (takeFocus)
		{
			ShowWindow(window, SW_SHOW);
			SetForegroundWindow(window);
			SetFocus(state.search);
		}
		else
		{
			// Opened by hovering: the keyboard stays in the menu until the picker is clicked.
			ShowWindow(window, SW_SHOWNA);
		}
		PickerPreview(state);
		return true;
	}

	// --- Menu -----------------------------------------------------------------

	bool Selectable(const MenuItem & item)
	{
		return item.kind == ITEM_CLIP || item.kind == ITEM_COMMAND || item.kind == ITEM_MORE;
	}

	// Same entries as the Win32 clips menu had.
	void BuildItems(State & state)
	{
		state.items.clear();
		const unsigned int size = state.manager->GetSize();
		const unsigned int maxDisplay = static_cast<unsigned int>(UserSettings::MaxDisplayClips());
		if (size > 0)
		{
			const unsigned int count = size > maxDisplay ? maxDisplay : size;
			bool previousPinned = false;
			for (unsigned int i = 0; i < count; i++)
			{
				std::shared_ptr<Clip> clip = state.manager->GetClipAt(i);
				if (clip == nullptr)
				{
					continue;
				}
				// Pinned clips come first; a separator follows them.
				const bool pinned = state.manager->IsClipPinnedAt(i);
				if (previousPinned && !pinned)
				{
					state.items.push_back({ ITEM_SEPARATOR, 0, L"" });
				}
				previousPinned = pinned;
				state.items.push_back({ ITEM_CLIP, static_cast<int>(i) + 1, RowText(clip) });
			}
			if (size > maxDisplay)
			{
				state.items.push_back({ ITEM_MORE, MORE_SELECT, L"More..." });
			}
		}
		else
		{
			state.items.push_back({ ITEM_DISABLED, 0, L"Nothing on the clipboard" });
		}
		state.items.push_back({ ITEM_SEPARATOR, 0, L"" });
		state.items.push_back({ ITEM_COMMAND, CLEARCLIPS_SELECT, L"&Clear Clips" });
		state.items.push_back({ ITEM_SEPARATOR, 0, L"" });
		if (!state.options->updateVersion.empty())
		{
			state.items.push_back({ ITEM_COMMAND, UPDATE_SELECT, L"&Install update " + state.options->updateVersion + L"..." });
		}
		state.items.push_back({ ITEM_COMMAND, SETTINGS_SELECT, L"&Settings" });
		if (state.options->showExit)
		{
			state.items.push_back({ ITEM_COMMAND, EXIT_SELECT, L"&Exit" });
		}
	}

	// Sizes the menu to its items. place: position at the cursor like a popup
	// menu; otherwise keep the current position (after a pin or delete).
	void LayoutMenu(State & state, bool place)
	{
		int widest = 0;
		int top = state.margin;
		if (HDC dc = GetDC(state.menu))
		{
			HGDIOBJ old = state.font != nullptr ? SelectObject(dc, state.font) : nullptr;
			for (MenuItem & item : state.items)
			{
				item.top = top;
				item.height = item.kind == ITEM_SEPARATOR ? state.separatorHeight : state.itemHeight;
				top += item.height;
				if (item.kind == ITEM_SEPARATOR)
				{
					continue;
				}
				const std::wstring text = item.kind == ITEM_CLIP ? item.text : StripMnemonic(item.text);
				const int w = TextWidth(state, dc, text);
				if (w > widest)
				{
					widest = w;
				}
			}
			if (old != nullptr)
			{
				SelectObject(dc, old);
			}
			ReleaseDC(state.menu, dc);
		}
		state.contentHeight = top + state.margin;

		int width = 2 * state.margin + state.gutter + widest + state.padding + state.arrowWidth;
		POINT anchor = state.options->pos;
		int x = anchor.x - Scale(state, 5);
		int y = anchor.y - Scale(state, 5);
		if (!place)
		{
			RECT current;
			GetWindowRect(state.menu, &current);
			x = current.left;
			y = current.top;
			anchor = { current.left, current.top };
		}
		int height = state.contentHeight;
		MONITORINFO monitor = { sizeof(monitor) };
		if (GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &monitor))
		{
			const RECT & work = monitor.rcWork;
			if (height > work.bottom - work.top) height = work.bottom - work.top;
			if (width > work.right - work.left) width = work.right - work.left;
			// Not enough room below the cursor: open upwards, like a menu.
			if (place && y + height > work.bottom && anchor.y - height >= work.top)
			{
				y = anchor.y - height;
			}
		}
		FitOnMonitor(anchor, x, y, width, height);
		state.viewHeight = height;
		const int maxScroll = state.contentHeight - state.viewHeight;
		if (state.scroll > maxScroll) state.scroll = maxScroll > 0 ? maxScroll : 0;
		SetWindowPos(state.menu, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
		InvalidateRect(state.menu, nullptr, FALSE);
	}

	RECT ItemRect(const State & state, int index)
	{
		RECT client;
		GetClientRect(state.menu, &client);
		const MenuItem & item = state.items[static_cast<size_t>(index)];
		return { client.left + state.margin, item.top - state.scroll, client.right - state.margin, item.top - state.scroll + item.height };
	}

	int ItemFromPoint(const State & state, POINT pt)
	{
		RECT client;
		GetClientRect(state.menu, &client);
		if (!PtInRect(&client, pt))
		{
			return -1;
		}
		for (size_t i = 0; i < state.items.size(); i++)
		{
			const RECT rect = ItemRect(state, static_cast<int>(i));
			if (pt.y >= rect.top && pt.y < rect.bottom)
			{
				return Selectable(state.items[i]) ? static_cast<int>(i) : -1;
			}
		}
		return -1;
	}

	void EnsureVisible(State & state, int index)
	{
		const MenuItem & item = state.items[static_cast<size_t>(index)];
		const int oldScroll = state.scroll;
		if (item.top - state.margin < state.scroll)
		{
			state.scroll = item.top - state.margin;
		}
		else if (item.top + item.height + state.margin > state.scroll + state.viewHeight)
		{
			state.scroll = item.top + item.height + state.margin - state.viewHeight;
		}
		if (state.scroll < 0) state.scroll = 0;
		if (state.scroll != oldScroll)
		{
			InvalidateRect(state.menu, nullptr, FALSE);
		}
	}

	void SetHot(State & state, int index, bool fromMouse)
	{
		if (index == state.hot)
		{
			return;
		}
		if (state.hot >= 0)
		{
			const RECT old = ItemRect(state, state.hot);
			InvalidateRect(state.menu, &old, FALSE);
		}
		state.hot = index;
		KillTimer(state.menu, TIMER_OPEN_PICKER);
		if (index < 0)
		{
			HidePreview(state);
			return;
		}
		if (!fromMouse)
		{
			EnsureVisible(state, index);
		}
		const RECT rect = ItemRect(state, index);
		InvalidateRect(state.menu, &rect, FALSE);

		const MenuItem & item = state.items[static_cast<size_t>(index)];
		if (item.kind == ITEM_CLIP)
		{
			RECT screen = rect;
			MapWindowPoints(state.menu, nullptr, reinterpret_cast<LPPOINT>(&screen), 2);
			ShowPreview(state, item.id - 1, screen);
		}
		else
		{
			HidePreview(state);
		}

		UINT delay = 400;
		SystemParametersInfoW(SPI_GETMENUSHOWDELAY, 0, &delay, 0);
		if (delay == 0) delay = 1;
		if (index == state.pickerItem)
		{
			KillTimer(state.menu, TIMER_CLOSE_PICKER);
		}
		else if (fromMouse)
		{
			// Hovering "More..." opens the picker, hovering another row closes it,
			// both after the system submenu delay, like a real submenu.
			if (item.kind == ITEM_MORE)
			{
				SetTimer(state.menu, TIMER_OPEN_PICKER, delay, nullptr);
			}
			if (state.picker != nullptr)
			{
				SetTimer(state.menu, TIMER_CLOSE_PICKER, delay, nullptr);
			}
		}
	}

	void OpenPickerForItem(State & state, int index, bool takeFocus)
	{
		if (state.picker != nullptr && state.pickerItem == index)
		{
			if (takeFocus)
			{
				SetForegroundWindow(state.picker);
				SetFocus(state.search);
			}
			return;
		}
		ClosePicker(state);
		const MenuItem & item = state.items[static_cast<size_t>(index)];
		RECT anchor = ItemRect(state, index);
		MapWindowPoints(state.menu, nullptr, reinterpret_cast<LPPOINT>(&anchor), 2);
		// "More..." starts at the first clip the menu does not show.
		const unsigned int first = item.kind == ITEM_MORE ? static_cast<unsigned int>(UserSettings::MaxDisplayClips()) : 0;
		if (OpenPicker(state, { anchor.left, anchor.top }, &anchor, first, takeFocus))
		{
			state.pickerItem = index;
		}
	}

	void ActivateItem(State & state, int index, bool byKeyboard)
	{
		if (index < 0 || static_cast<size_t>(index) >= state.items.size())
		{
			return;
		}
		const MenuItem & item = state.items[static_cast<size_t>(index)];
		if (item.kind == ITEM_MORE || (item.kind == ITEM_COMMAND && item.id == SEARCH_SELECT))
		{
			SetHot(state, index, !byKeyboard);
			OpenPickerForItem(state, index, true);
			return;
		}
		if (item.kind == ITEM_CLIP || item.kind == ITEM_COMMAND)
		{
			state.result = item.id;
			state.done = true;
		}
	}

	void MoveHot(State & state, int direction)
	{
		const int count = static_cast<int>(state.items.size());
		int index = state.hot;
		for (int step = 0; step < count; step++)
		{
			index = index < 0 ? (direction > 0 ? 0 : count - 1) : (index + direction + count) % count;
			if (Selectable(state.items[static_cast<size_t>(index)]))
			{
				SetHot(state, index, false);
				return;
			}
		}
	}

	// After a pin or delete the clip order changed; rebuild both lists.
	void Rebuild(State & state)
	{
		ClosePicker(state);
		HidePreview(state);
		state.hot = -1;
		BuildItems(state);
		LayoutMenu(state, false);
	}

	void ClipContextMenu(State & state, HWND owner, unsigned int clipIndex)
	{
		constexpr int DELETE_SELECT = 1;
		constexpr int PIN_SELECT = 2;
		HidePreview(state);
		HMENU menu = CreatePopupMenu();
		if (menu == nullptr)
		{
			return;
		}
		AppendMenuW(menu, MF_STRING, PIN_SELECT, state.manager->IsClipPinnedAt(clipIndex) ? L"Unpin" : L"Pin");
		AppendMenuW(menu, MF_STRING, DELETE_SELECT, L"Delete");
		POINT pt = {};
		GetCursorPos(&pt);
		const int choice = TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
			pt.x, pt.y, owner, nullptr);
		DestroyMenu(menu);
		if (choice != DELETE_SELECT && choice != PIN_SELECT)
		{
			return;
		}
		const bool inPicker = state.picker != nullptr && owner == state.picker;
		if (choice == DELETE_SELECT)
		{
			state.manager->DeleteClipAt(clipIndex);
		}
		else
		{
			state.manager->TogglePinAt(clipIndex);
		}
		if (state.menu == nullptr || inPicker)
		{
			// Stay in the picker; the menu behind it is rebuilt when it closes.
			Fill(state, static_cast<int>(clipIndex));
			if (state.menu != nullptr)
			{
				// Rebuild the menu behind the picker and keep the picker attached
				// to its row ("More..." may move or disappear).
				const int pickerId = state.pickerItem >= 0 ? state.items[static_cast<size_t>(state.pickerItem)].id : 0;
				state.hot = -1;
				state.pickerItem = -1;
				BuildItems(state);
				LayoutMenu(state, false);
				for (size_t i = 0; i < state.items.size(); i++)
				{
					if (pickerId != 0 && state.items[i].kind != ITEM_CLIP && state.items[i].id == pickerId)
					{
						state.pickerItem = static_cast<int>(i);
					}
				}
			}
		}
		else
		{
			Rebuild(state);
		}
	}

	void PaintMenu(State & state, HDC target)
	{
		RECT client;
		GetClientRect(state.menu, &client);
		const int width = client.right - client.left;
		const int height = client.bottom - client.top;
		// Double-buffered to avoid flicker while the highlight moves.
		HDC dc = CreateCompatibleDC(target);
		HBITMAP buffer = dc != nullptr ? CreateCompatibleBitmap(target, width, height) : nullptr;
		if (dc == nullptr || buffer == nullptr)
		{
			if (buffer != nullptr) DeleteObject(buffer);
			if (dc != nullptr) DeleteDC(dc);
			return;
		}
		HGDIOBJ oldBitmap = SelectObject(dc, buffer);
		HGDIOBJ oldFont = state.font != nullptr ? SelectObject(dc, state.font) : nullptr;
		FillRect(dc, &client, state.background);

		for (size_t i = 0; i < state.items.size(); i++)
		{
			const MenuItem & item = state.items[i];
			const RECT row = ItemRect(state, static_cast<int>(i));
			if (row.bottom <= client.top || row.top >= client.bottom)
			{
				continue;
			}
			if (item.kind == ITEM_SEPARATOR)
			{
				const int middle = row.top + (row.bottom - row.top) / 2;
				DrawSeparatorLine(state, dc, { row.left + state.gutter, middle, row.right - state.padding, middle + 1 });
				continue;
			}
			HBITMAP thumbnail = nullptr;
			if (item.kind == ITEM_CLIP)
			{
				std::shared_ptr<Clip> clip = state.manager->GetClipAt(static_cast<unsigned int>(item.id - 1));
				if (clip != nullptr && clip->ContainsFormat(CF_DIB))
				{
					thumbnail = clip->GetThumbnail();
				}
			}
			const bool hot = static_cast<int>(i) == state.hot || static_cast<int>(i) == state.pickerItem;
			DWORD flags = item.kind == ITEM_CLIP ? DT_NOPREFIX : (state.showCues ? 0 : DT_HIDEPREFIX);
			DrawItem(state, dc, row, item.text, thumbnail, hot, item.kind == ITEM_DISABLED, item.kind == ITEM_MORE, flags);
		}

		BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
		if (oldFont != nullptr) SelectObject(dc, oldFont);
		SelectObject(dc, oldBitmap);
		DeleteObject(buffer);
		DeleteDC(dc);
	}

	LRESULT CALLBACK MenuProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		State * state = reinterpret_cast<State *>(GetWindowLongPtrW(hWnd, AGNOSTIC_USERDATA));
		if (state == nullptr)
		{
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
		switch (message)
		{
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC dc = BeginPaint(hWnd, &ps);
				PaintMenu(*state, dc);
				EndPaint(hWnd, &ps);
				return 0;
			}
		case WM_MOUSEMOVE:
			{
				// Only a real cursor move counts (Windows also sends one when a
				// window appears under a resting cursor).
				const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				if (pt.x == state->lastMenuMouse.x && pt.y == state->lastMenuMouse.y)
				{
					return 0;
				}
				state->lastMenuMouse = pt;
				if (!state->trackingLeave)
				{
					TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, hWnd, 0 };
					state->trackingLeave = TrackMouseEvent(&track) != FALSE;
				}
				const int index = ItemFromPoint(*state, pt);
				if (index >= 0)
				{
					SetHot(*state, index, true);
				}
				return 0;
			}
		case WM_MOUSELEAVE:
			state->trackingLeave = false;
			state->lastMenuMouse = { LONG_MIN, LONG_MIN };
			// Like a menu, the highlight goes when the cursor leaves, unless
			// it belongs to the open picker.
			if (state->hot != state->pickerItem)
			{
				SetHot(*state, -1, true);
			}
			return 0;
		case WM_LBUTTONUP:
			ActivateItem(*state, ItemFromPoint(*state, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }), false);
			return 0;
		case WM_RBUTTONUP:
			{
				const int index = ItemFromPoint(*state, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
				if (index >= 0 && state->items[static_cast<size_t>(index)].kind == ITEM_CLIP)
				{
					SetHot(*state, index, true);
					ClipContextMenu(*state, hWnd, static_cast<unsigned int>(state->items[static_cast<size_t>(index)].id - 1));
				}
				return 0;
			}
		case WM_MOUSEWHEEL:
			// Only when the menu is taller than the screen; one line per notch.
			if (state->contentHeight > state->viewHeight)
			{
				state->menuWheelRemainder += GET_WHEEL_DELTA_WPARAM(wParam);
				const int lines = state->menuWheelRemainder / WHEEL_DELTA;
				state->menuWheelRemainder -= lines * WHEEL_DELTA;
				const int maxScroll = state->contentHeight - state->viewHeight;
				state->scroll -= lines * state->itemHeight;
				if (state->scroll > maxScroll) state->scroll = maxScroll;
				if (state->scroll < 0) state->scroll = 0;
				HidePreview(*state);
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			return 0;
		case WM_KEYDOWN:
			if (!state->showCues)
			{
				state->showCues = true;
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			switch (wParam)
			{
			case VK_UP:     MoveHot(*state, -1); return 0;
			case VK_DOWN:   MoveHot(*state, 1); return 0;
			case VK_HOME:   state->hot = -1; MoveHot(*state, 1); return 0;
			case VK_END:    state->hot = -1; MoveHot(*state, -1); return 0;
			case VK_RETURN:
			case VK_SPACE:  ActivateItem(*state, state->hot, true); return 0;
			case VK_ESCAPE: state->done = true; return 0;
			case VK_RIGHT:
				if (state->hot >= 0)
				{
					const MenuItem & item = state->items[static_cast<size_t>(state->hot)];
					if (item.kind == ITEM_MORE)
					{
						ActivateItem(*state, state->hot, true);
					}
				}
				return 0;
			case VK_LEFT:
				ClosePicker(*state);
				return 0;
			case VK_APPS:
				if (state->hot >= 0 && state->items[static_cast<size_t>(state->hot)].kind == ITEM_CLIP)
				{
					ClipContextMenu(*state, hWnd, static_cast<unsigned int>(state->items[static_cast<size_t>(state->hot)].id - 1));
				}
				return 0;
			}
			break;
		case WM_CHAR:
			{
				// Mnemonics: "&Clear Clips" -> C, "&Settings" -> S, ...
				const wchar_t key = static_cast<wchar_t>(towupper(static_cast<wint_t>(wParam)));
				for (size_t i = 0; i < state->items.size(); i++)
				{
					const MenuItem & item = state->items[i];
					if (item.kind == ITEM_COMMAND && Mnemonic(item.text) == key)
					{
						ActivateItem(*state, static_cast<int>(i), true);
						break;
					}
				}
				return 0;
			}
		case WM_TIMER:
			KillTimer(hWnd, wParam);
			if (wParam == TIMER_OPEN_PICKER && state->hot >= 0
				&& state->items[static_cast<size_t>(state->hot)].kind == ITEM_MORE)
			{
				OpenPickerForItem(*state, state->hot, false);
			}
			else if (wParam == TIMER_CLOSE_PICKER && state->picker != nullptr && state->hot != state->pickerItem)
			{
				// Keep a picker the user is typing in, and one the cursor went into.
				POINT cursor = {};
				RECT pickerRect = {};
				GetCursorPos(&cursor);
				GetWindowRect(state->picker, &pickerRect);
				if (GetActiveWindow() != state->picker && !PtInRect(&pickerRect, cursor))
				{
					ClosePicker(*state);
					if (state->hot >= 0)
					{
						// Re-show the preview of the row under the cursor.
						const int hot = state->hot;
						state->hot = -1;
						SetHot(*state, hot, true);
					}
				}
			}
			return 0;
		case WM_MOUSEACTIVATE:
			return MA_ACTIVATE;
		case WM_ACTIVATE:
			// Clicking anywhere outside the menu and the picker closes both, like a menu.
			if (LOWORD(wParam) == WA_INACTIVE && !IsOurs(*state, reinterpret_cast<HWND>(lParam)))
			{
				state->done = true;
			}
			break;
		case WM_CLOSE:
			state->done = true;
			return 0;
		}
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	bool RegisterClasses(HINSTANCE instance)
	{
		static bool registered = false;
		if (registered)
		{
			return true;
		}
		WNDCLASSEXW wc = { sizeof(wc) };
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.style = CS_DROPSHADOW;
		wc.lpfnWndProc = PickerProc;
		wc.lpszClassName = PICKER_CLASS;
		if (RegisterClassExW(&wc) == 0)
		{
			return false;
		}
		wc.lpfnWndProc = MenuProc;
		wc.lpszClassName = MENU_CLASS;
		if (RegisterClassExW(&wc) == 0)
		{
			UnregisterClassW(PICKER_CLASS, instance);
			return false;
		}
		registered = true;
		return true;
	}
}

int ShowClipMenu(ClipsManager & manager, const ClipMenuOptions & options)
{
	HINSTANCE instance = GetModuleHandleW(nullptr);
	if (!RegisterClasses(instance))
	{
		return CANCELED_SELECTION;
	}

	State state;
	state.manager = &manager;
	state.options = &options;
	LoadLook(state);

	bool opened = false;
	if (options.searchOnly)
	{
		opened = OpenPicker(state, options.pos, nullptr, 0, true);
	}
	else
	{
		state.menu = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, MENU_CLASS, L"Winclipper",
			WS_POPUP, options.pos.x, options.pos.y, 100, 100, options.owner, nullptr, instance, nullptr);
		if (state.menu != nullptr)
		{
			SetupPopup(state.menu);
			BuildItems(state);
			LayoutMenu(state, true);
			SetWindowLongPtrW(state.menu, AGNOSTIC_USERDATA, reinterpret_cast<LONG_PTR>(&state));
			ShowWindow(state.menu, SW_SHOW);
			SetForegroundWindow(state.menu);
			SetFocus(state.menu);
			// Highlight the first clip, or the second one if the user wants it.
			const int wanted = manager.GetSize() > 1 && UserSettings::Select2ndClip() ? 2 : 1;
			for (size_t i = 0; i < state.items.size(); i++)
			{
				if (state.items[i].kind == ITEM_CLIP && state.items[i].id == wanted)
				{
					// No preview for it until the user moves the highlight.
					state.previewSuppressed = true;
					SetHot(state, static_cast<int>(i), false);
					state.previewSuppressed = false;
					break;
				}
			}
			opened = true;
		}
	}

	MSG msg;
	bool quit = false;
	WPARAM quitCode = 0;
	while (opened && !state.done)
	{
		const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
		if (got == 0 || got == -1)
		{
			quit = got == 0;
			quitCode = msg.wParam;
			break;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	HidePreview(state);
	// Detach the state before destroying: WM_ACTIVATE arrives during DestroyWindow.
	if (state.picker != nullptr)
	{
		SetWindowLongPtrW(state.picker, AGNOSTIC_USERDATA, 0);
		RemoveWindowSubclass(state.search, SearchProc, 1);
		RemoveWindowSubclass(state.list, ListProc, 1);
		DestroyWindow(state.picker);
	}
	if (state.menu != nullptr)
	{
		SetWindowLongPtrW(state.menu, AGNOSTIC_USERDATA, 0);
		DestroyWindow(state.menu);
	}
	FreeLook(state);
	if (quit)
	{
		PostQuitMessage(static_cast<int>(quitCode));
	}
	return state.result;
}
