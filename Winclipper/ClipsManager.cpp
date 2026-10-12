#include "stdafx.h"
#include "wincodec.h"
#include "ClipsManager.h"

void ClipsManager::SaveClipsAsync()
{
    if (UserSettings::SaveToDisk())
    {
        // Only the caller that raises the count from 0 starts the writer.
        if (clipsWriterWaitCount++ == 0)
        {
            // Mark pending before the thread starts so NoPendingClipWrites
            // cannot report idle between the count reaching 0 and WriteClips.
            isWriterFinished = false;
            std::thread([this]() { DelayClipsWriter(&clipsWriterWaitCount, this); }).detach();
        }
    }
    else
    {
        if (File::Exists(fullClipsPath.c_str()))
        {
            File::Delete(fullClipsPath.c_str());
        }
    }
}

void ClipsManager::DelayClipsWriter(std::atomic<int> * waitCount, ClipsManager * cs)
{
    while (*waitCount != 0)
    {
        // we wait to make sure that no aditional actions
        // have been taken since we started waiting.
        (*waitCount) = 1;
        Sleep(CLIPS_WRITE_DELAY);
        (*waitCount)--;
    }
        
    cs->WriteClips();
}

void ClipsManager::WriteClips()
{
    isWriterFinished = false;
	try
	{
		File::WriteProtected(fullClipsPath, clips.Serialize());
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Winclipper", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL | MB_SETFOREGROUND);
	}
    isWriterFinished = true;
}

void ClipsManager::ReadClips()
{
    if (File::Exists(fullClipsPath.c_str()))
    {
		try
		{
			// A plaintext file from an older version is accepted once; the
			// constructor saves right after reading, which encrypts it.
			bool wasPlaintext = false;
			clips.Deserialize(File::ReadProtected(fullClipsPath, wasPlaintext));
		}
		catch (const std::exception& e)
		{
			MessageBoxA(nullptr, e.what(), "Winclipper", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL | MB_SETFOREGROUND);
		}
    }
}

bool ClipsManager::SendPasteInput(void) noexcept
{
	bool success = false;
	bool ctrlPressed = GetKeyState(VK_CONTROL) & 0x8000;

	// I hate duplicating this much code, but it seems
	// that arrays are the more correct and more reliable
	// way to handle this, and they require consts.
	// Unfortunately, that means this duplication is unavoidable.
	if (ctrlPressed)
	{
		const int INPUT_COUNT = 3;
		INPUT inputs[INPUT_COUNT] = {};
		ZeroMemory(inputs, sizeof(inputs));

		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.time = 0;
		inputs[0].ki.wVk = VK_CONTROL;

		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.time = 0;
		inputs[1].ki.wVk = 0x56;

		inputs[2].type = INPUT_KEYBOARD;
		inputs[2].ki.time = 0;
		inputs[2].ki.wVk = 0x56;
		inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

		success = (SendInput(INPUT_COUNT, inputs, sizeof(INPUT)) == INPUT_COUNT);
	}
	else
	{
		const int INPUT_COUNT = 4;
		INPUT inputs[4] = {};
		ZeroMemory(inputs, sizeof(inputs));

		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.time = 0;
		inputs[0].ki.wVk = VK_CONTROL;

		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.time = 0;
		inputs[1].ki.wVk = 0x56;

		inputs[2].type = INPUT_KEYBOARD;
		inputs[2].ki.time = 0;
		inputs[2].ki.wVk = 0x56;
		inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

		inputs[3].type = INPUT_KEYBOARD;
		inputs[3].ki.time = 0;
		inputs[3].ki.wVk = VK_CONTROL;
		inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

		success = (SendInput(INPUT_COUNT, inputs, sizeof(INPUT)) == INPUT_COUNT);
	}
	
	return success;
}

void ClipsManager::ClearBitmaps()
{
	clips.RemoveAllOfFormat(CF_DIB);
}

ClipsManager::ClipsManager()
{
	fullClipsPath = File::JoinPath(File::GetAppDir(), L"clips.dat");

	windows10ReleaseId = RegistryUtilities::GetWindows10ReleaseId();

	OnDemandBitmapManager::ProtectDiskCache();
	ReadClips();
    UpdateMaxClips();
	UpdateSavedImages();
	SaveClipsAsync();
}

ClipsManager::~ClipsManager()
{
}

