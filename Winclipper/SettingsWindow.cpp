#include "stdafx.h"
#include "SettingsWindow.h"
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")


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
		case BTN_ADD_PRIVATE:
			lResult = pThis->WmCommandBtnAddPrivate(hWnd, wParam, lParam);
			break;
		case BTN_REMOVE_PRIVATE:
			lResult = pThis->WmCommandBtnRemovePrivate(hWnd, wParam, lParam);
			break;
		case LST_PRIVATE_ALLOWED:
			lResult = pThis->WmCommandLstPrivateAllowed(hWnd, wParam, lParam);
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

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCSETTINGS, szSettingsWindowClass, MAX_LOADSTRING);

	RegisterSettingsWindowClass(hInstance);

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
	}
}

HWND SettingsWindow::AddButton(int x, int y, int width, LPCWSTR text, UINT_PTR id, HINSTANCE hInstance)
{
	HWND button = CreateWindowExW(0, WC_BUTTON, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		ScaleX(x), ScaleY(y), ScaleX(width), ScaleY(24), windowHandle, reinterpret_cast<HMENU>(id), hInstance, nullptr);
	SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	return button;
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

LRESULT SettingsWindow::WmCommandBtnAddPrivate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	wchar_t path[MAX_PATH] = L"";
	OPENFILENAMEW ofn = { sizeof(ofn) };
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"Programs (*.exe)\0*.exe\0";
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = L"Keep private content from this program";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
	if (GetOpenFileNameW(&ofn))
	{
		const std::wstring name = PathFindFileNameW(path);
		if (!UserSettings::AddPrivateAllowed(name))
		{
			MessageBoxW(hWnd, L"This program name cannot be added. Names may only contain letters, digits, spaces and . _ - ( ) +, "
				L"and the list holds at most 50 programs.", L"Winclipper", MB_OK | MB_ICONWARNING);
		}
		RefreshPrivateAllowedList();
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandBtnRemovePrivate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	const LRESULT index = SendMessageW(privateAllowedList, LB_GETCURSEL, 0, 0);
	const LRESULT length = index == LB_ERR ? LB_ERR : SendMessageW(privateAllowedList, LB_GETTEXTLEN, index, 0);
	if (length != LB_ERR)
	{
		std::wstring name(static_cast<size_t>(length) + 1, L'\0');
		SendMessageW(privateAllowedList, LB_GETTEXT, index, reinterpret_cast<LPARAM>(&name[0]));
		name.resize(static_cast<size_t>(length));
		UserSettings::RemovePrivateAllowed(name);
		RefreshPrivateAllowedList();
	}
	return 0;
}

LRESULT SettingsWindow::WmCommandBtnCheckUpdates(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	Updater::CheckNow();
	return 0;
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
		CW_USEDEFAULT, CW_USEDEFAULT, ScaleX(440), ScaleY(360), nullptr, nullptr, hInstance, this);

	if (!hWnd)
	{
		return false;
	}

	SetWindowTextW(hWnd, szTitle);

	windowHandle = hWnd;

	// Vertical tabs: a list on the left, one page of controls on the right.
	navigationList = CreateWindowExW(0, WC_LISTBOX, nullptr,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
		ScaleX(10), ScaleY(10), ScaleX(100), ScaleY(300), hWnd, reinterpret_cast<HMENU>(LST_NAVIGATION), hInstance, nullptr);
	SendMessageW(navigationList, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	SendMessageW(navigationList, LB_SETITEMHEIGHT, 0, ScaleY(26));
	for (LPCWSTR name : { L"General", L"Clips", L"Images", L"Updates", L"About" })
	{
		SendMessageW(navigationList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
	}

	const int left = 125;

	// General
	LabelShowClipsHotkey = Controls::Label(hWnd, LBL_SHOW_CLIPS_HOTK, hInstance);
	LabelShowClipsHotkey.SuspendLayout();
	LabelShowClipsHotkey.SetX(left);
	LabelShowClipsHotkey.SetY(12);
	LabelShowClipsHotkey.SetText(L"Clips menu shortcut:");
	LabelShowClipsHotkey.ResumeLayout();
	HWND hHotKey = AddHotkeyCtrl(hWnd, hFontStd, left + 120, 10, 150, 20, hInstance, HKY_SHOW_CLIPS_MENU);

	CheckboxRunAtStartup = Controls::Checkbox(hWnd, CHK_RUN_AT_STARTUP, hInstance);
	CheckboxRunAtStartup.SuspendLayout();
	CheckboxRunAtStartup.SetX(left);
	CheckboxRunAtStartup.SetY(44);
	CheckboxRunAtStartup.SetText(L"Run Winclipper at startup");
	CheckboxRunAtStartup.UseBasicTheme = !useLightTheme;
	CheckboxRunAtStartup.ResumeLayout();

	CheckboxPasteOnClick = Controls::Checkbox(hWnd, CHK_PASTE_ON_CLICK, hInstance);
	CheckboxPasteOnClick.SuspendLayout();
	CheckboxPasteOnClick.SetX(left);
	CheckboxPasteOnClick.SetY(68);
	CheckboxPasteOnClick.SetText(L"Paste immediately on click");
	CheckboxPasteOnClick.UseBasicTheme = !useLightTheme;
	CheckboxPasteOnClick.ResumeLayout();

	CheckboxSelectSecondClip = Controls::Checkbox(hWnd, CHK_SLCT_2ND_CLIP, hInstance);
	CheckboxSelectSecondClip.SuspendLayout();
	CheckboxSelectSecondClip.SetX(left);
	CheckboxSelectSecondClip.SetY(92);
	CheckboxSelectSecondClip.SetText(L"Auto-select second clip");
	CheckboxSelectSecondClip.UseBasicTheme = !useLightTheme;
	CheckboxSelectSecondClip.ResumeLayout();

	CheckboxShowPreview = Controls::Checkbox(hWnd, CHK_SHOW_PREVIEW, hInstance);
	CheckboxShowPreview.SuspendLayout();
	CheckboxShowPreview.SetX(left);
	CheckboxShowPreview.SetY(116);
	CheckboxShowPreview.SetText(L"Show clip preview");
	CheckboxShowPreview.UseBasicTheme = !useLightTheme;
	CheckboxShowPreview.ResumeLayout();
	AssignNewControlsToPage(PAGE_GENERAL);

	// Clips
	LabelMaxClipsDisplay = Controls::Label(hWnd, LBL_MAX_CLIPS_DISPLAY, hInstance);
	LabelMaxClipsDisplay.SuspendLayout();
	LabelMaxClipsDisplay.SetX(left);
	LabelMaxClipsDisplay.SetY(12);
	LabelMaxClipsDisplay.SetText(L"Number of clips to display:");
	LabelMaxClipsDisplay.ResumeLayout();

	SpinnerMaxClipsDisplay = Controls::Spinner(hWnd, TXT_MAX_CLIPS_DISPLAY, UD_MAX_CLIPS_DISPLAY, hInstance);
	SpinnerMaxClipsDisplay.SuspendLayout();
	SpinnerMaxClipsDisplay.SetX(left + 180);
	SpinnerMaxClipsDisplay.SetY(10);
	SpinnerMaxClipsDisplay.SetMinValue(UserSettings::MAX_DISPLAY_LOWER);
	SpinnerMaxClipsDisplay.SetMaxValue(UserSettings::MAX_DISPLAY_UPPER);
	SpinnerMaxClipsDisplay.ResumeLayout();

	LabelMaxClipsSaved = Controls::Label(hWnd, LBL_MAX_CLIPS_SAVED, hInstance);
	LabelMaxClipsSaved.SuspendLayout();
	LabelMaxClipsSaved.SetX(left);
	LabelMaxClipsSaved.SetY(42);
	LabelMaxClipsSaved.SetText(L"Maximum clips to save:");
	LabelMaxClipsSaved.ResumeLayout();

	SpinnerMaxClipsSaved = Controls::Spinner(hWnd, TXT_MAX_CLIPS_SAVED, UD_MAX_CLIPS_SAVED, hInstance);
	SpinnerMaxClipsSaved.SuspendLayout();
	SpinnerMaxClipsSaved.SetX(left + 180);
	SpinnerMaxClipsSaved.SetY(40);
	SpinnerMaxClipsSaved.SetMinValue(UserSettings::MAX_SAVED_LOWER);
	SpinnerMaxClipsSaved.SetMaxValue(UserSettings::MAX_SAVED_UPPER);
	SpinnerMaxClipsSaved.ResumeLayout();

	LabelMenuDisplayChars = Controls::Label(hWnd, LBL_MENU_DISP_CHARS, hInstance);
	LabelMenuDisplayChars.SuspendLayout();
	LabelMenuDisplayChars.SetX(left);
	LabelMenuDisplayChars.SetY(72);
	LabelMenuDisplayChars.SetText(L"Number of preview characters:");
	LabelMenuDisplayChars.ResumeLayout();

	SpinnerMenuDisplayChars = Controls::Spinner(hWnd, TXT_MENU_DISP_CHARS, UD_MENU_DISP_CHARS, hInstance);
	SpinnerMenuDisplayChars.SuspendLayout();
	SpinnerMenuDisplayChars.SetX(left + 180);
	SpinnerMenuDisplayChars.SetY(70);
	SpinnerMenuDisplayChars.SetMinValue(UserSettings::MENU_CHARS_LOWER);
	SpinnerMenuDisplayChars.SetMaxValue(UserSettings::MENU_CHARS_UPPER);
	SpinnerMenuDisplayChars.ResumeLayout();

	CheckboxSaveToDisk = Controls::Checkbox(hWnd, CHK_SAVE_TO_DISK, hInstance);
	CheckboxSaveToDisk.SuspendLayout();
	CheckboxSaveToDisk.SetX(left);
	CheckboxSaveToDisk.SetY(102);
	CheckboxSaveToDisk.SetText(L"Save clips to disk");
	CheckboxSaveToDisk.UseBasicTheme = !useLightTheme;
	CheckboxSaveToDisk.ResumeLayout();

	AddButton(left, 128, 110, L"&Clear clips", BTN_CLEAR_CLIPS, hInstance);

	AddText(left, 166, 280, 32, L"Keep content marked as private (e.g. passwords) when it comes from these programs:",
		LBL_PRIVATE_ALLOWED, hInstance);
	privateAllowedList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOX, nullptr,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_SORT | LBS_NOINTEGRALHEIGHT,
		ScaleX(left), ScaleY(200), ScaleX(180), ScaleY(80), hWnd, reinterpret_cast<HMENU>(LST_PRIVATE_ALLOWED), hInstance, nullptr);
	SendMessageW(privateAllowedList, WM_SETFONT, reinterpret_cast<WPARAM>(hFontStd), FALSE);
	AddButton(left + 190, 200, 90, L"&Add...", BTN_ADD_PRIVATE, hInstance);
	AddButton(left + 190, 230, 90, L"&Remove", BTN_REMOVE_PRIVATE, hInstance);
	AssignNewControlsToPage(PAGE_CLIPS);

	// Images
	CheckboxSaveImages = Controls::Checkbox(hWnd, CHK_SAVE_IMAGES, hInstance);
	CheckboxSaveImages.SuspendLayout();
	CheckboxSaveImages.SetX(left);
	CheckboxSaveImages.SetY(12);
	CheckboxSaveImages.SetText(L"Save images to the clipboard");
	CheckboxSaveImages.UseBasicTheme = !useLightTheme;
	CheckboxSaveImages.ResumeLayout();

	LabelMaxCacheMbytes = Controls::Label(hWnd, LBL_MAX_IMG_CACHE_MB, hInstance);
	LabelMaxCacheMbytes.SuspendLayout();
	LabelMaxCacheMbytes.SetX(left + 18);
	LabelMaxCacheMbytes.SetY(40);
	LabelMaxCacheMbytes.SetText(L"Max image cache size (MB):");
	LabelMaxCacheMbytes.ResumeLayout();

	SpinnerMaxCacheMbytes = Controls::Spinner(hWnd, TXT_MAX_IMG_CACHE_MB, UD_MAX_IMG_CACHE_MB, hInstance);
	SpinnerMaxCacheMbytes.SuspendLayout();
	SpinnerMaxCacheMbytes.SetX(left + 180);
	SpinnerMaxCacheMbytes.SetY(38);
	SpinnerMaxCacheMbytes.SetMinValue(UserSettings::MAX_CACHE_MBYTES_LOWER);
	SpinnerMaxCacheMbytes.SetMaxValue(UserSettings::MAX_CACHE_MBYTES_UPPER);
	SpinnerMaxCacheMbytes.ResumeLayout();

	AddButton(left, 72, 150, L"&Recreate thumbnails", BTN_RECREATE_THUMBNAILS, hInstance);
	AssignNewControlsToPage(PAGE_IMAGES);

	// Updates
	CheckboxNotifyUpdates = Controls::Checkbox(hWnd, CHK_NOTIFY_UPDATES, hInstance);
	CheckboxNotifyUpdates.SuspendLayout();
	CheckboxNotifyUpdates.SetX(left);
	CheckboxNotifyUpdates.SetY(12);
	CheckboxNotifyUpdates.SetText(L"Notify me about updates");
	CheckboxNotifyUpdates.UseBasicTheme = !useLightTheme;
	CheckboxNotifyUpdates.ResumeLayout();

	AddText(left, 42, 280, 32, L"Updates are installed only after you confirm them, and only if they are signed by this project.",
		IDC_STATIC, hInstance);
	AddText(left, 84, 280, 16, L"Installed version:", LBL_INSTALLED_VERSION, hInstance);
	AddButton(left, 108, 110, L"Check &now", BTN_CHECK_UPDATES, hInstance);
	AssignNewControlsToPage(PAGE_UPDATES);

	// About
	AddText(left, 12, 280, 16, (L"Winclipper " + Updater::CurrentVersion()).c_str(), IDC_STATIC, hInstance);
	AddText(left, 36, 280, 16, L"Copyright (C) 2021 Carson Shook", IDC_STATIC, hInstance);
	AddText(left, 54, 280, 16, L"Copyright (C) 2026 Tom - tomcek42 (fork)", IDC_STATIC, hInstance);
	AddText(left, 78, 280, 16, L"Released under the MIT License.", IDC_STATIC, hInstance);
	HWND link = CreateWindowExW(0, WC_LINK,
		L"<a href=\"https://github.com/tomcek42/Winclipper\">github.com/tomcek42/Winclipper</a>",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, ScaleX(left), ScaleY(102), ScaleX(280), ScaleY(18),
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
	CheckboxSaveImages.SetValue(UserSettings::SaveImages());
	SpinnerMaxCacheMbytes.SetEnabled(UserSettings::SaveImages());
	RefreshPrivateAllowedList();

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
