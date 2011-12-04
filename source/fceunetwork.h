/****************************************************************************
 * FCEUGX-Net
 * Nintendo Wii/Gamecube Port
 *
 * fceunetwork.h
 *
 * Description:
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * blazzy      11/25/2011  New file
 * midnak      12/02/2011  Netplay:  Added FCEUD_TellServerToggleReady(),
 *                         FCEUD_SendPlayerListToClients()
 ****************************************************************************/

#ifndef _FCEU_NETWORK_H_
#define _FCEU_NETWORK_H_

int FCEUD_NetworkConnect();

#define NETPLAY_MAX_NAME_LEN  20

/*
 * The host has sole authority over player ID assignment (Player 1, Player 2, etc.).
 * When a client clicks READY in the GUI (thereby toggling ready status on/off), it
 * sends a request to the host.  The server assigns player numbers in FIFO,
 * then updates all clients with the changes by sending the complete list of player
 * ID assignments to each client.  The clients parse this data, then feed it into
 * their GuiPlayerLists for display.  Until a response is received from the host,
 * no update to the client GUI will occur after clicking READY.
 *
 * It should be noted that receipt of this data can be completely unsolicited, so
 * clients need to be able to handle that.
 */
void FCEUD_SendPlayerListToClients();

bool FCEUD_TellServerToggleReady(char *name);

#endif
