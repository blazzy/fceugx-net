/****************************************************************************
 * FCE Ultra GX
 * Nintendo Wii/Gamecube Port
 *
 * gui_playerlist.cpp
 *
 * Description:  Lists connected players in netplay
 *
 * TODO:
 *     1.  Players can only be moused over/clicked once - after that, they
 *         become disabled.  Might be the update() method.  Don't know if
 *         we even need that functionality, but I could see a purpose arising in
 *         the future.  At least get it working, then maybe disable it with
 *         a setter.
 *     2.  Delete ToggleReady() once we're networked.
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * midnak      11/29/2011  New file (gutted GuiFileBrowser)
 ****************************************************************************/

//#include <debug.h>        // USB Gecko

#include "fceunetwork.h"
#include "gui_playerlist.h"
#include "menu.h"         // Error prompts
#include "../fceultra/utils/xstring.h"      // str_strip()


GuiPlayerList::GuiPlayerList(const int w, const int h)
{
	//DEBUG_Init(GDBSTUB_DEVICE_USB, 1);  // USB Gecko
	//_break();							// USB Gecko

	width = w;
	height = h;
	currIdx = -1;
	selectedItem = 0;
	selectable = true;
	listChanged = false; // Don't trigger a list update unless something changes (add/delete/ready/unready).  Setting this to true will prevent conrollers from ever being registered.
	focus = 0; // allow focus
	interactive = true;

	trigA = new GuiTrigger;
	trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	trig2 = new GuiTrigger;
	trig2->SetSimpleTrigger(-1, WPAD_BUTTON_2, 0);

	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);

	imgDataMainWindow = new GuiImageData(bg_player_list_png);

	imgMainWindow = new GuiImage(imgDataMainWindow);
	imgMainWindow->SetParent(this);
	imgMainWindow->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	imgMainWindow->SetPosition(0, 4);

	imgDataSelectionEntry = new GuiImageData(bg_player_list_entry_png);

	titleTxt = new GuiText("PLAYERS", 25, (GXColor){0, 0, 0, 255});
	titleTxt->SetParent(imgMainWindow);
	titleTxt->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt->SetPosition(0, 10);

	txtAllPlayersReady = new GuiText("ALL PLAYERS READY", 18, (GXColor{0,0,0,255}));
	txtAllPlayersReady->SetParent(imgMainWindow);
	txtAllPlayersReady->SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	txtAllPlayersReady->SetPosition(0, -10);

	imgDataPlayer1Ready = new GuiImageData(player1_ready_png);
	imgDataPlayer2Ready = new GuiImageData(player2_ready_png);
	imgDataPlayer3Ready = new GuiImageData(player3_ready_png);
	imgDataPlayer4Ready = new GuiImageData(player4_ready_png);

	imgPlayerReady[0] = new GuiImage(imgDataPlayer1Ready);
	imgPlayerReady[1] = new GuiImage(imgDataPlayer2Ready);
	imgPlayerReady[2] = new GuiImage(imgDataPlayer3Ready);
	imgPlayerReady[3] = new GuiImage(imgDataPlayer4Ready);;

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		imgPlayerReady[i]->SetPosition(5,0);
		imgRowSelected[i] = NULL;
		rowText[i] = NULL;
		rowButton[i] = NULL;
	}

	colorNotReady = new (GXColor){0, 0, 0, 100};
	colorReady = new GXColor[4] {
									(GXColor){61, 89, 144, 255},  // Player 1 ready (blue)
									(GXColor){144, 60, 60, 255},  // Player 2 ready (red)
									(GXColor){60, 144, 60, 255},  // Player 3 ready (green)
									(GXColor){148, 147, 65, 255}  // Player 4 ready (yellow)
								 };

	Append(imgMainWindow);
	Append(titleTxt);
	Append(txtAllPlayersReady);
}

