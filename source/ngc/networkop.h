/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric December 2008
 *
 * networkop.h
 *
 * Network and SMB support routines
 ****************************************************************************/

#ifndef _NETWORKOP_H_
#define _NETWORKOP_H_

void UpdateCheck();
bool DownloadUpdate();
void InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
void CloseShare();

extern bool updateFound;

#endif
