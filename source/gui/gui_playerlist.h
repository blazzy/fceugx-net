/****************************************************************************
 * FCE Ultra GX
 * Nintendo Wii/Gamecube Port
 *
 * gui_playerlist.h
 *
 * Description:  Lists connected players in netplay
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * midnak      12/13/2011  New file (moved out of gui.h)
 ****************************************************************************/

#include "gui.h"

#ifndef _GUI_PLAYERLIST_H_
#define _GUI_PLAYERLIST_H_

#define PLAYER_LIST_SUCCESS            0
#define PLAYER_LIST_ERR_NO_DATA       -1
#define PLAYER_LIST_ERR_STATUS_IND    -2
#define PLAYER_LIST_ERR_STATUS_DELIM  -3
#define PLAYER_LIST_ERR_RECORD_LEN    -4
#define PLAYER_LIST_ERR_FORMATTING    -5
#define PLAYER_LIST_ERR_OUT_OF_MEM    -6
#define PLAYER_LIST_ERR_LIST_FULL     -7
#define PLAYER_LIST_ERR_UNKNOWN       -8

struct Player
{
	char *name;
	bool ready;
};

class GuiPlayerList : public GuiWindow
{
	public:
		GuiPlayerList(int w, int h);
		~GuiPlayerList();
		void SetFocus(int f);
		void TriggerUpdate();
		void Update(GuiTrigger *t);

		uint8  GetPlayerCount();
		int BuildPlayerList(const char *playerInfo);
		int AddPlayer(Player player);
		void Clear();
		int  GetPlayerNumber(char *playerName);
		bool ToggleReady();
		bool IsPlayerReady(int playerNum);
		bool IsPlayerReady(char *playerName);
		bool IsEveryoneReady();

	private:
		GuiButton *rowButton[MAX_PLAYER_LIST_SIZE];

		GuiText *titleTxt,
				*txtAllPlayersReady,
				*rowText[MAX_PLAYER_LIST_SIZE];

		GuiImageData *imgDataPlayer1Ready,
					 *imgDataPlayer2Ready,
					 *imgDataPlayer3Ready,
					 *imgDataPlayer4Ready;

        GuiImage *imgPlayerReady[MAX_PLAYER_LIST_SIZE];

		GuiImageData *imgDataMainWindow;
		GuiImage *imgMainWindow;

		GuiImageData *imgDataSelectionEntry;
		GuiImage *imgRowSelected[MAX_PLAYER_LIST_SIZE];

		GuiSound *btnSoundClick;

		GuiTrigger *trigA;
		GuiTrigger *trig2;

		const GXColor *colorNotReady;
		const GXColor *colorReady;

		int selectedItem;
		int currIdx;
		bool listChanged;
};

#endif /* _GUI_PLAYERLIST_H_ */
