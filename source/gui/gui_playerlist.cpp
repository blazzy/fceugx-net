/****************************************************************************
 * FCE Ultra GX
 * Nintendo Wii/Gamecube Port
 *
 * gui_playerlist.cpp
 *
 * Description:  Lists connected players in netplay
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * midnak      11/29/2011  New file (gutted GuiFileBrowser)
 ****************************************************************************/


#include "gui.h"
#include "filebrowser.h"


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

	trigHeldA = new GuiTrigger;
	trigHeldA->SetHeldTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);

	imgDataMainWindow = new GuiImageData(bg_player_list_png);
	imgMainWindow = new GuiImage(imgDataMainWindow);
	imgMainWindow->SetParent(this);
	imgMainWindow->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	imgDataSelectionEntry = new GuiImageData(bg_player_list_entry_png);

	titleTxt = new GuiText("PLAYERS", 25, (GXColor){0, 0, 0, 255});
	titleTxt->SetParent(imgMainWindow);
	titleTxt->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt->SetPosition(0, 10);

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

	// Gotta get rid of this.  For now, it prevents a segfault.
	fileList[0] = new GuiButton(w,h);
	fileList[1] = new GuiButton(w,h);
	fileList[2] = new GuiButton(w,h);
	fileList[3] = new GuiButton(w,h);
	fileListText[0] = new GuiText("1", 25, (GXColor){0, 0, 0, 255});
	fileListText[1] = new GuiText("2", 25, (GXColor){0, 0, 0, 255});
	fileListText[2] = new GuiText("3", 25, (GXColor){0, 0, 0, 255});
	fileListText[3] = new GuiText("4", 25, (GXColor){0, 0, 0, 255});
}

GuiPlayerList::~GuiPlayerList()
{
	delete titleTxt;

	delete imgMainWindow;

	delete imgDataMainWindow;
	delete imgDataSelectionEntry;
	delete imgDataPlayer1Ready;
	delete imgDataPlayer2Ready;
	delete imgDataPlayer3Ready;
	delete imgDataPlayer4Ready;

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigHeldA;
	delete trigA;
	delete trig2;

	for(int i=0; i<MAX_PLAYER_LIST_SIZE; i++)
	{
		delete fileListText[i];
		delete fileList[i];
		delete fileListBg[i];

		if(fileListIcon[i])
		{
			delete fileListIcon[i];
		}
	}
}

void GuiPlayerList::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<MAX_PLAYER_LIST_SIZE; i++)
	{
		fileList[i]->ResetState();
	}

	if(f == 1)
	{
		fileList[selectedItem]->SetState(STATE_SELECTED);
	}
}

bool GuiPlayerList::AddPlayer(char *name)
{
	if(numEntries >= MAX_PLAYER_LIST_SIZE || name == NULL)
	{
		return false;
	}

	char substr[MAX_PLAYER_NAME_LEN];
	snprintf( substr, MAX_PLAYER_NAME_LEN, "%s", name );

	fileListText[numEntries]->SetText(name);
	fileListText[numEntries]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	fileListText[numEntries]->SetPosition(5,0);
	fileListText[numEntries]->SetMaxWidth(105);

	fileListBg[numEntries] = new GuiImage(imgDataSelectionEntry);
	fileListBg[numEntries]->SetPosition(2,-3);
	//fileListIcon[numEntries] = NULL;

	fileList[numEntries] = new GuiButton(this->GetWidth(), 26);
	fileList[numEntries]->SetParent(this);
	fileList[numEntries]->SetLabel(fileListText[numEntries]);
	fileList[numEntries]->SetImageOver(fileListBg[numEntries]);
	fileList[numEntries]->SetPosition(2, (26 * numEntries) + 55);
	fileList[numEntries]->SetTrigger(trigA);
	fileList[numEntries]->SetTrigger(trig2);
	fileList[numEntries]->SetSoundClick(btnSoundClick);

	numEntries++;
	listChanged = true;

	return true;
}

