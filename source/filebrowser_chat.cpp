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

#include "filebrowser_chat.h"
#include "menu.h"

BROWSERINFO_chat browser_chat;
BROWSERENTRY_chat * browserList_chat = NULL; // list of files/folders in browser

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

bool AddBrowserEntry_chat(char *msg)
{
	if(browser_chat.size >= MAX_BROWSER_SIZE)
	{
		ErrorPrompt("Out of memory: too many files!");
		return false; // out of space
	}

	// TODO:  For messages exceeding the max length, implement word wrapping by creating additional entries
	snprintf(browserList_chat[browser_chat.size].displayname, MAX_CHAT_MSG_LEN + 1, "%s", msg);

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
	int numEntries = 200;

	for( int i = 1; i <= numEntries; i++ )
	{
		char c[4];
		sprintf(c, "%d", i);

		AddBrowserEntry_chat(c);
	}

	browser_chat.numEntries = numEntries;

	return browser_chat.numEntries;
}
