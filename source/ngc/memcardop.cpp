/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * memcardop.c
 *
 * Memory Card routines
 ****************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include "fceugx.h"
#include "fceusupport.h"
#include "gcvideo.h"
#include "menu.h"
#include "preferences.h"
#include "filebrowser.h"
#include "fileop.h"
#include "images/saveicon.h"

static u8 * SysArea = NULL;
static char savecomments[2][32];
static u8 * verifybuffer = NULL;

/****************************************************************************
 * MountMC
 *
 * Mounts the memory card in the given slot.
 * Returns the result of the last attempted CARD_Mount command.
 ***************************************************************************/
static int MountMC(int slot, bool silent)
{
	int ret = -1;
	int tries = 0;

	// Initialize Card System
	SysArea = (u8 *)memalign(32, CARD_WORKAREA);
	memset (SysArea, 0, CARD_WORKAREA);
	CARD_Init ("FCEU", "00");

	// Mount the card
	while(tries < 10 && ret != 0)
	{
		EXI_ProbeReset();
		ret = CARD_Mount (slot, SysArea, NULL);
		VIDEO_WaitVSync();
		tries++;
	}

	if(ret != 0 && !silent)
	{
		if (slot == CARD_SLOTA)
			ErrorPrompt("Unable to mount Slot A Memory Card!");
		else
			ErrorPrompt("Unable to mount Slot B Memory Card!");
	}
	return ret;
}

/****************************************************************************
 * TestMC
 *
 * Checks to see if a card is in the card slot specified
 ***************************************************************************/
bool TestMC(int slot, bool silent)
{
	// Memory Cards do not work in Wii mode - disable
	#ifdef HW_RVL
	return false;
	#endif

	bool ret = false;

	// Try to mount the card
	if (MountMC(slot, silent) == 0)
	{
		// Mount successful!
		CARD_Unmount (slot);
		ret = true;
	}
	free(SysArea);
	return ret;
}

/****************************************************************************
 * ParseMCDirectory
 *
 * Parses a list of all files on the specified memory card
 ***************************************************************************/
int
ParseMCDirectory (int slot)
{
	card_dir CardDir;
	int CardError;
	int entryNum = 0;

	HaltDeviceThread();

	// Try to mount the card
	CardError = MountMC(slot, NOTSILENT);

	if (CardError == 0)
	{
		CardError = CARD_FindFirst (slot, &CardDir, TRUE);
		while (CardError != CARD_ERROR_NOFILE)
		{
			if(!AddBrowserEntry())
			{
				entryNum = 0;
				break;
			}

			strncpy(browserList[entryNum].filename, (char *)CardDir.filename, MAXJOLIET);
			StripExt(browserList[entryNum].displayname, browserList[entryNum].filename); // hide file extension
			browserList[entryNum].length = CardDir.filelen;

			entryNum++;

			CardError = CARD_FindNext (&CardDir);
		}
		CARD_Unmount(slot);
	}

	ResumeDeviceThread();

	// Sort the file list
	qsort(browserList, entryNum, sizeof(BROWSERENTRY), FileSortCallback);

	CancelAction();

	browser.numEntries = entryNum;
	return entryNum;
}

/****************************************************************************
 * Verify Memory Card file against buffer
 ***************************************************************************/
static int
VerifyMCFile (char *buf, int slot, char *filename, int datasize)
{
	card_file CardFile;
	int CardError;
	unsigned int blocks;
	unsigned int SectorSize;
	int bytesleft = 0;
	int bytesread = 0;

	verifybuffer = (u8 *)memalign(32, 262144);
	memset (verifybuffer, 0, 262144);

	// Get Sector Size
	CARD_GetSectorSize (slot, &SectorSize);

	memset (&CardFile, 0, sizeof (CardFile));
	CardError = CARD_Open (slot, filename, &CardFile);

	if(CardError)
	{
		ErrorPrompt("Unable to open file!");
	}
	else
	{
		blocks = CardFile.len;

		if (blocks < SectorSize)
			blocks = SectorSize;

		if (blocks % SectorSize)
			blocks += SectorSize;

		if (blocks > (unsigned int)datasize)
			blocks = datasize;

		bytesleft = blocks;
		bytesread = 0;
		while (bytesleft > 0)
		{
			CardError = CARD_Read (&CardFile, verifybuffer, SectorSize, bytesread);
			if (CardError || memcmp (buf + bytesread, verifybuffer, (unsigned int)bytesleft < SectorSize ? bytesleft : SectorSize) )
			{
				bytesread = 0;
				ErrorPrompt("File integrity could not be verified!");
				break;
			}

			bytesleft -= SectorSize;
			bytesread += SectorSize;
			ShowProgress ("Verifying...", bytesread, blocks);
		}
		CARD_Close (&CardFile);
		CancelAction();
	}
	free(verifybuffer);
	return bytesread;
}