// playerNum is 1-indexed
bool GuiPlayerList::SetPlayerReady(int playerNum, bool ready)
{
	if(playerNum < 0 || playerNum > numEntries)
	{
		return false;
	}

	switch(playerNum)
	{
		case 1:
			fileList[playerNum-1]->SetIcon(ready ? imgPlayer1Ready: NULL);
			break;
		case 2:
			fileList[playerNum-1]->SetIcon(ready ? imgPlayer2Ready: NULL);
			break;
		case 3:
			fileList[playerNum-1]->SetIcon(ready ? imgPlayer3Ready: NULL);
			break;
		case 4:
			fileList[playerNum-1]->SetIcon(ready ? imgPlayer4Ready: NULL);
			break;
	}

	if(ready)
	{
		// TODO:  Color the text
	}
	else
	{
		// TODO:  Change the text back to black
	}

	// TODO:  Sort the list, putting non-ready players at the bottom.

	return true;
}

void GuiPlayerList::ClearList()
{

}

void GuiPlayerList::ResetState()
{
	state = STATE_DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<MAX_PLAYER_LIST_SIZE; i++)
	{
		fileList[i]->ResetState();
	}
}

void GuiPlayerList::TriggerUpdate()
{
	int newIndex = browser.selIndex-browser.pageIndex;
	
	if(newIndex >= MAX_PLAYER_LIST_SIZE)
		newIndex = MAX_PLAYER_LIST_SIZE-1;
	else if(newIndex < 0)
		newIndex = 0;

	selectedItem = newIndex;
	listChanged = true;
}

/**
 * Draw the button on screen
 */
void GuiPlayerList::Draw()
{
	if(!this->IsVisible())
		return;

	imgMainWindow->Draw();

	for(u32 i=0; i<MAX_PLAYER_LIST_SIZE; ++i)
	{
		fileList[i]->Draw();
	}

	titleTxt->Draw();

	this->UpdateEffects();
}

