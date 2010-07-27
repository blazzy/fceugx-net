/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric December 2008
 *
 * networkop.cpp
 *
 * Network and SMB support routines
 ****************************************************************************/

#ifdef HW_DOL

bool
ConnectShare (bool silent)
{
	return false;
}

#else

#include <network.h>
#include <smb.h>
#include <mxml.h>
#include <malloc.h>

#include "fceugx.h"
#include "menu.h"
#include "fileop.h"
#include "filebrowser.h"
#include "utils/http.h"
#include "utils/unzip/unzip.h"
#include "utils/unzip/miniunz.h"

bool inNetworkInit = false;
static bool networkInit = false;
static bool autoNetworkInit = true;
static bool networkShareInit = false;
static bool updateChecked = false; // true if checked for app update
static char updateURL[128]; // URL of app update
bool updateFound = false; // true if an app update was found

/****************************************************************************
 * UpdateCheck
 * Checks for an update for the application
 ***************************************************************************/

void UpdateCheck()
{
	// we only check for an update if we have internet + SD/USB
	if(updateChecked || !networkInit)
		return;

	if(!isMounted[DEVICE_SD] && !isMounted[DEVICE_USB])
		return;

	updateChecked = true;
	u8 tmpbuffer[256];

	if (http_request("http://fceugc.googlecode.com/svn/trunk/update.xml", NULL, tmpbuffer, 256, SILENT) <= 0)
		return;

	mxml_node_t *xml;
	mxml_node_t *item;

	xml = mxmlLoadString(NULL, (char *)tmpbuffer, MXML_TEXT_CALLBACK);

	if(!xml)
		return;

	// check settings version
	item = mxmlFindElement(xml, xml, "app", "version", NULL, MXML_DESCEND);
	if(item) // a version entry exists
	{
		const char * version = mxmlElementGetAttr(item, "version");

		if(version && strlen(version) == 5)
		{
			int verMajor = version[0] - '0';
			int verMinor = version[2] - '0';
			int verPoint = version[4] - '0';
			int curMajor = APPVERSION[0] - '0';
			int curMinor = APPVERSION[2] - '0';
			int curPoint = APPVERSION[4] - '0';

			// check that the versioning is valid and is a newer version
			if((verMajor >= 0 && verMajor <= 9 &&
				verMinor >= 0 && verMinor <= 9 &&
				verPoint >= 0 && verPoint <= 9) &&
				(verMajor > curMajor ||
				(verMajor == curMajor && verMinor > curMinor) ||
				(verMajor == curMajor && verMinor == curMinor && verPoint > curPoint)))
			{
				item = mxmlFindElement(xml, xml, "file", NULL, NULL, MXML_DESCEND);
				if(item)
				{
					const char * tmp = mxmlElementGetAttr(item, "url");
					if(tmp)
					{
						snprintf(updateURL, 128, "%s", tmp);
						updateFound = true;
					}
				}
			}
		}
	}
	mxmlDelete(xml);
}

static bool unzipArchive(char * zipfilepath, char * unzipfolderpath)
{
	unzFile uf = unzOpen(zipfilepath);
	if (uf==NULL)
		return false;

	if(chdir(unzipfolderpath)) // can't access dir
	{
		makedir(unzipfolderpath); // attempt to make dir
		if(chdir(unzipfolderpath)) // still can't access dir
			return false;
	}

	extractZip(uf,0,1,0);

	unzCloseCurrentFile(uf);
	return true;
}

bool DownloadUpdate()
{
	bool result = false;

	if(updateURL[0] == 0 || appPath[0] == 0 || !ChangeInterface(appPath, NOTSILENT))
	{
		ErrorPrompt("Update failed!");
		updateFound = false; // updating is finished (successful or not!)
		return false;
	}

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceThread();

	int device;
	FindDevice(appPath, &device);

	char updateFile[50];
	sprintf(updateFile, "%s%s Update.zip", pathPrefix[device], APPNAME);

	FILE * hfile = fopen (updateFile, "wb");

	if (hfile)
	{
		if(http_request(updateURL, hfile, NULL, (1024*1024*10), NOTSILENT) > 0)
		{
			fclose (hfile);
			result = unzipArchive(updateFile, (char *)pathPrefix[device]);
		}
		else
		{
			fclose (hfile);
		}
		remove(updateFile); // delete update file
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	if(result)
		InfoPrompt("Update successful!");
	else
		ErrorPrompt("Update failed!");

	updateFound = false; // updating is finished (successful or not!)
	return result;
}

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ***************************************************************************/

void InitializeNetwork(bool silent)
{
	// stop if we're already initialized, or if auto-init has failed before
	// in which case, manual initialization is required
	if(networkInit || (silent && !autoNetworkInit))
		return;

	int retry = 1;
	char ip[16];
	s32 initResult;

	if(!silent)
		ShowAction ("Initializing network...");

	while(inNetworkInit) // a network init is already in progress!
		usleep(50);

	if(!networkInit) // check again if the network was inited
	{
		inNetworkInit = true;

		while(retry)
		{
			if(!silent)
				ShowAction ("Initializing network...");

			initResult = if_config(ip, NULL, NULL, true);

			if(initResult == 0)
				networkInit = true;

			if(networkInit || silent)
				break;

			retry = ErrorPromptRetry("Unable to initialize network!");
		}

		// do not automatically attempt a reconnection
		autoNetworkInit = false;
		inNetworkInit = false;
	}
	if(!silent)
		CancelAction();
}

void CloseShare()
{
	if(networkShareInit)
		smbClose("smb");
	networkShareInit = false;
	isMounted[DEVICE_SMB] = false;
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

	if(networkShareInit)
		return true;

	int retry = 1;
	int chkS = (strlen(GCSettings.smbshare) > 0) ? 0:1;
	int chkI = (strlen(GCSettings.smbip) > 0) ? 0:1;

	// check that all parameters have been set
	if(chkS + chkI > 0)
	{
		if(!silent)
		{
			char msg[50];
			char msg2[100];
			if(chkS + chkI > 1) // more than one thing is wrong
				sprintf(msg, "Check settings.xml.");
			else if(chkS)
				sprintf(msg, "Share name is blank.");
			else if(chkI)
				sprintf(msg, "Share IP is blank.");

			sprintf(msg2, "Invalid network settings - %s", msg);
			ErrorPrompt(msg2);
		}
		return false;
	}

	if(!networkInit)
		InitializeNetwork(silent);

	if(!networkInit)
		return false;

	while(retry)
	{
		if(!silent)
			ShowAction ("Connecting to network share...");
		
		if(smbInit(GCSettings.smbuser, GCSettings.smbpwd, GCSettings.smbshare, GCSettings.smbip))
			networkShareInit = true;

		if(networkShareInit || silent)
			break;

		retry = ErrorPromptRetry("Failed to connect to network share.");
	}

	if(!silent)
		CancelAction();

	return networkShareInit;
}

#endif
