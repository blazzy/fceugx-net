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

static char szpath_chat[MAXPATHLEN];
static bool inSz_chat = false;

char romFilename_chat[256];
bool loadingFile_chat = false;

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load device
* Returns device set
****************************************************************************/
int autoLoadMethod_chat()
{
	ShowAction ("Attempting to determine load device...");

	int device = DEVICE_AUTO;

	if(ChangeInterface(DEVICE_SD, SILENT))
		device = DEVICE_SD;
	else if(ChangeInterface(DEVICE_USB, SILENT))
		device = DEVICE_USB;
	else if(ChangeInterface(DEVICE_SD_SLOTA, SILENT))
		device = DEVICE_SD_SLOTA;
	else if(ChangeInterface(DEVICE_SD_SLOTB, SILENT))
		device = DEVICE_SD_SLOTB;
	else if(ChangeInterface(DEVICE_DVD, SILENT))
		device = DEVICE_DVD;
	else if(ChangeInterface(DEVICE_SMB, SILENT))
		device = DEVICE_SMB;
	else
		ErrorPrompt("Unable to locate a load device!");

	if(GCSettings.LoadMethod == DEVICE_AUTO)
		GCSettings.LoadMethod = device; // save device found for later use
	CancelAction();
	return device;
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save device
* Returns device set
****************************************************************************/
int autoSaveMethod_chat(bool silent)
{
	if(!silent)
		ShowAction ("Attempting to determine save device...");

	int device = DEVICE_AUTO;

	if(ChangeInterface(DEVICE_SD, SILENT))
		device = DEVICE_SD;
	else if(ChangeInterface(DEVICE_USB, SILENT))
		device = DEVICE_USB;
	else if(ChangeInterface(DEVICE_SD_SLOTA, SILENT))
		device = DEVICE_SD_SLOTA;
	else if(ChangeInterface(DEVICE_SD_SLOTB, SILENT))
		device = DEVICE_SD_SLOTB;
	else if(ChangeInterface(DEVICE_SMB, SILENT))
		device = DEVICE_SMB;
	else if(!silent)
		ErrorPrompt("Unable to locate a save device!");

	if(GCSettings.SaveMethod == DEVICE_AUTO)
		GCSettings.SaveMethod = device; // save device found for later use

	CancelAction();
	return device;
}

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

bool IsDeviceRoot_chat(char * path)
{
	if(path == NULL || path[0] == 0)
		return false;

	if( strcmp(path, "sd:/")    == 0 ||
		strcmp(path, "usb:/")   == 0 ||
		strcmp(path, "dvd:/")   == 0 ||
		strcmp(path, "smb:/")   == 0 ||
		strcmp(path, "carda:/") == 0 ||
		strcmp(path, "cardb:/") == 0)
	{
		return true;
	}
	return false;
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName_chat()
{
	int size=0;
	char * test;
	char temp[1024];
	int device = 0;

	if(browser_chat.numEntries == 0)
		return 1;

	FindDevice(browser_chat.dir, &device);

	/* current directory doesn't change */
	if (strcmp(browserList_chat[browser_chat.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList_chat[browser_chat.selIndex].filename,"..") == 0)
	{
		// already at the top level
		if(IsDeviceRoot_chat(browser_chat.dir))
		{
			browser_chat.dir[0] = 0; // remove device - we are going to the device listing screen
		}
		else
		{
			/* determine last subdirectory namelength */
			sprintf(temp,"%s",browser_chat.dir);
			test = strtok(temp,"/");
			while (test != NULL)
			{
				size = strlen(test);
				test = strtok(NULL,"/");
			}
	
			/* remove last subdirectory name */
			size = strlen(browser_chat.dir) - size - 1;
			browser_chat.dir[size] = 0;
		}

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(browser_chat.dir)+1+strlen(browserList_chat[browser_chat.selIndex].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(browser_chat.dir, "%s%s/",browser_chat.dir, browserList_chat[browser_chat.selIndex].filename);
			return 1;
		}
		else
		{
			ErrorPrompt("Directory name is too long!");
			return -1;
		}
	}
}

bool MakeFilePath_chat(char filepath[], int type, char * filename, int filenum)
{
	char file[512];
	char folder[1024];
	char ext[4];
	char temppath[MAXPATHLEN];

	if(type == FILE_ROM)
	{
		// Check path length
		if ((strlen(browser_chat.dir)+1+strlen(browserList_chat[browser_chat.selIndex].filename)) >= MAXPATHLEN)
		{
			ErrorPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		else
		{
			sprintf(temppath, "%s%s",browser_chat.dir,browserList_chat[browser_chat.selIndex].filename);
		}
	}
	else
	{
		if(GCSettings.SaveMethod == DEVICE_AUTO)
			GCSettings.SaveMethod = autoSaveMethod_chat(SILENT);

		if(GCSettings.SaveMethod == DEVICE_AUTO)
			return false;

		switch(type)
		{
			case FILE_RAM:
			case FILE_STATE:
				sprintf(folder, GCSettings.SaveFolder);

				if(type == FILE_RAM) sprintf(ext, "sav");
				else sprintf(ext, "fcs");

				if(filenum >= -1)
				{
					if(filenum == -1)
						sprintf(file, "%s.%s", filename, ext);
					else if(filenum == 0)
						sprintf(file, "%s Auto.%s", filename, ext);
					else
						sprintf(file, "%s %i.%s", filename, filenum, ext);
				}
				else
				{
					sprintf(file, "%s", filename);
				}
				break;

			case FILE_CHEAT:
				sprintf(folder, GCSettings.CheatFolder);
				sprintf(file, "%s.cht", romFilename_chat);
				break;
		}
		sprintf (temppath, "%s%s/%s", pathPrefix[GCSettings.SaveMethod], folder, file);
	}

	snprintf(filepath, MAXPATHLEN, "%s", temppath);
	return true;
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback_chat(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((BROWSERENTRY_chat *)f1)->filename[0] == '.' || ((BROWSERENTRY_chat *)f2)->filename[0] == '.')
	{
		if(strcmp(((BROWSERENTRY_chat *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY_chat *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((BROWSERENTRY_chat *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY_chat *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((BROWSERENTRY_chat *)f1)->isdir && !(((BROWSERENTRY_chat *)f2)->isdir)) return -1;
	if(!(((BROWSERENTRY_chat *)f1)->isdir) && ((BROWSERENTRY_chat *)f2)->isdir) return 1;

	return stricmp(((BROWSERENTRY_chat *)f1)->filename, ((BROWSERENTRY_chat *)f2)->filename);
}

/****************************************************************************
 * IsValidROM
 *
 * Checks if the specified file is a valid ROM
 * For now we will just check the file extension and file size
 * If the file is a zip, we will check the file extension / file size of the
 * first file inside
 ***************************************************************************/
static bool IsValidROM_chat()
{
	// file size should be between 8K and 3MB
	if(browserList_chat[browser_chat.selIndex].length < (1024*8) ||
		browserList_chat[browser_chat.selIndex].length > (1024*1024*4))
	{
		ErrorPrompt("Invalid file size!");
		return false;
	}

	if (strlen(browserList_chat[browser_chat.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList_chat[browser_chat.selIndex].filename, '.');

		if (p != NULL)
		{
			char * zippedFilename = NULL;

			if(stricmp(p, ".zip") == 0 && !inSz_chat)
			{
				// we need to check the file extension of the first file in the archive
				zippedFilename = GetFirstZipFilename ();

				if(zippedFilename && strlen(zippedFilename) > 4)
					p = strrchr(zippedFilename, '.');
				else
					p = NULL;
			}

			if(p != NULL)
			{
				if (
					stricmp(p, ".nes") == 0 ||
					stricmp(p, ".fds") == 0 ||
					stricmp(p, ".nsf") == 0 ||
					stricmp(p, ".unf") == 0 ||
					stricmp(p, ".nez") == 0 ||
					stricmp(p, ".unif") == 0
				)
				{
					if(zippedFilename) free(zippedFilename);
					return true;
				}
			}
			if(zippedFilename) free(zippedFilename);
		}
	}
	ErrorPrompt("Unknown file type!");
	return false;
}

/****************************************************************************
 * IsSz
 *
 * Checks if the specified file is a 7z
 ***************************************************************************/
bool IsSz_chat()
{
	if (strlen(browserList_chat[browser_chat.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList_chat[browser_chat.selIndex].filename, '.');

		if (p != NULL)
			if(stricmp(p, ".7z") == 0)
				return true;
	}
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/
void StripExt_chat(char* returnstring, char * inputstring)
{
	char* loc_dot;

	snprintf(returnstring, MAXJOLIET, "%s", inputstring);

	if(inputstring == NULL || strlen(inputstring) < 4)
		return;

	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != NULL)
		*loc_dot = 0; // strip file extension
}

/****************************************************************************
 * BrowserLoadFile
 *
 * Loads the selected ROM
 ***************************************************************************/
int BrowserLoadFile_chat()
{
	char filepath[1024];
	size_t filesize = 0;
	romLoaded = false;

	int device;

	if(!FindDevice(browser_chat.dir, &device))
		return 0;

	GetFileSize(browser_chat.selIndex);

	// check that this is a valid ROM
	if(!IsValidROM_chat())
		goto done;

	loadingFile_chat = true;

	if(!inSz_chat)
	{
		if(!MakeFilePath_chat(filepath, FILE_ROM))
			goto done;

		filesize = LoadFile ((char *)nesrom, filepath, browserList_chat[browser_chat.selIndex].length, NOTSILENT);
	}
	else
	{
		filesize = LoadSzFile(szpath_chat, nesrom);

		if(filesize <= 0)
		{
			browser_chat.selIndex = 0;
			//BrowserChangeFolder_chat();
		}
	}

	loadingFile_chat = false;

	if (filesize <= 0)
	{
		ErrorPrompt("Error loading game!");
	}
	else
	{
		// store the filename (w/o ext) - used for ram/state naming
		StripExt_chat(romFilename_chat, browserList_chat[browser_chat.selIndex].filename);
		strcpy(loadedFile, browserList_chat[browser_chat.selIndex].filename);
		
		// load UPS/IPS/PPF patch
		filesize = LoadPatch(filesize);

		if(GCMemROM(filesize) > 0)
		{
			romLoaded = true;

			// load RAM or state
			if (GCSettings.AutoLoad == 1)
				LoadRAMAuto(SILENT);
			else if (GCSettings.AutoLoad == 2)
				LoadStateAuto(SILENT);

			ResetNES();
			ResetBrowser_chat();
		}
	}
done:
	CancelAction();
	return romLoaded;
}

/****************************************************************************
 * BrowserChangeFolder
 *
 * Update current directory and set new entry list if directory has changed
 ***************************************************************************/
/*int BrowserChangeFolder_chat()
{
	int device = 0;
	FindDevice(browser_chat.dir, &device);
	
	if(inSz_chat && browser_chat.selIndex == 0) // inside a 7z, requesting to leave
	{
		inSz_chat = false;
		SzClose();
	}

	if(!UpdateDirName_chat())
		return -1;

	HaltParseThread(); // halt parsing
	ResetBrowser_chat(); // reset browser

	if(browser_chat.dir[0] != 0)
		ParseDirectory();

	if(browser_chat.numEntries == 0)
	{
		browser_chat.dir[0] = 0;
		int i=0;

#ifdef HW_RVL
		AddBrowserEntry_chat();
		sprintf(browserList_chat[i].filename, "sd:/");
		sprintf(browserList_chat[i].displayname, "SD Card");
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 1;
		browserList_chat[i].icon = ICON_SD_chat;
		i++;

		AddBrowserEntry_chat();
		sprintf(browserList_chat[i].filename, "usb:/");
		sprintf(browserList_chat[i].displayname, "USB Mass Storage");
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 1;
		browserList_chat[i].icon = ICON_USB_chat;
		i++;
#else
		AddBrowserEntry_chat();
		sprintf(browserList_chat[i].filename, "carda:/");
		sprintf(browserList_chat[i].displayname, "SD Gecko Slot A");
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 1;
		browserList_chat[i].icon = ICON_SD_chat;
		i++;
		
		AddBrowserEntry_chat();
		sprintf(browserList_chat[i].filename, "cardb:/");
		sprintf(browserList_chat[i].displayname, "SD Gecko Slot B");
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 1;
		browserList_chat[i].icon = ICON_SD_chat;
		i++;
#endif
		AddBrowserEntry_chat();
		sprintf(browserList_chat[i].filename, "smb:/");
		sprintf(browserList_chat[i].displayname, "Network Share");
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 1;
		browserList_chat[i].icon = ICON_SMB_chat;
		i++;
		
		AddBrowserEntry_chat();
		sprintf(browserList_chat[i].filename, "dvd:/");
		sprintf(browserList_chat[i].displayname, "Data DVD");
		browserList_chat[i].length = 0;
		browserList_chat[i].isdir = 1;
		browserList_chat[i].icon = ICON_DVD_chat;
		i++;
		
		browser_chat.numEntries += i;
	}
	
	if(browser_chat.dir[0] == 0)
	{
		GCSettings.LoadFolder[0] = 0;
		GCSettings.LoadMethod = 0;
	}
	else
	{
		char * path = StripDevice(browser_chat.dir);
		if(path != NULL)
			strcpy(GCSettings.LoadFolder, path);
		FindDevice(browser_chat.dir, &GCSettings.LoadMethod);
	}

	return browser_chat.numEntries;
}
*/
/****************************************************************************
 * OpenROM
 * Displays a list of ROMS on load device
 ***************************************************************************/
int
OpenGameList_chat()
{

		AddBrowserEntry_chat();
		sprintf(browserList_chat[0].filename, "smb:/");
		sprintf(browserList_chat[0].displayname, "What packets through yonder socket breaks?");
		browserList_chat[0].length = 0;
		browserList_chat[0].isdir = 0;

		browser_chat.numEntries = 1;


	/*
	int device = GCSettings.LoadMethod;

	if(device == DEVICE_AUTO && strlen(GCSettings.LoadFolder) > 0)
		device = autoLoadMethod_chat();

	// change current dir to roms directory
	if(device > 0)
		sprintf(browser_chat.dir, "%s%s/", pathPrefix[device], GCSettings.LoadFolder);
	else
		browser_chat.dir[0] = 0;
	
	//BrowserChangeFolder_chat();
	*/

	return browser_chat.numEntries;
}
