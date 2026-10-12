#include "stdafx.h"
#include "MainWindow.h"


ATOM MainWindow::RegisterMainWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWindow::MainWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(LONG_PTR);
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINCLIPPER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szMainWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_WINCLIPPER));

	return RegisterClassExW(&wcex);
}

LRESULT MainWindow::MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// We want to capture a pointer to the current instance of the class
	// so we can call non-static methods from a static context.
	MainWindow * pThis = nullptr;

	if (message == WM_NCCREATE)
	{
#pragma warning( suppress : 26490 )
		pThis = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
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
		pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, AGNOSTIC_USERDATA));
	}

	if (!pThis)
	{
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_CLEARCLIPS:
			pThis->WmCommandIdmClearClips(hWnd, wParam, lParam);
			break;
		case IDM_RECREATE_THUMBNAILS:
			pThis->WmCommandIdmRecreateThumbnails(hWnd, wParam, lParam);
			break;
		case IDM_SETTINGS:
			pThis->WmCommandIdmSettings(hWnd, wParam, lParam);
			break;
		case IDM_INSTALL_UPDATE:
			pThis->WmCommandIdmInstallUpdate(hWnd, wParam, lParam);
			break;
		case IDM_ABOUT:
			pThis->WmCommandIdmAbout(hWnd, wParam, lParam);
			break;
		case IDM_EXIT:
			pThis->WmCommandIdmExit(hWnd, wParam, lParam);
			break;
		case NTF_CMENUHOTKEY_CHANGED:
			pThis->WmCommandNtfCmenuHotkeyChanged(hWnd, wParam, lParam);
			break;
		case NTF_MAXDISPLAY_CHANGED:
			pThis->WmCommandNtfMaxDisplayChanged(hWnd, wParam, lParam);
			break;
		case NTF_MAXSAVED_CHANGED:
			pThis->WmCommandNtfMaxSavedChanged(hWnd, wParam, lParam);
			break;
		case NTF_MENUCHARS_CHANGED:
			pThis->WmCommandNtfMenuCharsChanged(hWnd, wParam, lParam);
			break;
		case NTF_SAVETODISK_CHANGED:
			pThis->WmCommandNtfSaveToDiskChanged(hWnd, wParam, lParam);
			break;
		case NTF_SAVEIMAGES_CHANGED:
			pThis->WmCommandNtfSaveImagesChanged(hWnd, wParam, lParam);
			break;
		case NTF_MAXCACHEMBYTES_CHANGED:
			pThis->WmCommandNtfMaxcacheMBytesChanged(hWnd, wParam, lParam);
			break;
		case NTF_AUTOUPDATE_CHANGED:
			pThis->WmCommandNtfNotifyUpdatesChanged(hWnd, wParam, lParam);
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WMAPP_NOTIFYCALLBACK:
		// React to clicks on the notify icon
		switch (LOWORD(lParam))
		{
		case NIN_KEYSELECT:
			pThis->WmappNCallNinKeySelect(hWnd, wParam, lParam);
			break;
		case WM_LBUTTONDBLCLK:
			pThis->WmappNCallWmLButtonDblClk(hWnd, wParam, lParam);
			break;
		case WM_CONTEXTMENU:
			pThis->WmappNCallWmContextMenu(hWnd, wParam, lParam);
			break;
		case NIN_BALLOONUSERCLICK:
			pThis->PromptInstallUpdate();
			break;
		}
		break;
	case Updater::WMAPP_UPDATE_STATE:
		pThis->WmappUpdateState(hWnd, wParam, lParam);
		break;
	case WM_TIMER:
		pThis->WmTimer(hWnd, wParam, lParam);
		break;
	case WM_CLOSE:
		// The Restart Manager closes the app this way when an update replaces it.
		pThis->WmCommandIdmExit(hWnd, wParam, lParam);
		break;
	case WM_ENDSESSION:
		if (wParam)
		{
			pThis->WmCommandIdmExit(hWnd, wParam, lParam);
		}
		break;
	case WM_CLIPBOARDUPDATE:
		pThis->WmClipboardUpdate(hWnd, wParam, lParam);
		break;
	case WM_HOTKEY:
		pThis->WmHotkey(hWnd, wParam, lParam);
		break;
	case WM_CREATE:
		pThis->WmCreate(hWnd, wParam, lParam);
		break;
	case WM_DESTROY:
		pThis->WmDestroy(hWnd, wParam, lParam);
		break;
	case WM_INITMENU:
		pThis->WmInitMenu(hWnd, wParam, lParam);
		break;
	case WM_INITMENUPOPUP:
		pThis->WmInitMenuPopup(hWnd, wParam, lParam);
		break;
	case WM_MENURBUTTONUP:
		pThis->WmMenuRButtonUp(hWnd, wParam, lParam);
		break;
	case WM_UNINITMENUPOPUP:
		pThis->WmUninitMenuPopup(hWnd, wParam, lParam);
		break;
	case WM_MENUSELECT:
		pThis->WmMenuSelect(hWnd, wParam, lParam);
		break;
	default:
		{
			if (message == taskbarRestart)
			{
				pThis->WmRestartTaskbar(hWnd, wParam, lParam);
			}
			pThis->WndProcDefault(hWnd, wParam, lParam);
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	return 0;
}

// Reads the file version from the EXE's version resource, so the About box
// cannot fall behind a version bump.
static std::wstring ExecutableVersion()
{
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
	{
		return L"?";
	}
	DWORD handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(path, &handle);
	std::vector<BYTE> data(size);
	VS_FIXEDFILEINFO * info = nullptr;
	UINT infoLength = 0;
	if (size == 0 || !GetFileVersionInfoW(path, 0, size, data.data())
		|| !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &infoLength) || infoLength < sizeof(*info))
	{
		return L"?";
	}
	return std::to_wstring(HIWORD(info->dwFileVersionMS)) + L"." + std::to_wstring(LOWORD(info->dwFileVersionMS))
		+ L"." + std::to_wstring(HIWORD(info->dwFileVersionLS));
}