GuiPlayerList::~GuiPlayerList()
{
	Remove(imgMainWindow);
	Remove(titleTxt);
	Remove(txtAllPlayersReady);

	delete titleTxt;
	delete txtAllPlayersReady;

	delete imgDataPlayer1Ready;
	delete imgDataPlayer2Ready;
	delete imgDataPlayer3Ready;
	delete imgDataPlayer4Ready;

	delete imgDataMainWindow;
	delete imgMainWindow;
	delete imgDataSelectionEntry;

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(rowText[i])
		{
			delete rowText[i];  // player1Text, player2Text, etc
		}

		if(imgPlayerReady[i])
		{
			delete imgPlayerReady[i];  // imgPlayer1Ready, imagePlayer2Ready, etc
		}

		if(imgRowSelected[i])
		{
			delete imgRowSelected[i];
		}

		if(rowButton[i])
		{
			Remove(rowButton[i]);
			delete rowButton[i];
		}
	}

	delete btnSoundClick;

	delete trigA;
	delete trig2;

	delete colorNotReady;
	delete colorReady;
}

int GuiPlayerList::GetState()
{
	// Please hold the particle physics jokes.

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(rowButton[i] != NULL && rowButton[i]->GetState() == STATE_CLICKED)
		{
			state = STATE_CLICKED;
			break;
		}
	}

	return state;
}

void GuiPlayerList::ResetState()
{
	if(state != STATE_DISABLED)
	{
		state = STATE_DEFAULT;
		stateChan = -1;
	}

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(rowButton[i] != NULL)
		{
			rowButton[i]->ResetState();
		}
	}
}

void GuiPlayerList::SetFocus(const int f)
{
	focus = f;

	for(int i=0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(rowButton[i] != NULL)
		{
			rowButton[i]->ResetState();
		}
	}

	if(f == 1 && rowButton[selectedItem] != NULL)
	{
		rowButton[selectedItem]->SetState(STATE_SELECTED);
	}
}

uint8 GuiPlayerList::GetPlayerCount()
{
	return currIdx < 0 ? 0 : currIdx + 1;
}

int GuiPlayerList::BuildPlayerList(const char *playerInfo)
{
	const char *RECORD_SEPARATOR = "|";

	int len = 0;
	char name[NETPLAY_MAX_NAME_LEN];
	int controller = 0;

	int status = PLAYER_LIST_SUCCESS,
		trailingStatus = PLAYER_LIST_SUCCESS;

	char *edible = NULL;

	if(playerInfo == NULL)
	{
		return PLAYER_LIST_ERR_NO_DATA;
	}
	else if(strlen(playerInfo) == 0)
	{
		return PLAYER_LIST_ERR_NO_DATA;
	}

	edible = strdup(playerInfo);

	if(edible != NULL)
	{
		char *token = NULL;

		Clear();
		token = strtok(edible, RECORD_SEPARATOR);

		while(token != NULL)
		{
			len = strlen(token);
			name[0] = '\0';

			//printf("\n\ntoken to parse:  (%s)\n", token);

			if(len == NETPLAY_MAX_NAME_LEN + 1)  // 21st space + 1 = colon plus controller indicator
			{
				if(token[NETPLAY_MAX_NAME_LEN - 1] == ':')
				{
					if(token[NETPLAY_MAX_NAME_LEN] >= '0' && token[NETPLAY_MAX_NAME_LEN] <= '4')
					{
						snprintf(name, NETPLAY_MAX_NAME_LEN, token);
						controller = token[NETPLAY_MAX_NAME_LEN] - '0';

						//printf("%s is not ready\n", name);
					}
					else
					{
						status = PLAYER_LIST_ERR_STATUS_IND;
					}
				}
				else
				{
					status = PLAYER_LIST_ERR_STATUS_DELIM;
				}
			}
			else
			{
				status = PLAYER_LIST_ERR_RECORD_LEN;
			}

			if(status == PLAYER_LIST_SUCCESS)
			{
				if(str_strip(name, STRIP_SP | STRIP_TAB | STRIP_CR | STRIP_LF) < 0)
				{
					status = PLAYER_LIST_ERR_FORMATTING;
				}
				else
				{
					const int addPlayerStatus = AddPlayer(Player{name, controller});

					if(addPlayerStatus != PLAYER_LIST_SUCCESS)
					{
						status = addPlayerStatus;
					}
				}
			}

			if(status != PLAYER_LIST_SUCCESS)
			{
				trailingStatus = status;
			}
			status = PLAYER_LIST_SUCCESS;

			token = strtok(NULL, RECORD_SEPARATOR);
		}

		free(edible);
	}
	else
	{
		status = PLAYER_LIST_ERR_OUT_OF_MEM;
	}

	return trailingStatus;
}