bool ClipsManager::NoPendingClipWrites() noexcept
{
    return clipsWriterWaitCount == 0 && isWriterFinished;
}


void ClipsManager::UpdateMaxClips()
{
	clips.SetMaxSize(UserSettings::MaxSavedClips());
    SaveClipsAsync();
}

void ClipsManager::SetSaveToDisk(bool saveToDisk)
{
	OnDemandBitmapManager::SetSaveToDisk(saveToDisk);

	if (!UserSettings::SaveToDisk())
	{
		clips.LoadAllResources();
		OnDemandBitmapManager::DeleteAllFromDisk();
	}
    SaveClipsAsync();
}

void ClipsManager::UpdateSavedImages()
{
	if (!UserSettings::SaveImages())
	{
		ClearBitmaps();
		SaveClipsAsync();
	}
}

void ClipsManager::RecreateThumbnails()
{
	clips.PurgeThumbnails();
}

void ClipsManager::ClearClips()
{
	clips.Clear();
    SaveClipsAsync();
}

// Copies a CF_DIB block from the clipboard. Any process can put arbitrary
// bytes there, so every header field is checked against the block size
// before it is used for an allocation or a copy.
static bool CopyClipboardDib(const BYTE * data, SIZE_T available, bool forceOpaque,
	std::shared_ptr<BITMAPINFOHEADER> & header, std::vector<RGBQUAD> & quads, std::shared_ptr<BYTE> & bits)
{
	if (available < sizeof(BITMAPINFOHEADER))
	{
		return false;
	}

	BITMAPINFOHEADER bmih;
	memcpy(&bmih, data, sizeof(bmih));

	const WORD bitCount = bmih.biBitCount;
	// See: https://docs.microsoft.com/en-us/windows/desktop/api/d2d1/nf-d2d1-id2d1rendertarget-getmaximumbitmapsize
	if (bmih.biSize < sizeof(BITMAPINFOHEADER)
		|| bmih.biSize > available
		|| bmih.biWidth <= 0
		|| bmih.biWidth >= 16384
		|| bmih.biHeight == 0
		|| bmih.biHeight >= 16384
		|| bmih.biHeight <= -16384
		|| bmih.biPlanes != 1
		|| (bitCount != 1 && bitCount != 4 && bitCount != 8
			&& bitCount != 16 && bitCount != 24 && bitCount != 32)
		|| (bmih.biCompression != BI_RGB && bmih.biCompression != BI_BITFIELDS))
	{
		return false;
	}

	SIZE_T offset = bmih.biSize;

	if (bmih.biCompression == BI_BITFIELDS && bmih.biSize > sizeof(BITMAPINFOHEADER))
	{
		// V4/V5 headers carry the three masks inside the header; only a
		// plain BITMAPINFOHEADER is stored, so move them behind it.
		if (bmih.biSize < sizeof(BITMAPINFOHEADER) + 3 * sizeof(RGBQUAD))
		{
			return false;
		}
		quads.resize(3);
		memcpy(quads.data(), data + sizeof(BITMAPINFOHEADER), 3 * sizeof(RGBQUAD));
	}
	else
	{
		unsigned int colorBytes = bmih.biClrUsed;
		if (!colorBytes)
		{
			if (bitCount <= 8)
			{
				colorBytes = 1u << bitCount;
			}
			else if (bmih.biCompression == BI_BITFIELDS)
			{
				colorBytes = 3;
			}
		}
		const unsigned int maxColors = bitCount <= 8 ? (1u << bitCount) : 256u;
		if (colorBytes > maxColors || colorBytes * sizeof(RGBQUAD) > available - offset)
		{
			return false;
		}
		quads.resize(colorBytes);
		if (colorBytes > 0)
		{
			memcpy(quads.data(), data + offset, colorBytes * sizeof(RGBQUAD));
			offset += colorBytes * sizeof(RGBQUAD);
		}
	}

	// Only the size the header implies is needed; a larger biSizeImage
	// is ignored rather than trusted.
	const unsigned long long size = Bitmap::ComputeImageSize(bmih);
	if (size == 0 || size > available - offset)
	{
		return false;
	}

	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biSizeImage = static_cast<DWORD>(size);

	try
	{
		bits = std::shared_ptr<BYTE>(new BYTE[static_cast<size_t>(size)], array_deleter<BYTE>());
	}
	catch (const std::bad_alloc&)
	{
		return false;
	}
	memcpy(bits.get(), data + offset, static_cast<size_t>(size));

	if (bitCount == 32 && forceOpaque)
	{
		for (unsigned long long i = 3; i < size; i += 4)
		{
			bits.get()[i] = 255;
		}
	}

	header = std::make_shared<BITMAPINFOHEADER>(bmih);
	return true;
}