// Message handler for about box.
INT_PTR CALLBACK MainWindow::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, (L"Winclipper, Version " + ExecutableVersion()).c_str());
		// Opened from the tray menu, the owner is the hidden main window.
		SetForegroundWindow(hDlg);
		return (INT_PTR)true;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)true;
		}
		break;
	}
	return (INT_PTR)false;
}

LRESULT MainWindow::WmCommandIdmClearClips(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	int choice = MessageBoxA(hWnd,
		"This will delete all clips on the Winclipper clipboard.\r\n\r\n Are you sure you want to do this?", "Winclipper",
		MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_TASKMODAL | MB_SETFOREGROUND);

	if (choice == IDYES)
	{
		cManager->ClearClips();
	}
	return 0;
}

LRESULT MainWindow::WmCommandIdmRecreateThumbnails(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	cManager->RecreateThumbnails();
	return 0;
}

LRESULT MainWindow::WmCommandIdmSettings(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HWND settingsHwnd = settingsWindow.GetHandle();

	if (!IsWindowVisible(settingsHwnd))
	{
		ShowWindow(settingsHwnd, SW_SHOW);
	}
	SetForegroundWindow(settingsHwnd);

	return 0;
}

LRESULT MainWindow::WmCommandIdmAbout(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HWND sender = (HWND)lParam == nullptr ? hWnd : (HWND)lParam;
	DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), sender, About);
	return 0;
}

LRESULT MainWindow::WmCommandIdmExit(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// wait until finished writing, or give up after 10 seconds (should be long enough)
	int waitUntilFinishedAttempts = 5;
	while (!(cManager->NoPendingClipWrites() && UserSettings::NoPendingSettingWrites()) && waitUntilFinishedAttempts > 0)
	{
		Sleep(2000);
		waitUntilFinishedAttempts--;
	}
	OnDemandBitmapManager::SafeClose();
	DestroyWindow(hWnd);

	return 0;
}

LRESULT MainWindow::WmCommandNtfCmenuHotkeyChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	UnregisterHotKey(GetHandle(), ID_REG_HOTKEY);
	RegisterHotKey(GetHandle(), ID_REG_HOTKEY, HIBYTE(UserSettings::ClipsMenuHotkeyTrl()), LOBYTE(UserSettings::ClipsMenuHotkeyTrl()));
	return 0;
}

LRESULT MainWindow::WmCommandNtfMaxDisplayChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT MainWindow::WmCommandNtfMaxSavedChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	cManager->UpdateMaxClips();
	return 0;
}

LRESULT MainWindow::WmCommandNtfMenuCharsChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT MainWindow::WmCommandNtfSaveToDiskChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	cManager->SetSaveToDisk(UserSettings::SaveToDisk());
	return 0;
}

