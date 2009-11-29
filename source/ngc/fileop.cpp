/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * fileop.cpp
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
#include <malloc.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>
#include <di/di.h>
#include <ogc/dvd.h>
#include <iso9660.h>

#include "fceusupport.h"
#include "fceugx.h"
#include "fileop.h"
#include "networkop.h"
#include "gcunzip.h"
#include "menu.h"
#include "filebrowser.h"

#define THREAD_SLEEP 100

unsigned char savebuffer[SAVEBUFFERSIZE] ATTRIBUTE_ALIGN(32);
static mutex_t bufferLock = LWP_MUTEX_NULL;
FILE * file; // file pointer - the only one we should ever use!
bool unmountRequired[9] = { false, false, false, false, false, false, false, false, false };
bool isMounted[9] = { false, false, false, false, false, false, false, false, false };

#ifdef HW_RVL
	const DISC_INTERFACE* sd = &__io_wiisd;
	const DISC_INTERFACE* usb = &__io_usbstorage;
	const DISC_INTERFACE* dvd = &__io_wiidvd;
#else
	const DISC_INTERFACE* carda = &__io_gcsda;
	const DISC_INTERFACE* cardb = &__io_gcsdb;
	const DISC_INTERFACE* dvd = &__io_gcdvd;
#endif

// folder parsing thread
static lwp_t parsethread = LWP_THREAD_NULL;
static DIR_ITER * dirIter = NULL;
static bool parseHalt = true;
bool ParseDirEntries();

// device thread
static lwp_t devicethread = LWP_THREAD_NULL;
static bool deviceHalt = true;

/****************************************************************************
 * ResumeDeviceThread
 *
 * Signals the device thread to start, and resumes the thread.
 ***************************************************************************/
void
ResumeDeviceThread()
{
	deviceHalt = false;
	LWP_ResumeThread(devicethread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the device thread to stop.
 ***************************************************************************/
void
HaltDeviceThread()
{
	deviceHalt = true;

	#ifdef HW_RVL
	if(inNetworkInit) // don't wait for network to initialize
		return;
	#endif

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(devicethread))
		usleep(THREAD_SLEEP);
}

/****************************************************************************
 * HaltParseThread
 *
 * Signals the parse thread to stop.
 ***************************************************************************/
void
HaltParseThread()
{
	parseHalt = true;

	while(!LWP_ThreadIsSuspended(parsethread))
		usleep(THREAD_SLEEP);
}

/****************************************************************************
 * devicecallback
 *
 * This checks our devices for changes (SD/USB removed) and
 * initializes the network in the background
 ***************************************************************************/
static int devsleep = 1*1000*1000;

static void *
devicecallback (void *arg)
{
	while(devsleep > 0)
	{
		if(deviceHalt)
			LWP_SuspendThread(devicethread);
		usleep(THREAD_SLEEP);
		devsleep -= THREAD_SLEEP;
	}

	while (1)
	{
#ifdef HW_RVL
		if(isMounted[DEVICE_SD])
		{
			if(!sd->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD] = true;
				isMounted[DEVICE_SD] = false;
			}
		}

		if(isMounted[DEVICE_USB])
		{
			if(!usb->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_USB] = true;
				isMounted[DEVICE_USB] = false;
			}
		}

		UpdateCheck();
		InitializeNetwork(SILENT);
#else
		if(isMounted[DEVICE_SD_SLOTA])
		{
			if(!carda->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD_SLOTA] = true;
				isMounted[DEVICE_SD_SLOTA] = false;
			}
		}
		if(isMounted[DEVICE_SD_SLOTB])
		{
			if(!cardb->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD_SLOTB] = true;
				isMounted[DEVICE_SD_SLOTB] = false;
			}
		}
#endif
		if(isMounted[DEVICE_DVD])
		{
			if(!dvd->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_DVD] = true;
				isMounted[DEVICE_DVD] = false;
			}
		}

		devsleep = 1000*1000; // 1 sec

		while(devsleep > 0)
		{
			if(deviceHalt)
				LWP_SuspendThread(devicethread);
			usleep(THREAD_SLEEP);
			devsleep -= THREAD_SLEEP;
		}
	}
	return NULL;
}

