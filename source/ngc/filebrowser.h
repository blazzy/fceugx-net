/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * filesel.h
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/

#ifndef _NGCFILESEL_
#define _NGCFILESEL_

#include <unistd.h>

#define MAXJOLIET 255
#define MAXDISPLAY 50

typedef struct
{
	char dir[MAXPATHLEN]; // directory path of browserList
	int numEntries; // # of entries in browserList
	int selIndex; // currently selected index of browserList
	int pageIndex; // starting index of browserList page display
} BROWSERINFO;

typedef struct
{
	u64 offset; // DVD offset
	unsigned int length; // file length
	char isdir; // 0 - file, 1 - directory
	char filename[MAXJOLIET + 1]; // full filename
	char displayname[MAXDISPLAY + 1]; // name for browser display
} BROWSERENTRY;

extern BROWSERINFO browser;
extern BROWSERENTRY * browserList;
extern char rootdir[10];

extern char romFilename[];
extern int nesGameType;

bool MakeFilePath(char filepath[], int type, int method);
int OpenROM (int method);
int autoLoadMethod();
int autoSaveMethod(bool silent);
int FileSortCallback(const void *f1, const void *f2);
void StripExt(char* returnstring, char * inputstring);
void ResetBrowser();

#endif