LRESULT MainWindow::WmCommandNtfSaveImagesChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	cManager->UpdateSavedImages();
	return 0;
}

LRESULT MainWindow::WmCommandNtfMaxcacheMBytesChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	OnDemandBitmapManager::SetMaxBytes(unsigned long long{ UserSettings::MaxCacheMegabytes() } * 1048576);
	return 0;
}

LRESULT MainWindow::WmCommandNtfNotifyUpdatesChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	Updater::SetEnabled(UserSettings::NotifyUpdates());
	return 0;
}

void MainWindow::StartBlinking(const wchar_t * tip)
{
	SetTimer(windowHandle, ID_BLINK_TIMER, BLINK_INTERVAL_MS, nullptr);
	SetNotificationIcon(trayIcon, tip);
}

void MainWindow::StopBlinking()
{
	KillTimer(windowHandle, ID_BLINK_TIMER);
	trayIconVisible = true;
	SetNotificationIcon(trayIcon, L"Winclipper");
}

// A found update makes the tray icon blink and shows a notification once per
// version; nothing is installed until the user confirms in PromptInstallUpdate.
LRESULT MainWindow::WmappUpdateState(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case Updater::STATE_AVAILABLE:
		{
			const std::wstring version = Updater::AvailableVersion();
			if (!version.empty() && version != notifiedUpdateVersion)
			{
				notifiedUpdateVersion = version;
				const std::wstring title = L"Winclipper " + version + L" is available";
				StartBlinking(title.c_str());
				ShowBalloon(title.c_str(), L"Click here to see what's new and install it.");
			}
		}
		break;
	case Updater::STATE_INSTALLING:
		StartBlinking(L"Winclipper - installing update");
		break;
	case Updater::STATE_UP_TO_DATE:
		MessageBoxW(settingsWindow.GetHandle(), (L"Winclipper " + Updater::CurrentVersion() + L" is up to date.").c_str(),
			L"Winclipper", MB_OK | MB_ICONINFORMATION);
		break;
	case Updater::STATE_CHECK_FAILED:
		MessageBoxW(settingsWindow.GetHandle(), L"Could not check for updates. Please try again later.",
			L"Winclipper", MB_OK | MB_ICONWARNING);
		break;
	default:
		StopBlinking();
		break;
	}
	return 0;
}

static HRESULT CALLBACK UpdateDialogCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR refData)
{
	if (msg == TDN_HYPERLINK_CLICKED)
	{
		const wchar_t * url = reinterpret_cast<const wchar_t *>(lParam);
		// Only the release page link that the dialog itself contains.
		if (url != nullptr && wcsncmp(url, L"https://github.com/tomcek42/Winclipper/", 39) == 0)
		{
			ShellExecuteW(hwnd, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
		}
	}
	return S_OK;
}

void MainWindow::PromptInstallUpdate()
{
	const std::wstring version = Updater::AvailableVersion();
	if (version.empty())
	{
		return;
	}
	StopBlinking();

	const std::wstring instruction = L"Winclipper " + version + L" is available";
	const std::wstring content = L"You are using version " + Updater::CurrentVersion() + L". <a href=\""
		+ Updater::ReleaseNotesUrl() + L"\">See what's new</a>\n\nInstalling needs administrator rights. "
		L"Winclipper closes during the installation and starts again afterwards.";
	const TASKDIALOG_BUTTON buttons[] = { { IDOK, L"&Install" }, { IDCANCEL, L"&Later" } };

	TASKDIALOGCONFIG config = { sizeof(config) };
	config.hwndParent = windowHandle;
	config.hInstance = hInst;
	config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
	config.pszWindowTitle = L"Winclipper update";
	config.pszMainIcon = TD_INFORMATION_ICON;
	config.pszMainInstruction = instruction.c_str();
	config.pszContent = content.c_str();
	config.cButtons = ARRAYSIZE(buttons);
	config.pButtons = buttons;
	config.nDefaultButton = IDOK;
	config.pfCallback = UpdateDialogCallback;

	SetForegroundWindow(windowHandle);
	int pressed = IDCANCEL;
	if (SUCCEEDED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr)) && pressed == IDOK)
	{
		Updater::InstallAvailable();
	}
}