static void *
parsecallback (void *arg)
{
	while(1)
	{
		while(ParseDirEntries())
			usleep(THREAD_SLEEP);
		LWP_SuspendThread(parsethread);
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
	LWP_CreateThread (&devicethread, devicecallback, NULL, NULL, 0, 40);
	LWP_CreateThread (&parsethread, parsecallback, NULL, NULL, 0, 80);
}

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
#ifdef HW_RVL
	fatUnmount("sd:");
	fatUnmount("usb:");
#else
	fatUnmount("carda:");
	fatUnmount("cardb:");
#endif
}

/****************************************************************************
 * MountFAT
 * Checks if the device needs to be (re)mounted
 * If so, unmounts the device
 * Attempts to mount the device specified
 * Sets libfat to use the device by default
 ***************************************************************************/

static bool MountFAT(int device, int silent)
{
	bool mounted = false;
	int retry = 1;
	char name[10], name2[10];
	const DISC_INTERFACE* disc = NULL;

	switch(device)
	{
#ifdef HW_RVL
		case DEVICE_SD:
			sprintf(name, "sd");
			sprintf(name2, "sd:");
			disc = sd;
			break;
		case DEVICE_USB:
			sprintf(name, "usb");
			sprintf(name2, "usb:");
			disc = usb;
			break;
#else
		case DEVICE_SD_SLOTA:
			sprintf(name, "carda");
			sprintf(name2, "carda:");
			disc = carda;
			break;

		case DEVICE_SD_SLOTB:
			sprintf(name, "cardb");
			sprintf(name2, "cardb:");
			disc = cardb;
			break;
#endif
		default:
			return false; // unknown device
	}

	if(unmountRequired[device])
	{
		unmountRequired[device] = false;
		fatUnmount(name2);
		disc->shutdown();
		isMounted[device] = false;
	}

	while(retry)
	{
		if(disc->startup() && fatMountSimple(name, disc))
			mounted = true;

		if(mounted || silent)
			break;

		if(device == DEVICE_SD)
			retry = ErrorPromptRetry("SD card not found!");
		else
			retry = ErrorPromptRetry("USB drive not found!");
	}

	isMounted[device] = mounted;
	return mounted;
}

void MountAllFAT()
{
#ifdef HW_RVL
	MountFAT(DEVICE_SD, SILENT);
	MountFAT(DEVICE_USB, SILENT);
#else
	MountFAT(DEVICE_SD_SLOTA, SILENT);
	MountFAT(DEVICE_SD_SLOTB, SILENT);
#endif
}

/****************************************************************************
 * MountDVD()
 *
 * Tests if a ISO9660 DVD is inserted and available, and mounts it
 ***************************************************************************/
bool MountDVD(bool silent)
{
	bool mounted = false;
	int retry = 1;

	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		ISO9660_Unmount();
	}

	while(retry)
	{
		ShowAction("Loading DVD...");

		if(!dvd->isInserted())
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("No disc inserted!");
		}
		else if(!ISO9660_Mount())
		{
			if(silent)
				break;
			
			retry = ErrorPromptRetry("Invalid DVD.");
		}
		else
		{
			mounted = true;
			break;
		}
	}
	CancelAction();
	isMounted[DEVICE_DVD] = mounted;
	return mounted;
}

bool FindDevice(char * filepath, int * device)
{
	if(!filepath || filepath[0] == 0)
		return false;
	
	if(strncmp(filepath, "sd:", 3) == 0)
	{
		*device = DEVICE_SD;
		return true;
	}
	else if(strncmp(filepath, "usb:", 4) == 0)
	{
		*device = DEVICE_USB;
		return true;
	}
	else if(strncmp(filepath, "dvd:", 4) == 0)
	{
		*device = DEVICE_DVD;
		return true;
	}
	else if(strncmp(filepath, "smb:", 4) == 0)
	{
		*device = DEVICE_SMB;
		return true;
	}
	else if(strncmp(filepath, "carda:", 5) == 0)
	{
		*device = DEVICE_SD_SLOTA;
		return true;
	}
	else if(strncmp(filepath, "cardb:", 5) == 0)
	{
		*device = DEVICE_SD_SLOTB;
		return true;
	}
	return false;
}

