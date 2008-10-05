/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * filesel.c
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>

#ifdef WII_DVD
#include <di/di.h>
#endif

#include "common.h"
#include "fceuconfig.h"
#include "dvd.h"
#include "menudraw.h"
#include "filesel.h"
#include "smbop.h"
#include "fileop.h"
#include "memcardop.h"
#include "pad.h"
#include "fceuload.h"
#include "gcunzip.h"

int offset;
int selection;
char currentdir[MAXPATHLEN];
char romFilename[200];
int nesGameType;
int maxfiles;
extern int screenheight;

extern u64 dvddir;
extern int dvddirlength;

// Global file entry table
FILEENTRIES filelist[MAXFILES];

unsigned char savebuffer[SAVEBUFFERSIZE];

/****************************************************************************
 * Clear the savebuffer
 ****************************************************************************/
void
ClearSaveBuffer ()
{
    memset (savebuffer, 0, SAVEBUFFERSIZE);
}

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load method
* Returns method set
****************************************************************************/
int autoLoadMethod()
{
	ShowAction ((char*) "Attempting to determine load method...");

	if(ChangeFATInterface(METHOD_SD, SILENT))
		return METHOD_SD;
	else if(ChangeFATInterface(METHOD_USB, SILENT))
		return METHOD_USB;
	else if(TestDVD())
		return METHOD_DVD;
	else if(ConnectShare (SILENT))
		return METHOD_SMB;
	else
	{
		WaitPrompt((char*) "Unable to auto-determine load method!");
		return 0; // no method found
	}
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save method
* Returns method set
****************************************************************************/
int autoSaveMethod()
{
	ShowAction ((char*) "Attempting to determine save method...");

	if(ChangeFATInterface(METHOD_SD, SILENT))
		return METHOD_SD;
	else if(ChangeFATInterface(METHOD_USB, SILENT))
		return METHOD_USB;
	else if(TestCard(CARD_SLOTA, SILENT))
		return METHOD_MC_SLOTA;
	else if(TestCard(CARD_SLOTB, SILENT))
		return METHOD_MC_SLOTB;
	else if(ConnectShare (SILENT))
		return METHOD_SMB;
	else
	{
		WaitPrompt((char*) "Unable to auto-determine save method!");
		return 0; // no method found
	}
}

/***************************************************************************
 * Update curent directory name
 ***************************************************************************/
int UpdateDirName(int method)
{
	int size=0;
	char *test;
	char temp[1024];

	// update DVD directory (does not utilize 'currentdir')
	if(method == METHOD_DVD)
	{
		dvddir = filelist[selection].offset;
		dvddirlength = filelist[selection].length;
		return 1;
	}

	/* current directory doesn't change */
	if (strcmp(filelist[selection].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(filelist[selection].filename,"..") == 0)
	{
		/* determine last subdirectory namelength */
		sprintf(temp,"%s",currentdir);
		test = strtok(temp,"/");
		while (test != NULL)
		{
			size = strlen(test);
			test = strtok(NULL,"/");
		}

		/* remove last subdirectory name */
		size = strlen(currentdir) - size - 1;
		currentdir[size] = 0;

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(currentdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(currentdir, "%s/%s",currentdir, filelist[selection].filename);
			return 1;
		}
		else
		{
			WaitPrompt((char*)"Directory name is too long !");
			return -1;
		}
	}
}

/***************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((FILEENTRIES *)f1)->filename[0] == '.' || ((FILEENTRIES *)f2)->filename[0] == '.')
	{
		if(strcmp(((FILEENTRIES *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((FILEENTRIES *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((FILEENTRIES *)f1)->flags && !(((FILEENTRIES *)f2)->flags)) return -1;
	if(!(((FILEENTRIES *)f1)->flags) && ((FILEENTRIES *)f2)->flags) return 1;

	return stricmp(((FILEENTRIES *)f1)->filename, ((FILEENTRIES *)f2)->filename);
}

/****************************************************************************
 * IsValidROM
 *
 * Checks if the specified file is a valid ROM
 * For now we will just check the file extension and file size
 * If the file is a zip, we will check the file extension / file size of the
 * first file inside
 ***************************************************************************/

bool IsValidROM(int method)
{
	// file size should be between 10K and 3MB
	if(filelist[selection].length < (1024*10) ||
		filelist[selection].length > (1024*1024*3))
	{
		WaitPrompt((char *)"Invalid file size!");
		return false;
	}

	if (strlen(filelist[selection].filename) > 4)
	{
		char * p = strrchr(filelist[selection].filename, '.');

		if (p != NULL)
		{
			if(stricmp(p, ".zip") == 0)
			{
				// we need to check the file extension of the first file in the archive
				char * zippedFilename = GetFirstZipFilename (method);

				if(strlen(zippedFilename) > 4)
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
					return true;
				}
			}
		}
	}
	WaitPrompt((char *)"Unknown file type!");
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
****************************************************************************/

void StripExt(char* returnstring, char * inputstring)
{
	char* loc_dot;

	strcpy (returnstring, inputstring);
	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != NULL)
		*loc_dot = '\0';	// strip file extension
}

/****************************************************************************
 * FileSelector
 *
 * Let user select a file from the listing
****************************************************************************/
int FileSelector (int method)
{
    u32 p = 0;
	u32 wp = 0;
	u32 ph = 0;
	u32 wh = 0;
    signed char gc_ay = 0;
	signed char gc_sx = 0;
	signed char wm_ay = 0;
	signed char wm_sx = 0;

    int haverom = 0;
    int redraw = 1;
    int selectit = 0;

	int scroll_delay = 0;
	bool move_selection = 0;
	#define SCROLL_INITIAL_DELAY	15
	#define SCROLL_LOOP_DELAY		2

    while (haverom == 0)
    {
        if (redraw)
            ShowFiles (filelist, maxfiles, offset, selection);
        redraw = 0;

		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads

		gc_ay = PAD_StickY (0);
		gc_sx = PAD_SubStickX (0);

        p = PAD_ButtonsDown (0);
		ph = PAD_ButtonsHeld (0);
#ifdef HW_RVL
		wm_ay = WPAD_StickY (0, 0);
		wm_sx = WPAD_StickX (0, 1);

		wp = WPAD_ButtonsDown (0);
		wh = WPAD_ButtonsHeld (0);
#endif

		/*** Check for exit combo ***/
		if ( (gc_sx < -70) || (wm_sx < -70) || (wp & WPAD_BUTTON_HOME) || (wp & WPAD_CLASSIC_BUTTON_HOME) )
			return 0;

		/*** Check buttons, perform actions ***/
		if ( (p & PAD_BUTTON_A) || selectit || (wp & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) )
		{
			if ( selectit )
				selectit = 0;
			if (filelist[selection].flags) // This is directory
			{
				/* update current directory and set new entry list if directory has changed */
				int status = UpdateDirName(method);
				if (status == 1) // ok, open directory
				{
					switch (method)
					{
						case METHOD_SD:
						case METHOD_USB:
						maxfiles = ParseFATdirectory(method);
						break;

						case METHOD_DVD:
						maxfiles = ParseDVDdirectory();
						break;

						case METHOD_SMB:
						maxfiles = ParseSMBdirectory();
						break;
					}

					if (!maxfiles)
					{
						WaitPrompt ((char*) "Error reading directory !");
						haverom = 1; // quit menu
					}
				}
				else if (status == -1)	// directory name too long
				{
					haverom = 1; // quit menu
				}
			}
			else	// this is a file
			{
				// check that this is a valid ROM
				if(!IsValidROM(method))
					return 0;

				// store the filename (w/o ext) - used for state saving
				StripExt(romFilename, filelist[selection].filename);

				ShowAction ((char *)"Loading...");

				int size = 0;

				switch (method)
				{
				case METHOD_SD:
				case METHOD_USB:
					size = LoadFATFile((char *)nesrom, 0);
					break;

				case METHOD_DVD:
					dvddir = filelist[selection].offset;
					dvddirlength = filelist[selection].length;
					size = LoadDVDFile(nesrom, 0);
					break;

				case METHOD_SMB:
					size = LoadSMBFile((char *)nesrom, 0);
					break;
				}

				if (size > 0)
				{
					if(GCMemROM(method, size) > 0)
						return 1;
					else
						return 0;
				}
				else
				{
					WaitPrompt((char*) "Error loading ROM!");
					return 0;
				}
			}
			redraw = 1;
		}	// End of A
        if ( (p & PAD_BUTTON_B) || (wp & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)) )
        {
            while ( (PAD_ButtonsDown(0) & PAD_BUTTON_B)
#ifdef HW_RVL
					|| (WPAD_ButtonsDown(0) & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B))
#endif
					)
                VIDEO_WaitVSync();
			if ( strcmp(filelist[0].filename,"..") == 0 )
			{
				selection = 0;
				selectit = 1;
			}
			else if ( strcmp(filelist[1].filename,"..") == 0 )
			{
                selection = selectit = 1;
			} else {
                return 0;
			}
        }	// End of B
        if ( ((p | ph) & PAD_BUTTON_DOWN) || ((wp | wh) & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) || (gc_ay < -PADCAL) || (wm_ay < -PADCAL) )
        {
			if ( (p & PAD_BUTTON_DOWN) || (wp & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) ) { /*** Button just pressed ***/
				scroll_delay = SCROLL_INITIAL_DELAY;	// reset scroll delay.
				move_selection = 1;	//continue (move selection)
			}
			else if (scroll_delay == 0) { 		/*** Button is held ***/
				scroll_delay = SCROLL_LOOP_DELAY;
				move_selection = 1;	//continue (move selection)
			} else {
				scroll_delay--;	// wait
			}

			if (move_selection)
			{
	            selection++;
	            if (selection == maxfiles)
	                selection = offset = 0;
	            if ((selection - offset) >= PAGESIZE)
	                offset += PAGESIZE;
	            redraw = 1;
				move_selection = 0;
			}
        }	// End of down
        if ( ((p | ph) & PAD_BUTTON_UP) || ((wp | wh) & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) || (gc_ay > PADCAL) || (wm_ay > PADCAL) )
        {
			if ( (p & PAD_BUTTON_UP) || (wp & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) ) { /*** Button just pressed***/
				scroll_delay = SCROLL_INITIAL_DELAY;	// reset scroll delay.
				move_selection = 1;	//continue (move selection)
			}
			else if (scroll_delay == 0) { 		/*** Button is held ***/
				scroll_delay = SCROLL_LOOP_DELAY;
				move_selection = 1;	//continue (move selection)
			} else {
				scroll_delay--;	// wait
			}

			if (move_selection)
			{
	            selection--;
	            if (selection < 0) {
	                selection = maxfiles - 1;
	                offset = selection - PAGESIZE + 1;
	            }
	            if (selection < offset)
	                offset -= PAGESIZE;
	            if (offset < 0)
	                offset = 0;
	            redraw = 1;
				move_selection = 0;
			}
        }	// End of Up
        if ( (p & PAD_BUTTON_LEFT) || (wp & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)) )
        {
            /*** Go back a page ***/
            selection -= PAGESIZE;
            if (selection < 0)
            {
                selection = maxfiles - 1;
                offset = selection - PAGESIZE + 1;
            }
            if (selection < offset)
                offset -= PAGESIZE;
            if (offset < 0)
                offset = 0;
            redraw = 1;
        }
        if ( (p & PAD_BUTTON_RIGHT) || (wp & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)) )
        {
            /*** Go forward a page ***/
            selection += PAGESIZE;
            if (selection > maxfiles - 1)
                selection = offset = 0;
            if ((selection - offset) >= PAGESIZE)
                offset += PAGESIZE;
            redraw = 1;
        }
    }
    return 0;
}

