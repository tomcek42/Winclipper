#pragma once
#include "resource.h"
#include "string.h"
#include "Controls.h"
#include "ControlUtilities.h"
#include "RegistryUtilities.h"
#include "Notify.h"
#include "UserSettings.h"
#include "Updater.h"
#include <map>

#define LBL_MAX_CLIPS_DISPLAY   200
#define TXT_MAX_CLIPS_DISPLAY   201
#define UD_MAX_CLIPS_DISPLAY    202

#define LBL_MAX_CLIPS_SAVED     203
#define TXT_MAX_CLIPS_SAVED     204
#define UD_MAX_CLIPS_SAVED      205

#define LBL_MENU_DISP_CHARS     206
#define TXT_MENU_DISP_CHARS     207
#define UD_MENU_DISP_CHARS      208

#define CHK_RUN_AT_STARTUP      209
#define CHK_SAVE_TO_DISK        210
#define CHK_SLCT_2ND_CLIP       211
#define CHK_SHOW_PREVIEW        212
#define CHK_SAVE_IMAGES	        213

#define LBL_SHOW_CLIPS_HOTK     214
#define HKY_SHOW_CLIPS_MENU     215

#define LBL_MAX_IMG_CACHE_MB    216
#define TXT_MAX_IMG_CACHE_MB    217
#define UD_MAX_IMG_CACHE_MB     218

#define CHK_PASTE_ON_CLICK	    219
#define CHK_NOTIFY_UPDATES	        220

#define LST_NAVIGATION          221
#define BTN_CLEAR_CLIPS         222
#define BTN_RECREATE_THUMBNAILS 223
#define BTN_CHECK_UPDATES       224
#define LBL_INSTALLED_VERSION   225
#define LBL_PRIVATE_ALLOWED     226
#define LST_PRIVATE_ALLOWED     227
#define BTN_ADD_PRIVATE         228
#define BTN_REMOVE_PRIVATE      229
#define LNK_REPOSITORY          230
#define LBL_EXCLUDED_PROGRAMS   231
#define LST_EXCLUDED_PROGRAMS   232
#define BTN_ADD_EXCLUDED        233
#define BTN_REMOVE_EXCLUDED     234
#define LBL_UPDATE_STATUS       236
#define CHK_HISTORY_MARKER_IGN  235
#define BTN_INSTALL_UPDATE      237
#define ICO_INFO                238


class SettingsWindow : public Notify
{
private:
	bool						useLightTheme = true;
	HBRUSH						APP_BACKGROUND_COLOR_LIGHT = (HBRUSH)(COLOR_WINDOW + 1);
	HBRUSH						APP_BACKGROUND_COLOR_DARK = CreateSolidBrush(RGB(32, 32, 32));
	COLORREF					FOREGROUND_COLOR_LIGHT = RGB(0, 0, 0);
	COLORREF					FOREGROUND_COLOR_DARK = RGB(255, 255, 255);
	COLORREF					BACKGROUND_COLOR_LIGHT = GetSysColor(COLOR_WINDOW);
	COLORREF					BACKGROUND_COLOR_DARK = RGB(32, 32, 32);

	wchar_t						szSettingsWindowClass[MAX_LOADSTRING];      // the settings window class name
	wchar_t						szTitle[MAX_LOADSTRING];                    // The title bar text
	
	HFONT						hFontStd;
	HFONT						hFontHeading = nullptr;
	HBRUSH						hbrSidebar = nullptr;
	static const int			SIDEBAR_WIDTH = 140;
	HBRUSH						hbrBkgnd;
	HBRUSH						hbrCtrlBkgnd;
	HWND						windowHandle = nullptr;