LRESULT MainWindow::WmCommandIdmInstallUpdate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	PromptInstallUpdate();
	return 0;
}

void MainWindow::ShowBalloon(const wchar_t * title, const wchar_t * text)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = windowHandle;
	nid.uID = UID_NOTIFYICON;
	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_INFO;
	wcscpy_s(nid.szInfoTitle, title);
	wcscpy_s(nid.szInfo, text);
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

LRESULT MainWindow::WmTimer(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (wParam == ID_BLINK_TIMER)
	{
		trayIconVisible = !trayIconVisible;
		SetNotificationIcon(trayIconVisible ? trayIcon : blankTrayIcon, nullptr);
	}
	return 0;
}

LRESULT MainWindow::WmappNCallNinKeySelect(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// This prevents any additional keypresses from being 
	// interpreted until after ShowClipsMenu has finished.
	if (!ninKeypressLocked)
	{
		ninKeypressLocked = true;
		const int xCoord GET_X_LPARAM(wParam);
		const int yCoord GET_Y_LPARAM(wParam);

		POINT cPos = { 0, 0 };
		if (xCoord != NULL && yCoord != NULL)
		{
			cPos.x = xCoord;
			cPos.y = yCoord;
		}
		else
		{
			cPos.x = 10;
			cPos.y = 10;
		}
		cManager->ShowClipsMenu(GetHandle(), &cPos, true);
		previewWindow->ClearCache();
		ninKeypressLocked = false;
	}
	return 0;
}

LRESULT MainWindow::WmappNCallWmLButtonDblClk(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HWND settingsHwnd = settingsWindow.GetHandle();

	if (!IsWindowVisible(settingsHwnd))
	{
		ShowWindow(settingsHwnd, SW_SHOW);
	}
	SetForegroundWindow(settingsHwnd);

	return 0;
}

LRESULT MainWindow::WmappNCallWmContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	POINT cPos = { 0, 0 };
	if (!GetCursorPos(&cPos))
	{
		cPos.x = 10;
		cPos.y = 10;
	}
	cManager->ShowClipsMenu(GetHandle(), &cPos, true);
	previewWindow->ClearCache();

	return 0;
}

LRESULT MainWindow::WmClipboardUpdate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (!cManager->ClipLock)
	{
		// Add clips to the clips manager when the clipboard updates.
		bool success = false;
		int retries = 10;
		while (!success && retries > 0)
		{
			success = cManager->AddToClips(hWnd);
			retries--;
			if (!success && retries > 0)
			{
				// Only wait when another program still holds the clipboard;
				// sleeping unconditionally blocked the UI on every copy.
				Sleep(100);
			}
		}
	}
	else
	{
		cManager->ClipLock = false;
	}
	return 0;
}

LRESULT MainWindow::WmHotkey(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// When the global hotkey is called, get the current window
	// so we can reactivate it later, and then show the clips menu.
	POINT cPos = { 0, 0 };
	if (!GetCursorPos(&cPos))
	{
		cPos.x = 10;
		cPos.y = 10;
	}
	cManager->ShowClipsMenu(GetHandle(), &cPos, false);
	previewWindow->ClearCache();

	return 0;
}

LRESULT MainWindow::WmCreate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	taskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));
	// Add the notification icon
	if (!AddNotificationIcon(hWnd, ((LPCREATESTRUCT) lParam)->hInstance))
	{
		MessageBox(hWnd,
			L"There was an error adding the notification icon. You might want to try restarting your computer, or reinstalling this application.",
			L"Error adding icon", MB_OK);

		return -1;
	}

	UserSettings::Subscribe(hWnd);
	SettingsWindow::Subscribe(hWnd);

	cManager = new ClipsManager();

	// Lets the Restart Manager start Winclipper again after an update closed it.
	RegisterApplicationRestart(L"", RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_REBOOT);
	Updater::Start(hWnd);
	Updater::SetEnabled(UserSettings::NotifyUpdates());
	return 0;
}

LRESULT MainWindow::WmDestroy(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	delete previewWindow;
	delete cManager;
	previewWindow = nullptr;
	cManager = nullptr;
	KillTimer(hWnd, ID_BLINK_TIMER);
	DeleteNotificationIcon();
	PostQuitMessage(0);
	return 0;
}

