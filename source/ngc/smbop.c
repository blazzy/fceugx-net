/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * smbload.c
 *
 * SMB support routines
 ****************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <network.h>
#include <smb.h>
#include <zlib.h>
#include <errno.h>

#include "common.h"
#include "fceuconfig.h"
#include "smbop.h"
#include "gcunzip.h"
#include "menudraw.h"
#include "filesel.h"

bool networkInit = false;
bool networkShareInit = false;
unsigned int SMBTimer = 0;
#define SMBTIMEOUT ( 3600 ) // Some implementations timeout in 10 minutes

// SMB connection/file handles - the only ones we should ever use!
SMBCONN smbconn;
SMBFILE smbfile;

#define ZIPCHUNK 16384

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ****************************************************************************/

bool InitializeNetwork(bool silent)
{
	ShowAction ((char*) "Initializing network...");
	s32 result;

	while ((result = net_init()) == -EAGAIN);

	if (result >= 0)
	{
		char myIP[16];

		if (if_config(myIP, NULL, NULL, true) < 0)
		{
			if(!silent)
				WaitPrompt((char*) "Error reading IP address.");
			return false;
		}
		else
		{
			return true;
		}
	}

	if(!silent)
		WaitPrompt((char*) "Unable to initialize network.");
	return false;
}

/****************************************************************************
 * Mount SMB Share
 ****************************************************************************/

bool
ConnectShare (bool silent)
{
	// Crashes or stalls system in GameCube mode - so disable
	#ifndef HW_RVL
	return false;
	#endif

	// check that all parameter have been set
	if(strlen(GCSettings.smbuser) == 0 ||
		strlen(GCSettings.smbpwd) == 0 ||
		strlen(GCSettings.smbshare) == 0 ||
		strlen(GCSettings.smbip) == 0)
	{
		if(!silent)
			WaitPrompt((char*) "Invalid network settings. Check FCEUGX.xml.");
		return false;
	}

	if(!networkInit)
		networkInit = InitializeNetwork(silent);

	if(networkInit)
	{
		// connection may have expired
		if (networkShareInit && SMBTimer > SMBTIMEOUT)
		{
			networkShareInit = false;
			SMBTimer = 0;
			SMB_Close(smbconn);
		}

		if(!networkShareInit)
		{
			if(!silent)
				ShowAction ((char*) "Connecting to network share...");

			if(SMB_Connect(&smbconn, GCSettings.smbuser, GCSettings.smbpwd,
			GCSettings.smbgcid, GCSettings.smbsvid, GCSettings.smbshare, GCSettings.smbip) == SMB_SUCCESS)
				networkShareInit = true;
		}

		if(!networkShareInit && !silent)
			WaitPrompt ((char*) "Failed to connect to network share.");
	}

	return networkShareInit;
}

/****************************************************************************
 * SMBPath
 *
 * Returns a SMB-style path
 *****************************************************************************/

char * SMBPath(char * path)
{
	// fix path - replace all '/' with '\'
	int i;
	for(i=0; i < strlen(path); i++)
		if(path[i] == '/')
			path[i] = '\\';

	return path;
}

/****************************************************************************
 * parseSMBDirectory
 *
 * Load the directory and put in the filelist array
 *****************************************************************************/
int
ParseSMBdirectory ()
{
	if(!ConnectShare (NOTSILENT))
		return 0;

	int filecount = 0;
	char searchpath[1024];
	SMBDIRENTRY smbdir;

	// initialize selection
	selection = offset = 0;

	// Clear any existing values
	memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

	if(strlen(currentdir) <= 1) // root
		sprintf(searchpath, "*");
	else
		sprintf(searchpath, "%s/*", currentdir);

	if (SMB_FindFirst
	(SMBPath(searchpath), SMB_SRCH_READONLY | SMB_SRCH_DIRECTORY, &smbdir, smbconn) != SMB_SUCCESS)
	{
		char msg[200];
		sprintf(msg, "Could not open %s", currentdir);
		WaitPrompt (msg);

		// if we can't open the dir, open root dir
		sprintf(searchpath, "/");
		sprintf(searchpath,"*");

		if (SMB_FindFirst
		(SMBPath(searchpath), SMB_SRCH_READONLY | SMB_SRCH_DIRECTORY, &smbdir, smbconn) != SMB_SUCCESS)
			return 0;
	}

	// index files/folders
	do
	{
		if(strcmp(smbdir.name,".") != 0 &&
		!(strlen(currentdir) <= 1 && strcmp(smbdir.name,"..") == 0))
		{
			memset (&filelist[filecount], 0, sizeof (FILEENTRIES));
			filelist[filecount].length = smbdir.size_low;
			smbdir.name[MAXJOLIET] = 0;

			if(smbdir.attributes == SMB_SRCH_DIRECTORY)
				filelist[filecount].flags = 1; // flag this as a dir
			else
				filelist[filecount].flags = 0;

			// Update display name
			memcpy (&filelist[filecount].displayname, smbdir.name, MAXDISPLAY);
			filelist[filecount].displayname[MAXDISPLAY] = 0;

			strcpy (filelist[filecount].filename, smbdir.name);
			filecount++;
		}
	} while (SMB_FindNext (&smbdir, smbconn) == SMB_SUCCESS);

	// close directory
	SMB_FindClose (smbconn);

	// Sort the file list
	qsort(filelist, filecount, sizeof(FILEENTRIES), FileSortCallback);

	return filecount;
}