// Password managers and other apps mark sensitive content with these formats
// so that clipboard history tools skip it. Must be called with the clipboard open.
// See: https://learn.microsoft.com/windows/win32/dataxchg/clipboard-formats#cloud-clipboard-and-clipboard-history-formats
static bool ClipboardContentExcluded()
{
	static const UINT excludeFormat = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
	static const UINT viewerIgnoreFormat = RegisterClipboardFormatW(L"Clipboard Viewer Ignore");
	static const UINT historyFormat = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");

	if ((excludeFormat != 0 && IsClipboardFormatAvailable(excludeFormat))
		|| (viewerIgnoreFormat != 0 && IsClipboardFormatAvailable(viewerIgnoreFormat)))
	{
		return true;
	}

	bool excluded = false;
	if (historyFormat != 0 && IsClipboardFormatAvailable(historyFormat))
	{
		HANDLE hData = GetClipboardData(historyFormat);
		// Fail closed: an unreadable marker still means the owner set it.
		excluded = true;
		if (hData != nullptr && GlobalSize(hData) >= sizeof(DWORD))
		{
			const DWORD * value = static_cast<const DWORD *>(GlobalLock(hData));
			if (value != nullptr)
			{
				excluded = *value == 0;
				GlobalUnlock(hData);
			}
		}
	}
	return excluded;
}

// File name of the program that owns the clipboard, or empty if it cannot be
// determined (e.g. content set without an owner window).
static std::wstring ClipboardOwnerExeName()
{
	HWND owner = GetClipboardOwner();
	DWORD processId = 0;
	if (owner == nullptr || GetWindowThreadProcessId(owner, &processId) == 0 || processId == 0)
	{
		return std::wstring();
	}
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (process == nullptr)
	{
		return std::wstring();
	}
	wchar_t path[MAX_PATH];
	DWORD length = MAX_PATH;
	const BOOL ok = QueryFullProcessImageNameW(process, 0, path, &length);
	CloseHandle(process);
	if (!ok || length == 0)
	{
		return std::wstring();
	}
	return std::wstring(PathFindFileNameW(path));
}

bool ClipsManager::AddToClips(HWND hWnd)
{
    if (!OpenClipboard(hWnd))
    {
        return false;
    }

	// Private content is kept only from programs the user allowed explicitly.
	// The owner is matched by file name, so this is a convenience, not a
	// security boundary (any process of the user can read the clipboard anyway).
	if (ClipboardContentExcluded() && !UserSettings::IsPrivateAllowed(ClipboardOwnerExeName()))
	{
		CloseClipboard();
		return true;
	}

	std::shared_ptr<Clip> clip = std::make_shared<Clip>();

	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		HANDLE psClipboardData = GetClipboardData(CF_UNICODETEXT);
		if (psClipboardData != nullptr)
		{
			const wchar_t * data = static_cast<wchar_t*>(GlobalLock(psClipboardData));
			if (data != nullptr)
			{
				// The data comes from any process; it need not be terminated
				// within its block, so never read past GlobalSize.
				const size_t maxChars = GlobalSize(psClipboardData) / sizeof(wchar_t);
				clip->SetUnicodeText(std::wstring(data, wcsnlen(data, maxChars)));
				GlobalUnlock(psClipboardData);
				clip->AddFormat(CF_UNICODETEXT);
			}
			else
			{
				GlobalUnlock(psClipboardData);
			}
		}
	}

	if (UserSettings::SaveImages() && IsClipboardFormatAvailable(CF_DIB))
	{
		bool hasTransparentBitmapFormat = false;

		UINT currentFormat = 0;
		do
		{
			currentFormat = EnumClipboardFormats(currentFormat);
			// If CF_BITMAP is listed before the other better formats, then we can assume
			// that it was "real" format and the others are synthesized.
			if (currentFormat == CF_BITMAP)
			{
				currentFormat = 0;
			}
			hasTransparentBitmapFormat = currentFormat == CF_DIBV5 || currentFormat == CF_DIB || currentFormat == CF_PNG;
		} while (currentFormat != 0 && !hasTransparentBitmapFormat);

		HANDLE psClipboardData = GetClipboardData(CF_DIB);

		if (psClipboardData != nullptr)
		{
			const BYTE * dibData = static_cast<const BYTE *>(GlobalLock(psClipboardData));

			std::shared_ptr<BITMAPINFOHEADER> bitmapInfoHeader;
			std::vector<RGBQUAD> quads;
			std::shared_ptr<BYTE> pMyBits;

			if (dibData != nullptr && CopyClipboardDib(dibData, GlobalSize(psClipboardData), !hasTransparentBitmapFormat, bitmapInfoHeader, quads, pMyBits))
			{
				GlobalUnlock(psClipboardData);

				try
				{
					clip->SetDibBitmap(
						bitmapInfoHeader,
						quads,
						pMyBits
					);
					clip->AddFormat(CF_DIB);
				}
				catch (const std::exception&)
				{
					// Not really sure I should put up a message if this fails
				}
			}
			else
			{
				GlobalUnlock(psClipboardData);
				
			}
		}
	}

	CloseClipboard();

	if (clip->AnyFormats())
	{
		clips.AddFront(clip);
	}
	else
	{
		clip->MarkForDelete();
	}
    SaveClipsAsync();
    return true;
}

