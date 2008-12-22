/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * fileop.c
 *
 * File operations
 ****************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <zlib.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

#include "dvd.h"
#include "common.h"
#include "fceugx.h"
#include "fileop.h"
#include "memcardop.h"
#include "smbop.h"
#include "gcunzip.h"
#include "menudraw.h"
#include "filesel.h"

// file pointer - the only one we should ever use!
FILE * file;
bool unmountRequired[9] = { false, false, false, false, false, false, false, false, false };
bool isMounted[9] = { false, false, false, false, false, false, false, false, false };

#ifdef HW_RVL
	const DISC_INTERFACE* sd = &__io_wiisd;
	const DISC_INTERFACE* usb = &__io_usbstorage;
#else
	const DISC_INTERFACE* carda = &__io_gcsda;
	const DISC_INTERFACE* cardb = &__io_gcsdb;
#endif

/****************************************************************************
 * deviceThreading
 ***************************************************************************/
lwp_t devicethread;

/****************************************************************************
 * devicecallback
 *
 * This checks our devices for changes (SD/USB removed) and
 * initializes the network in the background
 ***************************************************************************/
static void *
devicecallback (void *arg)
{
	while (1)
	{
#ifdef HW_RVL
		if(isMounted[METHOD_SD])
		{
			if(!sd->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_SD] = true;
				isMounted[METHOD_SD] = false;
			}
		}

		if(isMounted[METHOD_USB])
		{
			if(!usb->isInserted()) // check if the device was removed - doesn't work on USB!
			{
				unmountRequired[METHOD_USB] = true;
				isMounted[METHOD_USB] = false;
			}
		}

		InitializeNetwork(SILENT);
#else
		if(isMounted[METHOD_SD_SLOTA])
		{
			if(!carda->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_SD_SLOTA] = true;
				isMounted[METHOD_SD_SLOTA] = false;
			}
		}
		if(isMounted[METHOD_SD_SLOTB])
		{
			if(!cardb->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_SD_SLOTB] = true;
				isMounted[METHOD_SD_SLOTB] = false;
			}
		}
#endif
		usleep(500000); // suspend thread for 1/2 sec
	}

	return NULL;
}

/****************************************************************************
 * InitDeviceThread
 *
 * libOGC provides a nice wrapper for LWP access.
 * This function sets up a new local queue and attaches the thread to it.
 ***************************************************************************/
void
InitDeviceThread()
{
	LWP_CreateThread (&devicethread, devicecallback, NULL, NULL, 0, 80);
}

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
#ifdef HW_RVL
	fatUnmount("sd");
	fatUnmount("usb");
#else
	fatUnmount("carda");
	fatUnmount("cardb");
#endif
}

/****************************************************************************
 * MountFAT
 * Checks if the device needs to be (re)mounted
 * If so, unmounts the device
 * Attempts to mount the device specified
 * Sets libfat to use the device by default
 ***************************************************************************/

bool MountFAT(int method)
{
	bool mounted = true; // assume our disc is already mounted
	char name[10];
	const DISC_INTERFACE* disc = NULL;

	switch(method)
	{
#ifdef HW_RVL
		case METHOD_SD:
			sprintf(name, "sd");
			disc = sd;
			break;
		case METHOD_USB:
			sprintf(name, "usb");
			disc = usb;
			break;
#else
		case METHOD_SD_SLOTA:
			sprintf(name, "carda");
			disc = carda;
			break;

		case METHOD_SD_SLOTB:
			sprintf(name, "cardb");
			disc = cardb;
			break;
#endif
		default:
			return false; // unknown device
	}

	sprintf(rootdir, "%s:/", name);

	if(unmountRequired[method])
	{
		unmountRequired[method] = false;
		fatUnmount(name);
		disc->shutdown();
	}
	if(!isMounted[method])
	{
		if(!disc->startup())
			mounted = false;
		else if(!fatMountSimple(name, disc))
			mounted = false;
		else
			fatEnableReadAhead(name, 6, 64);
	}

	isMounted[method] = mounted;
	return mounted;
}

