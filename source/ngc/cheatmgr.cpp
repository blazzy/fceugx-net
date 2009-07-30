/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * cheatmgr.cpp
 *
 * Cheat handling
 ***************************************************************************/

#include <malloc.h>

#include "fceugx.h"
#include "fceusupport.h"
#include "fileop.h"
#include "filebrowser.h"
#include "cheat.h"
#include "menu.h"

int numcheats = 0;

/****************************************************************************
 * LoadCheats
 *
 * Loads cheat file from save buffer
 * Custom version of FCEU_LoadGameCheats()
 ***************************************************************************/

static int LoadCheats (int length)
{
	unsigned int addr;
	unsigned int val;
	unsigned int status;
	unsigned int type;
	unsigned int compare;
	int x;

	char *namebuf;
	int tc=0;

	char * linebreak = strtok((char *)savebuffer, "\n");

	while(linebreak != NULL)
	{
		char *tbuf=linebreak;
		int doc=0;

		addr=val=compare=status=type=0;

		if(tbuf[0]=='S')
		{
			tbuf++;
			type=1;
		}
		else type=0;

		if(tbuf[0]=='C')
		{
			tbuf++;
			doc=1;
		}

		if(tbuf[0]==':')
		{
			tbuf++;
			status=0;
		}
		else status=1;

		if(doc)
		{
			char *neo=&tbuf[4+2+2+1+1+1];
			if(sscanf(tbuf,"%04x%*[:]%02x%*[:]%02x",&addr,&val,&compare)!=3)
				continue;
			namebuf=(char *)malloc(strlen(neo)+1);
			strcpy(namebuf,neo);
		}
		else
		{
			char *neo=&tbuf[4+2+1+1];
			if(sscanf(tbuf,"%04x%*[:]%02x",&addr,&val)!=2)
				continue;
			namebuf=(char *)malloc(strlen(neo)+1);
			strcpy(namebuf,neo);
		}

		for(x=0;x<(int)strlen(namebuf);x++)
		{
			if(namebuf[x]==10 || namebuf[x]==13)
			{
				namebuf[x]=0;
				break;
			}
			else if(namebuf[x]<0x20) namebuf[x]=' ';
		}

		AddCheatEntry(namebuf,addr,val,doc?compare:-1,status,type);
		FCEUI_ToggleCheat(tc); // turn cheat off
		tc++;

		// find next line break
		linebreak = strtok(NULL, "\n");
	}
	RebuildSubCheats();
	return tc;
}

/****************************************************************************
 * SetupCheats
 *
 * Erases any prexisting cheats, loads cheats from a cheat file
 * Called when a ROM is first loaded
 ***************************************************************************/
void
SetupCheats()
{
	FCEU_PowerCheats();
	numcheats = 0;

	if(GameInfo->type == GIT_NSF)
		return;

	char filepath[1024];
	int offset = 0;

	int method = GCSettings.SaveMethod;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(SILENT);

	if(method == METHOD_AUTO)
		return;

	if(!MakeFilePath(filepath, FILE_CHEAT, method))
		return;

	AllocSaveBuffer();

	offset = LoadFile(filepath, method, SILENT);

	// load cheat file if present
	if(offset > 0)
		numcheats = LoadCheats(offset);

	FreeSaveBuffer ();
}

void OpenGameGenie()
{
	if (GENIEROM) // already loaded
	{
		geniestage=1;
		return;
	}

	int romSize = 0;
	char * tmpbuffer = (char *) memalign(32, 512 * 1024);
	if(!tmpbuffer)
		return;
	char filepath[1024];

	if (MakeFilePath(filepath, FILE_GGROM, GCSettings.LoadMethod))
	{
		romSize = LoadFile(tmpbuffer, filepath, 0, GCSettings.LoadMethod, SILENT);
	}

	if (romSize > 0)
	{
		GENIEROM=(uint8 *)malloc(4096+1024);

		if(tmpbuffer[0]==0x4E)  /* iNES ROM image */
		{
			memcpy(GENIEROM,tmpbuffer+16,4096);
			memcpy(GENIEROM+4096,tmpbuffer+16400,256);
		}
		else
		{
			memcpy(GENIEROM,tmpbuffer,4352);
		}

		/* Workaround for the FCE Ultra CHR page size only being 1KB */
		for(int x=0; x<4; x++)
			memcpy(GENIEROM+4096+(x<<8),GENIEROM+4096,256);

		geniestage=1;
	}
	else
	{
		free(GENIEROM);
		GENIEROM=0;
	}
	free(tmpbuffer);
}
