/* FCE Ultra - NES/Famicom Emulator
*
* Copyright notice for this file:
*  Copyright (C) 2002 Xodnizel
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h> //mbg merge 7/17/06 removed

#include <zlib.h>

#include "types.h"
#include "file.h"
#include "utils/endian.h"
#include "netplay.h"
#include "fceu.h"
#include "state.h"
#include "cheat.h"
#include "input.h"
#include "driver.h"
#include "utils/memory.h"

ExecutionMode executionMode = OFFLINE;

NetplayClient *NetplayControllers[4], NetplayClients[4], *NetplayThisClient;

static uint8 netjoy[4]; // Controller cache.
static int numlocal;
static int netdivisor;
static int netdcount;

//NetError should only be called after a FCEUD_*Data function returned 0, in the function
//that called FCEUD_*Data, to prevent it from being called twice.

static void NetError(void)
{
	NetError("Network error/connection lost!");
}

void NetError(const char *error, ...)
{
	va_list arg;

	va_start(arg, error);
		int length = vsnprintf(0, 0, error, arg);
		char str[length + 1];
		vsprintf(str, error, arg);
	va_end(arg);

	FCEU_DispMessage(str, 0);
	FCEUD_NetworkClose();
	FCEUD_PrintError(str);
}


void FCEUI_NetplayStop(void)
{
	if(executionMode != OFFLINE)
	{
		executionMode = OFFLINE;

		FCEU_FlushGameCheats(0,1);  //Don't save netplay cheats.
		FCEU_LoadGameCheats(0);    //Reload our original cheats.
	}
	else puts("Check your code!");
}

int FCEUI_NetplayStart(int nlocal, int divisor)
{
	FCEU_FlushGameCheats(0, 0);  //Save our pre-netplay cheats.
	FCEU_LoadGameCheats(0);    // Load them again, for pre-multiplayer action.

	// We know to set this to client instead of server because FCEUI_NetplayStart() is only
	// called by FCEUD_NetworkConnect(), and that's only a client function.  Verified by
	// looking at the FCEUD_NetworkConnect() of FCEUX SDL. If FCEUI_NetplayStart() should
	// ever be called by the host (the function name is generic enough to suggest someone
	// might repurpose it in the future), we'll have a problem and will have to resort to
	// having two global netplay variables:  int FCEUnetplay and its ExecutionMode
	// replacement.
	executionMode = NETPLAY_CLIENT;

	memset(netjoy,0,sizeof(netjoy));
	numlocal = nlocal;
	netdivisor = divisor;
	netdcount = 0;

	for (int i = 0; i < 4; ++i) {
		NetplayControllers[i] = 0;
		NetplayClients[i].connected = 0;
	}
	NetplayThisClient = 0;

	return(1);
}

int FCEUNET_SendCommand(uint8 cmd, uint32 len)
{
	//mbg merge 7/17/06 changed to alloca
	//uint8 buf[numlocal + 1 + 4];
	uint8 *buf = (uint8*)alloca(numlocal+1+4);


	buf[0] = 0xFF;
	FCEU_en32lsb(&buf[numlocal], len);
	buf[numlocal + 4] = cmd;
	if(!FCEUD_SendData(buf,numlocal + 1 + 4)) 
	{
		NetError();
		return(0);
	}
	return(1);
}

void FCEUI_NetplayText(uint8 *text)
{
	uint32 len;

	len = strlen((char*)text); //mbg merge 7/17/06 added cast

	if(!FCEUNET_SendCommand(FCEUNPCMD_TEXT,len)) return;

	if(!FCEUD_SendData(text,len))
		NetError();
}

int FCEUNET_SendFile(uint8 cmd, char *fn)
{
	uint32 len;
	uLongf clen;
	char *buf, *cbuf;
	FILE *fp;
	struct stat sb;

	if(!(fp=FCEUD_UTF8fopen(fn,"rb"))) return(0);

	fstat(fileno(fp),&sb);
	len = sb.st_size;
	buf = (char*)FCEU_dmalloc(len); //mbg merge 7/17/06 added cast
	fread(buf, 1, len, fp);
	fclose(fp);

	cbuf = (char*)FCEU_dmalloc(4 + len + len / 1000 + 12); //mbg merge 7/17/06 added cast
	FCEU_en32lsb((uint8*)cbuf, len); //mbg merge 7/17/06 added cast
	compress2((uint8*)cbuf + 4, &clen, (uint8*)buf, len, 7); //mbg merge 7/17/06 added casts
	free(buf);

	//printf("Sending file: %s, %d, %d\n",fn,len,clen);

	len = clen + 4;

	if(!FCEUNET_SendCommand(cmd,len))
	{
		free(cbuf);
		return(0);
	}
	if(!FCEUD_SendData(cbuf, len))
	{
		NetError();
		free(cbuf);
		return(0);
	}
	free(cbuf);

	return(1);
}

static FILE *FetchFile(uint32 remlen)
{
	uint32 clen = remlen;
	char *cbuf;
	uLongf len;
	char *buf;  
	FILE *fp;

	if(clen > 500000)  // Sanity check
	{
		NetError();
		return(0);
	}

	//printf("Receiving file: %d...\n",clen);
	if((fp = tmpfile()))
	{
		cbuf = (char *)FCEU_dmalloc(clen); //mbg merge 7/17/06 added cast
		if(!FCEUD_RecvData(cbuf, clen))
		{
			NetError();
			fclose(fp);
			free(cbuf);
			return(0);
		}

		len = FCEU_de32lsb((uint8*)cbuf); //mbg merge 7/17/06 added cast
		if(len > 500000)    // Another sanity check
		{
			NetError();
			fclose(fp);
			free(cbuf);
			return(0);
		}
		buf = (char *)FCEU_dmalloc(len); //mbg merge 7/17/06 added cast
		uncompress((uint8*)buf, &len, (uint8*)cbuf + 4, clen - 4); //mbg merge 7/17/06 added casts

		fwrite(buf, 1, len, fp);
		free(buf);
		fseek(fp, 0, SEEK_SET);
		return(fp);
	}
	return(0);
}

void NetplayUpdate(uint8 *joyp)
{
	static uint8 buf[5];  /* 4 play states, + command/extra byte */
	static uint8 joypb[4];

	memcpy(joypb,joyp,4);

	/* This shouldn't happen, but just in case.  0xFF is used as a command escape elsewhere. */
	if(joypb[0] == 0xFF) 
		joypb[0] = 0xF;
	if(!netdcount)
		if(!FCEUD_SendData(joypb,numlocal))
		{
			NetError();
			return;
		}

	if(!netdcount)
	{
		do
		{
			if(!FCEUD_RecvData(buf,5))
			{
				NetError();
				return;
			}

			switch(buf[4])
			{
			default: FCEU_DoSimpleCommand(buf[4]);break;
			case FCEUNPCMD_TEXT:
				{
					uint8 *tbuf;
					uint32 len = FCEU_de32lsb(buf);

					if(len > 100000)  // Insanity check!
					{
						NetError();
						return;
					}
					tbuf = (uint8*)malloc(len + 1); //mbg merge 7/17/06 added cast
					tbuf[len] = 0;
					if(!FCEUD_RecvData(tbuf, len))
					{
						NetError();
						free(tbuf);
						return;
					}
					FCEUD_NetplayText(tbuf);
					free(tbuf);
				}
				break;
			case FCEUNPCMD_NEWCLIENT:
				{
					uint8 client_buf[1 + NETPLAY_MAX_NAME_LEN];
					if(!FCEUD_RecvData(client_buf, 1 + NETPLAY_MAX_NAME_LEN) || client_buf[0] > 3)
					{
						NetError("Connection lost. New client error %i", client_buf[0]);
						return;
					}

					client_buf[1 + NETPLAY_MAX_NAME_LEN - 1] = '\0';
					strncpy(NetplayClients[client_buf[0]].name, (char*)&client_buf[1], NETPLAY_MAX_NAME_LEN);
					NetplayClients[client_buf[0]].connected = true;

					if (!NetplayThisClient)
						NetplayThisClient = &NetplayClients[client_buf[0]];

					FCEUD_NetplayClient(client_buf[0], &client_buf[1]);
				}
				break;
			case FCEUNPCMD_PICKUPCONTROLLER:
				{
					uint8 buf[2];
					if(!FCEUD_RecvData(buf, 2) || buf[0] > 3 || buf[1] > 3)
					{
						NetError("Connection lost. Pick up controller error %i %i", buf[0], buf[1]);
						return;
					}
					NetplayControllers[buf[1]] = &NetplayClients[buf[0]];
					FCEUD_NetplayPickupController(buf[0], buf[1]);
				}
				break;

			case FCEUNPCMD_DROPCONTROLLER:
				{
					uint8 buf[1];
					if(!FCEUD_RecvData(buf, 1) || buf[0] > 3)
					{
						NetError("Connection lost. Drop controller error %i", buf[0]);
						return;
					}
					NetplayControllers[buf[0]] = 0;
					FCEUD_NetplayDropController(buf[0]);
				}
				break;

			case FCEUNPCMD_CLIENTDISCONNECT:
				{
					uint8 buf[1];
					if(!FCEUD_RecvData(buf, 1) || buf[0] > 3)
					{
						NetError("Connection lost. Drop controller error %i", buf[0]);
						return;
					}

					for (int i = 0; i < 4; ++i) {
						if (NetplayControllers[i] == &NetplayClients[buf[0]]) {
							NetplayControllers[i] = 0;
						}
					}

					NetplayClients[buf[0]].connected = false;
					FCEUD_NetplayClientDisconnect(buf[0]);
				}
				break;
			case FCEUNPCMD_SAVESTATE:	
				{
					//mbg todo netplay
					//char *fn;
					//FILE *fp;

					////Send the cheats first, then the save state, since
					////there might be a frame or two in between the two sendfile
					////commands on the server side.

					//fn = strdup(FCEU_MakeFName(FCEUMKF_CHEAT,0,0).c_str());

					////why??????
					////if(!
					//	FCEUNET_SendFile(FCEUNPCMD_LOADCHEATS,fn);
					//// {
					////  free(fn);
					////  return;
					//// }

					//free(fn);
					//if(executionMode == OFFLINE) return;

					//fn = strdup(FCEU_MakeFName(FCEUMKF_NPTEMP,0,0).c_str());
					//fp = fopen(fn, "wb");
					//if(FCEUSS_SaveFP(fp,Z_BEST_COMPRESSION))
					//{
					//	fclose(fp);
					//	if(!FCEUNET_SendFile(FCEUNPCMD_LOADSTATE, fn))
					//	{
					//		unlink(fn);
					//		free(fn);
					//		return;
					//	}
					//	unlink(fn);
					//	free(fn);
					//}
					//else
					//{
					//	fclose(fp);
					//	FCEUD_PrintError("File error.  (K)ill, (M)aim, (D)estroy?  Now!");
					//	unlink(fn);
					//	free(fn);
					//	return;
					//}

				}
				break;
			case FCEUNPCMD_LOADCHEATS:
				{
					FILE *fp = FetchFile(FCEU_de32lsb(buf));
					if(!fp) return;
					FCEU_FlushGameCheats(0,1);
					FCEU_LoadGameCheats(fp);
				}
				break;
				//mbg 6/16/08 - netplay doesnt work right now anyway
				/*case FCEUNPCMD_LOADSTATE:
				{
				FILE *fp = FetchFile(FCEU_de32lsb(buf));
				if(!fp) return;
				if(FCEUSS_LoadFP(fp,SSLOADPARAM_BACKUP))
			 {
			 fclose(fp);				
			 FCEU_DispMessage("Remote state loaded.",0);
			 } else FCEUD_PrintError("File error.  (K)ill, (M)aim, (D)estroy?");
			 }
			 break;*/
			}
		} while(buf[4]);

		netdcount=(netdcount+1)%netdivisor;

		memcpy(netjoy,buf,4);
		*(uint32 *)joyp=*(uint32 *)netjoy;
	}
}