void MountAllFAT()
{
#ifdef HW_RVL
	MountFAT(METHOD_SD);
	MountFAT(METHOD_USB);
#else
	MountFAT(METHOD_SD_SLOTA);
	MountFAT(METHOD_SD_SLOTB);
#endif
}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeInterface(int method, bool silent)
{
	bool mounted = false;

	if(method == METHOD_SD)
	{
		#ifdef HW_RVL
		mounted = MountFAT(METHOD_SD); // try Wii internal SD
		#else
		mounted = MountFAT(METHOD_SD_SLOTA); // try SD Gecko on slot A
		if(!mounted) // internal SD and SD Gecko (on slot A) not found
			mounted = MountFAT(METHOD_SD_SLOTB); // try SD Gecko on slot B
		#endif
		if(!mounted && !silent) // no SD device found
			WaitPrompt ("SD card not found!");
	}
	else if(method == METHOD_USB)
	{
		#ifdef HW_RVL
		mounted = MountFAT(method);

		if(!mounted && !silent)
			WaitPrompt ("USB drive not found!");
		#endif
	}
	else if(method == METHOD_DVD)
	{
		sprintf(rootdir, "/");
		mounted = MountDVD(silent);
	}
	else if(method == METHOD_SMB)
	{
		sprintf(rootdir, "smb:/");
		mounted = ConnectShare(silent);
	}

	return mounted;
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory()
{
	int nbfiles = 0;
	DIR_ITER *dir;
	char fulldir[MAXPATHLEN];
	char filename[MAXPATHLEN];
	char tmpname[MAXPATHLEN];
	struct stat filestat;
	char msg[128];

	// initialize selection
	selection = offset = 0;

	// Clear any existing values
	memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

	// add device to path
	sprintf(fulldir, "%s%s", rootdir, currentdir);

	// open the directory
	dir = diropen(fulldir);

	if (dir == NULL)
	{
		sprintf(msg, "Error opening %s", fulldir);
		WaitPrompt(msg);

		// if we can't open the dir, open root dir
		sprintf(fulldir,"%s",rootdir);

		dir = diropen(currentdir);

		if (dir == NULL)
		{
			sprintf(msg, "Error opening %s", fulldir);
			WaitPrompt(msg);
			return 0;
		}
	}

	// index files/folders
	while(dirnext(dir,filename,&filestat) == 0)
	{
		if(strcmp(filename,".") != 0)
		{
			memset(&filelist[nbfiles], 0, sizeof(FILEENTRIES));
			strncpy(filelist[nbfiles].filename, filename, MAXPATHLEN);
			StripExt(tmpname, filename); // hide file extension
			strncpy(filelist[nbfiles].displayname, tmpname, MAXDISPLAY+1);	// crop name for display
			filelist[nbfiles].length = filestat.st_size;
			filelist[nbfiles].flags = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir
			nbfiles++;
		}
	}

	// close directory
	dirclose(dir);

	// Sort the file list
	qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

	return nbfiles;
}

/****************************************************************************
 * LoadSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
u32
LoadSzFile(char * filepath, unsigned char * rbuffer)
{
	u32 size = 0;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	LWP_SuspendThread (devicethread);

	file = fopen (filepath, "rb");
	if (file > 0)
	{
		size = SzExtractFile(filelist[selection].offset, rbuffer);
		fclose (file);
	}
	else
	{
		WaitPrompt("Error opening file");
	}

	// go back to checking if devices were inserted/removed
	LWP_ResumeThread (devicethread);

	return size;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
u32
LoadFileBuf (char * rbuffer, char *filepath, u32 length, int method, bool silent)
{
	char zipbuffer[2048];
	u32 size = 0;
	u32 readsize = 0;

	if(!ChangeInterface(method, NOTSILENT))
		return 0;

	switch(method)
	{
		case METHOD_DVD:
			return LoadDVDFile (rbuffer, filepath, length, silent);
			break;
		case METHOD_MC_SLOTA:
			return LoadMCFile (rbuffer, CARD_SLOTA, filepath, silent);
			break;
		case METHOD_MC_SLOTB:
			return LoadMCFile (rbuffer, CARD_SLOTB, filepath, silent);
			break;
	}

	// stop checking if devices were removed/inserted
	// since we're loading a file
	LWP_SuspendThread (devicethread);

	// add device to filepath
	char fullpath[1024];
	sprintf(fullpath, "%s%s", rootdir, filepath);

	file = fopen (fullpath, "rb");

	if (file > 0)
	{
		if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
		{
			size = fread (rbuffer, 1, length, file);
		}
		else // load whole file
		{
			readsize = fread (zipbuffer, 1, 2048, file);

			if(readsize > 0)
			{
				if (IsZipFile (zipbuffer))
				{
					size = UnZipBuffer ((unsigned char *)rbuffer, method); // unzip
				}
				else
				{
					struct stat fileinfo;
					fstat(file->_file, &fileinfo);
					size = fileinfo.st_size;

					memcpy (rbuffer, zipbuffer, 2048); // copy what we already read

					ShowProgress ("Loading...", 2048, length);

					u32 offset = 2048;
					while(offset < size)
					{
						readsize = fread (rbuffer + offset, 1, (1024*512), file); // read in 512K chunks

						if(readsize <= 0 || readsize > (1024*512))
							break; // read failure

						if(readsize > 0)
							offset += readsize;
						ShowProgress ("Loading...", offset, length);
					}

					if(offset != size) // # bytes read doesn't match # expected
						size = 0;
				}
			}
		}
		fclose (file);
	}
	if(!size && !silent)
	{
		unmountRequired[method] = true;
		WaitPrompt("Error loading file!");
	}

	// go back to checking if devices were inserted/removed
	LWP_ResumeThread (devicethread);

	return size;
}

u32 LoadFile(char filepath[], int method, bool silent)
{
	return LoadFileBuf((char *)savebuffer, filepath, 0, method, silent);
}

/****************************************************************************
 * SaveFile
 * Write buffer to file
 ***************************************************************************/
u32
SaveFileBuf (char * buffer, char *filepath, u32 datasize, int method, bool silent)
{
	u32 written = 0;

	if(!ChangeInterface(method, NOTSILENT))
		return 0;

	switch(method)
	{
		case METHOD_MC_SLOTA:
			return SaveMCFile (buffer, CARD_SLOTA, filepath, datasize, silent);
			break;
		case METHOD_MC_SLOTB:
			return SaveMCFile (buffer, CARD_SLOTB, filepath, datasize, silent);
			break;
	}

	if (datasize)
    {
		// stop checking if devices were removed/inserted
		// since we're saving a file
		LWP_SuspendThread (devicethread);

		// add device to filepath
		char fullpath[1024];
		sprintf(fullpath, "%s%s", rootdir, filepath);

		// open file for writing
		file = fopen (fullpath, "wb");

		if (file > 0)
		{
			written = fwrite (savebuffer, 1, datasize, file);
			fclose (file);
		}

		if(!written && !silent)
		{
			unmountRequired[method] = true;
			WaitPrompt ("Error saving file!");
		}

		// go back to checking if devices were inserted/removed
		LWP_ResumeThread (devicethread);
    }
    return written;
}

u32 SaveFile(char filepath[], u32 datasize, int method, bool silent)
{
	return SaveFileBuf((char *)savebuffer, filepath, datasize, method, silent);
}

