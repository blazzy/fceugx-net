/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * fceload.c
 *
 * NES Memory Load Game
 *
 * This performs the functions of LoadGame and iNESLoad from a single module
 * Helper function for GameCube injected ROMS
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "fceugx.h"
#include "gcaudio.h"
#include "fceusupport.h"
#include "pad.h"
#include "menu.h"
#include "fileop.h"
#include "filebrowser.h"
#include "cheatmgr.h"

bool romLoaded = false;

#define SAMPLERATE 48000

int GCMemROM(int size)
{
	bool biosError = false;

	ResetGameLoaded();

	CloseGame();
	GameInfo = new FCEUGI();
	memset(GameInfo, 0, sizeof(FCEUGI));

	GameInfo->filename = strdup(romFilename);
	GameInfo->archiveCount = 0;

	/*** Set some default values ***/
	GameInfo->soundchan = 1;
	GameInfo->soundrate = SAMPLERATE;
	GameInfo->name=0;
	GameInfo->type=GIT_CART;
	GameInfo->vidsys=GIV_USER;
	GameInfo->input[0]=GameInfo->input[1]=SI_UNSET;
	GameInfo->inputfc=SIFC_UNSET;
	GameInfo->cspecial=SIS_NONE;

	/*** Set internal sound information ***/
	FCEUI_Sound(SAMPLERATE);
	FCEUI_SetSoundVolume(100); // 0-100
	FCEUI_SetLowPass(0);

	FCEUFILE * fceufp = new FCEUFILE();
	fceufp->size = size;
	fceufp->filename = romFilename;
	memorystream * fceumem;

	romLoaded = false;

	// for some reason FCEU_fseek(fp,0,SEEK_SET); fails, so we will do this
	fceumem = new memorystream((char *) nesrom, size);
	fceufp->stream = fceumem;
	romLoaded = iNESLoad(romFilename, fceufp, 1);

	if(!romLoaded)
	{
		delete fceumem;
		fceumem = new memorystream((char *) nesrom, size);
		fceufp->stream = fceumem;
		romLoaded = UNIFLoad(romFilename, fceufp);
	}

	if(!romLoaded)
	{
		delete fceumem;
		fceumem = new memorystream((char *) nesrom, size);
		fceufp->stream = fceumem;
		romLoaded = NSFLoad(romFilename, fceufp);
	}

	if(!romLoaded)
	{
		// read FDS BIOS into FDSBIOS - should be 8192 bytes
		if (FDSBIOS[1] == 0)
		{
			size_t biosSize = 0;
			char * tmpbuffer = (char *) memalign(32, 64 * 1024);

			char filepath[1024];

			if (MakeFilePath(filepath, FILE_FDSBIOS))
			{
				biosSize = LoadFile(tmpbuffer, filepath, 0, SILENT);
			}

			if (biosSize == 8192)
			{
				memcpy(FDSBIOS, tmpbuffer, 8192);
			}
			else
			{
				biosError = true;

				if (biosSize > 0)
					ErrorPrompt("FDS BIOS file is invalid!");
				else
					ErrorPrompt("FDS BIOS file not found!");
			}
			free(tmpbuffer);
		}
		if (FDSBIOS[1] != 0)
		{
			// load game
			delete fceumem;
			fceumem = new memorystream((char *) nesrom, size);
			fceufp->stream = fceumem;

			romLoaded = FDSLoad(romFilename, fceufp);
		}
	}

	delete fceufp;

	if (romLoaded)
	{
		FCEU_ResetVidSys();

		if(GameInfo->type!=GIT_NSF)
			if(FSettings.GameGenie)
				OpenGameGenie();
		PowerNES();

		//if(GameInfo->type!=GIT_NSF)
		//	FCEU_LoadGamePalette();

		FCEU_ResetPalette();
		FCEU_ResetMessages();	// Save state, status messages, etc.
		SetupCheats();
		ResetAudio();
		return 1;
	}
	else
	{
		delete GameInfo;
		GameInfo = 0;

		if(!biosError)
			ErrorPrompt("Invalid game file!");
		romLoaded = false;
		return 0;
	}
}
