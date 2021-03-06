/*
 * libwiigui
 *
 * Tantric 2009, Kurtis LoVerde 2011
 *
 * gui_chatwindow.cpp
 *
 * Description:
 *
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * kloverde    12/24/2011  New class (based on GuiFileBrowser)
 */

#include "gui_chatwindow.h"
#include "filelist.h"
#include "filebrowser.h"

/**
 * Constructor for the GuiChatWindow class.
 */
GuiChatWindow::GuiChatWindow(int w, int h)
{
	width = w;
	height = h;
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

	bgFileSelection = new GuiImageData(bg_game_selection_png);
	bgFileSelectionImg = new GuiImage(bgFileSelection);
	bgFileSelectionImg->SetParent(this);
	bgFileSelectionImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	bgFileSelectionEntry = new GuiImageData(bg_game_selection_entry_png);

	iconFolder = new GuiImageData(icon_folder_png);
	iconSD = new GuiImageData(icon_sd_png);
	iconUSB = new GuiImageData(icon_usb_png);
	iconDVD = new GuiImageData(icon_dvd_png);
	iconSMB = new GuiImageData(icon_smb_png);

	scrollbar = new GuiImageData(scrollbar_png);
	scrollbarImg = new GuiImage(scrollbar);
	scrollbarImg->SetParent(this);
	scrollbarImg->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarImg->SetPosition(0, 30);

	arrowDown = new GuiImageData(scrollbar_arrowdown_png);
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
	scrollbarBoxOverImg = new GuiImage(scrollbarBoxOver);

	arrowUpBtn = new GuiButton(arrowUpImg->GetWidth(), arrowUpImg->GetHeight());
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
	arrowDownBtn->SetSoundClick(btnSoundClick);

	scrollbarBoxBtn = new GuiButton(scrollbarBoxImg->GetWidth(), scrollbarBoxImg->GetHeight());
	scrollbarBoxBtn->SetParent(this);
	scrollbarBoxBtn->SetImage(scrollbarBoxImg);
	scrollbarBoxBtn->SetImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->SetMinY(0);
	scrollbarBoxBtn->SetMaxY(156);
	scrollbarBoxBtn->SetSelectable(false);
	scrollbarBoxBtn->SetClickable(false);
	scrollbarBoxBtn->SetHoldable(true);
	scrollbarBoxBtn->SetTrigger(trigHeldA);

	for(int i=0; i<CHAT_PAGESIZE; ++i)
	{
		fileListText[i] = new GuiText(NULL, 20, (GXColor){0, 0, 0, 0xff});
		fileListText[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		fileListText[i]->SetPosition(5,0);
		fileListText[i]->SetMaxWidth(380);

		fileListBg[i] = new GuiImage(bgFileSelectionEntry);
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

	windowInfo.selIndex = 0;
	windowInfo.numEntries = 20;
}

/**
 * Destructor for the GuiChatWindow class.
 */
GuiChatWindow::~GuiChatWindow()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete scrollbarBoxBtn;

	delete bgFileSelectionImg;
	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;
	delete scrollbarBoxImg;
	delete scrollbarBoxOverImg;

	delete bgFileSelection;
	delete bgFileSelectionEntry;
	delete iconFolder;
	delete iconSD;
	delete iconUSB;
	delete iconDVD;
	delete iconSMB;
	delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;
	delete scrollbarBox;
	delete scrollbarBoxOver;

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigHeldA;
	delete trigA;
	delete trig2;

	for(int i=0; i<CHAT_PAGESIZE; i++)
	{
		delete fileListText[i];
		delete fileList[i];
		delete fileListBg[i];

		if(fileListIcon[i])
			delete fileListIcon[i];
	}
}

void GuiChatWindow::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<CHAT_PAGESIZE; i++)
		fileList[i]->ResetState();

	if(f == 1)
		fileList[selectedItem]->SetState(STATE_SELECTED);
}

void GuiChatWindow::ResetState()
{
	state = STATE_DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<CHAT_PAGESIZE; i++)
	{
		fileList[i]->ResetState();
	}
}

void GuiChatWindow::TriggerUpdate()
{
	int newIndex = windowInfo.selIndex-browser.pageIndex;

	if(newIndex >= CHAT_PAGESIZE)
		newIndex = CHAT_PAGESIZE-1;
	else if(newIndex < 0)
		newIndex = 0;

	selectedItem = newIndex;
	listChanged = true;
}

/**
 * Draw the button on screen
 */
