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
#include "fceugx.h"
#include "fileop.h"
#include "gcunzip.h"
#include "menudraw.h"
#include "filesel.h"

bool networkInit = false;
bool networkShareInit = false;
bool networkInitHalt = false;

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ****************************************************************************/

void InitializeNetwork(bool silent)
{
	if(networkInit || networkInitHalt)
		return;

	if(!silent)
		ShowAction ("Initializing network...");

	s32 result;

	while ((result = net_init()) == -EAGAIN);

	if (result >= 0)
	{
		char myIP[16];

		if (if_config(myIP, NULL, NULL, true) < 0)
		{
			networkInitHalt = true; // do not attempt a reconnection again

			if(!silent)
				WaitPrompt("Error reading IP address.");
		}
		else
		{
			networkInit = true;
		}
	}
	else
	{
		if(!silent)
			WaitPrompt("Unable to initialize network.");
	}
}

void CloseShare()
{
	if(networkShareInit)
		smbClose();
	networkShareInit = false;
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
			WaitPrompt("Invalid network settings. Check settings.xml.");
		return false;
	}

	if(!networkInit)
		InitializeNetwork(silent);

	if(networkInit)
	{
		if(unmountRequired[METHOD_SMB])
			CloseShare();

		if(!networkShareInit)
		{
			if(!silent)
				ShowAction ("Connecting to network share...");

			if(smbInit(GCSettings.smbuser, GCSettings.smbpwd,
					GCSettings.smbshare, GCSettings.smbip))
			{
				networkShareInit = true;
			}
		}

		if(!networkShareInit && !silent)
			WaitPrompt ("Failed to connect to network share.");
	}

	return networkShareInit;
}