int GuiPlayerList::AddPlayer(const Player player)
{
	uint8 newIdx = GetPlayerCount();

	if(newIdx < MAX_PLAYER_LIST_SIZE)
	{
		char truncName[MAX_PLAYER_NAME_LEN+1];
		snprintf(truncName, MAX_PLAYER_NAME_LEN+1, "%s", player.name);

		bool alloc = (rowText[newIdx] = new GuiText(truncName, 25, player.controller ? colorReady[player.controller-1] : *colorNotReady)) &&
					 (imgRowSelected[newIdx] = new GuiImage(imgDataSelectionEntry)) &&
					 (rowButton[newIdx] = new GuiButton(this->GetWidth(), imgDataSelectionEntry->GetHeight()));

		if(!alloc)
		{
			if(rowText[newIdx]) delete rowText[newIdx];
			if(imgRowSelected[newIdx]) delete imgRowSelected[newIdx];
			if(rowButton[newIdx]) delete rowButton[newIdx];

			return PLAYER_LIST_ERR_OUT_OF_MEM;
		}

		rowText[newIdx]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		rowText[newIdx]->SetPosition(40,0);
		rowText[newIdx]->SetMaxWidth(105);

		imgRowSelected[newIdx]->SetPosition(2,-3);

		rowButton[newIdx]->SetParent(this);
		rowButton[newIdx]->SetLabel(rowText[newIdx]);
		rowButton[newIdx]->SetPosition(2, (imgRowSelected[newIdx]->GetHeight() * newIdx) + 50);
		rowButton[newIdx]->SetTrigger(trigA);
		rowButton[newIdx]->SetTrigger(trig2);
		rowButton[newIdx]->SetSoundClick(btnSoundClick);

		if(player.controller)
		{
			rowButton[newIdx]->SetIcon(imgPlayerReady[player.controller - 1]);
		}

		rowButton[newIdx]->SetClickable(interactive);
		rowButton[newIdx]->SetSelectable(interactive);
		rowButton[newIdx]->SetImageOver(interactive ? imgRowSelected[newIdx] : NULL);

		Append(rowButton[newIdx]);

		currIdx = newIdx;
		listChanged = true;

		return PLAYER_LIST_SUCCESS;
	}
	else
	{
		return PLAYER_LIST_ERR_LIST_FULL;
	}

	return PLAYER_LIST_ERR_UNKNOWN;
}

void GuiPlayerList::Clear()
{
	for(int i = 0; i <= currIdx; i++)
	{
		if(rowButton[i] != NULL)
		{
			Remove(rowButton[i]);
			delete rowButton[i];
			rowButton[i] = NULL;
		}

		if(rowText[i] != NULL)
		{
			delete rowText[i];
			rowText[i] = NULL;
		}

		if(imgRowSelected[i] != NULL)
		{
			delete imgRowSelected[i];
			imgRowSelected[i] = NULL;
		}
	}

	txtAllPlayersReady->SetVisible(false);

	currIdx = -1;
	listChanged = true;
}