void GuiPlayerList::Update(GuiTrigger * t)
{
	if(state == STATE_DISABLED || !t)
		return;

	int position = 0;
	int positionWiimote = 0;

	/*arrowUpBtn->Update(t);
	arrowDownBtn->Update(t);
	scrollbarBoxBtn->Update(t);

	// move the file listing to respond to wiimote cursor movement
	if(scrollbarBoxBtn->GetState() == STATE_HELD &&
		scrollbarBoxBtn->GetStateChan() == t->chan &&
		t->wpad->ir.valid &&
		browser.numEntries > MAX_PLAYER_LIST_SIZE
		)
	{
		scrollbarBoxBtn->SetPosition(0,0);
		positionWiimote = t->wpad->ir.y - 60 - scrollbarBoxBtn->GetTop();

		if(positionWiimote < scrollbarBoxBtn->GetMinY())
			positionWiimote = scrollbarBoxBtn->GetMinY();
		else if(positionWiimote > scrollbarBoxBtn->GetMaxY())
			positionWiimote = scrollbarBoxBtn->GetMaxY();

		browser.pageIndex = (positionWiimote * browser.numEntries)/156.0f - selectedItem;

		if(browser.pageIndex <= 0)
		{
			browser.pageIndex = 0;
		}
		else if(browser.pageIndex+MAX_PLAYER_LIST_SIZE >= browser.numEntries)
		{
			browser.pageIndex = browser.numEntries-MAX_PLAYER_LIST_SIZE;
		}
		listChanged = true;
		focus = false;
	}*/

	/*if(arrowDownBtn->GetState() == STATE_HELD && arrowDownBtn->GetStateChan() == t->chan)
	{
		t->wpad->btns_d |= WPAD_BUTTON_DOWN;
		if(!this->IsFocused())
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
	}
	else if(arrowUpBtn->GetState() == STATE_HELD && arrowUpBtn->GetStateChan() == t->chan)
	{
		t->wpad->btns_d |= WPAD_BUTTON_UP;
		if(!this->IsFocused())
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
	}

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(t->Right())
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > MAX_PLAYER_LIST_SIZE)
		{
			browser.pageIndex += MAX_PLAYER_LIST_SIZE;
			if(browser.pageIndex+MAX_PLAYER_LIST_SIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-MAX_PLAYER_LIST_SIZE;
			listChanged = true;
		}
	}
	else if(t->Left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= MAX_PLAYER_LIST_SIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(t->Down())
	{
		if(browser.pageIndex + selectedItem + 1 < browser.numEntries)
		{
			if(selectedItem == MAX_PLAYER_LIST_SIZE-1)
			{
				// move list down by 1
				++browser.pageIndex;
				listChanged = true;
			}
			else if(fileList[selectedItem+1]->IsVisible())
			{
				fileList[selectedItem]->ResetState();
				fileList[++selectedItem]->SetState(STATE_SELECTED, t->chan);
			}
		}
	}
	else if(t->Up())
	{
		if(selectedItem == 0 &&	browser.pageIndex + selectedItem > 0)
		{
			// move list up by 1
			--browser.pageIndex;
			listChanged = true;
		}
		else if(selectedItem > 0)
		{
			fileList[selectedItem]->ResetState();
			fileList[--selectedItem]->SetState(STATE_SELECTED, t->chan);
		}
	}

	endNavigation:
	*/
//-------------------------------------------------------------------------------------------------------------------------
	for(int i=0; i<numEntries; ++i)
	{
		if(listChanged)
		{
			//if(browser.pageIndex+i < browser.numEntries)
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

				/*switch(browserList[browser.pageIndex+i].icon)
				{
					case ICON_FOLDER:
						fileListIcon[i] = new GuiImage(imgDataPlayer1Ready);
						break;
					case ICON_SD:
						fileListIcon[i] = new GuiImage(imgDataPlayer2Ready);
						break;
					case ICON_USB:
						fileListIcon[i] = new GuiImage(imgDataPlayer3Ready);
						break;
					case ICON_DVD:
						fileListIcon[i] = new GuiImage(imgDataPlayer4Ready);
						break;
					default:
						fileListIcon[i] = new GuiImage(imgDataPlayer4Ready);
						break;
				}*/

				// Temporarily hack the new images into the list so I can see them
				//fileListIcon[0] = new GuiImage(imgDataPlayer1Ready);
				//fileListIcon[1] = new GuiImage(imgDataPlayer2Ready);
				//fileListIcon[2] = new GuiImage(imgDataPlayer3Ready);
				//fileListIcon[3] = new GuiImage(imgDataPlayer4Ready);

				fileList[i]->SetIcon(fileListIcon[i]);

				if(fileListIcon[i] != NULL)
				{
					fileListIcon[i]->SetPosition(5,0);
					fileListText[i]->SetPosition(40,0);
				}
			}
			//else
			//{
			//	fileList[i]->SetVisible(false);
			//	fileList[i]->SetState(STATE_DISABLED);
			//}
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
//-------------------------------------------------------------------------------------------------------------------------
	// update the location of the scroll box based on the position in the file list
	/*if(positionWiimote > 0)
	{
		position = positionWiimote; // follow wiimote cursor
		//scrollbarBoxBtn->SetPosition(0,position+36);
	}
	else if(listChanged || numEntries != browser.numEntries)
	{
		if(float((browser.pageIndex<<1))/(float(MAX_PLAYER_LIST_SIZE)) < 1.0)
		{
			position = 0;
		}
		else if(browser.pageIndex+MAX_PLAYER_LIST_SIZE >= browser.numEntries)
		{
			position = 156;
		}
		else
		{
			position = 156 * (browser.pageIndex + MAX_PLAYER_LIST_SIZE/2) / (float)browser.numEntries;
		}
		//scrollbarBoxBtn->SetPosition(0,position+36);
	}
	 */

	listChanged = false;
	//numEntries = browser.numEntries;

	//if(updateCB)
	//	updateCB(this);
}
