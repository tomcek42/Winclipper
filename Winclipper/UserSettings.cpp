#include "stdafx.h"
#include "UserSettings.h"
#include <algorithm>

std::wstring UserSettings::fullSettingPath;
int	UserSettings::maxDisplayClips;
int	UserSettings::maxSavedClips;
int	UserSettings::menuDisplayChars;
WORD UserSettings::clipsMenuHotkey;
bool UserSettings::saveToDisk;
bool UserSettings::select2ndClip;
bool UserSettings::showPreview;
bool UserSettings::pasteOnClick;
bool UserSettings::notifyUpdates;
std::vector<std::wstring> UserSettings::privateAllowed;
std::mutex UserSettings::privateAllowedMutex;
bool UserSettings::saveImages;
unsigned int UserSettings::maxCacheMegabytes;
std::atomic<int> UserSettings::settingWriterWaitCount{ 0 };
std::atomic<bool> UserSettings::isWriterFinished{ true };

// stous - string to unsigned short
// NOTE: May move to static utility class in future
unsigned short stous(std::string str)
{
    try
    {
        const int convint = stoi(str);

        if (convint < (std::numeric_limits<unsigned short>::min)())
        {
            throw std::range_error("Converted sequence below minimum range of unsigned short");
        }
        if (convint > (std::numeric_limits<unsigned short>::max)())
        {
            throw std::range_error("Converted sequence above maximum range of unsigned short");
        }

        return (unsigned short)convint;
    }
    catch(const std::exception e)
    {
        printf("An exception occured at %p", &e);
        throw;
    }
}

// stous - wstring to unsigned short
// NOTE: May move to static utility class in future
unsigned short stous(std::wstring str)
{
    try
    {
        const int convint = stoi(str);

        if (convint < (std::numeric_limits<unsigned short>::min)())
        {
            throw std::range_error("Converted sequence below minimum range of unsigned short");
        }
        if (convint > (std::numeric_limits<unsigned short>::max)())
        {
            throw std::range_error("Converted sequence above maximum range of unsigned short");
        }

        return (unsigned short)convint;
    }
    catch (const std::exception e)
    {
        printf("An exception occured at %p", &e);
        throw;
    }
}

bool UserSettings::NoPendingSettingWrites()
{
    return settingWriterWaitCount == 0 && isWriterFinished;
}

// Attempt to load any settings from the disk if they exist.
// If no settings can be found, set all defaults for first time run. 
// They will only be written to disk once a change has been made.
void UserSettings::InitializeSettings()
{
	maxDisplayClips = MAX_DISPLAY_DEF;
	maxSavedClips = MAX_SAVED_DEF;
	menuDisplayChars = MENU_CHARS_DEF;
	clipsMenuHotkey = CMENU_HOTKEY_DEF;
	saveToDisk = SAVE_TO_DISK_DEF;
	select2ndClip = SLCT_2ND_CLIP_DEF;
	showPreview = SHOW_PREVIEW_DEF;
	saveImages = SAVE_IMAGES_DEF;
	pasteOnClick = PASTE_ON_CLICK_DEF;
	notifyUpdates = NOTIFY_UPDATES_DEF;
	// Requested by the project owner; removable in Settings.
	privateAllowed = { L"1Password.exe" };
	maxCacheMegabytes = MAX_CACHE_MBYTES_DEF;

	settingWriterWaitCount = 0;
	isWriterFinished = true;

	fullSettingPath = File::JoinPath(File::GetAppDir(), L"settings.dat");

	if (File::Exists(fullSettingPath.c_str()))
	{
		std::vector<std::wstring> settings = File::ReadAllLines(fullSettingPath.c_str());

		if (!settings.empty())
		{
			UserSettings::Deserialize(settings);
		}
	}
	else
	{
		SaveSettingsAsync();
	}
}

int UserSettings::MaxDisplayClips()
{
    if (maxDisplayClips < MAX_DISPLAY_LOWER)
    {
        maxDisplayClips = MAX_DISPLAY_LOWER;
    }
    else if (maxDisplayClips > MAX_DISPLAY_UPPER)
    {
        maxDisplayClips = MAX_DISPLAY_UPPER;
    }
    return maxDisplayClips;
}

