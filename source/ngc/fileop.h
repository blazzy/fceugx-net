/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * fileop.h
 *
 * File operations
 ****************************************************************************/

#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <unistd.h>

#define SAVEBUFFERSIZE (1024 * 512)

void InitDeviceThread();
void ResumeDeviceThread();
void HaltDeviceThread();
void HaltParseThread();
void MountAllFAT();
void UnmountAllFAT();
bool FindDevice(char * filepath, int * device);
char * StripDevice(char * path);
bool ChangeInterface(int device, bool silent);
bool ChangeInterface(char * filepath, bool silent);
void CreateAppPath(char * origpath);
int ParseDirectory(bool waitParse = false);
void AllocSaveBuffer();
void FreeSaveBuffer();
u32 LoadFile(char * rbuffer, char *filepath, u32 length, bool silent);
u32 LoadFile(char * filepath, bool silent);
u32 LoadSzFile(char * filepath, unsigned char * rbuffer);
u32 SaveFile(char * buffer, char *filepath, u32 datasize, bool silent);
u32 SaveFile(char * filepath, u32 datasize, bool silent);

extern unsigned char savebuffer[];
extern FILE * file;
extern bool unmountRequired[];
extern bool isMounted[];

#endif