void GuiChatWindow::Draw()
{
	if(!this->IsVisible())
		return;

	bgFileSelectionImg->Draw();

	for(u32 i=0; i<CHAT_PAGESIZE; ++i)
	{
		fileList[i]->Draw();
	}

	scrollbarImg->Draw();
	arrowUpBtn->Draw();
	arrowDownBtn->Draw();
	scrollbarBoxBtn->Draw();

	this->UpdateEffects();
}

void GuiChatWindow::DrawTooltip()
{
}

void GuiChatWindow::Update(GuiTrigger * t)
{
	if(state == STATE_DISABLED || !t)
		return;

	int position = 0;
	int positionWiimote = 0;

	arrowUpBtn->Update(t);
	arrowDownBtn->Update(t);
	scrollbarBoxBtn->Update(t);

	// move the file listing to respond to wiimote cursor movement
	if(scrollbarBoxBtn->GetState() == STATE_HELD &&
		scrollbarBoxBtn->GetStateChan() == t->chan &&
		t->wpad->ir.valid &&
		windowInfo.numEntries > CHAT_PAGESIZE
		)
	{
		scrollbarBoxBtn->SetPosition(0,0);
		positionWiimote = t->wpad->ir.y - 60 - scrollbarBoxBtn->GetTop();

		if(positionWiimote < scrollbarBoxBtn->GetMinY())
			positionWiimote = scrollbarBoxBtn->GetMinY();
		else if(positionWiimote > scrollbarBoxBtn->GetMaxY())
			positionWiimote = scrollbarBoxBtn->GetMaxY();

		browser.pageIndex = (positionWiimote * windowInfo.numEntries)/156.0f - selectedItem;

		if(browser.pageIndex <= 0)
		{
			browser.pageIndex = 0;
		}
		else if(browser.pageIndex+CHAT_PAGESIZE >= windowInfo.numEntries)
		{
			browser.pageIndex = windowInfo.numEntries-CHAT_PAGESIZE;
		}
		listChanged = true;
		focus = false;
	}

	if(arrowDownBtn->GetState() == STATE_HELD && arrowDownBtn->GetStateChan() == t->chan)
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
		if(browser.pageIndex < windowInfo.numEntries && windowInfo.numEntries > CHAT_PAGESIZE)
		{
			browser.pageIndex += CHAT_PAGESIZE;
			if(browser.pageIndex+CHAT_PAGESIZE >= windowInfo.numEntries)
				browser.pageIndex = windowInfo.numEntries-CHAT_PAGESIZE;
			listChanged = true;
		}
	}
	else if(t->Left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= CHAT_PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(t->Down())
	{
		if(browser.pageIndex + selectedItem + 1 < windowInfo.numEntries)
		{
			if(selectedItem == CHAT_PAGESIZE-1)
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

	for(int i=0; i<CHAT_PAGESIZE; ++i)
	{
		if(listChanged)
		{
			if(browser.pageIndex+i < windowInfo.numEntries)
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
						fileListIcon[i] = new GuiImage(iconFolder);
						break;
					case ICON_SD:
						fileListIcon[i] = new GuiImage(iconSD);
						break;
					case ICON_USB:
						fileListIcon[i] = new GuiImage(iconUSB);
						break;
					case ICON_DVD:
						fileListIcon[i] = new GuiImage(iconDVD);
						break;
					case ICON_SMB:
						fileListIcon[i] = new GuiImage(iconSMB);
						break;
				}
				fileList[i]->SetIcon(fileListIcon[i]);
				if(fileListIcon[i] != NULL)
					fileListText[i]->SetPosition(30,0);
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
			windowInfo.selIndex = browser.pageIndex + i;
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
		scrollbarBoxBtn->SetPosition(0,position+36);
	}
	else if(listChanged)
	{
		if(float((browser.pageIndex<<1))/(float(CHAT_PAGESIZE)) < 1.0)
		{
			position = 0;
		}
		else if(browser.pageIndex+CHAT_PAGESIZE >= windowInfo.numEntries)
		{
			position = 156;
		}
		else
		{
			position = 156 * (browser.pageIndex + CHAT_PAGESIZE/2) / (float)windowInfo.numEntries;
		}
		scrollbarBoxBtn->SetPosition(0,position+36);
	}

	listChanged = false;

	if(updateCB)
		updateCB(this);
}


bool GuiChatWindow::IsKeyboardHotspotClicked()
{
	bool clicked = false;

	for(int i = 0; i < CHAT_PAGESIZE; i++)
	{
		if(fileList[i]->GetState() == STATE_CLICKED)
		{
			clicked = true;
			break;
		}
	}

	return clicked;
}

bool GuiChatWindow::WriteLn(const char *msg)
{
	return false;

	//char c[30];
	//sprintf(c, "selIndex (%d) pageIndex (%d)", windowInfo.selIndex, browser.pageIndex);
	//InfoPrompt(c);

	if(msg == NULL || strlen(msg) <= 0)
	{
		return false;
	}

	if(windowInfo.numEntries >= CHAT_SCROLLBACK_SIZE)
	{
		ErrorPrompt("Out of memory: too many message lines!");
		return false; // out of space
	}

	// Wrapping logic adapted from GuiText::Draw().  Didn't want to modify GuiText to get to textDyn.
	const int maxWidth = bgFileSelectionImg->GetWidth() - 35;
	const wchar_t *msgWide = charToWideChar(msg);

	u32 n = 0,
		ch = 0,
		textDynNum = 0;

	int linenum = 0,
		lastSpace = -1,
		lastSpaceIndex = -1;

	// TODO:  currentSize should be shared with the value passed to the GuiText constructor
	const int currentSize = 20;

	wchar_t *textDyn[20];
	const u32 textlen = wcslen(msgWide);

	while(ch < textlen && linenum < 20)
	{
		if(n == 0)
		{
			textDyn[linenum] = new wchar_t[textlen + 1];

			if(textDyn[linenum] == NULL)
			{
				return false; // TODO:  Rename and reuse player list's OUT_OF_MEMORY
			}
		}

		textDyn[linenum][n] = msgWide[ch];
		textDyn[linenum][n+1] = 0;

		if(msgWide[ch] == ' ' || ch == textlen-1)
		{
			if(fontSystem[currentSize]->getWidth(textDyn[linenum]) > maxWidth)
			{
				if(lastSpace >= 0)
				{
					textDyn[linenum][lastSpaceIndex] = 0; // discard space, and everything after
					ch = lastSpace; // go backwards to the last space
					lastSpace = -1; // we have used this space
					lastSpaceIndex = -1;
				}
				++linenum;
				n = -1;
			}
			else if(ch == textlen-1)
			{
				++linenum;
			}
		}
		if(msgWide[ch] == ' ' && n >= 0)
		{
			lastSpace = ch;
			lastSpaceIndex = n;
		}

		++ch;
		++n;
	}

	textDynNum = linenum;
	// End of wrapping logic

	for(uint i = 0; i < textDynNum; i++)
	{
		wcstombs(browserList[windowInfo.numEntries].displayname, textDyn[i], 200);  // 200 is safely larger than textDyn[i]'s length
		windowInfo.numEntries++;
	}

	//int viewportIdx = windowInfo.numEntries < CHAT_PAGESIZE ? windowInfo.numEntries : CHAT_PAGESIZE - 1;
	//viewportButton[viewportIdx]->SetState(STATE_SELECTED);
browser.pageIndex = 0;
	TriggerUpdate();


	//if(!IsVisible())
	{

	}

	//// begin /////

	// Adjust the window appearance:
	// 0.  Scroll down the bottom of the window (the last elements of the backing array are displayed)
	// 1.  Select the last line in the viewport
	// 2.  Position the scrollbar box button at the bottom of the window
	// 3.  Hope and pray that the up/down buttons on the remote and scrollbar work



	///// end /////

	//ResetState();
	//viewportButton[CHAT_PAGESIZE - 1]->SetState(STATE_SELECTED);
	//TriggerUpdate();   // doesn't seem to do anything


//	browser.pageIndex = 9;
//	selectedItem = 9;
//	listChanged = true;



//	if(browser.pageIndex + selectedItem + 1 < windowInfo.numEntries)
//	{
//		if(selectedItem == CHAT_PAGESIZE-1)
//		{
//			// move list down by 1
//			++browser.pageIndex;
//			listChanged = true;
//		}
//		else if(viewportButton[selectedItem+1]->IsVisible())
//		{
//			viewportButton[selectedItem]->ResetState();
//			viewportButton[++selectedItem]->SetState(STATE_SELECTED);
//		}
//	}


//	arrowUpBtn->SetClickable(true);
//	arrowDownBtn->SetClickable(true);
//	arrowUpBtn->SetTrigger(trigHeldA);
//	arrowDownBtn->SetTrigger(trigHeldA);

	return true;
}