void UserSettings::SetMaxDisplayClips(int maxDisplayClips)
{
    if (maxDisplayClips != UserSettings::maxDisplayClips)
    {
        if (maxDisplayClips < MAX_DISPLAY_LOWER)
        {
            UserSettings::maxDisplayClips = MAX_DISPLAY_LOWER;
        }
        else if (maxDisplayClips > MAX_DISPLAY_UPPER)
        {
            UserSettings::maxDisplayClips = MAX_DISPLAY_UPPER;
        }
        else
        {
            UserSettings::maxDisplayClips = maxDisplayClips;
        }
        SaveSettingsAsync();
    }
}

int UserSettings::MaxSavedClips()
{
    if (maxSavedClips < MAX_SAVED_LOWER)
    {
        maxSavedClips = MAX_SAVED_LOWER;
    }
    else if (maxSavedClips > MAX_SAVED_UPPER)
    {
        maxSavedClips = MAX_SAVED_UPPER;
    }
    return maxSavedClips;
}

void UserSettings::SetMaxSavedClips(int maxSavedClips)
{
    if (maxSavedClips != UserSettings::maxSavedClips)
    {
        if (maxSavedClips < MAX_SAVED_LOWER)
        {
            UserSettings::maxSavedClips = MAX_SAVED_LOWER;
        }
        else if (maxSavedClips > MAX_SAVED_UPPER)
        {
            UserSettings::maxSavedClips = MAX_SAVED_UPPER;
        }
        else
        {
            UserSettings::maxSavedClips = maxSavedClips;
        }
		UserSettings::SendNotifcation(NTF_MAXSAVED_CHANGED, nullptr);
        SaveSettingsAsync();
    }
}

int UserSettings::MenuDisplayChars()
{
    if (menuDisplayChars < MENU_CHARS_LOWER)
    {
        menuDisplayChars = MENU_CHARS_LOWER;
    }
    else if (menuDisplayChars > MENU_CHARS_UPPER)
    {
        menuDisplayChars = MENU_CHARS_UPPER;
    }
    return menuDisplayChars;
}

void UserSettings::SetMenuDisplayChars(int menuDisplayChars)
{
    if (menuDisplayChars != UserSettings::menuDisplayChars)
    {
        if (menuDisplayChars < MENU_CHARS_LOWER)
        {
            UserSettings::menuDisplayChars = MENU_CHARS_LOWER;
        }
        else if (menuDisplayChars > MENU_CHARS_UPPER)
        {
            UserSettings::menuDisplayChars = MENU_CHARS_UPPER;
        }
        else
        {
            UserSettings::menuDisplayChars = menuDisplayChars;
        }
		SaveSettingsAsync();
    }
}

WORD UserSettings::ClipsMenuHotkey()
{
    if (clipsMenuHotkey == 0)
    {
        clipsMenuHotkey = CMENU_HOTKEY_DEF;
    }
    return clipsMenuHotkey;
}

// This translates the ClipsMenuHotkey values (which are
// what Winclipper stores internally) into the modifier
// keys used by the RegisterHotkey method.
//
// For some reason MS thought it was a good idea to
// flip the values used by SHIFT and ALT for the
// two different kinds of hotkeys.
WORD UserSettings::ClipsMenuHotkeyTrl()
{
    if (clipsMenuHotkey == 0)
    {
        clipsMenuHotkey = CMENU_HOTKEY_DEF;
    }

    const BYTE lByte = LOBYTE(clipsMenuHotkey);
    const BYTE hByte = HIBYTE(clipsMenuHotkey);

    BYTE hByteAdjusted = 0;

    if ((hByte & HOTKEYF_SHIFT) == HOTKEYF_SHIFT)
    {
        hByteAdjusted |= MOD_SHIFT;
    }
    if ((hByte & HOTKEYF_CONTROL) == HOTKEYF_CONTROL)
    {
        hByteAdjusted |= MOD_CONTROL;
    }
    if ((hByte & HOTKEYF_ALT) == HOTKEYF_ALT)
    {
        hByteAdjusted |= MOD_ALT;
    }
    if ((hByte & HOTKEYF_EXT) == HOTKEYF_EXT)
    {
        hByteAdjusted |= MOD_WIN;
    }

    return MAKEWORD(lByte, hByteAdjusted);;
}

