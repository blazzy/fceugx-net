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

enum
{
	ICON_NONE_chat,
	ICON_FOLDER_chat,
	ICON_SD_chat,
	ICON_USB_chat,
	ICON_DVD_chat,
	ICON_SMB_chat
};

extern char romFilename_chat[];
extern bool loadingFile_chat;

bool MakeFilePath_chat(char filepath[], int type, char * filename = NULL, int filenum = -2);
int UpdateDirName_chat();
int OpenGameList_chat();
int autoLoadMethod_chat();
int autoSaveMethod_chat(bool silent);
int FileSortCallback_chat(const void *f1, const void *f2);
void StripExt_chat(char* returnstring, char * inputstring);
bool IsSz_chat();
void ResetBrowser_chat();
bool AddBrowserEntry_chat();
bool IsDeviceRoot_chat(char * path);
//int BrowserChangeFolder_chat();
int BrowserLoadFile_chat();

#endif
