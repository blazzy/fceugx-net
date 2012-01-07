/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * filebrowser.h
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/

#ifndef _FILEBROWSER_CHAT_H_
#define _FILEBROWSER_CHAT_H_

#include <unistd.h>

#define MAX_CHAT_MSG_LEN		255
#define CHAT_SCROLLBACK_SIZE	1000

typedef struct
{
	char dir[MAXPATHLEN + 1]; // directory path of browserList
	int numEntries; // # of entries in browserList
	int selIndex; // currently selected index of browserList
	int pageIndex; // starting index of browserList page display
	int size; // # of entries browerList has space allocated to store
} BROWSERINFO_chat;

typedef struct
{
	char displayname[MAX_CHAT_MSG_LEN + 1];
} BROWSERENTRY_chat;

extern BROWSERINFO_chat browser_chat;
extern BROWSERENTRY_chat * browserList_chat;

int OpenGameList_chat();
void ResetBrowser_chat();
bool AddBrowserEntry_chat(char *name);

#endif