char * StripDevice(char * path)
{
	if(path == NULL)
		return NULL;
	
	char * newpath = strchr(path,'/');
	
	if(newpath != NULL)
		newpath++;
	
	return newpath;
}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeInterface(int device, bool silent)
{
	if(isMounted[device])
		return true;

	bool mounted = false;

	switch(device)
	{
		case DEVICE_SD:
		case DEVICE_USB:
			mounted = MountFAT(device, silent);
			break;
		case DEVICE_DVD:
			mounted = MountDVD(silent);
			break;
		case DEVICE_SMB:
			mounted = ConnectShare(silent);
			break;
	}

	return mounted;
}

bool ChangeInterface(char * filepath, bool silent)
{
	int device = -1;

	if(!FindDevice(filepath, &device))
		return false;

	return ChangeInterface(device, silent);
}

void CreateAppPath(char * origpath)
{
	if(!origpath || origpath[0] == 0)
		return;
	
	char * path = strdup(origpath); // make a copy so we don't mess up original

	if(!path)
		return;
	
	char * loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	int pos = 0;

	// replace fat:/ with sd:/
	if(strncmp(path, "fat:/", 5) == 0)
	{
		pos++;
		path[1] = 's';
		path[2] = 'd';
	}
	if(ChangeInterface(&path[pos], SILENT))
		strncpy(appPath, &path[pos], MAXPATHLEN);
	appPath[MAXPATHLEN-1] = 0;
	free(path);
}

