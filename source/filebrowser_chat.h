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

#define MAXJOLIET 255
#ifdef HW_DOL
#define MAX_BROWSER_SIZE	1000
#else
#define MAX_BROWSER_SIZE	3000
#endif

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
	size_t length; // file length
	int isdir; // 0 - file, 1 - directory
	char filename[MAXJOLIET + 1]; // full filename
	char displayname[MAXJOLIET + 1]; // name for browser display
	int filenum; // file # (for 7z support)
	int icon; // icon to display
} BROWSERENTRY_chat;

extern BROWSERINFO_chat browser_chat;
extern BROWSERENTRY_chat * browserList_chat;

extern char romFilename_chat[];
extern bool loadingFile_chat;

int OpenGameList_chat();
void ResetBrowser_chat();
bool AddBrowserEntry_chat();

#endif
