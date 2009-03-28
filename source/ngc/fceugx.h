/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * fceugx.h
 *
 * This file controls overall program flow. Most things start and end here!
 ****************************************************************************/

#ifndef _FCEUGX_H_
#define _FCEUGX_H_

#define APPNAME			"FCE Ultra GX"
#define APPVERSION		"2.0.9"
#define PREF_FILE_NAME	"settings.xml"

#define NOTSILENT 0
#define SILENT 1

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB,
	METHOD_SD_SLOTA,
	METHOD_SD_SLOTB
};

enum {
	FILE_ROM,
	FILE_RAM,
	FILE_STATE,
	FILE_FDSBIOS,
	FILE_CHEAT,
	FILE_PREF
};

enum
{
	CTRL_PAD,
	CTRL_ZAPPER,
	CTRL_PAD2,
	CTRL_PAD4,
	CTRL_LENGTH
};

const char ctrlName[6][20] =
{ "NES Controller", "NES Zapper", "NES Controllers (2)", "NES Controllers (4)" };

struct SGCSettings{
	int		AutoLoad;
	int		AutoSave;
	int		LoadMethod; // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	int		SaveMethod; // For SRAM, Freeze, Prefs: Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, SMB
	char	LoadFolder[200]; // Path to game files
	char	SaveFolder[200]; // Path to save files
	char	CheatFolder[200]; // Path to cheat files

	char	smbip[16];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbshare[20];

	float	ZoomLevel; // zoom amount
	int		VerifySaves;
	int		render;		// 0 - original, 1 - filtered, 2 - unfiltered
	int		widescreen;
	int		hideoverscan;
	int		currpal;
	int		timing;
	int		Controller;
	int		crosshair;
	int		xshift;		// video output shift
	int		yshift;
	int		WiimoteOrientation;
	int		ExitAction;
	int		MusicVolume;
	int		SFXVolume;
};

void ExitApp();
void ShutdownWii();
extern struct SGCSettings GCSettings;
extern int ConfigRequested;
extern int ShutdownRequested;
extern int ExitRequested;
extern char appPath[];
extern int frameskip;

#endif