void UserSettings::SetClipsMenuHotkey(WORD clipsMenuHotkey)
{
    if (clipsMenuHotkey != UserSettings::clipsMenuHotkey)
    {
        UserSettings::clipsMenuHotkey = clipsMenuHotkey;
		UserSettings::SendNotifcation(NTF_CMENUHOTKEY_CHANGED, nullptr);
        SaveSettingsAsync();
    }
}

bool UserSettings::SaveToDisk()
{
    return UserSettings::saveToDisk;
}

void UserSettings::SetSaveToDisk(bool saveToDisk)
{
    if (saveToDisk != UserSettings::saveToDisk)
    {
        UserSettings::saveToDisk = saveToDisk;
		UserSettings::SendNotifcation(NTF_SAVETODISK_CHANGED, nullptr);
        SaveSettingsAsync();
    }
}

bool UserSettings::Select2ndClip()
{
    return UserSettings::select2ndClip;
}

void UserSettings::SetSelect2ndClip(bool select2ndClip)
{
    if (select2ndClip != UserSettings::select2ndClip)
    {
        UserSettings::select2ndClip = select2ndClip;
		SaveSettingsAsync();
    }
}

bool UserSettings::ShowPreview()
{
    return UserSettings::showPreview;
}

void UserSettings::SetShowPreview(bool showPreview)
{
    if (showPreview != UserSettings::showPreview)
    {
        UserSettings::showPreview = showPreview;
		SaveSettingsAsync();
    }
}

bool UserSettings::PasteOnClick()
{
	return UserSettings::pasteOnClick;
}

void UserSettings::SetPasteOnClick(bool pasteOnClick)
{
	if (pasteOnClick != UserSettings::pasteOnClick)
	{
		UserSettings::pasteOnClick = pasteOnClick;
		SaveSettingsAsync();
	}
}

bool UserSettings::NotifyUpdates()
{
	return UserSettings::notifyUpdates;
}

void UserSettings::SetNotifyUpdates(bool notifyUpdates)
{
	if (notifyUpdates != UserSettings::notifyUpdates)
	{
		UserSettings::notifyUpdates = notifyUpdates;
		UserSettings::SendNotifcation(NTF_AUTOUPDATE_CHANGED, nullptr);
		SaveSettingsAsync();
	}
}

// Only bare file names of printable ASCII (no path, no separator): settings.dat
// is untrusted and written through a narrow stream.
bool UserSettings::IsValidProgramName(const std::wstring & name)
{
	if (name.empty() || name.size() > 100 || name.front() == L' ' || name.back() == L' ')
	{
		return false;
	}
	for (wchar_t c : name)
	{
		const bool allowed = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')
			|| c == L' ' || c == L'.' || c == L'_' || c == L'-' || c == L'(' || c == L')' || c == L'+';
		if (!allowed)
		{
			return false;
		}
	}
	return true;
}

std::vector<std::wstring> UserSettings::PrivateAllowedPrograms()
{
	std::lock_guard<std::mutex> lock(privateAllowedMutex);
	return privateAllowed;
}

bool UserSettings::IsPrivateAllowed(const std::wstring & exeName)
{
	std::lock_guard<std::mutex> lock(privateAllowedMutex);
	for (const auto & name : privateAllowed)
	{
		if (_wcsicmp(name.c_str(), exeName.c_str()) == 0)
		{
			return true;
		}
	}
	return false;
}

bool UserSettings::AddPrivateAllowed(const std::wstring & exeName)
{
	if (!IsValidProgramName(exeName))
	{
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(privateAllowedMutex);
		for (const auto & name : privateAllowed)
		{
			if (_wcsicmp(name.c_str(), exeName.c_str()) == 0)
			{
				return true;
			}
		}
		if (privateAllowed.size() >= PRIVATE_ALLOWED_MAX)
		{
			return false;
		}
		privateAllowed.push_back(exeName);
	}
	SaveSettingsAsync();
	return true;
}

void UserSettings::RemovePrivateAllowed(const std::wstring & exeName)
{
	{
		std::lock_guard<std::mutex> lock(privateAllowedMutex);
		privateAllowed.erase(std::remove_if(privateAllowed.begin(), privateAllowed.end(),
			[&](const std::wstring & name) { return _wcsicmp(name.c_str(), exeName.c_str()) == 0; }),
			privateAllowed.end());
	}
	SaveSettingsAsync();
}