bool ClipsManager::SetClipboardToClipAtIndex(HWND hWnd, unsigned int index)
{
    if (!OpenClipboard(hWnd))
    {
        return false;
    }

    EmptyClipboard();

	std::shared_ptr<Clip> clip = index == 0 ? GetClipAt(index) : clips.RemoveAt(index);

	if (clip != nullptr)
	{
		if (clip->ContainsFormat(CF_UNICODETEXT))
		{
			size_t clipLength = clip->UnicodeTextWString().length();

			HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (clipLength + 1) * sizeof(wchar_t));
			if (hClipboardData != nullptr)
			{
				wchar_t * pwcData = static_cast<wchar_t*>(GlobalLock(hClipboardData));
				if (pwcData != nullptr)
				{
					wcscpy_s(pwcData, clipLength + 1, clip->UnicodeText());

					GlobalUnlock(hClipboardData);

					SetClipboardData(CF_UNICODETEXT, hClipboardData);
				}
				else
				{
					GlobalUnlock(hClipboardData);
				}
			}
		}

		if (clip->ContainsFormat(CF_DIB))
		{
			auto bmiHeader = clip->DibBitmapInfoHeader();
			auto quadCollection = clip->RgbQuadCollection();
			const size_t rgbQuadsCount = quadCollection.size();
			const DWORD bitmapSize = clip->DibSize();

			HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, bmiHeader->biSize + rgbQuadsCount * sizeof(RGBQUAD) + bitmapSize);

			if (hClipboardData != nullptr && bmiHeader != nullptr)
			{
				BITMAPINFO * bmi = static_cast<BITMAPINFO *>(GlobalLock(hClipboardData));
				
				if (bmi != nullptr)
				{
					bmi->bmiHeader = *bmiHeader;

					for (size_t i = 0; i < rgbQuadsCount; i++)
					{
						bmi->bmiColors[i] = quadCollection[i];
					}

					memcpy_s(reinterpret_cast<BYTE *>(bmi) + (bmiHeader->biSize + rgbQuadsCount * sizeof(RGBQUAD)), bitmapSize, clip->DibBitmapBits().get(), bitmapSize);

					GlobalUnlock(hClipboardData);
					
					if (!SetClipboardData(CF_DIB, hClipboardData))
					{
						GlobalFree(hClipboardData);
					}
				}
				else
				{
					GlobalUnlock(hClipboardData);
				}
			}
		}

		if (index != 0)
		{
			clips.AddFront(clip);
		}
	}
    CloseClipboard();
    SaveClipsAsync();
    return true;
}

const unsigned int ClipsManager::GetSize() noexcept
{
	return clips.Size();
}

void ClipsManager::DeleteClipAt(unsigned int index)
{
	auto clip = clips.RemoveAt(index);
	if (clip != nullptr)
	{
		clip->MarkForDelete();
		SaveClipsAsync();
	}
}

