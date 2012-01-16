/****************************************************************************
 * FCE Ultra
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * preferences.h
 *
 * Preferences save/load preferences utilities
 ****************************************************************************/

#define DEFAULT_NETPLAY_PORT   "4046"

void FixInvalidSettings();
void DefaultSettings();
bool SavePrefs (bool silent);
bool LoadPrefs ();
