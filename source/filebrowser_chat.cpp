/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * filebrowser_chat.cpp
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "fceugx.h"
#include "fceusupport.h"
#include "menu.h"
#include "filebrowser_chat.h"
#include "networkop.h"
#include "fileop.h"
#include "pad.h"
#include "fceuload.h"
#include "gcunzip.h"
#include "fceuram.h"
#include "fceustate.h"
#include "patch.h"

BROWSERINFO_chat browser_chat;
BROWSERENTRY_chat * browserList_chat = NULL; // list of files/folders in browser

char romFilename_chat[256];
bool loadingFile_chat = false;

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser_chat()
{
	browser_chat.numEntries = 0;
	browser_chat.selIndex = 0;
	browser_chat.pageIndex = 0;
	browser_chat.size = 0;
}

bool AddBrowserEntry_chat()
{
	if(browser_chat.size >= MAX_BROWSER_SIZE)
	{
		ErrorPrompt("Out of memory: too many files!");
		return false; // out of space
	}

	memset(&(browserList_chat[browser_chat.size]), 0, sizeof(BROWSERENTRY_chat)); // clear the new entry
	browser_chat.size++;
	return true;
}

/****************************************************************************
 * OpenROM
 * Displays a list of ROMS on load device
 ***************************************************************************/
int
OpenGameList_chat()
{

	for( int i = 0; i < 200; i++ )
	{
		AddBrowserEntry_chat();

		sprintf(browserList_chat[i].displayname, "%d", i);
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 0;

		browser_chat.numEntries = i;
	}

	return browser_chat.numEntries;
}