std::shared_ptr<Clip> ClipsManager::GetClipAt(unsigned int index)
{
	try
	{
		return clips.At(index);
	}
	catch (const std::exception e)
	{
		return nullptr;
	}
}

void ClipsManager::ShowClipsMenu(HWND hWnd, const LPPOINT cPos, bool showExit)
{
    HWND curWin = GetForegroundWindow();
	HWND taskbarWnd = FindWindow(L"Shell_TrayWnd", nullptr);
	HWND child = GetWindow(curWin, GW_CHILD);

	// Ensures that the last *real* active window is selected
	// when the user tries to paste using the shell_notifyicon.
	if (curWin == taskbarWnd || IsChild(taskbarWnd, curWin))
	{
		curWin = taskbarWnd; // just in case isChild instead of equal.
		do
		{
			curWin = GetWindow(curWin, GW_HWNDNEXT);
		} while (!IsWindowVisible(curWin));
	}
	else if (child != nullptr)
	{
		if (windows10ReleaseId > 0 && windows10ReleaseId < 1809)
		{
			// Microsoft Edge hack.
			// (Note: as of 1809, this doesn't appear to be necessary)
			// If a child window looks like it belongs to MS Edge,
			// then search previous from curWin until we get to a
			// visible window. That should be the one we care about.
			child = FindWindowExW(curWin, nullptr, L"Windows.UI.Core.CoreWindow", L"Microsoft Edge");
			if (child != nullptr)
			{
				do
				{
					curWin = GetWindow(curWin, GW_CHILD);
				} while (!IsWindowVisible(curWin));
			}
		}
	}

    if (hWnd != nullptr && curWin != nullptr)
    {
		SetForegroundWindow(curWin);

        // Since WindowsNT (I think), it's necessary to attach the process to the thread
        // that we want to get the active window of, otherwise we get an unspecified value.
        // GetForegroundWindow was unreliable with UWP applications because it took too long
        // to restore the active window (the text field) after the foreground window activated.
        AttachThreadInput(GetWindowThreadProcessId(curWin, nullptr), GetWindowThreadProcessId(hWnd, nullptr), true);
		HWND actWin = GetActiveWindow();

        SetForegroundWindow(hWnd);

        HMENU menu = CreatePopupMenu();

        const unsigned int curSize = GetSize();

        if (curSize > 0)
        {
            unsigned int j = curSize > UserSettings::MaxDisplayClips() ? UserSettings::MaxDisplayClips() : curSize;

            for (unsigned int i = 0; i < j; i++)
            {
                std::shared_ptr<Clip> clip = GetClipAt(i);
                if (clip != nullptr)
                {
					if (clip->ContainsFormat(CF_UNICODETEXT))
					{
						std::wstring menuText;
						menuText = clip->GetUnicodeMenuText(UserSettings::MenuDisplayChars());

						AppendMenuW(menu, MF_STRING, UINT_PTR{ i } +1, menuText.c_str());
						if (clip->ContainsFormat(CF_DIB))
						{
							SetMenuItemBitmaps(menu, UINT{ i } +1, MF_BYCOMMAND, clip->GetThumbnail(), clip->GetThumbnail());
						}
					}
					else if (clip->ContainsFormat(CF_DIB))
					{
						AppendMenuW(menu, MF_STRING, UINT_PTR{ i } + 1, (L"[IMAGE] " + std::to_wstring(clip->DibHeight()) + L" x " + std::to_wstring(clip->DibWidth())).c_str());
						SetMenuItemBitmaps(menu, UINT{ i } + 1, MF_BYCOMMAND, clip->GetThumbnail(), clip->GetThumbnail());
					}
                }
            }
            if (curSize > UserSettings::MaxDisplayClips())
            {
                HMENU sMenu = CreatePopupMenu();
                
                for (/*j is equal to displayCount */; j < curSize; j++)
                {
					std::shared_ptr<Clip> clip = GetClipAt(j);
                    if (clip != nullptr)
                    {
						if (clip->ContainsFormat(CF_UNICODETEXT))
						{
							std::wstring menuText;
							menuText = clip->GetUnicodeMenuText(UserSettings::MenuDisplayChars());

							AppendMenuW(sMenu, MF_STRING, UINT_PTR{ j } + 1, menuText.c_str());
							if (clip->ContainsFormat(CF_DIB))
							{
								SetMenuItemBitmaps(sMenu, UINT{ j } + 1, MF_BYCOMMAND, clip->GetThumbnail(), clip->GetThumbnail());
							}
						}
						else if (clip->ContainsFormat(CF_DIB))
						{
							AppendMenuW(sMenu, MF_STRING, UINT_PTR{ j } + 1, (L"[IMAGE] " + std::to_wstring(clip->DibHeight()) + L" x " + std::to_wstring(clip->DibWidth())).c_str());
							SetMenuItemBitmaps(sMenu, UINT{ j } + 1, MF_BYCOMMAND, clip->GetThumbnail(), clip->GetThumbnail());
						}
					}
                }
                AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(sMenu), L"More...");
            }
        }
        else
        {
            AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, L"Nothing on the clipboard");
        }

        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CLEARCLIPS_SELECT, L"&Clear Clips");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, SETTINGS_SELECT, L"&Settings");
		// Debug info
		//wchar_t str[128];
		//wsprintf(str, L"Window handle is 0x%08x", curWin);
		//AppendMenuW(menu, MF_STRING, SETTINGS_SELECT, str);
        if (showExit)
        {
			AppendMenuW(menu, MF_STRING, EXIT_SELECT, L"&Exit");
        }
        
        int menuSelection;
        menuSelection = TrackPopupMenu(
            menu,
            TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
            cPos->x - 5,
            cPos->y - 5,
            0,
            hWnd,
            NULL
        );
		// apparently this line is needed to ensure
		// consistent popup menu behavior.
		PostMessageW(hWnd, WM_USER, 0, 0);
        switch (menuSelection)
        {
		case CANCELED_SELECTION:
			{
			SetForegroundWindow(curWin);
			SetActiveWindow(actWin);
				int retries = 10;
				HWND temp = GetActiveWindow();
				while (temp != actWin && retries > 0)
				{
					temp = GetActiveWindow();
					retries--;
					Sleep(20);
				}
			}
			break;
        case CLEARCLIPS_SELECT:
            PostMessageW(hWnd, WM_COMMAND, IDM_CLEARCLIPS, 0);
			SetForegroundWindow(curWin);
			SetActiveWindow(actWin);
            break;
        case SETTINGS_SELECT:
            PostMessageW(hWnd, WM_COMMAND, IDM_SETTINGS, 0);
            break;
        case EXIT_SELECT:
			PostMessageW(hWnd, WM_COMMAND, IDM_EXIT, 0);
            break;
        default:
			ClipLock = true;

			// Have to subtract 1 from index because returning 0 in
			// TrackPopupMenu means cancelation
			int retries = 10;
			bool success = SetClipboardToClipAtIndex(actWin, menuSelection - 1);
			while (!success && retries > 0)
			{
				Sleep(100);
				success = SetClipboardToClipAtIndex(actWin, menuSelection - 1);
				retries--;
			}

			// Attempts to set the active window. Usually succeeds immediately, but not always 
			SetForegroundWindow(curWin);
			SetActiveWindow(actWin);
			retries = 10;
			HWND temp = GetActiveWindow();
			while (temp != actWin && retries > 0)
			{
				Sleep(100);
				temp = GetActiveWindow();
				retries--;
			}

			if (UserSettings::PasteOnClick())
			{
				retries = 10;
				bool pasteSuccess = SendPasteInput();
				while (!pasteSuccess && retries > 0)
				{
					OutputDebugString(L"retying Paste\r\n");
					Sleep(100);
					pasteSuccess = SendPasteInput();
					retries--;
				}
			}
            break;
        }

        // detatch from thread of the active window
        AttachThreadInput(GetWindowThreadProcessId(curWin, nullptr), GetWindowThreadProcessId(hWnd, nullptr), false);

        PostMessageW(hWnd, WM_NULL, 0, 0);
    }
}

void ClipsManager::SelectDefaultMenuItem(bool select2ndClip) noexcept
{
	INPUT inputs[2] = {};
	ZeroMemory(inputs, sizeof(inputs));

	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.time = 0;
	inputs[0].ki.wVk = VK_DOWN;

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.time = 0;
	inputs[1].ki.wVk = VK_DOWN;
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));

    if (select2ndClip)
    {
		SendInput(2, inputs, sizeof(INPUT));
    }
    return;
}