bool ParseDirEntries()
{
	if(!dirIter)
		return false;

	char filename[MAXPATHLEN];
	struct stat filestat;

	int i, res;

	for(i=0; i < 20; i++)
	{
		res = dirnext(dirIter,filename,&filestat);

		if(res != 0)
			break;

		if(strcmp(filename,".") == 0)
		{
			i--;
			continue;
		}
		
		if(!AddBrowserEntry())
		{
			i=0;
			parseHalt = true;
			break;
		}

		strncpy(browserList[browser.numEntries+i].filename, filename, MAXJOLIET);
		browserList[browser.numEntries+i].length = filestat.st_size;
		browserList[browser.numEntries+i].mtime = filestat.st_mtime;
		browserList[browser.numEntries+i].isdir = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir

		if(browserList[browser.numEntries+i].isdir)
		{
			if(strcmp(filename, "..") == 0)
				sprintf(browserList[browser.numEntries+i].displayname, "Up One Level");
			else
				strncpy(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename, MAXJOLIET);
			browserList[browser.numEntries+i].icon = ICON_FOLDER;
		}
		else
		{
			StripExt(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename); // hide file extension
		}
	}

	// Sort the file list
	if(i >= 0)
	{
		browser.numEntries += i;
		qsort(browserList, browser.numEntries, sizeof(BROWSERENTRY), FileSortCallback);
	}

	if(res != 0 || parseHalt)
	{
		dirclose(dirIter); // close directory
		dirIter = NULL;
		return false; // no more entries
	}
	return true; // more entries
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory(bool waitParse)
{
	char msg[128];
	int retry = 1;
	bool mounted = false;
	
	ResetBrowser(); // reset browser

	// open the directory
	while(dirIter == NULL && retry == 1)
	{
		mounted = ChangeInterface(browser.dir, NOTSILENT);

		if(mounted)
			dirIter = diropen(browser.dir);
		else
			return -1;

		if(dirIter == NULL)
		{
			sprintf(msg, "Error opening %s", browser.dir);
			retry = ErrorPromptRetry(msg);
		}
	}

	// if we can't open the dir, try higher levels
	if (dirIter == NULL)
	{
		char * devEnd = strrchr(browser.dir, '/');
		
		while(!IsDeviceRoot(browser.dir))
		{
			devEnd[0] = 0; // strip slash
			devEnd = strrchr(browser.dir, '/');

			if(devEnd == NULL)
				break;

			devEnd[1] = 0; // strip remaining file listing
			dirIter = diropen(browser.dir);
			if (dirIter)
				break;
		}
	}
	
	if(dirIter == NULL)
		return -1;

	if(IsDeviceRoot(browser.dir))
	{
		browser.numEntries = 1;
		sprintf(browserList[0].filename, "..");
		sprintf(browserList[0].displayname, "Up One Level");
		browserList[0].length = 0;
		browserList[0].mtime = 0;
		browserList[0].isdir = 1; // flag this as a dir
		browserList[0].icon = ICON_FOLDER;
	}

	parseHalt = false;
	ParseDirEntries(); // index first 20 entries
	LWP_ResumeThread(parsethread); // index remaining entries

	if(waitParse) // wait for complete parsing
	{
		ShowAction("Loading...");

		while(!LWP_ThreadIsSuspended(parsethread))
			usleep(THREAD_SLEEP);

		CancelAction();
	}

	return browser.numEntries;
}

/****************************************************************************
 * AllocSaveBuffer ()
 * Clear and allocate the savebuffer
 ***************************************************************************/
void
AllocSaveBuffer ()
{
	if(bufferLock == LWP_MUTEX_NULL)
		LWP_MutexInit(&bufferLock, false);

	if(bufferLock != LWP_MUTEX_NULL)
		LWP_MutexLock(bufferLock);
	memset (savebuffer, 0, SAVEBUFFERSIZE);
}

/****************************************************************************
 * FreeSaveBuffer ()
 * Free the savebuffer memory
 ***************************************************************************/
void
FreeSaveBuffer ()
{
	if(bufferLock != LWP_MUTEX_NULL)
		LWP_MutexUnlock(bufferLock);
}

/****************************************************************************
 * LoadSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
size_t
LoadSzFile(char * filepath, unsigned char * rbuffer)
{
	size_t size = 0;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// halt parsing
	HaltParseThread();

	file = fopen (filepath, "rb");
	if (file > 0)
	{
		size = SzExtractFile(browserList[browser.selIndex].filenum, rbuffer);
		fclose (file);
	}
	else
	{
		ErrorPrompt("Error opening file");
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	return size;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
size_t
LoadFile (char * rbuffer, char *filepath, size_t length, bool silent)
{
	char zipbuffer[2048];
	size_t size = 0, offset = 0, readsize = 0;
	int retry = 1;
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// halt parsing
	HaltParseThread();

	// open the file
	while(!size && retry)
	{
		if(!ChangeInterface(device, silent))
			break;

		file = fopen (filepath, "rb");

		if(!file)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error opening file!");
			continue;
		}

		if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
		{
			size = fread (rbuffer, 1, length, file);
		}
		else // load whole file
		{
			readsize = fread (zipbuffer, 1, 32, file);

			if(!readsize)
			{
				unmountRequired[device] = true;
				retry = ErrorPromptRetry("Error reading file!");
				continue;
			}

			if (IsZipFile (zipbuffer))
			{
				size = UnZipBuffer ((unsigned char *)rbuffer); // unzip
			}
			else
			{
				fseeko(file,0,SEEK_END);
				size = ftello(file);
				fseeko(file,0,SEEK_SET);

				while(!feof(file))
				{
					ShowProgress ("Loading...", offset, size);
					readsize = fread (rbuffer + offset, 1, 4096, file); // read in next chunk

					if(readsize <= 0)
						break; // reading finished (or failed)

					offset += readsize;
				}
				size = offset;
				CancelAction();
			}
		}
		fclose (file);
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();
	CancelAction();
	return size;
}

size_t LoadFile(char * filepath, bool silent)
{
	return LoadFile((char *)savebuffer, filepath, 0, silent);
}

/****************************************************************************
 * SaveFile
 * Write buffer to file
 ***************************************************************************/
size_t
SaveFile (char * buffer, char *filepath, size_t datasize, bool silent)
{
	size_t written = 0;
	size_t writesize, nextwrite;
	int retry = 1;
	int device;
		
	if(!FindDevice(filepath, &device))
		return 0;

	if(datasize == 0)
		return 0;

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceThread();

	// halt parsing
	HaltParseThread();

	ShowAction("Saving...");

	while(!written && retry == 1)
	{
		if(!ChangeInterface(device, silent))
			break;

		file = fopen (filepath, "wb");

		if(!file)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error creating file!");
			continue;
		}

		while(written < datasize)
		{
			if(datasize - written > 4096) nextwrite=4096;
			else nextwrite = datasize-written;
			writesize = fwrite (buffer+written, 1, nextwrite, file);
			if(writesize != nextwrite) break; // write failure
			written += writesize;
		}
		fclose (file);

		if(written != datasize) written = 0;

		if(!written)
		{
			unmountRequired[device] = true;
			retry = ErrorPromptRetry("Error saving file!");
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	CancelAction();
	return written;
}

size_t SaveFile(char * filepath, size_t datasize, bool silent)
{
	return SaveFile((char *)savebuffer, filepath, datasize, silent);
}
