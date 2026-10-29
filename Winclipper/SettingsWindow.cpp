#include "stdafx.h"
#include "SettingsWindow.h"
#include "ClipsManager.h"
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")


ATOM SettingsWindow::RegisterSettingsWindowClass(HINSTANCE hInstance)
{
	useLightTheme = RegistryUtilities::AppsUseLightTheme();

	WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = SettingsWindow::SettingsWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(LONG_PTR);
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINCLIPPER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	if (useLightTheme)
	{
		wcex.hbrBackground = APP_BACKGROUND_COLOR_LIGHT;
	}
	else
	{
		wcex.hbrBackground = APP_BACKGROUND_COLOR_DARK;
	}
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szSettingsWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_WINCLIPPER));

	return RegisterClassExW(&wcex);
}

LRESULT SettingsWindow::SettingsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// We want to capture a pointer to the current instance of the class
	// so we can call non-static methods from a static context.
	SettingsWindow * pThis = nullptr;

	if (message == WM_NCCREATE)
	{
#pragma warning( suppress : 26490 )
		pThis = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
		SetLastError(0);
#pragma warning( suppress : 26490 )
		if (!SetWindowLongPtrW(hWnd, AGNOSTIC_USERDATA, reinterpret_cast<LONG_PTR>(pThis)))
		{
			if (GetLastError() != 0)
				return FALSE;
		}
		return TRUE;
	}
	else
	{
#pragma warning( suppress : 26490 )
		pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hWnd, AGNOSTIC_USERDATA));
	}

	if (!pThis)
	{
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	LRESULT lResult = 0;

	switch (message)
	{
	case WM_COMMAND:
	{
		const int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			lResult = pThis->WmCommandIdmAbout(hWnd, wParam, lParam);
			break;
		case IDM_EXIT:
			lResult = pThis->WmCommandIdmExit();
			break;
		case IDM_RECREATE_THUMBNAILS:
			lResult = pThis->WmCommandIdmRecreateThumbnails();
			break;
		case IDM_CLEARCLIPS:
			lResult = pThis->WmCommandIdmClearClips();
			break;
		case BTN_CLEAR_CLIPS:
			lResult = pThis->WmCommandIdmClearClips();
			break;
		case BTN_RECREATE_THUMBNAILS:
			lResult = pThis->WmCommandIdmRecreateThumbnails();
			break;
		case BTN_CHECK_UPDATES:
			lResult = pThis->WmCommandBtnCheckUpdates(hWnd, wParam, lParam);
			break;
		case BTN_INSTALL_UPDATE:
			lResult = pThis->WmCommandBtnInstallUpdate(hWnd, wParam, lParam);
			break;
		case BTN_ADD_PRIVATE:
			lResult = pThis->WmCommandBtnAddPrivate(hWnd, wParam, lParam);
			break;
		case BTN_REMOVE_PRIVATE:
			lResult = pThis->WmCommandBtnRemovePrivate(hWnd, wParam, lParam);
			break;
		case LST_PRIVATE_ALLOWED:
			lResult = pThis->WmCommandLstPrivateAllowed(hWnd, wParam, lParam);
			break;
		case BTN_ADD_EXCLUDED:
			lResult = pThis->WmCommandBtnAddExcluded(hWnd, wParam, lParam);
			break;
		case BTN_REMOVE_EXCLUDED:
			lResult = pThis->WmCommandBtnRemoveExcluded(hWnd, wParam, lParam);
			break;
		case LST_EXCLUDED_PROGRAMS:
			lResult = pThis->WmCommandLstExcluded(hWnd, wParam, lParam);
			break;
		case LST_NAVIGATION:
			lResult = pThis->WmCommandNavigation(hWnd, wParam, lParam);
			break;
		case TXT_MAX_CLIPS_DISPLAY:
			lResult = pThis->WmCommandTxtMaxClipsDisplay(hWnd, wParam, lParam);
			break;
		case TXT_MAX_CLIPS_SAVED:
			lResult = pThis->WmCommandTxtMaxClipsSaved(hWnd, wParam, lParam);
			break;
		case TXT_MENU_DISP_CHARS:
			lResult = pThis->WmCommandTxtMenuDispChars(hWnd, wParam, lParam);
			break;
		case CHK_RUN_AT_STARTUP:
			lResult = pThis->WmCommandChkRunAtStartup(hWnd, wParam, lParam);
			break;
		case CHK_SAVE_TO_DISK:
			lResult = pThis->WmCommandChkSaveToDisk(hWnd, wParam, lParam);
			break;
		case CHK_SLCT_2ND_CLIP:
			lResult = pThis->WmCommandChkSlct2ndClip(hWnd, wParam, lParam);
			break;
		case CHK_SHOW_PREVIEW:
			lResult = pThis->WmCommandChkShowPreview(hWnd, wParam, lParam);
			break;
		case CHK_PASTE_ON_CLICK:
			lResult = pThis->WmCommandChkPasteOnClick(hWnd, wParam, lParam);
			break;
		case CHK_NOTIFY_UPDATES:
			lResult = pThis->WmCommandChkNotifyUpdates(hWnd, wParam, lParam);
			break;
		case CHK_HISTORY_MARKER_IGN:
			lResult = pThis->WmCommandChkHistoryMarkerIgnored(hWnd, wParam, lParam);
			break;
		case CHK_SAVE_IMAGES:
			lResult = pThis->WmCommandChkSaveImages(hWnd, wParam, lParam);
			break;
		case HKY_SHOW_CLIPS_MENU:
			lResult = pThis->WmCommandHkyShowClipsMenu(hWnd, wParam, lParam);
			break;
		case TXT_MAX_IMG_CACHE_MB:
			lResult = pThis->WmCommandTxtMaxImageCacheMb(hWnd, wParam, lParam);
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_LBUTTONDOWN:
		lResult = pThis->WmLbuttondown(hWnd);
		break;
	case WM_CTLCOLORSTATIC:
		lResult = pThis->WmCtlColorStatic(hWnd, wParam, lParam);
		break;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
		lResult = pThis->WmCtlColorEdit(hWnd, wParam, lParam);
		break;
	case WM_DRAWITEM:
		lResult = pThis->WmDrawItem(hWnd, wParam, lParam);
		break;
	case WM_NOTIFY:
		lResult = pThis->WmNotify(hWnd, wParam, lParam);
		break;
	case WM_CLOSE:
		lResult = pThis->WmClose(hWnd);
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return lResult;
}

LRESULT SettingsWindow::WmCommandIdmAbout(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	SettingsWindow::SendNotifcation(IDM_ABOUT, GetHandle());
	return 0;
}

LRESULT SettingsWindow::WmCommandIdmExit()
{
	SettingsWindow::SendNotifcation(IDM_EXIT, GetHandle());
	return 0;
}

LRESULT SettingsWindow::WmCommandIdmRecreateThumbnails()
{
	SettingsWindow::SendNotifcation(IDM_RECREATE_THUMBNAILS, GetHandle());
	return 0;
}

LRESULT SettingsWindow::WmCommandIdmClearClips()
{
	SettingsWindow::SendNotifcation(IDM_CLEARCLIPS, GetHandle());
	return 0;
}

LRESULT SettingsWindow::WmCommandTxtMaxClipsDisplay(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == EN_KILLFOCUS)
	{
		const int value = GetDlgItemInt(hWnd, TXT_MAX_CLIPS_DISPLAY, nullptr, false);

		if (value < UserSettings::MAX_DISPLAY_LOWER)
		{
			SetDlgItemInt(hWnd, TXT_MAX_CLIPS_DISPLAY, UserSettings::MAX_DISPLAY_LOWER, false);
		}
		else if (value > UserSettings::MAX_DISPLAY_UPPER)
		{
			SetDlgItemInt(hWnd, TXT_MAX_CLIPS_DISPLAY, UserSettings::MAX_DISPLAY_UPPER, false);
		}

		return 0; // set focus on the main window again
	}
	else if (HIWORD(wParam) == EN_CHANGE)
	{
		const int value = GetDlgItemInt(hWnd, TXT_MAX_CLIPS_DISPLAY, nullptr, false);

		if (value >= UserSettings::MAX_DISPLAY_LOWER && value <= UserSettings::MAX_DISPLAY_UPPER)
		{
			UserSettings::SetMaxDisplayClips(value);
			if (value > UserSettings::MaxSavedClips())
			{
				SetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, value, false);
			}
			SendDlgItemMessageW(hWnd, UD_MAX_CLIPS_SAVED, UDM_SETRANGE, 0, MAKELPARAM(UserSettings::MAX_SAVED_UPPER, value));
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandTxtMaxClipsSaved(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == EN_KILLFOCUS)
	{
		const int value = GetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, nullptr, false);

		if (value < UserSettings::MaxDisplayClips())
		{
			SetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, UserSettings::MaxDisplayClips(), false);
		}
		else if (value > UserSettings::MAX_SAVED_UPPER)
		{
			SetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, UserSettings::MAX_SAVED_UPPER, false);
		}

		return 0; // set focus on the main window again
	}
	else if (HIWORD(wParam) == EN_CHANGE)
	{
		const int value = GetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, nullptr, false);

		if (value <= UserSettings::MAX_SAVED_UPPER && value >= UserSettings::MaxDisplayClips())
		{
			UserSettings::SetMaxSavedClips(GetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, nullptr, false));
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandTxtMenuDispChars(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == EN_KILLFOCUS)
	{
		const int value = GetDlgItemInt(hWnd, TXT_MENU_DISP_CHARS, nullptr, false);

		if (value < UserSettings::MENU_CHARS_LOWER)
		{
			SetDlgItemInt(hWnd, TXT_MENU_DISP_CHARS, UserSettings::MENU_CHARS_LOWER, false);
		}
		else if (value > UserSettings::MENU_CHARS_UPPER)
		{
			SetDlgItemInt(hWnd, TXT_MENU_DISP_CHARS, UserSettings::MENU_CHARS_UPPER, false);
		}

		return 0; // set focus on the main window again
	}
	else if (HIWORD(wParam) == EN_CHANGE)
	{
		const int value = GetDlgItemInt(hWnd, TXT_MENU_DISP_CHARS, nullptr, false);

		if (value >= UserSettings::MENU_CHARS_LOWER && value <= UserSettings::MENU_CHARS_UPPER)
		{
			UserSettings::SetMenuDisplayChars(value);
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkRunAtStartup(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxRunAtStartup.Value();
		if (checkValue)
		{
			DeleteRegistryRun();
		}
		else
		{
			WriteRegistryRun();
		}
		CheckboxRunAtStartup.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkSaveToDisk(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxSaveToDisk.Value();
		UserSettings::SetSaveToDisk(!checkValue);
		CheckboxSaveToDisk.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkSlct2ndClip(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxSelectSecondClip.Value();
		UserSettings::SetSelect2ndClip(!checkValue);
		CheckboxSelectSecondClip.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkShowPreview(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxShowPreview.Value();
		UserSettings::SetShowPreview(!checkValue);
		CheckboxShowPreview.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkPasteOnClick(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxPasteOnClick.Value();
		UserSettings::SetPasteOnClick(!checkValue);
		CheckboxPasteOnClick.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkNotifyUpdates(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxNotifyUpdates.Value();
		UserSettings::SetNotifyUpdates(!checkValue);
		CheckboxNotifyUpdates.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkHistoryMarkerIgnored(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxHistoryMarkerIgnored.Value();
		UserSettings::SetHistoryMarkerIgnored(!checkValue);
		CheckboxHistoryMarkerIgnored.SetValue(!checkValue);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandChkSaveImages(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == BN_CLICKED)
	{
		bool checkValue = CheckboxSaveImages.Value();

		if (checkValue)
		{
			int choice = MessageBoxA(hWnd, 
				"This will delete all images on the Winclipper clipboard.\r\n\r\n Are you sure you want to do this?", "Winclipper", 
				MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_TASKMODAL | MB_SETFOREGROUND);

			if (choice == IDYES)
			{
				UserSettings::SetSaveImages(!checkValue);
				CheckboxSaveImages.SetValue(!checkValue);
				SpinnerMaxCacheMbytes.SetEnabled(!checkValue);
			}
		}
		else
		{
			UserSettings::SetSaveImages(!checkValue);
			CheckboxSaveImages.SetValue(!checkValue);
			SpinnerMaxCacheMbytes.SetEnabled(!checkValue);;
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandTxtMaxImageCacheMb(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == EN_KILLFOCUS)
	{
		const unsigned int value = GetDlgItemInt(hWnd, TXT_MAX_IMG_CACHE_MB, nullptr, false);

		if (value < UserSettings::MAX_CACHE_MBYTES_LOWER)
		{
			SetDlgItemInt(hWnd, TXT_MAX_IMG_CACHE_MB, UserSettings::MAX_CACHE_MBYTES_LOWER, false);
		}
		else if (value > UserSettings::MAX_CACHE_MBYTES_UPPER)
		{
			SetDlgItemInt(hWnd, TXT_MAX_IMG_CACHE_MB, UserSettings::MAX_CACHE_MBYTES_UPPER, false);
		}

		return 0; // set focus on the main window again
	}
	else if (HIWORD(wParam) == EN_CHANGE)
	{
		const unsigned int value = GetDlgItemInt(hWnd, TXT_MAX_IMG_CACHE_MB, nullptr, false);

		if (value >= UserSettings::MAX_CACHE_MBYTES_LOWER && value <= UserSettings::MAX_CACHE_MBYTES_UPPER)
		{
			UserSettings::SetMaxCacheMegabytes(value);
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandHkyShowClipsMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == EN_CHANGE)
	{
		WORD wHotkey;

		// Retrieve the hot key (virtual key code and modifiers). 
		wHotkey = (WORD)SendMessageW(GetDlgItem(hWnd, HKY_SHOW_CLIPS_MENU), HKM_GETHOTKEY, 0, 0);

		// Resets to the default hotkey if the combo is cleared.
		if (wHotkey == 0)
		{
			SendMessageW(GetDlgItem(hWnd, HKY_SHOW_CLIPS_MENU), HKM_SETHOTKEY, CMENU_HOTKEY_DEF, 0);
			UserSettings::SetClipsMenuHotkey(CMENU_HOTKEY_DEF);
		}
		else
		{
			// So the WORD returned from HKM_GETHOTKEY stuffs all of the
			// hotkey info in the LOWORD side into the LOBYTE and HIBYTE
			// intead of on each side of the WORD, so we have to split
			// these out for consistency with storing in settings.
			UserSettings::SetClipsMenuHotkey(MAKEWORD(LOBYTE(LOWORD(wHotkey)), HIBYTE(LOWORD(wHotkey))));
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmLbuttondown(HWND hWnd)
{
	SetFocus(hWnd);
	return 0;
}

LRESULT SettingsWindow::WmCtlColorStatic(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HDC hdcStatic = (HDC)wParam;
	SetBkMode(hdcStatic, TRANSPARENT);
	if (useLightTheme)
	{
		SetTextColor(hdcStatic, FOREGROUND_COLOR_LIGHT);
	}
	else
	{
		SetTextColor(hdcStatic, FOREGROUND_COLOR_DARK);
	}

	if (hbrBkgnd == NULL)
	{
		if (useLightTheme)
		{
			hbrBkgnd = CreateSolidBrush(BACKGROUND_COLOR_LIGHT);
		}
		else
		{
			hbrBkgnd = CreateSolidBrush(BACKGROUND_COLOR_DARK);
		}
	}
	return (INT_PTR)hbrBkgnd;
}

LRESULT SettingsWindow::WmCtlColorEdit(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (reinterpret_cast<HWND>(lParam) == navigationList)
	{
		return reinterpret_cast<LRESULT>(hbrSidebar);
	}
	HDC hdcStatic = (HDC)wParam;
	SetBkMode(hdcStatic, TRANSPARENT);
	if (useLightTheme)
	{
		SetTextColor(hdcStatic, FOREGROUND_COLOR_LIGHT);
	}
	else
	{
		SetTextColor(hdcStatic, FOREGROUND_COLOR_DARK);
	}

	if (hbrCtrlBkgnd == NULL)
	{
		if (useLightTheme)
		{
			hbrCtrlBkgnd = CreateSolidBrush(BACKGROUND_COLOR_LIGHT);
		}
		else
		{
			hbrCtrlBkgnd = CreateSolidBrush(BACKGROUND_COLOR_DARK);
		}
	}
	return (INT_PTR)hbrCtrlBkgnd;
}

LRESULT SettingsWindow::WmClose(HWND hWnd)
{
	SetFocus(hWnd);
	ShowWindow(hWnd, SW_HIDE);
	return 0;
}

// Writes the "run at startup" registry key for Winclipper
bool SettingsWindow::WriteRegistryRun()
{
	wchar_t pPath[MAX_PATH];
	const DWORD pathLength = GetModuleFileNameW(nullptr, pPath, MAX_PATH);
	if (pathLength == 0 || pathLength >= MAX_PATH)
	{
		return false;
	}

	// Quote the path: the Run key is parsed like a command line, and an
	// unquoted "C:\Program Files\..." would first try "C:\Program.exe".
	const std::wstring command = L"\"" + std::wstring(pPath, pathLength) + L"\"";

	HKEY hOpened;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hOpened) !=ERROR_SUCCESS)
	{
		return false;
	}

	const LSTATUS status = RegSetValueExW(hOpened, L"Winclipper", 0, REG_SZ,
		reinterpret_cast<const BYTE *>(command.c_str()), static_cast<DWORD>((command.length() + 1) * sizeof(wchar_t)));
	RegCloseKey(hOpened);
	return status == ERROR_SUCCESS;
}

// Deletes the "run at startup" registry key for Winclipper
bool SettingsWindow::DeleteRegistryRun()
{
	HKEY hOpened;

	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hOpened) == ERROR_SUCCESS)
	{
		RegDeleteValueW(hOpened, L"Winclipper");
	}
	else
	{
		return false;
	}

	RegCloseKey(hOpened);
	return true;
}

SettingsWindow::SettingsWindow(HINSTANCE hInstance)
{
	NONCLIENTMETRICS hfDefault;
	hfDefault.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &hfDefault, 0);
	hFontStd = CreateFontIndirectW(&hfDefault.lfMessageFont);
	LOGFONTW heading = hfDefault.lfMessageFont;
	heading.lfHeight = heading.lfHeight * 5 / 3;
	heading.lfWeight = FW_SEMIBOLD;
	hFontHeading = CreateFontIndirectW(&heading);

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCSETTINGS, szSettingsWindowClass, MAX_LOADSTRING);

	RegisterSettingsWindowClass(hInstance);
	hbrSidebar = CreateSolidBrush(useLightTheme ? RGB(243, 243, 243) : RGB(26, 26, 26));

	InitSettingsWindow(hInstance);
}

SettingsWindow::~SettingsWindow()
{
}

// Records every child window created since the last call as belonging to page.
void SettingsWindow::AssignNewControlsToPage(int page)
{
	struct Context { SettingsWindow * self; int page; };
	Context context = { this, page };
	EnumChildWindows(windowHandle, [](HWND child, LPARAM lParam) -> BOOL
	{
		Context * c = reinterpret_cast<Context *>(lParam);
		if (GetParent(child) == c->self->windowHandle && child != c->self->navigationList
			&& c->self->pageOfControl.find(child) == c->self->pageOfControl.end())
		{
			c->self->pageOfControl[child] = c->page;
		}
		return TRUE;
	}, reinterpret_cast<LPARAM>(&context));
}

void SettingsWindow::ShowPage(int page)
{
	for (const auto & entry : pageOfControl)
	{
		ShowWindow(entry.first, entry.second == page ? SW_SHOW : SW_HIDE);
	}
	if (SendMessageW(navigationList, LB_GETCURSEL, 0, 0) != page)
	{
		SendMessageW(navigationList, LB_SETCURSEL, page, 0);
	}
	if (page == PAGE_UPDATES)
	{
		SetDlgItemTextW(windowHandle, LBL_INSTALLED_VERSION, (L"Installed version: " + Updater::CurrentVersion()).c_str());
		if (!updateInstallable)
		{
			ShowWindow(GetDlgItem(windowHandle, BTN_INSTALL_UPDATE), SW_HIDE);
		}
	}
}

HWND SettingsWindow::AddButton(int x, int y, int width, LPCWSTR text, UINT_PTR id, HINSTANCE hInstance)
{
	HWND button = CreateWindowExW(0, WC_BUTTON, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		ScaleX(x), ScaleY(y), ScaleX(width), ScaleY(24), windowHandle, reinterpret_cast<HMENU>(id), hInstance, nullptr);
	SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	return button;
}

HWND SettingsWindow::AddHeading(int x, LPCWSTR text, HINSTANCE hInstance)
{
	HWND heading = AddText(x, 14, 300, 30, text, IDC_STATIC, hInstance);
	SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(hFontHeading), FALSE);
	return heading;
}

// Rounded corners on Windows 11 and a dark title bar when apps use the dark theme.
// Older Windows versions ignore both attributes.
void SettingsWindow::ApplyWindowStyle(HWND hWnd)
{
	const BOOL dark = useLightTheme ? FALSE : TRUE;
	DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
	const int corners = 2 /* DWMWCP_ROUND */;
	DwmSetWindowAttribute(hWnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corners, sizeof(corners));
}

COLORREF SettingsWindow::AccentColor()
{
	DWORD colorization = 0;
	BOOL opaque = FALSE;
	if (SUCCEEDED(DwmGetColorizationColor(&colorization, &opaque)))
	{
		return RGB((colorization >> 16) & 0xFF, (colorization >> 8) & 0xFF, colorization & 0xFF);
	}
	return RGB(0, 103, 192);
}

// Draws a navigation entry: a rounded highlight with an accent bar for the selected page.
LRESULT SettingsWindow::WmDrawItem(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const DRAWITEMSTRUCT * item = reinterpret_cast<const DRAWITEMSTRUCT *>(lParam);
	if (item->CtlID != LST_NAVIGATION || item->itemID == static_cast<UINT>(-1))
	{
		return FALSE;
	}
	HDC dc = item->hDC;
	RECT rect = item->rcItem;
	FillRect(dc, &rect, hbrSidebar);

	if (item->itemState & ODS_SELECTED)
	{
		const RECT pill = { rect.left + ScaleX(6), rect.top + ScaleY(3), rect.right - ScaleX(6), rect.bottom - ScaleY(3) };
		HBRUSH pillBrush = CreateSolidBrush(useLightTheme ? RGB(228, 228, 228) : RGB(50, 50, 50));
		HBRUSH accentBrush = CreateSolidBrush(AccentColor());
		HPEN oldPen = static_cast<HPEN>(SelectObject(dc, GetStockObject(NULL_PEN)));
		HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, pillBrush));
		RoundRect(dc, pill.left, pill.top, pill.right, pill.bottom, ScaleX(8), ScaleY(8));
		SelectObject(dc, accentBrush);
		const int barHeight = ScaleY(16);
		const int barTop = (pill.top + pill.bottom - barHeight) / 2;
		RoundRect(dc, pill.left, barTop, pill.left + ScaleX(3), barTop + barHeight, ScaleX(3), ScaleY(3));
		SelectObject(dc, oldBrush);
		SelectObject(dc, oldPen);
		DeleteObject(accentBrush);
		DeleteObject(pillBrush);
	}

	wchar_t text[64] = L"";
	if (SendMessageW(item->hwndItem, LB_GETTEXTLEN, item->itemID, 0) < 64)
	{
		SendMessageW(item->hwndItem, LB_GETTEXT, item->itemID, reinterpret_cast<LPARAM>(text));
	}
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, useLightTheme ? FOREGROUND_COLOR_LIGHT : FOREGROUND_COLOR_DARK);
	HFONT oldFont = static_cast<HFONT>(SelectObject(dc, hFontStd));
	rect.left += ScaleX(20);
	DrawTextW(dc, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
	SelectObject(dc, oldFont);
	return TRUE;
}

HWND SettingsWindow::AddText(int x, int y, int width, int height, LPCWSTR text, UINT_PTR id, HINSTANCE hInstance)
{
	HWND label = CreateWindowExW(0, WC_STATIC, text, WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
		ScaleX(x), ScaleY(y), ScaleX(width), ScaleY(height), windowHandle, reinterpret_cast<HMENU>(id), hInstance, nullptr);
	SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	return label;
}

void SettingsWindow::RefreshPrivateAllowedList()
{
	SendMessageW(privateAllowedList, LB_RESETCONTENT, 0, 0);
	for (const auto & name : UserSettings::PrivateAllowedPrograms())
	{
		SendMessageW(privateAllowedList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
	}
	EnableWindow(GetDlgItem(windowHandle, BTN_REMOVE_PRIVATE), FALSE);
}

LRESULT SettingsWindow::WmCommandNavigation(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == LBN_SELCHANGE)
	{
		const LRESULT page = SendMessageW(navigationList, LB_GETCURSEL, 0, 0);
		if (page >= 0 && page < PAGE_COUNT)
		{
			ShowPage(static_cast<int>(page));
		}
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandLstPrivateAllowed(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == LBN_SELCHANGE)
	{
		EnableWindow(GetDlgItem(hWnd, BTN_REMOVE_PRIVATE), SendMessageW(privateAllowedList, LB_GETCURSEL, 0, 0) != LB_ERR);
	}
	return 0;
}

// File name of an .exe the user picks, or empty if cancelled.
std::wstring SettingsWindow::PickProgramName(HWND hWnd, LPCWSTR title)
{
	wchar_t path[MAX_PATH] = L"";
	OPENFILENAMEW ofn = { sizeof(ofn) };
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"Programs (*.exe)\0*.exe\0";
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
	return GetOpenFileNameW(&ofn) ? std::wstring(PathFindFileNameW(path)) : std::wstring();
}

// Text of the selected list box item, or empty if none is selected.
std::wstring SettingsWindow::SelectedListText(HWND list)
{
	const LRESULT index = SendMessageW(list, LB_GETCURSEL, 0, 0);
	const LRESULT length = index == LB_ERR ? LB_ERR : SendMessageW(list, LB_GETTEXTLEN, index, 0);
	if (length == LB_ERR)
	{
		return std::wstring();
	}
	std::wstring text(static_cast<size_t>(length) + 1, L'\0');
	SendMessageW(list, LB_GETTEXT, index, reinterpret_cast<LPARAM>(&text[0]));
	text.resize(static_cast<size_t>(length));
	return text;
}

static void ShowProgramNameRejected(HWND hWnd)
{
	MessageBoxW(hWnd, L"This program name cannot be added. Names may only contain letters, digits, spaces and . _ - ( ) +, "
		L"and the list holds at most 50 programs.", L"Winclipper", MB_OK | MB_ICONWARNING);
}

LRESULT SettingsWindow::WmCommandBtnAddPrivate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const std::wstring name = PickProgramName(hWnd, L"Always record clips from this program");
	if (!name.empty())
	{
		if (!UserSettings::AddPrivateAllowed(name))
		{
			ShowProgramNameRejected(hWnd);
		}
		RefreshPrivateAllowedList();
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandBtnRemovePrivate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const std::wstring name = SelectedListText(privateAllowedList);
	if (!name.empty())
	{
		UserSettings::RemovePrivateAllowed(name);
		RefreshPrivateAllowedList();
	}
	return 0;
}

void SettingsWindow::RefreshExcludedList()
{
	SendMessageW(excludedList, LB_RESETCONTENT, 0, 0);
	for (const auto & name : UserSettings::ExcludedPrograms())
	{
		SendMessageW(excludedList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
	}
	EnableWindow(GetDlgItem(windowHandle, BTN_REMOVE_EXCLUDED), FALSE);
}

LRESULT SettingsWindow::WmCommandLstExcluded(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == LBN_SELCHANGE)
	{
		EnableWindow(GetDlgItem(hWnd, BTN_REMOVE_EXCLUDED), SendMessageW(excludedList, LB_GETCURSEL, 0, 0) != LB_ERR);
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandBtnAddExcluded(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const std::wstring name = PickProgramName(hWnd, L"Never record clips from this program");
	if (!name.empty())
	{
		if (!UserSettings::AddExcludedProgram(name))
		{
			ShowProgramNameRejected(hWnd);
		}
		RefreshExcludedList();
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandBtnRemoveExcluded(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const std::wstring name = SelectedListText(excludedList);
	if (!name.empty())
	{
		UserSettings::RemoveExcludedProgram(name);
		RefreshExcludedList();
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandBtnCheckUpdates(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	SetUpdateStatus(L"Checking for updates...");
	Updater::CheckNow();
	return 0;
}

// The button only opens the main window's confirmation dialog; it never installs directly.
LRESULT SettingsWindow::WmCommandBtnInstallUpdate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	SettingsWindow::SendNotifcation(IDM_INSTALL_UPDATE, nullptr);
	return 0;
}

void SettingsWindow::SetUpdateStatus(const std::wstring & text, bool installable)
{
	SetDlgItemTextW(windowHandle, LBL_UPDATE_STATUS, text.c_str());
	updateInstallable = installable;
	const bool onUpdatesPage = SendMessageW(navigationList, LB_GETCURSEL, 0, 0) == PAGE_UPDATES;
	ShowWindow(GetDlgItem(windowHandle, BTN_INSTALL_UPDATE), installable && onUpdatesPage ? SW_SHOW : SW_HIDE);
}

LRESULT SettingsWindow::WmNotify(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const NMHDR * header = reinterpret_cast<const NMHDR *>(lParam);
	if (header->idFrom == LNK_REPOSITORY && (header->code == NM_CLICK || header->code == NM_RETURN))
	{
		const NMLINK * link = reinterpret_cast<const NMLINK *>(lParam);
		if (wcsncmp(link->item.szUrl, L"https://github.com/tomcek42/Winclipper", 38) == 0)
		{
			ShellExecuteW(hWnd, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
		}
	}
	return 0;
}

bool SettingsWindow::InitSettingsWindow(HINSTANCE hInstance)
{
	HWND hWnd = CreateWindowExW(0, szSettingsWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, ScaleX(500), ScaleY(590), nullptr, nullptr, hInstance, this);

	if (!hWnd)
	{
		return false;
	}

	SetWindowTextW(hWnd, szTitle);

	windowHandle = hWnd;

	// Vertical tabs: a list on the left, one page of controls on the right.
	ApplyWindowStyle(hWnd);

	// Owner-drawn and borderless, so it reads as a sidebar rather than a list box.
	RECT client;
	GetClientRect(hWnd, &client);
	navigationList = CreateWindowExW(0, WC_LISTBOX, nullptr,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
		0, 0, ScaleX(SIDEBAR_WIDTH), client.bottom, hWnd, reinterpret_cast<HMENU>(LST_NAVIGATION), hInstance, nullptr);
	SendMessageW(navigationList, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	SendMessageW(navigationList, LB_SETITEMHEIGHT, 0, ScaleY(36));
	for (LPCWSTR name : { L"General", L"Clips", L"Images", L"Updates", L"About" })
	{
		SendMessageW(navigationList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
	}

	const int left = SIDEBAR_WIDTH + 24;
	const int top = 52;

	// General
	AddHeading(left, L"General", hInstance);
	LabelShowClipsHotkey = Controls::Label(hWnd, LBL_SHOW_CLIPS_HOTK, hInstance);
	LabelShowClipsHotkey.SuspendLayout();
	LabelShowClipsHotkey.SetX(left);
	LabelShowClipsHotkey.SetY(top + 12);
	LabelShowClipsHotkey.SetText(L"Clips menu shortcut:");
	LabelShowClipsHotkey.ResumeLayout();
	HWND hHotKey = AddHotkeyCtrl(hWnd, hFontStd, left + 140, top + 10, 150, 20, hInstance, HKY_SHOW_CLIPS_MENU);

	CheckboxRunAtStartup = Controls::Checkbox(hWnd, CHK_RUN_AT_STARTUP, hInstance);
	CheckboxRunAtStartup.SuspendLayout();
	CheckboxRunAtStartup.SetX(left);
	CheckboxRunAtStartup.SetY(top + 44);
	CheckboxRunAtStartup.SetText(L"Run Winclipper at startup");
	CheckboxRunAtStartup.UseBasicTheme = !useLightTheme;
	CheckboxRunAtStartup.ResumeLayout();

	CheckboxPasteOnClick = Controls::Checkbox(hWnd, CHK_PASTE_ON_CLICK, hInstance);
	CheckboxPasteOnClick.SuspendLayout();
	CheckboxPasteOnClick.SetX(left);
	CheckboxPasteOnClick.SetY(top + 68);
	CheckboxPasteOnClick.SetText(L"Paste immediately on click");
	CheckboxPasteOnClick.UseBasicTheme = !useLightTheme;
	CheckboxPasteOnClick.ResumeLayout();

	CheckboxSelectSecondClip = Controls::Checkbox(hWnd, CHK_SLCT_2ND_CLIP, hInstance);
	CheckboxSelectSecondClip.SuspendLayout();
	CheckboxSelectSecondClip.SetX(left);
	CheckboxSelectSecondClip.SetY(top + 92);
	CheckboxSelectSecondClip.SetText(L"Auto-select second clip");
	CheckboxSelectSecondClip.UseBasicTheme = !useLightTheme;
	CheckboxSelectSecondClip.ResumeLayout();

	CheckboxShowPreview = Controls::Checkbox(hWnd, CHK_SHOW_PREVIEW, hInstance);
	CheckboxShowPreview.SuspendLayout();
	CheckboxShowPreview.SetX(left);
	CheckboxShowPreview.SetY(top + 116);
	CheckboxShowPreview.SetText(L"Show clip preview");
	CheckboxShowPreview.UseBasicTheme = !useLightTheme;
	CheckboxShowPreview.ResumeLayout();
	AssignNewControlsToPage(PAGE_GENERAL);

	// Clips
	AddHeading(left, L"Clips", hInstance);
	LabelMaxClipsDisplay = Controls::Label(hWnd, LBL_MAX_CLIPS_DISPLAY, hInstance);
	LabelMaxClipsDisplay.SuspendLayout();
	LabelMaxClipsDisplay.SetX(left);
	LabelMaxClipsDisplay.SetY(top + 12);
	LabelMaxClipsDisplay.SetText(L"Number of clips to display:");
	LabelMaxClipsDisplay.ResumeLayout();

	SpinnerMaxClipsDisplay = Controls::Spinner(hWnd, TXT_MAX_CLIPS_DISPLAY, UD_MAX_CLIPS_DISPLAY, hInstance);
	SpinnerMaxClipsDisplay.SuspendLayout();
	SpinnerMaxClipsDisplay.SetX(left + 200);
	SpinnerMaxClipsDisplay.SetY(top + 10);
	SpinnerMaxClipsDisplay.SetMinValue(UserSettings::MAX_DISPLAY_LOWER);
	SpinnerMaxClipsDisplay.SetMaxValue(UserSettings::MAX_DISPLAY_UPPER);
	SpinnerMaxClipsDisplay.ResumeLayout();

	LabelMaxClipsSaved = Controls::Label(hWnd, LBL_MAX_CLIPS_SAVED, hInstance);
	LabelMaxClipsSaved.SuspendLayout();
	LabelMaxClipsSaved.SetX(left);
	LabelMaxClipsSaved.SetY(top + 42);
	LabelMaxClipsSaved.SetText(L"Maximum clips to save:");
	LabelMaxClipsSaved.ResumeLayout();

	SpinnerMaxClipsSaved = Controls::Spinner(hWnd, TXT_MAX_CLIPS_SAVED, UD_MAX_CLIPS_SAVED, hInstance);
	SpinnerMaxClipsSaved.SuspendLayout();
	SpinnerMaxClipsSaved.SetX(left + 200);
	SpinnerMaxClipsSaved.SetY(top + 40);
	SpinnerMaxClipsSaved.SetMinValue(UserSettings::MAX_SAVED_LOWER);
	SpinnerMaxClipsSaved.SetMaxValue(UserSettings::MAX_SAVED_UPPER);
	SpinnerMaxClipsSaved.ResumeLayout();

	LabelMenuDisplayChars = Controls::Label(hWnd, LBL_MENU_DISP_CHARS, hInstance);
	LabelMenuDisplayChars.SuspendLayout();
	LabelMenuDisplayChars.SetX(left);
	LabelMenuDisplayChars.SetY(top + 72);
	LabelMenuDisplayChars.SetText(L"Number of preview characters:");
	LabelMenuDisplayChars.ResumeLayout();

	SpinnerMenuDisplayChars = Controls::Spinner(hWnd, TXT_MENU_DISP_CHARS, UD_MENU_DISP_CHARS, hInstance);
	SpinnerMenuDisplayChars.SuspendLayout();
	SpinnerMenuDisplayChars.SetX(left + 200);
	SpinnerMenuDisplayChars.SetY(top + 70);
	SpinnerMenuDisplayChars.SetMinValue(UserSettings::MENU_CHARS_LOWER);
	SpinnerMenuDisplayChars.SetMaxValue(UserSettings::MENU_CHARS_UPPER);
	SpinnerMenuDisplayChars.ResumeLayout();

	CheckboxSaveToDisk = Controls::Checkbox(hWnd, CHK_SAVE_TO_DISK, hInstance);
	CheckboxSaveToDisk.SuspendLayout();
	CheckboxSaveToDisk.SetX(left);
	CheckboxSaveToDisk.SetY(top + 102);
	CheckboxSaveToDisk.SetText(L"Save clips to disk");
	CheckboxSaveToDisk.UseBasicTheme = !useLightTheme;
	CheckboxSaveToDisk.ResumeLayout();

	AddButton(left, top + 128, 110, L"&Clear clips", BTN_CLEAR_CLIPS, hInstance);

	AddText(left, top + 166, 280, 32, L"Always record clips from these programs, even if marked as private (e.g. passwords):",
		LBL_PRIVATE_ALLOWED, hInstance);
	privateAllowedList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOX, nullptr,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_SORT | LBS_NOINTEGRALHEIGHT,
		ScaleX(left), ScaleY(top + 200), ScaleX(180), ScaleY(80), hWnd, reinterpret_cast<HMENU>(LST_PRIVATE_ALLOWED), hInstance, nullptr);
	SendMessageW(privateAllowedList, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	AddButton(left + 190, top + 200, 90, L"&Add...", BTN_ADD_PRIVATE, hInstance);
	AddButton(left + 190, top + 230, 90, L"&Remove", BTN_REMOVE_PRIVATE, hInstance);

	AddText(left, top + 294, 280, 32, L"Never record clips from these programs:",
		LBL_EXCLUDED_PROGRAMS, hInstance);
	excludedList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOX, nullptr,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_SORT | LBS_NOINTEGRALHEIGHT,
		ScaleX(left), ScaleY(top + 328), ScaleX(180), ScaleY(80), hWnd, reinterpret_cast<HMENU>(LST_EXCLUDED_PROGRAMS), hInstance, nullptr);
	SendMessageW(excludedList, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	AddButton(left + 190, top + 328, 90, L"A&dd...", BTN_ADD_EXCLUDED, hInstance);
	AddButton(left + 190, top + 358, 90, L"R&emove", BTN_REMOVE_EXCLUDED, hInstance);

	CheckboxHistoryMarkerIgnored = Controls::Checkbox(hWnd, CHK_HISTORY_MARKER_IGN, hInstance);
	CheckboxHistoryMarkerIgnored.SuspendLayout();
	CheckboxHistoryMarkerIgnored.SetX(left);
	CheckboxHistoryMarkerIgnored.SetY(top + 422);
	CheckboxHistoryMarkerIgnored.SetText(L"Record clips from RDP sessions (only hidden\nfrom Windows history)");
	CheckboxHistoryMarkerIgnored.UseBasicTheme = !useLightTheme;
	CheckboxHistoryMarkerIgnored.ResumeLayout();
	AssignNewControlsToPage(PAGE_CLIPS);

	// Images
	AddHeading(left, L"Images", hInstance);
	CheckboxSaveImages = Controls::Checkbox(hWnd, CHK_SAVE_IMAGES, hInstance);
	CheckboxSaveImages.SuspendLayout();
	CheckboxSaveImages.SetX(left);
	CheckboxSaveImages.SetY(top + 12);
	CheckboxSaveImages.SetText(L"Save images to the clipboard");
	CheckboxSaveImages.UseBasicTheme = !useLightTheme;
	CheckboxSaveImages.ResumeLayout();

	LabelMaxCacheMbytes = Controls::Label(hWnd, LBL_MAX_IMG_CACHE_MB, hInstance);
	LabelMaxCacheMbytes.SuspendLayout();
	LabelMaxCacheMbytes.SetX(left + 18);
	LabelMaxCacheMbytes.SetY(top + 40);
	LabelMaxCacheMbytes.SetText(L"Max image cache size (MB):");
	LabelMaxCacheMbytes.ResumeLayout();

	SpinnerMaxCacheMbytes = Controls::Spinner(hWnd, TXT_MAX_IMG_CACHE_MB, UD_MAX_IMG_CACHE_MB, hInstance);
	SpinnerMaxCacheMbytes.SuspendLayout();
	SpinnerMaxCacheMbytes.SetX(left + 200);
	SpinnerMaxCacheMbytes.SetY(top + 38);
	SpinnerMaxCacheMbytes.SetMinValue(UserSettings::MAX_CACHE_MBYTES_LOWER);
	SpinnerMaxCacheMbytes.SetMaxValue(UserSettings::MAX_CACHE_MBYTES_UPPER);
	SpinnerMaxCacheMbytes.ResumeLayout();

	AddButton(left, top + 72, 150, L"&Recreate thumbnails", BTN_RECREATE_THUMBNAILS, hInstance);
	AssignNewControlsToPage(PAGE_IMAGES);

	// Updates
	AddHeading(left, L"Updates", hInstance);
	CheckboxNotifyUpdates = Controls::Checkbox(hWnd, CHK_NOTIFY_UPDATES, hInstance);
	CheckboxNotifyUpdates.SuspendLayout();
	CheckboxNotifyUpdates.SetX(left);
	CheckboxNotifyUpdates.SetY(top + 12);
	CheckboxNotifyUpdates.SetText(L"Notify me about updates");
	CheckboxNotifyUpdates.UseBasicTheme = !useLightTheme;
	CheckboxNotifyUpdates.ResumeLayout();

	AddText(left, top + 42, 280, 32, L"Updates are installed only after you confirm them, and only if they are signed by this project.",
		IDC_STATIC, hInstance);
	AddText(left, top + 84, 280, 16, L"Installed version:", LBL_INSTALLED_VERSION, hInstance);
	AddButton(left, top + 108, 110, L"Check &now", BTN_CHECK_UPDATES, hInstance);
	AddText(left, top + 140, 280, 32, L"", LBL_UPDATE_STATUS, hInstance);
	AddButton(left, top + 176, 110, L"&Install update...", BTN_INSTALL_UPDATE, hInstance);
	AssignNewControlsToPage(PAGE_UPDATES);

	// About
	AddHeading(left, L"About", hInstance);
	AddText(left, top + 12, 280, 16, (L"Winclipper " + Updater::CurrentVersion()).c_str(), IDC_STATIC, hInstance);
	AddText(left, top + 36, 280, 16, L"Copyright (C) 2021 Carson Shook", IDC_STATIC, hInstance);
	AddText(left, top + 54, 280, 16, L"Copyright (C) 2026 Tom - tomcek42 (fork)", IDC_STATIC, hInstance);
	AddText(left, top + 78, 280, 16, L"Released under the MIT License.", IDC_STATIC, hInstance);
	HWND link = CreateWindowExW(0, WC_LINK,
		L"<a href=\"https://github.com/tomcek42/Winclipper\">github.com/tomcek42/Winclipper</a>",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, ScaleX(left), ScaleY(top + 102), ScaleX(280), ScaleY(18),
		hWnd, reinterpret_cast<HMENU>(LNK_REPOSITORY), hInstance, nullptr);
	SendMessageW(link, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	AssignNewControlsToPage(PAGE_ABOUT);

	// Load values from user settings
	SetDlgItemInt(hWnd, TXT_MAX_CLIPS_DISPLAY, UserSettings::MaxDisplayClips(), false);
	SetDlgItemInt(hWnd, TXT_MAX_CLIPS_SAVED, UserSettings::MaxSavedClips(), false);
	SetDlgItemInt(hWnd, TXT_MENU_DISP_CHARS, UserSettings::MenuDisplayChars(), false);
	SetDlgItemInt(hWnd, TXT_MAX_IMG_CACHE_MB, UserSettings::MaxCacheMegabytes(), false);

	HKEY hOpened;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hOpened) == ERROR_SUCCESS)
	{
		if (RegistryUtilities::QueryKeyForValue(hOpened, L"Winclipper") == true)
		{
			CheckboxRunAtStartup.SetValue(true);
		}
		RegCloseKey(hOpened);
	}
	CheckboxSaveToDisk.SetValue(UserSettings::SaveToDisk());
	CheckboxSelectSecondClip.SetValue(UserSettings::Select2ndClip());
	CheckboxShowPreview.SetValue(UserSettings::ShowPreview());
	CheckboxPasteOnClick.SetValue(UserSettings::PasteOnClick());
	CheckboxNotifyUpdates.SetValue(UserSettings::NotifyUpdates());
	CheckboxHistoryMarkerIgnored.SetValue(UserSettings::HistoryMarkerIgnored());
	CheckboxSaveImages.SetValue(UserSettings::SaveImages());
	SpinnerMaxCacheMbytes.SetEnabled(UserSettings::SaveImages());
	RefreshPrivateAllowedList();
	RefreshExcludedList();

	SendMessageW(hHotKey, HKM_SETHOTKEY, UserSettings::ClipsMenuHotkey(), 0);

	ShowPage(PAGE_GENERAL);

	// Hide the window and update the layout
	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);

	return true;
}

HWND SettingsWindow::GetHandle() noexcept
{
	return windowHandle;
}