/****************************************************************************
 * Open SMB file
 ***************************************************************************/

SMBFILE OpenSMBFile(char * filepath)
{
	return SMB_OpenFile (SMBPath(filepath), SMB_OPEN_READING, SMB_OF_OPEN, smbconn);
}

/****************************************************************************
 * LoadSMBSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
int
LoadSMBSzFile(char * filepath, unsigned char * rbuffer)
{
	if(!ConnectShare (NOTSILENT))
		return 0;

	smbfile = OpenSMBFile(filepath);

	if (smbfile)
	{
		u32 size = SzExtractFile(filelist[selection].offset, rbuffer);
		SMB_CloseFile (smbfile);
		return size;
	}
	else
	{
		WaitPrompt((char*) "Error opening file");
		return 0;
	}
}

/****************************************************************************
 * SaveSMBFile
 * Write buffer to SMB file
 ****************************************************************************/

int
SaveSMBFile (char * sbuffer, char *filepath, int datasize, bool silent)
{
	if(!ConnectShare (NOTSILENT))
		return 0;

	int dsize = datasize;
	int wrote = 0;
	int boffset = 0;

	smbfile =
	SMB_OpenFile (SMBPath(filepath), SMB_OPEN_WRITING | SMB_DENY_NONE,
	SMB_OF_CREATE | SMB_OF_TRUNCATE, smbconn);

	if (smbfile)
	{
		while (dsize > 0)
		{
			if (dsize > 1024)
				wrote =
					SMB_WriteFile ((char *) sbuffer + boffset, 1024, boffset, smbfile);
			else
				wrote =
					SMB_WriteFile ((char *) sbuffer + boffset, dsize, boffset, smbfile);

			boffset += wrote;
			dsize -= wrote;
		}
		SMB_CloseFile (smbfile);
	}
	else
	{
		char msg[100];
		sprintf(msg, "Couldn't save SMB: %s", SMBPath(filepath));
		WaitPrompt (msg);
	}

	return boffset;
}

/****************************************************************************
 * LoadSMBFile
 * Load up a buffer from SMB file
 ****************************************************************************/

int
LoadSMBFile (char * sbuffer, char *filepath, int length, bool silent)
{
	if(!ConnectShare (NOTSILENT))
		return 0;

	int ret;
	int boffset = 0;

	smbfile = OpenSMBFile(filepath);

	if (!smbfile)
	{
		if(!silent)
		{
			char msg[100];
			sprintf(msg, "Couldn't open SMB: %s", SMBPath(filepath));
			WaitPrompt (msg);
		}
		return 0;
	}

	if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
	{
		boffset = SMB_ReadFile (sbuffer, length, 0, smbfile);
	}
	else // load whole file
	{
		ret = SMB_ReadFile (sbuffer, 2048, boffset, smbfile);

		if (IsZipFile (sbuffer))
		{
			boffset = UnZipBuffer ((unsigned char *)sbuffer, METHOD_SMB); // unzip from SMB
		}
		else
		{
			// Just load the file up
			while ((ret = SMB_ReadFile (sbuffer + boffset, 2048, boffset, smbfile)) > 0)
			{
				boffset += ret;
			}
		}
	}
	SMB_CloseFile (smbfile);

	return boffset;
}
