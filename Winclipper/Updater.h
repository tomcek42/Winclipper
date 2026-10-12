#pragma once
#include <string>

// Checks the fork's GitHub releases for a signed update manifest and, once the
// user agrees, installs the newer MSI. Runs on a detached worker thread; it
// only talks to the UI via PostMessage, so it never touches a destroyed
// window object.
//
// Trust model: the manifest (winclipper-update.txt) carries a minisign
// (Ed25519) signature from a key whose public half is compiled in. The
// manifest pins the SHA-256 of each MSI. TLS, GitHub and the download hosts
// are not trusted on their own.
class Updater
{
public:
	// Posted to the notify window with one of the State values as wParam.
	static const UINT WMAPP_UPDATE_STATE = WM_APP + 2;

	enum State : WPARAM
	{
		STATE_IDLE = 0,				// nothing running
		STATE_INSTALLING = 1,		// download and msiexec are running
		STATE_AVAILABLE = 2,		// a verified newer version can be installed
		STATE_UP_TO_DATE = 3,		// answer to CheckNow: no newer version
		STATE_CHECK_FAILED = 4,		// answer to CheckNow: no valid manifest
	};

	static void Start(HWND notifyWindow);
	// Enables the daily background check that notifies about new versions.
	static void SetEnabled(bool enabled);
	// Checks once right away, regardless of SetEnabled, and always reports.
	static void CheckNow();
	// Installs the version reported by STATE_AVAILABLE. The user must have
	// confirmed it; nothing is installed without this call.
	static void InstallAvailable();

	// Empty if no verified newer version is known.
	static std::wstring AvailableVersion();
	static std::wstring ReleaseNotesUrl();
	static std::wstring CurrentVersion();

	Updater() = delete;
};