	ATOM						RegisterSettingsWindowClass(HINSTANCE hInstance);
	static LRESULT CALLBACK		SettingsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandIdmAbout(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandIdmExit();
	LRESULT						WmCommandIdmRecreateThumbnails();
	LRESULT						WmCommandIdmClearClips();
	LRESULT						WmCommandTxtMaxClipsDisplay(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandTxtMaxClipsSaved(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandTxtMenuDispChars(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkRunAtStartup(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkSaveToDisk(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkSlct2ndClip(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkShowPreview(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkPasteOnClick(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkNotifyUpdates(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkHistoryMarkerIgnored(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandChkSaveImages(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandTxtMaxImageCacheMb(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandHkyShowClipsMenu(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmLbuttondown(HWND hWnd);
	LRESULT						WmCtlColorStatic(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCtlColorEdit(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmClose(HWND hWnd);
	LRESULT						WmNotify(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmDrawItem(HWND hWnd, WPARAM wParam, LPARAM lParam);
	HWND						AddHeading(int x, LPCWSTR text, HINSTANCE hInstance);
	void						ApplyWindowStyle(HWND hWnd);
	static COLORREF				AccentColor();
	LRESULT						WmCommandNavigation(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandLstPrivateAllowed(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandBtnAddPrivate(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandBtnRemovePrivate(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandLstExcluded(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandBtnAddExcluded(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandBtnRemoveExcluded(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandBtnCheckUpdates(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT						WmCommandBtnInstallUpdate(HWND hWnd, WPARAM wParam, LPARAM lParam);

	enum Page { PAGE_GENERAL, PAGE_CLIPS, PAGE_IMAGES, PAGE_UPDATES, PAGE_ABOUT, PAGE_COUNT };
	HWND						navigationList = nullptr;
	HWND						privateAllowedList = nullptr;
	HWND						excludedList = nullptr;
	std::map<HWND, int>			pageOfControl;
	void						AssignNewControlsToPage(int page);
	void						ShowPage(int page);
	void						RefreshPrivateAllowedList();
	void						RefreshExcludedList();
	static std::wstring			PickProgramName(HWND hWnd, LPCWSTR title);
	static std::wstring			SelectedListText(HWND list);
	HWND						AddButton(int x, int y, int width, LPCWSTR text, UINT_PTR id, HINSTANCE hInstance);
	void						AddInfoIcon(HWND control, LPCWSTR text, HINSTANCE hInstance);
	HWND						AddText(int x, int y, int width, int height, LPCWSTR text, UINT_PTR id, HINSTANCE hInstance);

	Controls::Label				LabelMaxClipsDisplay;
	Controls::Spinner			SpinnerMaxClipsDisplay;
	Controls::Label				LabelMaxClipsSaved;
	Controls::Spinner			SpinnerMaxClipsSaved;
	Controls::Label				LabelMenuDisplayChars;
	Controls::Spinner			SpinnerMenuDisplayChars;
	Controls::Label				LabelShowClipsHotkey;
	Controls::Checkbox			CheckboxSaveToDisk;
	Controls::Checkbox			CheckboxRunAtStartup;
	Controls::Checkbox			CheckboxSelectSecondClip;
	Controls::Checkbox			CheckboxShowPreview;
	Controls::Checkbox			CheckboxPasteOnClick;
	Controls::Checkbox			CheckboxNotifyUpdates;
	Controls::Checkbox			CheckboxHistoryMarkerIgnored;
	Controls::Checkbox			CheckboxSaveImages;
	Controls::Label				LabelMaxCacheMbytes;
	Controls::Spinner			SpinnerMaxCacheMbytes;

	bool						WriteRegistryRun();
	bool						DeleteRegistryRun();
public:
								SettingsWindow(HINSTANCE hInstance);
								~SettingsWindow();

	bool						InitSettingsWindow(HINSTANCE hInstance);
	HWND						GetHandle() noexcept;
	// Result of "Check now", shown below the button instead of a message box.
	// installable shows a button that opens the confirmation dialog for the found update.
	void						SetUpdateStatus(const std::wstring & text, bool installable = false);
private:
	bool						updateInstallable = false;
};

