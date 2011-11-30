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

	bgFileSelection = new GuiImageData(bg_player_list_png);
	bgFileSelectionImg = new GuiImage(bgFileSelection);
	bgFileSelectionImg->SetParent(this);
	bgFileSelectionImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	bgFileSelectionEntry = new GuiImageData(bg_player_list_entry_png);

	iconPlayer1Ready = new GuiImageData(player1_grab_small_png);
	iconPlayer2Ready = new GuiImageData(player1_grab_small_png);
	iconPlayer3Ready = new GuiImageData(player1_grab_small_png);
	iconPlayer4Ready = new GuiImageData(player1_grab_small_png);

	/*scrollbar = new GuiImageData(scrollbar_png);
	scrollbarImg = new GuiImage(scrollbar);
	scrollbarImg->SetParent(this);
	scrollbarImg->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarImg->SetPosition(0, 30);*/

	/*arrowDown = new GuiImageData(scrollbar_arrowdown_png);
	arrowDownImg = new GuiImage(arrowDown);
	arrowDownOver = new GuiImageData(scrollbar_arrowdown_over_png);
	arrowDownOverImg = new GuiImage(arrowDownOver);
	arrowUp = new GuiImageData(scrollbar_arrowup_png);
	arrowUpImg = new GuiImage(arrowUp);
	arrowUpOver = new GuiImageData(scrollbar_arrowup_over_png);
	arrowUpOverImg = new GuiImage(arrowUpOver);
	scrollbarBox = new GuiImageData(scrollbar_box_png);
	scrollbarBoxImg = new GuiImage(scrollbarBox);
	scrollbarBoxOver = new GuiImageData(scrollbar_box_over_png);
	scrollbarBoxOverImg = new GuiImage(scrollbarBoxOver);*/

	/*arrowUpBtn = new GuiButton(arrowUpImg->GetWidth(), arrowUpImg->GetHeight());
	arrowUpBtn->SetParent(this);
	arrowUpBtn->SetImage(arrowUpImg);
	arrowUpBtn->SetImageOver(arrowUpOverImg);
	arrowUpBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	arrowUpBtn->SetSelectable(false);
	arrowUpBtn->SetClickable(false);
	arrowUpBtn->SetHoldable(true);
	arrowUpBtn->SetTrigger(trigHeldA);
	arrowUpBtn->SetSoundOver(btnSoundOver);
	arrowUpBtn->SetSoundClick(btnSoundClick);

	arrowDownBtn = new GuiButton(arrowDownImg->GetWidth(), arrowDownImg->GetHeight());
	arrowDownBtn->SetParent(this);
	arrowDownBtn->SetImage(arrowDownImg);
	arrowDownBtn->SetImageOver(arrowDownOverImg);
	arrowDownBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	arrowDownBtn->SetSelectable(false);
	arrowDownBtn->SetClickable(false);
	arrowDownBtn->SetHoldable(true);
	arrowDownBtn->SetTrigger(trigHeldA);
	arrowDownBtn->SetSoundOver(btnSoundOver);
	arrowDownBtn->SetSoundClick(btnSoundClick);*/

	/*scrollbarBoxBtn = new GuiButton(scrollbarBoxImg->GetWidth(), scrollbarBoxImg->GetHeight());
	scrollbarBoxBtn->SetParent(this);
	scrollbarBoxBtn->SetImage(scrollbarBoxImg);
	scrollbarBoxBtn->SetImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->SetMinY(0);
	scrollbarBoxBtn->SetMaxY(156);
	scrollbarBoxBtn->SetSelectable(false);
	scrollbarBoxBtn->SetClickable(false);
	scrollbarBoxBtn->SetHoldable(true);
	scrollbarBoxBtn->SetTrigger(trigHeldA);*/

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		fileListText[i] = new GuiText(NULL, 20, (GXColor){0, 0, 0, 0xff});
		fileListText[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		fileListText[i]->SetPosition(5,0);
		fileListText[i]->SetMaxWidth(105);

		fileListBg[i] = new GuiImage(bgFileSelectionEntry);
		fileListBg[i]->SetPosition(2,0);
		fileListIcon[i] = NULL;

		fileList[i] = new GuiButton(380, 26);
		fileList[i]->SetParent(this);
		fileList[i]->SetLabel(fileListText[i]);
		fileList[i]->SetImageOver(fileListBg[i]);
		fileList[i]->SetPosition(2,26*i+3);
		fileList[i]->SetTrigger(trigA);
		fileList[i]->SetTrigger(trig2);
		fileList[i]->SetSoundClick(btnSoundClick);
	}
}