bool UserSettings::SaveImages()
{
	return UserSettings::saveImages;
}

void UserSettings::SetSaveImages(bool saveImages)
{
	if (saveImages != UserSettings::saveImages)
	{
		UserSettings::saveImages = saveImages;
		UserSettings::SendNotifcation(NTF_SAVEIMAGES_CHANGED, nullptr);
		SaveSettingsAsync();
	}
}

unsigned int UserSettings::MaxCacheMegabytes()
{
	if (maxCacheMegabytes < MENU_CHARS_LOWER)
	{
		maxCacheMegabytes = MENU_CHARS_LOWER;
	}
	else if (maxCacheMegabytes > MENU_CHARS_UPPER)
	{
		maxCacheMegabytes = MENU_CHARS_UPPER;
	}
	return maxCacheMegabytes;
}

void UserSettings::SetMaxCacheMegabytes(unsigned int maxCacheMegabytes)
{
	if (maxCacheMegabytes != UserSettings::maxCacheMegabytes)
	{
		if (maxCacheMegabytes < MAX_CACHE_MBYTES_LOWER)
		{
			UserSettings::maxCacheMegabytes = MAX_CACHE_MBYTES_LOWER;
		}
		else if (maxCacheMegabytes > MAX_CACHE_MBYTES_UPPER)
		{
			UserSettings::maxCacheMegabytes = MAX_CACHE_MBYTES_UPPER;
		}
		else
		{
			UserSettings::maxCacheMegabytes = maxCacheMegabytes;
		}
		UserSettings::SendNotifcation(NTF_MAXCACHEMBYTES_CHANGED, nullptr);
		SaveSettingsAsync();
	}
}

// Writes a serialized version of the current settings to disk.
void UserSettings::WriteSettings()
{
    isWriterFinished = false;
    std::vector<std::wstring> settings = UserSettings::Serialize();

    File::WriteAllLines(fullSettingPath.c_str(), settings);
    isWriterFinished = true;
}

// Returns a vector of type std::wstring containing std::wstring
// representations of all current settings.
std::vector<std::wstring> UserSettings::Serialize()
{
    std::vector<std::wstring> retVal = {
        (std::to_wstring(MAX_DISPLAY) + SEPARATOR + std::to_wstring(MaxDisplayClips())),
        (std::to_wstring(MAX_SAVED) + SEPARATOR + std::to_wstring(MaxSavedClips())),
        (std::to_wstring(MENU_CHARS) + SEPARATOR + std::to_wstring(MenuDisplayChars())),
        (std::to_wstring(CMENU_HOTKEY) + SEPARATOR + std::to_wstring(ClipsMenuHotkey())),
        (std::to_wstring(SAVE_TO_DISK) + SEPARATOR + std::to_wstring(SaveToDisk())),
        (std::to_wstring(SLCT_2ND_CLIP) + SEPARATOR + std::to_wstring(Select2ndClip())),
		(std::to_wstring(SHOW_PREVIEW) + SEPARATOR + std::to_wstring(ShowPreview())),
		(std::to_wstring(SAVE_IMAGES) + SEPARATOR + std::to_wstring(SaveImages())),
		(std::to_wstring(MAX_CACHE_MBYTES) + SEPARATOR + std::to_wstring(MaxCacheMegabytes())),
		(std::to_wstring(PASTE_ON_CLICK) + SEPARATOR + std::to_wstring(PasteOnClick())),
		(std::to_wstring(NOTIFY_UPDATES) + SEPARATOR + std::to_wstring(NotifyUpdates())),
    };

	std::wstring allowed;
	for (const auto & name : PrivateAllowedPrograms())
	{
		if (!allowed.empty())
		{
			allowed += L"/";
		}
		allowed += name;
	}
	retVal.push_back(std::to_wstring(PRIVATE_ALLOWED) + SEPARATOR + allowed);

    return retVal;
}

