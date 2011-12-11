/****************************************************************************
 * FCE Ultra GX
 * Nintendo Wii/Gamecube Port
 *
 * gui_playerlist.cpp
 *
 * Description:  Lists connected players in netplay
 *
 * TODO:
 *     1.  ToggleReady():
 *             - Make this method aware of whether it's running as client or
 *               host (no need to invoke TellServer method if server)
 *             - Implement FCEUD_TellServerToggleReady()
 *     2.  Players can only be moused over/clicked once - after that, they
 *         become disabled.  Might be the update() method.  Don't know if
 *         we even need that functionality, but I could see a purpose arising in
 *         the future.  At least get it working, then maybe disable it with
 *         a setter.
 *     3.  Implement RemovePlayer()?  Probably not, since the server would
 *         send out an updated list which the GUI would just rebuild.
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * midnak      11/29/2011  New file (gutted GuiFileBrowser)
 ****************************************************************************/

//#include <debug.h>        // USB Gecko

#include "fceunetwork.h"  // FCEUD_TellServerToggleReady()
#include "gui.h"
#include "menu.h"         // Error prompts.  Don't know why they're in menu.h.

GuiPlayerList::GuiPlayerList(int w, int h)
{
	//DEBUG_Init(GDBSTUB_DEVICE_USB, 1);  // USB Gecko
	//_break();							// USB Gecko

	width = w;
	height = h;
	numEntries = 0;
	selectedItem = 0;
	selectable = true;
	listChanged = true; // trigger an initial list update
	focus = 0; // allow focus

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
/*
	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		imgRowSelected[numEntries] = new GuiImage(imgDataSelectionEntry);
		imgRowSelected[numEntries]->SetPosition(2,-3);
	}
*/
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

	imgPlayer1Ready = new GuiImage(imgDataPlayer1Ready);
	imgPlayer2Ready = new GuiImage(imgDataPlayer2Ready);
	imgPlayer3Ready = new GuiImage(imgDataPlayer3Ready);
	imgPlayer4Ready = new GuiImage(imgDataPlayer4Ready);

	imgPlayerReady[0] = imgPlayer1Ready;
	imgPlayerReady[1] = imgPlayer2Ready;
	imgPlayerReady[2] = imgPlayer3Ready;
	imgPlayerReady[3] = imgPlayer4Ready;

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		imgPlayerReady[i]->SetPosition(5,0);
	}

	// Gotta get rid of this.  For now, it prevents a segfault.
	rowButton[0] = new GuiButton(w,h);
	rowButton[1] = new GuiButton(w,h);
	rowButton[2] = new GuiButton(w,h);
	rowButton[3] = new GuiButton(w,h);

	colorNotReady = new (GXColor){0, 0, 0, 155};
	colorReady = new GXColor[4] {
									(GXColor){61, 89, 144, 255},  // Player 1 ready (blue)
									(GXColor){144, 60, 60, 255},  // Player 2 ready (red)
									(GXColor){60, 144, 60, 255},  // Player 3 ready (green)
									(GXColor){148, 147, 65, 255}  // Player 4 ready (yellow)
								 };

	rowText[0] = new GuiText("", 25, *colorNotReady);
	rowText[1] = new GuiText("", 25, *colorNotReady);
	rowText[2] = new GuiText("", 25, *colorNotReady);
	rowText[3] = new GuiText("", 25, *colorNotReady);
}

GuiPlayerList::~GuiPlayerList()
{
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
			delete rowButton[i];
		}
	}

	delete btnSoundClick;

	delete trigA;
	delete trig2;

	delete colorNotReady;
	delete colorReady;
}

void GuiPlayerList::SetFocus(int f)
{
	focus = f;

	for(int i=0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		rowButton[i]->ResetState();
	}

	if(f == 1)
	{
		rowButton[selectedItem]->SetState(STATE_SELECTED);
	}
}