GuiPlayerList::~GuiPlayerList()
{
	/*delete arrowUpBtn;
	delete arrowDownBtn;
	delete scrollbarBoxBtn;*/

	delete bgFileSelectionImg;
	/*delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;
	delete scrollbarBoxImg;
	delete scrollbarBoxOverImg;*/

	delete bgFileSelection;
	delete bgFileSelectionEntry;

	delete iconPlayer1Ready;
	delete iconPlayer2Ready;
	delete iconPlayer3Ready;
	delete iconPlayer4Ready;

	/*delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;
	delete scrollbarBox;
	delete scrollbarBoxOver;*/

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigHeldA;
	delete trigA;
	delete trig2;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		delete fileListText[i];
		delete fileList[i];
		delete fileListBg[i];

		if(fileListIcon[i])
			delete fileListIcon[i];
	}
}

void GuiPlayerList::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<FILE_PAGESIZE; i++)
		fileList[i]->ResetState();

	if(f == 1)
		fileList[selectedItem]->SetState(STATE_SELECTED);
}

void GuiPlayerList::ResetState()
{
	state = STATE_DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		fileList[i]->ResetState();
	}
}

void GuiPlayerList::TriggerUpdate()
{
	int newIndex = browser.selIndex-browser.pageIndex;
	
	if(newIndex >= FILE_PAGESIZE)
		newIndex = FILE_PAGESIZE-1;
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

	bgFileSelectionImg->Draw();

	for(u32 i=0; i<FILE_PAGESIZE; ++i)
	{
		fileList[i]->Draw();
	}

	/*scrollbarImg->Draw();
	arrowUpBtn->Draw();
	arrowDownBtn->Draw();
	scrollbarBoxBtn->Draw();*/

	this->UpdateEffects();
}

void GuiPlayerList::DrawTooltip()
{
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
		browser.numEntries > FILE_PAGESIZE
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
		else if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
		{
			browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
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
	}*/

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(t->Right())
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > FILE_PAGESIZE)
		{
			browser.pageIndex += FILE_PAGESIZE;
			if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
			listChanged = true;
		}
	}
	else if(t->Left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= FILE_PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(t->Down())
	{
		if(browser.pageIndex + selectedItem + 1 < browser.numEntries)
		{
			if(selectedItem == FILE_PAGESIZE-1)
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

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		if(listChanged || numEntries != browser.numEntries)
		{
			if(browser.pageIndex+i < browser.numEntries)
			{
				if(fileList[i]->GetState() == STATE_DISABLED)
					fileList[i]->SetState(STATE_DEFAULT);

				fileList[i]->SetVisible(true);

				fileListText[i]->SetText(browserList[browser.pageIndex+i].displayname);

				if(fileListIcon[i])
				{
					delete fileListIcon[i];
					fileListIcon[i] = NULL;
					fileListText[i]->SetPosition(5,0);
				}

				switch(browserList[browser.pageIndex+i].icon)
				{
					case ICON_FOLDER:
						fileListIcon[i] = new GuiImage(iconPlayer1Ready);
						break;
					case ICON_SD:
						fileListIcon[i] = new GuiImage(iconPlayer2Ready);
						break;
					case ICON_USB:
						fileListIcon[i] = new GuiImage(iconPlayer3Ready);
						break;
					case ICON_DVD:
						fileListIcon[i] = new GuiImage(iconPlayer4Ready);
						break;
					default:
						fileListIcon[i] = new GuiImage(iconPlayer4Ready);
						break;
				}

				fileList[i]->SetIcon(fileListIcon[i]);

				if(fileListIcon[i] != NULL)
				{
					fileListIcon[i]->SetPosition(5,0);
					fileListText[i]->SetPosition(40,0);
				}
			}
			else
			{
				fileList[i]->SetVisible(false);
				fileList[i]->SetState(STATE_DISABLED);
			}
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
			browser.selIndex = browser.pageIndex + i;
		}

		if(selectedItem == i)
			fileListText[i]->SetScroll(SCROLL_HORIZONTAL);
		else
			fileListText[i]->SetScroll(SCROLL_NONE);
	}

	// update the location of the scroll box based on the position in the file list
	if(positionWiimote > 0)
	{
		position = positionWiimote; // follow wiimote cursor
		//scrollbarBoxBtn->SetPosition(0,position+36);
	}
	else if(listChanged || numEntries != browser.numEntries)
	{
		if(float((browser.pageIndex<<1))/(float(FILE_PAGESIZE)) < 1.0)
		{
			position = 0;
		}
		else if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
		{
			position = 156;
		}
		else
		{
			position = 156 * (browser.pageIndex + FILE_PAGESIZE/2) / (float)browser.numEntries;
		}
		//scrollbarBoxBtn->SetPosition(0,position+36);
	}

	listChanged = false;
	numEntries = browser.numEntries;

	if(updateCB)
		updateCB(this);
}
