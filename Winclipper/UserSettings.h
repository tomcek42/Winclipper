#pragma once
// Reads and writes settings to and from
// the current users's AppData folder
// and makes them available to read or change
#include <atomic>
#include <mutex>
#include "Notify.h"
#include "resource.h"
#include "Shlobj.h"
#include "Shlwapi.h"
#include "File.h"
#define SEPARATOR           L"|"
#define WRITE_DELAY         2000

#define MAX_DISPLAY         1
#define MAX_DISPLAY_DEF		20

#define MAX_SAVED           2
#define MAX_SAVED_DEF       50

#define MENU_CHARS          3
#define MENU_CHARS_DEF      48

#define CMENU_HOTKEY        4
#define CMENU_HOTKEY_DEF    MAKEWORD(0x56, HOTKEYF_CONTROL | HOTKEYF_SHIFT)

#define SAVE_TO_DISK        5
#define SAVE_TO_DISK_DEF    true

#define SLCT_2ND_CLIP       6
#define SLCT_2ND_CLIP_DEF   true

#define SHOW_PREVIEW        7
#define SHOW_PREVIEW_DEF    true

#define SAVE_IMAGES			8
#define SAVE_IMAGES_DEF		true

#define MAX_CACHE_MBYTES		9
#define MAX_CACHE_MBYTES_DEF	100

#define PASTE_ON_CLICK		10
#define PASTE_ON_CLICK_DEF	true

#define NOTIFY_UPDATES			11
#define NOTIFY_UPDATES_DEF		false

// Programs whose clipboard content is kept even if it is marked as private
// (e.g. by a password manager). Stored as file names separated by '/'.
#define PRIVATE_ALLOWED		12

// Programs whose clipboard content is never recorded, marked or not.
// Same format as PRIVATE_ALLOWED.
#define EXCLUDED_PROGRAMS	13

// Save and load user settings to and from disk
class UserSettings : public Notify
{
private:
    static std::wstring		fullSettingPath;
	static int				maxDisplayClips;
    static int				maxSavedClips;
    static int				menuDisplayChars;
    static WORD				clipsMenuHotkey;
    static bool				saveToDisk;
    static bool				select2ndClip;
    static bool				showPreview;
	static bool				pasteOnClick;
	static bool				saveImages;
	static unsigned int		maxCacheMegabytes;
	static bool				notifyUpdates;
	static std::vector<std::wstring>	privateAllowed;
	static std::mutex		privateAllowedMutex;
	static std::vector<std::wstring>	excludedPrograms;
	static std::mutex		excludedProgramsMutex;

	static std::vector<std::wstring>	ParseProgramList(const std::wstring & value);
	static bool				AddToProgramList(std::vector<std::wstring> & list, std::mutex & listMutex, const std::wstring & exeName);
	static void				RemoveFromProgramList(std::vector<std::wstring> & list, std::mutex & listMutex, const std::wstring & exeName);

	static std::atomic<int>	settingWriterWaitCount;
	static std::atomic<bool>	isWriterFinished;

    static void				SaveSettingsAsync();
    static void				DelaySettingWriter(std::atomic<int>* waitCount);
    static void				WriteSettings();

protected:
	static std::vector<std::wstring>  Serialize();
	static void                  Deserialize(std::vector<std::wstring> srData);

public:
	UserSettings() = delete;
	~UserSettings() = delete;

	static const int		MAX_DISPLAY_UPPER = 99;
	static const int		MAX_DISPLAY_LOWER = 1;

	static const int		MAX_SAVED_UPPER = 500;
	static const int		MAX_SAVED_LOWER = 1;

	static const int		MENU_CHARS_UPPER = 200;
	static const int		MENU_CHARS_LOWER = 5;

	static const int		MAX_CACHE_MBYTES_UPPER = 999;
	static const int		MAX_CACHE_MBYTES_LOWER = 1;

    static bool				NoPendingSettingWrites();

	static void				InitializeSettings();

    static int				MaxDisplayClips();
    static void				SetMaxDisplayClips(int maxDisplayClips);

	static int				MaxSavedClips();
	static void				SetMaxSavedClips(int maxSavedClips);

	static int				MenuDisplayChars();
	static void				SetMenuDisplayChars(int menuDisplayChars);

	static WORD				ClipsMenuHotkey();
	static WORD				ClipsMenuHotkeyTrl();
	static void				SetClipsMenuHotkey(WORD clipsMenuHotkey);

	static bool				SaveToDisk();
	static void				SetSaveToDisk(bool saveToDisk);

	static bool				Select2ndClip();
	static void				SetSelect2ndClip(bool select2ndClip);

	static bool				ShowPreview();
	static void				SetShowPreview(bool showPreview);

	static bool				PasteOnClick();
	static void				SetPasteOnClick(bool pasteOnClick);

	static bool				SaveImages();
	static void				SetSaveImages(bool saveImages);

	static unsigned int		MaxCacheMegabytes();
	static void				SetMaxCacheMegabytes(unsigned int maxCacheMegabytes);

	static bool				NotifyUpdates();
	static void				SetNotifyUpdates(bool notifyUpdates);

	static const size_t		PROGRAM_LIST_MAX = 50;
	static bool				IsValidProgramName(const std::wstring & name);
	static std::vector<std::wstring>	PrivateAllowedPrograms();
	static bool				IsPrivateAllowed(const std::wstring & exeName);
	static bool				AddPrivateAllowed(const std::wstring & exeName);
	static void				RemovePrivateAllowed(const std::wstring & exeName);
	static std::vector<std::wstring>	ExcludedPrograms();
	static bool				IsProgramExcluded(const std::wstring & exeName);
	static bool				AddExcludedProgram(const std::wstring & exeName);
	static void				RemoveExcludedProgram(const std::wstring & exeName);

};