// Deserializes a vector of type std::wstring into setting values,
// and then sets them where availble. 
// Define a constant for each setting, and then add it to the switch. 
// Expects a format of:
// 1|settingValue1
// 2|settingValue2
// n|settingValueN
void UserSettings::Deserialize(std::vector<std::wstring> srData)
{
    std::map<int, std::wstring> store;

    for (std::wstring value : srData)
    {
        try
        {
            store.emplace(
                stoi(value.substr(0, value.find(SEPARATOR))),
                value.substr(value.find(SEPARATOR) + 1)
            );
        }
        catch (const std::exception e)
        {
            // do nothing
        }
    }

    for (std::pair<int, std::wstring> pair : store)
    {
        // settings.dat is untrusted: a non-numeric or out-of-range value must
        // not abort startup. Skip the entry and keep the default.
        try
        {
            switch (pair.first)
            {
            case MAX_DISPLAY:
                SetMaxDisplayClips(stoi(pair.second));
                break;
            case MAX_SAVED:
                SetMaxSavedClips(stoi(pair.second));
                break;
            case MENU_CHARS:
                SetMenuDisplayChars(stoi(pair.second));
                break;
            case CMENU_HOTKEY:
                SetClipsMenuHotkey(stous(pair.second));
                break;
            case SAVE_TO_DISK:
                {
                    bool result = pair.second.compare(std::to_wstring(true)) == 0;
                    SetSaveToDisk(result);
                }
                break;
            case SLCT_2ND_CLIP:
                {
                    bool result = pair.second.compare(std::to_wstring(true)) == 0;
                    SetSelect2ndClip(result);
                }
            break;
            case SHOW_PREVIEW:
                {
                    bool result = pair.second.compare(std::to_wstring(true)) == 0;
                    SetShowPreview(result);
                }
                break;
    		case SAVE_IMAGES:
    			{
    				bool result = pair.second.compare(std::to_wstring(true)) == 0;
    				SetSaveImages(result);
    			}
    		break;
    		case MAX_CACHE_MBYTES:
    			{
    				SetMaxCacheMegabytes(stoi(pair.second));
    			}
    			break;
    		case PASTE_ON_CLICK:
    			{
    				bool result = pair.second.compare(std::to_wstring(true)) == 0;
    				SetPasteOnClick(result);
    			}
    		break;
    		case NOTIFY_UPDATES:
    			{
    				bool result = pair.second.compare(std::to_wstring(true)) == 0;
    				SetNotifyUpdates(result);
    			}
    		break;
    		case PRIVATE_ALLOWED:
    			{
    				// Invalid names are dropped; the list is capped.
    				std::vector<std::wstring> names;
    				size_t start = 0;
    				while (start <= pair.second.size() && names.size() < PRIVATE_ALLOWED_MAX)
    				{
    					size_t end = pair.second.find(L'/', start);
    					if (end == std::wstring::npos)
    					{
    						end = pair.second.size();
    					}
    					std::wstring name = pair.second.substr(start, end - start);
    					if (IsValidProgramName(name))
    					{
    						names.push_back(name);
    					}
    					start = end + 1;
    				}
    				std::lock_guard<std::mutex> lock(privateAllowedMutex);
    				privateAllowed = names;
    			}
    		break;
            default:
                break;
            }
        }
        catch (const std::exception&)
        {
        }
    }
}

// Schedules a write operation to the settings file.
void UserSettings::SaveSettingsAsync()
{
    // Only the caller that raises the count from 0 starts the writer.
    if (settingWriterWaitCount++ == 0)
    {
        // Mark pending before the thread starts so NoPendingSettingWrites
        // cannot report idle between the count reaching 0 and WriteSettings.
        isWriterFinished = false;
        std::thread([]() { DelaySettingWriter(&settingWriterWaitCount); }).detach();
    }
}

// Sets the value pointed to by waitCount to 1, sleeps for an
// amount of time, decrements the value by 1, and repeats if it does
// not have a value of zero. If it does, it writes the settings file to disk.
void UserSettings::DelaySettingWriter(std::atomic<int>* waitCount)
{
    while (*waitCount != 0)
    {
        // we wait to make sure that no aditional actions
        // have been taken since we started waiting.
        (*waitCount) = 1;
        Sleep(WRITE_DELAY);
        (*waitCount)--;
    }

    UserSettings::WriteSettings();
}