int GuiPlayerList::GetClickedIdx()
{
	int idx = -1;

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(rowButton[i] != NULL && rowButton[i]->GetState() == STATE_CLICKED)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

char *GuiPlayerList::GetPlayerName(const uint idx)
{
	if(idx < MAX_PLAYER_LIST_SIZE)
	{
		if(rowText[idx] != NULL)
		{
			return rowText[idx]->ToString();
		}
	}

	return NULL;
}

int GuiPlayerList::GetPlayerNumber(const char *playerName)
{
	int idx = -1;

	if(playerName != NULL)
	{
		for(int i = 0; i <= currIdx; i++)
		{
			if(rowText[i] != NULL)
			{
				char *textCopy = rowText[i]->ToString();

				if(textCopy != NULL)
				{
					if(strcmp(playerName, textCopy) == 0)
					{
						idx = i;
						free(textCopy);
						break;
					}

					free(textCopy);
				}
			}
		}
	}

	return idx;
}

bool GuiPlayerList::IsInteractive()
{
	return interactive;
}

void GuiPlayerList::SetInteractive(const bool allowInteract)
{
	interactive = allowInteract;

	for(uint i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(rowButton[i] != NULL)
		{
			rowButton[i]->SetClickable(allowInteract);
			rowButton[i]->SetSelectable(allowInteract);
			rowButton[i]->SetImageOver(allowInteract ? imgRowSelected[i] : NULL);
		}
	}
}

// Note:  playerNum is 0-indexed
bool GuiPlayerList::IsPlayerReady(const int playerNum)
{
	bool ready = true;

	if(playerNum < 0 || playerNum > currIdx
	|| rowButton[playerNum] == NULL || rowButton[playerNum]->GetIcon() == NULL)
	{
		ready = false;
	}

	return ready;
}

bool GuiPlayerList::IsPlayerReady(const char *playerName)
{
	return IsPlayerReady(GetPlayerNumber(playerName));
}

bool GuiPlayerList::IsEveryoneReady()
{
	bool ready = true;

	if(currIdx < 0)
	{
		ready = false;
	}
	else
	{
		for(int i = 0; i <= currIdx; i++)
		{
			if(!IsPlayerReady(i))
			{
				ready = false;
				break;
			}
		}
	}

	return ready;
}

void GuiPlayerList::Update(GuiTrigger * t)
{
	if(GetState() == STATE_DISABLED || !t || !IsVisible())
	{
		return;
	}

	for(int i = 0; i <= currIdx; i++)
	{
		if(rowButton[i] == NULL)
		{
			continue;
		}

		if(listChanged)
		{
			if(rowButton[i]->GetState() == STATE_DISABLED)
			{
				rowButton[i]->SetState(STATE_DEFAULT);
			}
		}

		if(i != selectedItem && rowButton[i]->GetState() == STATE_SELECTED)
		{
			rowButton[i]->ResetState();
		}
		else if(focus && i == selectedItem && rowButton[i]->GetState() == STATE_DEFAULT)
		{
			rowButton[selectedItem]->SetState(STATE_SELECTED, t->chan);
		}

		int currChan = t->chan;

		if(t->wpad->ir.valid && !rowButton[i]->IsInside(t->wpad->ir.x, t->wpad->ir.y))
		{
			t->chan = -1;
		}

		rowButton[i]->Update(t);
		t->chan = currChan;

		// Store the index of the row with focus
		if(rowButton[i]->GetState() == STATE_SELECTED)
		{
			selectedItem = i;
		}

		// If the width of the row with focus is too small to display the
		// entire text, scroll it.  Stop scrolling text in the other rows.
		if(selectedItem == i)
		{
			rowText[i]->SetScroll(SCROLL_HORIZONTAL);
		}
		else
		{
			rowText[i]->SetScroll(SCROLL_NONE);
		}
	}

	focus = false;

	if(updateCB != NULL && listChanged)
	{
		txtAllPlayersReady->SetVisible(IsEveryoneReady());
		listChanged = false;
		updateCB(this);
	}
	else
	{
		listChanged = false;
	}
}