LRESULT MainWindow::WmInitMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// keep track of the top level menu to differentiate for display purposes
	activePopupMenu = topPopupMenu = (HMENU)wParam;
	cManager->SelectDefaultMenuItem(cManager->GetSize() > 1 ? UserSettings::Select2ndClip() : false);
	return 0;
}

LRESULT MainWindow::WmMenuRButtonUp(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HMENU menu = CreatePopupMenu();
	AppendMenuW(menu, MF_STRING, 1, L"Delete");

	POINT cPos = { 0, 0 };
	if (!GetCursorPos(&cPos))
	{
		cPos.x = 10;
		cPos.y = 10;
	}

	int menuSelection;
	menuSelection = TrackPopupMenuEx(
		menu,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NONOTIFY | TPM_RETURNCMD | TPM_RECURSE,
		cPos.x - 5,
		cPos.y - 5,
		hWnd,
		nullptr
	);

	if (menuSelection == 1)
	{
		// wParam is the item position, which also counts non-clip items
		// (More..., separators, Settings). Clip items carry the ID index + 1
		// in both the top menu and the More... submenu, so use that.
		const UINT itemId = GetMenuItemID(reinterpret_cast<HMENU>(lParam), static_cast<int>(wParam));

		if (itemId != static_cast<UINT>(-1) && itemId >= 1 && itemId <= cManager->GetSize())
		{
			cManager->DeleteClipAt(itemId - 1);
			PostMessageW(hWnd, WM_CANCELMODE, 0, 0);
		}
	}

	return 0;
}

LRESULT MainWindow::WmInitMenuPopup(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// whenever a submenu appears, we save it the active popup
	activePopupMenu = (HMENU)wParam;
	return 0;
}

LRESULT MainWindow::WmUninitMenuPopup(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// when a submenu is destroyed, we set the active popup back to the top
	activePopupMenu = topPopupMenu;
	return 0;
}

LRESULT MainWindow::WmMenuSelect(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (UserSettings::ShowPreview())
	{
		// Display preview window when user hovers on a clip
		DWORD flags = HIWORD(wParam);
		if ((flags & MF_HILITE) && !(flags & MF_POPUP))
		{
			const unsigned int i = LOWORD(wParam);
			if (i != 0 && i <= cManager->GetSize())
			{
				previewWindow->SetPreviewClip(cManager->GetClipAt(i - 1));

				unsigned int offset = 1;

				if (activePopupMenu != topPopupMenu)
				{
					offset += UserSettings::MaxDisplayClips();
				}

				/* BLACK MAGIC 0_0 */
				// This gets a MenuItemRect for an item position we know (0),
				// and then performs a hit-test on the menuItem that sits over.
				// This might be the same item, or it could be a different one
				// if the menu has been scrolled. That is our delta value that
				// can be used to calculate the true offset for the preview.
				// We do this by getting the real rect at the top of the menu
				// and subtracting it's bottom value from the original first
				// item's rect top, and then we can use the resulting value
				// as an offset in pixels to subtract from our desired item.
				RECT hitTestRect = { 0, 0, 0, 0 };
				int hitTestDelta = 0;
				long sizeOffset = 0;
				if (GetMenuItemRect(hWnd, activePopupMenu, 0, &hitTestRect))
				{
					POINT hitTest = { hitTestRect.left + 1, hitTestRect.top + 1 };
					hitTestDelta = MenuItemFromPoint(hWnd, activePopupMenu, hitTest);

					RECT currentRect = { 0, 0, 0, 0 };
					if (GetMenuItemRect(hWnd, activePopupMenu, hitTestDelta - 1, &currentRect))
					{
						sizeOffset = currentRect.bottom - hitTestRect.top;
					}
				}

				RECT menuItemDims = { 0, 0, 0, 0 };
				if (GetMenuItemRect(hWnd, activePopupMenu, i - offset, &menuItemDims))
				{
					menuItemDims.top = menuItemDims.top - sizeOffset;
					menuItemDims.bottom = menuItemDims.bottom - sizeOffset;
					previewWindow->MoveRelativeToRect(&menuItemDims, i);
					previewWindow->Show();
				}
			}
			else
			{
				previewWindow->Hide();
			}
		}
		else
		{
			previewWindow->Hide();
		}
	}
	return 0;
}
LRESULT MainWindow::WmRestartTaskbar(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (!AddNotificationIcon(hWnd, hInst))
	{
		MessageBox(hWnd,
			L"There was an error adding the notification icon. You might want to try restarting your computer, or reinstalling this application.",
			L"Error adding icon", MB_OK);

		return -1;
	}
	return 0;
}

