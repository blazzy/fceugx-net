/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * fceustate.cpp
 *
 * Memory Based Load/Save State Manager
 *
 * These are simply the state routines, brought together as GCxxxxx
 * The original file I/O is replaced with Memory Read/Writes to the
 * statebuffer below
 ****************************************************************************/

#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <fat.h>
#include <zlib.h>
#include "pngu.h"

#include "fceugx.h"
#include "fceusupport.h"
#include "menu.h"
#include "filebrowser.h"
#include "memcardop.h"
#include "fileop.h"
#include "gcvideo.h"

bool SaveState (char * filepath, bool silent)
{
	bool retval = false;
	int datasize;
	int offset = 0;
	int imgSize = 0; // image screenshot bytes written
	int device;
			
	if(!FindDevice(filepath, &device))
		return 0;

	// save screenshot - I would prefer to do this from gameScreenTex
	if(gameScreenTex2 != NULL && device != DEVICE_MC_SLOTA && device != DEVICE_MC_SLOTB)
	{
		AllocSaveBuffer ();

		IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

		if (pngContext != NULL)
		{
			imgSize = PNGU_EncodeFromGXTexture(pngContext, screenwidth, screenheight, gameScreenTex2, 0);
			PNGU_ReleaseImageContext(pngContext);
		}

		if(imgSize > 0)
		{
			char screenpath[1024];
			strncpy(screenpath, filepath, 1024);
			screenpath[strlen(screenpath)-4] = 0;
			sprintf(screenpath, "%s.png", screenpath);
			SaveFile(screenpath, imgSize, silent);
		}

		FreeSaveBuffer ();
	}

	memorystream save(SAVEBUFFERSIZE);
	FCEUSS_SaveMS(&save, Z_BEST_COMPRESSION);
	save.sync();
	datasize = save.tellp();

	if (datasize)
	{
		if(device == DEVICE_MC_SLOTA || device == DEVICE_MC_SLOTB)
		{
			// Set the comments
			char comments[2][32];
			memset(comments, 0, 64);
			sprintf (comments[0], "%s State", APPNAME);
			snprintf (comments[1], 32, romFilename);
			SetMCSaveComments(comments);
		}
		offset = SaveFile(save.buf(), filepath, datasize, silent);
	}

	if (offset > 0)
	{
		if (!silent)
			InfoPrompt("Save successful");
		retval = true;
	}
	return retval;
}

bool
SaveStateAuto (bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_STATE, romFilename, 0))
		return false;

	return SaveState(filepath, silent);
}

bool LoadState (char * filepath, bool silent)
{
	int offset = 0;
	bool retval = false;
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	AllocSaveBuffer ();

	offset = LoadFile(filepath, silent);

	if (offset > 0)
	{
		memorystream save((char *)savebuffer, offset);
		FCEUSS_LoadFP(&save, SSLOADPARAM_NOBACKUP);
		retval = true;
	}
	else
	{
		// if we reached here, nothing was done!
		if(!silent)
			ErrorPrompt ("State file not found");
	}
	FreeSaveBuffer ();
	return retval;
}

bool
LoadStateAuto (bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_STATE, romFilename, 0))
		return false;

	return LoadState(filepath, silent);
}
