#pragma once
#include <string>

// Checks the fork's GitHub releases for a signed update manifest and installs
// a newer MSI. Runs on a detached worker thread; it only talks to the UI via
// PostMessage, so it never touches a destroyed window object.
//
// Trust model: the manifest (winclipper-update.txt) is signed with an ECDSA
// P-256 key whose public half is compiled in. The manifest pins the SHA-256 of
// each MSI. TLS, GitHub and the download hosts are not trusted on their own.
class Updater
{
public:
	// Posted to the notify window: wParam = 1 while an update is downloaded
	// and installed, 0 when the updater is idle again.
	static const UINT WMAPP_UPDATE_STATE = WM_APP + 2;

	static void Start(HWND notifyWindow);
	static void SetEnabled(bool enabled);

	Updater() = delete;
};