// This ensures that if the mouse is moved outside of the popup menu,
// then the preview window will be hidden. This is achieved by 
// comparing the previous mouse location against the current, and if
// they are different, we check to see if they are now selecting a
// menuItem. If not, then hide the preview window.
LRESULT MainWindow::WndProcDefault(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (previewWindow != nullptr && IsWindowVisible(previewWindow->GetHandle()))
	{
		POINT cPos = { 0, 0 };
		if (GetCursorPos(&cPos))
		{
			if (cPos.x != previousMouseLoc->x || cPos.y != previousMouseLoc->y)
			{
				if (MenuItemFromPoint(hWnd, activePopupMenu, cPos) == -1)
				{
					previewWindow->Hide();
				}

				previousMouseLoc->x = cPos.x;
				previousMouseLoc->y = cPos.y;

			}
		}
	}
	return 0;
}

// Adds the application notification icon to the notifcation area
bool MainWindow::AddNotificationIcon(HWND hWnd, HINSTANCE hInstance)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = hWnd;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
	nid.uID = UID_NOTIFYICON;
	wcscpy_s(nid.szTip, L"Winclipper");
	if (trayIcon == nullptr)
	{
		// Load the icon for high DPI.
		LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_WINCLIPPER), LIM_SMALL, &trayIcon);
	}
	if (blankTrayIcon == nullptr)
	{
		// Fully transparent icon for the "off" phase of the update blink.
		const int width = GetSystemMetrics(SM_CXSMICON);
		const int height = GetSystemMetrics(SM_CYSMICON);
		const size_t maskBytes = static_cast<size_t>((width + 15) / 16 * 2) * height;
		std::vector<BYTE> andMask(maskBytes, 0xFF);
		std::vector<BYTE> xorMask(maskBytes, 0x00);
		blankTrayIcon = CreateIcon(hInstance, width, height, 1, 1, andMask.data(), xorMask.data());
	}
	nid.hIcon = trayIcon;
	// Show the notification.
	Shell_NotifyIcon(NIM_ADD, &nid);

	nid.uVersion = NOTIFYICON_VERSION_4;
	return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void MainWindow::SetNotificationIcon(HICON icon, const wchar_t * tip)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = windowHandle;
	nid.uID = UID_NOTIFYICON;
	nid.uFlags = NIF_ICON;
	nid.hIcon = icon;
	if (tip != nullptr)
	{
		nid.uFlags |= NIF_TIP;
		wcscpy_s(nid.szTip, tip);
	}
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

// Removes the application notification icon from the notifcation area
bool MainWindow::DeleteNotificationIcon()
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = windowHandle;
	nid.uID = UID_NOTIFYICON;
	return Shell_NotifyIcon(NIM_DELETE, &nid);
}

MainWindow::MainWindow(HINSTANCE hInstance, SettingsWindow & settingsWindow)
	:settingsWindow(settingsWindow)
{
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCLIPPER, szMainWindowClass, MAX_LOADSTRING);

	RegisterMainWindowClass(hInstance);
	if (InitMainWindow(hInstance))
	{
		previewWindow = new PreviewWindow(hInstance, GetHandle());
	}

	// used for the dialog box.
	hInst = hInstance;
}

MainWindow::~MainWindow()
{
}

bool MainWindow::InitMainWindow(HINSTANCE hInstance)
{
	HWND hWnd = CreateWindowExW(0, szMainWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		0, 0, 100, 100, nullptr, nullptr, hInstance, this);

	if (!hWnd)
	{
		return false;
	}

	windowHandle = hWnd;

	// Adds the main window to the clipboard format listener list.
	AddClipboardFormatListener(hWnd);

	// Register the hotkey that we have stored in settings
	RegisterHotKey(hWnd, ID_REG_HOTKEY, HIBYTE(UserSettings::ClipsMenuHotkeyTrl()), LOBYTE(UserSettings::ClipsMenuHotkeyTrl()));

	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);

	OnDemandBitmapManager::SetMaxBytes(unsigned long long{ UserSettings::MaxCacheMegabytes() } * 1048576);

	return true;
}

HWND MainWindow::GetHandle()
{
	return windowHandle;
}

UINT MainWindow::taskbarRestart = 0;

