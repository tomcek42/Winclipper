#pragma once
#include "stdafx.h"
#include <functional>

class ClipsManager;

// The clips menu and the clip picker, both drawn by Winclipper with the
// system menu theme. A Win32 popup menu cannot scroll with the mouse wheel and
// moves its highlight on every mouse move, and while it is open no other
// window can take input, so the menu is an own window too.
//
// The menu lists the first clips and the commands. "More" opens the picker
// beside it like a submenu, and both stay open. The picker has a search box
// and the full list; it scrolls one line per wheel notch, and its highlight
// only changes when the cursor moves onto a different row.
//
// Searching happens in memory only; nothing is persisted.

struct ClipMenuOptions
{
	HWND owner = nullptr;
	POINT pos = {};
	bool showExit = false;
	bool searchOnly = false;		// open only the picker at pos (search hotkey)
	std::wstring updateVersion;		// non-empty: show "Install update <version>..."
	// Called with the clip index and the item's screen rect when a clip is
	// highlighted, and with -1 / nullptr to hide the preview.
	std::function<void(int, const RECT *)> preview;
};

// Runs a modal loop. Returns 0 if cancelled, clip index + 1 for a clip, or one
// of the *_SELECT command IDs from ClipsManager.h.
int ShowClipMenu(ClipsManager & manager, const ClipMenuOptions & options);
