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
 *     4.  Fix inability to unready after coming back from a game
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * midnak      11/29/2011  New file (gutted GuiFileBrowser)
 ****************************************************************************/

#include "fceunetwork.h"  // FCEUD_TellServerToggleReady()
#include "gui.h"
#include "filebrowser.h"
#include "menu.h"         // Error prompts.  Don't know why they're in menu.h.

GuiPlayerList::GuiPlayerList(int w, int h)
{
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

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
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

	imgPlayer1Ready = new GuiImage(imgDataPlayer1Ready);
	imgPlayer2Ready = new GuiImage(imgDataPlayer2Ready);
	imgPlayer3Ready = new GuiImage(imgDataPlayer3Ready);
	imgPlayer4Ready = new GuiImage(imgDataPlayer4Ready);

	fileListIcon[0] = imgPlayer1Ready;
	fileListIcon[1] = imgPlayer2Ready;
	fileListIcon[2] = imgPlayer3Ready;
	fileListIcon[3] = imgPlayer4Ready;

	for(int i = 0; i < 4; i++)
	{
		fileListIcon[i]->SetPosition(5,0);
	}

	// Gotta get rid of this.  For now, it prevents a segfault.
	fileList[0] = new GuiButton(w,h);
	fileList[1] = new GuiButton(w,h);
	fileList[2] = new GuiButton(w,h);
	fileList[3] = new GuiButton(w,h);
	for (int i = 0; i < MAX_PLAYER_LIST_SIZE; ++i)
	{
		fileListBg[i] = 0;
	}

	player1ColorText = new GuiText("", 25, (GXColor){61, 89, 144, 255});
	player2ColorText = new GuiText("", 25, (GXColor){144, 60, 60, 255});
	player3ColorText = new GuiText("", 25, (GXColor){60, 144, 60, 255});
	player4ColorText = new GuiText("", 25, (GXColor){148, 147, 65, 255});

	fileListText[0] = player1ColorText;
	fileListText[1] = player2ColorText;
	fileListText[2] = player3ColorText;
	fileListText[3] = player4ColorText;
}

GuiPlayerList::~GuiPlayerList()
{
	delete titleTxt;
	delete txtAllPlayersReady;

	delete imgMainWindow;

	delete imgDataMainWindow;
	delete imgDataSelectionEntry;
	delete imgDataPlayer1Ready;
	delete imgDataPlayer2Ready;
	delete imgDataPlayer3Ready;
	delete imgDataPlayer4Ready;

	delete btnSoundOver;
	delete btnSoundClick;

	delete trigA;
	delete trig2;

	for(int i=0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		if(fileListText[i])
		{
			delete fileListText[i];
		}

		if(fileList[i])
		{
			delete fileList[i];
		}

		if(fileListBg[i])
		{
			delete fileListBg[i];
		}

		if(fileListIcon[i])
		{
			delete fileListIcon[i];
		}
	}
}

void GuiPlayerList::SetFocus(int f)
{
	focus = f;

	for(int i=0; i < MAX_PLAYER_LIST_SIZE; i++)
	{
		fileList[i]->ResetState();
	}

	if(f == 1)
	{
		fileList[selectedItem]->SetState(STATE_SELECTED);
	}
}

bool GuiPlayerList::AddPlayer(Player player)
{
	if(numEntries < MAX_PLAYER_LIST_SIZE)
	{
		char truncName[MAX_PLAYER_NAME_LEN+1];
		snprintf(truncName, MAX_PLAYER_NAME_LEN+1, "%s", player.name);

		fileListText[numEntries]->SetText(truncName);
		fileListText[numEntries]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		fileListText[numEntries]->SetPosition(40,0);
		fileListText[numEntries]->SetMaxWidth(105);

		fileListBg[numEntries] = new GuiImage(imgDataSelectionEntry);
		fileListBg[numEntries]->SetPosition(2,-3);

		fileList[numEntries] = new GuiButton(this->GetWidth(), imgDataSelectionEntry->GetHeight());
		fileList[numEntries]->SetParent(this);
		fileList[numEntries]->SetLabel(fileListText[numEntries]);
		fileList[numEntries]->SetImageOver(fileListBg[numEntries]);
		fileList[numEntries]->SetPosition(2, (fileListBg[numEntries]->GetHeight() * numEntries) + 50);
		fileList[numEntries]->SetTrigger(trigA);
		fileList[numEntries]->SetTrigger(trig2);
		fileList[numEntries]->SetSoundClick(btnSoundClick);

		if(player.ready)
		{
			fileList[numEntries]->SetIcon(fileListIcon[numEntries]);
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
	numEntries--;

	// blow stuff away

	listChanged = true;
}

int GuiPlayerList::GetPlayerNumber(char *name)
{
	int idx = -1;

	if(fileListText != NULL)
	{
		for(int i = 0; i < numEntries; i++)
		{
			if(strcmp(name, fileListText[i]->ToString()) == 0)
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
		fileList[0]->SetIcon(IsPlayerReady(0) ? NULL : fileListIcon[0]);
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

	if(playerNum < 0 || playerNum >= MAX_PLAYER_LIST_SIZE)
	{
		ready = false;
	}

	if(fileList[playerNum]->GetIcon() == NULL)
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
		fileList[i]->ResetState();
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

	for(u32 i = 0; i < MAX_PLAYER_LIST_SIZE; ++i)
	{
		fileList[i]->Draw();
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

	for(int i=0; i<numEntries; ++i)
	{
		if(listChanged)
		{
			if(fileList[i]->GetState() == STATE_DISABLED)
			{
				fileList[i]->SetState(STATE_DEFAULT);
			}

			fileList[i]->SetVisible(true);

			//fileListText[i]->SetText(browserList[browser.pageIndex+i].displayname);

			/*if(fileListIcon[i])
			{
				delete fileListIcon[i];
				fileListIcon[i] = NULL;
				fileListText[i]->SetPosition(5,0);
			}*/
		}

		if(i != selectedItem && fileList[i]->GetState() == STATE_SELECTED)
			fileList[i]->ResetState();
		else if(focus && i == selectedItem && fileList[i]->GetState() == STATE_DEFAULT)
			fileList[selectedItem]->SetState(STATE_SELECTED, t->chan);

		int currChan = t->chan;

		if(t->wpad->ir.valid && !fileList[i]->IsInside(t->wpad->ir.x, t->wpad->ir.y))
			t->chan = -1;

		fileList[i]->Update(t);
		t->chan = currChan;

		if(fileList[i]->GetState() == STATE_SELECTED)
		{
			selectedItem = i;
			//browser.selIndex = browser.pageIndex + i;
		}

		if(selectedItem == i)
			fileListText[i]->SetScroll(SCROLL_HORIZONTAL);
		else
			fileListText[i]->SetScroll(SCROLL_NONE);
	}

	listChanged = false;

	//if(updateCB)
	//	updateCB(this);
}