/****************************************************************************
 * OpenDVD
 *
 * Function to load a DVD directory and display to user.
****************************************************************************/
int
OpenDVD (int method)
{
	if (!getpvd())
	{
		ShowAction((char*) "Loading DVD...");
		#ifdef HW_DOL
		DVD_Mount(); // mount the DVD unit again
		#elif WII_DVD
		u32 val;
		DI_GetCoverRegister(&val);
		if(val & 0x1)	// True if no disc inside, use (val & 0x2) for true if disc inside.
		{
			WaitPrompt((char *)"No disc inserted!");
			return 0;
		}
		DI_Mount();
		while(DI_GetStatus() & DVD_INIT);
		#endif

		if (!getpvd())
		{
			WaitPrompt ((char *)"Invalid DVD.");
			return 0; // not a ISO9660 DVD
		}
	}

	maxfiles = ParseDVDdirectory(); // load root folder

	// switch to rom folder
	SwitchDVDFolder(GCSettings.LoadFolder);

	if (maxfiles > 0)
	{
		return FileSelector (method);
	}
	else
	{
		// no entries found
		WaitPrompt ((char *)"No Files Found!");
		return 0;
	}
}

/****************************************************************************
 * OpenSMB
 *
 * Function to load from an SMB share
****************************************************************************/
int
OpenSMB (int method)
{
	// Connect to network share
	if(ConnectShare (NOTSILENT))
	{
		// change current dir to root dir
		sprintf(currentdir, "/%s", GCSettings.LoadFolder);

		maxfiles = ParseSMBdirectory ();
		if (maxfiles > 0)
		{
			return FileSelector (method);
		}
		else
		{
			// no entries found
			WaitPrompt ((char *)"No Files Found!");
			return 0;
		}
	}
	return 0;
}

/****************************************************************************
 * OpenFAT
 *
 * Function to load from FAT
 ****************************************************************************/
int
OpenFAT (int method)
{
	if(ChangeFATInterface(method, NOTSILENT))
	{
		// change current dir to snes roms directory
		sprintf ( currentdir, "%s/%s", ROOTFATDIR, GCSettings.LoadFolder );

		// Parse initial root directory and get entries list
		maxfiles = ParseFATdirectory (method);
		if (maxfiles > 0)
		{
			// Select an entry
			return FileSelector (method);
		}
		else
		{
			// no entries found
			WaitPrompt ((char *)"No Files Found!");
			return 0;
		}
	}
	return 0;
}

/****************************************************************************
 * OpenROM
 * Opens device specified by method, displays a list of ROMS
 ****************************************************************************/

int
OpenROM (int method)
{
	int loadROM = 0;

	if(method == METHOD_AUTO)
		method = autoLoadMethod();

	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
			loadROM = OpenFAT (method);
			break;
		case METHOD_DVD:
			// Load from DVD
			loadROM = OpenDVD (method);
			break;
		case METHOD_SMB:
			// Load from Network (SMB)
			loadROM = OpenSMB (method);
			break;
	}

	return loadROM;
}