/****************************************************************************
 * LoadMCFile
 * Load savebuffer from Memory Card file
 ***************************************************************************/
int
LoadMCFile (char *buf, int slot, char *filename, bool silent)
{
	card_file CardFile;
	int CardError;
	unsigned int blocks;
	unsigned int SectorSize;
	int bytesleft = 0;
	int bytesread = 0;

	// Try to mount the card
	CardError = MountMC(slot, NOTSILENT);

	if (CardError == 0)
	{
		// Get Sector Size
		CARD_GetSectorSize (slot, &SectorSize);

		memset (&CardFile, 0, sizeof (CardFile));
		CardError = CARD_Open (slot, filename, &CardFile);

		if(CardError)
		{
			if(!silent)
				ErrorPrompt("Unable to open file!");
		}
		else
		{
			blocks = CardFile.len;

			if (blocks < SectorSize)
				blocks = SectorSize;

			if (blocks % SectorSize)
				blocks += SectorSize;

			bytesleft = blocks;
			bytesread = 0;
			while (bytesleft > 0)
			{
				CardError = CARD_Read (&CardFile, buf + bytesread, SectorSize, bytesread);

				if(CardError)
				{
					ErrorPrompt("Error loading file!");
					bytesread = 0;
					break;
				}

				bytesleft -= SectorSize;
				bytesread += SectorSize;
				ShowProgress ("Loading...", bytesread, blocks);
			}
			CARD_Close (&CardFile);
			CancelAction();
		}
		CARD_Unmount(slot);
	}

	// discard save icon and comments
	memmove(buf, buf+sizeof(saveicon)+64, bytesread);
	bytesread -= (sizeof(saveicon)+64);

	free(SysArea);
	return bytesread;
}

/****************************************************************************
 * SaveMCFile
 * Write savebuffer to Memory Card file
 ***************************************************************************/
int
SaveMCFile (char *buf, int slot, char *filename, int datasize, bool silent)
{
	card_file CardFile;
	card_stat CardStatus;
	int CardError;
	unsigned int blocks;
	unsigned int SectorSize;
	int byteswritten = 0;
	int bytesleft = 0;

	if(datasize <= 0)
		return 0;

	// add save icon and comments
	memmove(buf+sizeof(saveicon)+64, buf, datasize);
	memcpy(buf, saveicon, sizeof(saveicon));
	memcpy(buf+sizeof(saveicon), savecomments, 64);
	datasize += (sizeof(saveicon)+64);

	// Try to mount the card
	CardError = MountMC(slot, NOTSILENT);

	if (CardError == 0)
	{
		// Get Sector Size
		CARD_GetSectorSize (slot, &SectorSize);

		// Calculate number of blocks required
		blocks = (datasize / SectorSize) * SectorSize;
		if (datasize % SectorSize)
			blocks += SectorSize;

		// Delete existing file (if present)
		memset(&CardStatus, 0, sizeof(card_stat));
		CardError = CARD_Open (slot, filename, &CardFile);

		if(CardError == 0)
		{
			CARD_Close (&CardFile);
			CardError = CARD_Delete(slot, filename);
			if (CardError)
			{
				ErrorPrompt("Unable to delete existing file!");
				goto done;
			}
		}

		// Create new file
		memset(&CardStatus, 0, sizeof(card_stat));
		CardError = CARD_Create (slot, filename, blocks, &CardFile);
		if (CardError)
		{
			if (CardError == CARD_ERROR_INSSPACE)
				ErrorPrompt("Insufficient space to create file!");
			else
				ErrorPrompt("Unable to create card file!");
			goto done;
		}

		// Now, have an open file handle, ready to send out the data
		CARD_GetStatus (slot, CardFile.filenum, &CardStatus);
		CardStatus.icon_addr = 0x0;
		CardStatus.icon_fmt = 2;
		CardStatus.icon_speed = 1;
		CardStatus.comment_addr = 2048;
		CARD_SetStatus (slot, CardFile.filenum, &CardStatus);

		bytesleft = blocks;

		while (bytesleft > 0)
		{
			CardError =
				CARD_Write (&CardFile, buf + byteswritten, SectorSize, byteswritten);

			if(CardError)
			{
				ErrorPrompt("Error writing file!");
				byteswritten = 0;
				break;
			}

			bytesleft -= SectorSize;
			byteswritten += SectorSize;

			ShowProgress ("Saving...", byteswritten, blocks);
		}
		CARD_Close (&CardFile);
		CancelAction();

		if (byteswritten > 0 && GCSettings.VerifySaves)
		{
			// Verify the written file
			if (!VerifyMCFile (buf, slot, filename, byteswritten) )
				byteswritten = 0;
		}
done:
		CARD_Unmount (slot);
	}

	free(SysArea);
	return byteswritten;
}

void SetMCSaveComments(char comments[2][32])
{
	memcpy(savecomments, comments, 64);
}