bool GuiPlayerList::AddPlayer(Player player)
{
	if(numEntries < MAX_PLAYER_LIST_SIZE)
	{
		char truncName[MAX_PLAYER_NAME_LEN+1];
		snprintf(truncName, MAX_PLAYER_NAME_LEN+1, "%s", player.name);

		rowText[numEntries]->SetText(truncName);
		rowText[numEntries]->SetColor(player.ready ? colorReady[numEntries] : *colorNotReady);
		rowText[numEntries]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		rowText[numEntries]->SetPosition(40,0);
		rowText[numEntries]->SetMaxWidth(105);

		imgRowSelected[numEntries] = new GuiImage(imgDataSelectionEntry);
		imgRowSelected[numEntries]->SetPosition(2,-3);

		rowButton[numEntries] = new GuiButton(this->GetWidth(), imgDataSelectionEntry->GetHeight());
		rowButton[numEntries]->SetParent(this);
		rowButton[numEntries]->SetLabel(rowText[numEntries]);
		rowButton[numEntries]->SetImageOver(imgRowSelected[numEntries]);
		rowButton[numEntries]->SetPosition(2, (imgRowSelected[numEntries]->GetHeight() * numEntries) + 50);
		rowButton[numEntries]->SetTrigger(trigA);
		rowButton[numEntries]->SetTrigger(trig2);
		rowButton[numEntries]->SetSoundClick(btnSoundClick);

		if(player.ready)
		{
			rowButton[numEntries]->SetIcon(imgPlayerReady[numEntries]);
		}

		numEntries++;
		listChanged = true;

		return true;
	}

	return false;
}

// TODO:  To implement or to remove, that is the question.
void GuiPlayerList::RemovePlayer(int playerNum)
{
	// blow stuff away

	numEntries--;
	listChanged = true;
}

int GuiPlayerList::GetPlayerNumber(char *name)
{
	int idx = -1;

	if(rowText != NULL)
	{
		for(int i = 0; i < numEntries; i++)
		{
			if(strcmp(name, rowText[i]->ToString()) == 0)
			{
				idx = i;
				break;
			}
		}
	}

	return idx;
}

// This function requests the server mark the player as ready/not ready.
// No action is taken in the GUI until the server sends back the new
// player list containing everyone's status.  The list is rebuilt when
// this message is received, though not by this method (keep in mind
// that the server may also send such updates unsolicited).
bool GuiPlayerList::ToggleReady()
{
	bool ret = true;

	if(executionMode == NETPLAY_HOST)
	{
		if(IsPlayerReady(0))
		{
			rowButton[0]->SetIcon(NULL);
			rowText[0]->SetColor(*colorNotReady);
		}
		else
		{
			rowButton[0]->SetIcon(imgPlayerReady[0]);
			rowText[0]->SetColor(colorReady[0]);
		}

		FCEUD_SendPlayerListToClients();
	}
	else if(executionMode == NETPLAY_CLIENT)
	{
		if(!FCEUD_TellServerToggleReady())
		{
			ErrorPrompt("Could not send 'ready' message to server");
			ret = false;
		}
	}

	listChanged = true;

	return ret;
}

bool GuiPlayerList::IsPlayerReady(int playerNum)
{
	bool ready = true;

	if(playerNum < 0 || playerNum > numEntries || rowButton[playerNum]->GetIcon() == NULL)
	{
		ready = false;
	}

	return ready;
}

bool GuiPlayerList::IsEveryoneReady()
{
	bool ready = true;

	if(numEntries>=0)
	{
		for(int i = 0; i < numEntries; i++)
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

void GuiPlayerList::ResetState()
{
	state = STATE_DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i = 0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		rowButton[i]->ResetState();
	}
}

/**
 * Draw the button on screen
 */
void GuiPlayerList::Draw()
{
	if(!this->IsVisible())
	{
		return;
	}

	imgMainWindow->Draw();

	for(int i = 0; i <= numEntries; ++i)
	{
		rowButton[i]->Draw();
	}

	titleTxt->Draw();

	txtAllPlayersReady->SetVisible(IsEveryoneReady());
	txtAllPlayersReady->Draw();

	this->UpdateEffects();
}

void GuiPlayerList::Update(GuiTrigger * t)
{
	if(state == STATE_DISABLED || !t)
	{
		return;
	}

	// Uncommenting this resolves the issue where player names no longer receive focus
	// after being clicked, but it seems like this shouldn't be necessary -- other code
	// appears to be attempting to do this.
	//rowButton[selectedItem]->ResetState();

	for(int i=0; i<numEntries; ++i)
	{
		if(listChanged)
		{
			if(rowButton[i]->GetState() == STATE_DISABLED)
			{
				rowButton[i]->SetState(STATE_DEFAULT);
			}

			rowButton[i]->SetVisible(true);
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
			t->chan = -1;

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

	listChanged = false;

	//if(updateCB)
	//	updateCB(this);
}
