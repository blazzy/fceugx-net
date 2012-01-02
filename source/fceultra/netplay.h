/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * netplay.h
 *
 * Description:
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * midnak      12/02/2011  GUI and server use the same length for nickname
 * midnak      12/07/2011  ExecutionMode replaces FCEUnetplay
 ****************************************************************************/


#ifndef _NETPLAY_H_
#define _NETPLAY_H_

#define NETPLAY_MAX_NAME_LEN  21

struct NetplayClient {
	char name[NETPLAY_MAX_NAME_LEN];
	bool connected;
}  extern NetplayClients[4],
          *NetplayControllers[4],
          *NetplayThisClient;

enum ExecutionMode { OFFLINE, NETPLAY_HOST, NETPLAY_CLIENT };
extern ExecutionMode executionMode;


#define FCEUNPCMD_RESET   0x01
#define FCEUNPCMD_POWER   0x02

#define FCEUNPCMD_VSUNICOIN     0x07
#define FCEUNPCMD_VSUNIDIP0	0x08
#define FCEUNPCMD_FDSINSERTx	0x10
#define FCEUNPCMD_FDSINSERT	0x18
//#define FCEUNPCMD_FDSEJECT	0x19
#define FCEUNPCMD_FDSSELECT	0x1A

#define FCEUNPCMD_LOADSTATE     0x80

#define FCEUNPCMD_SAVESTATE     0x81 /* Sent from server to client. */
#define FCEUNPCMD_LOADCHEATS	0x82
#define FCEUNPCMD_TEXT		0x90

#define FCEUNPCMD_NEWCLIENT        0xA0
#define FCEUNPCMD_PICKUPCONTROLLER 0xA1
#define FCEUNPCMD_DROPCONTROLLER   0xA2
#define FCEUNPCMD_CLIENTDISCONNECT 0xA3
#define FCEUNPCMD_HALTEMULATION    0xA4
#define FCEUNPCMD_RESUMEEMULATION  0xA5

int InitNetplay(void);
void NetplayUpdate(uint8 *joyp);
void NetError(const char *error, ...);

int FCEUNET_SendCommand(uint8, uint32);
int FCEUNET_SendFile(uint8 cmd, char *);

#endif
