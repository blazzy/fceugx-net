/****************************************************************************
 * FCE Ultra GX
 * Nintendo Wii/Gamecube Port
 *
 * gui_playerlist.cpp
 *
 * Description:  A chat window
 *
 * History:
 *
 * Name               Date     Description
 * --------------  mm/dd/yyyy  --------------------------------------------------
 * Kurtis LoVerde   Jan 2012   New file (based on Tantric's GuiFileBrowser)
 ****************************************************************************/

#include "gui_chatwindow.h"
#include "menu.h"
#include "../utils/FreeTypeGX.h"   // charToWideChar()
#include "wchar.h"        // wcslen()
#include <cstdlib>   // wcstombs()

GuiChatWindow::GuiChatWindow(int w, int h)
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

	bgFileSelection = new GuiImageData(bg_game_selection_png);
	bgFileSelectionImg = new GuiImage(bgFileSelection);
	bgFileSelectionImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	scrollbar = new GuiImageData(scrollbar_png);
	scrollbarImg = new GuiImage(scrollbar);
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
	scrollbarBoxBtn->SetImage(scrollbarBoxImg);
	scrollbarBoxBtn->SetImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->SetMinY(0);
	scrollbarBoxBtn->SetMaxY(156);
	scrollbarBoxBtn->SetSelectable(false);
	scrollbarBoxBtn->SetClickable(false);
	scrollbarBoxBtn->SetHoldable(true);
	scrollbarBoxBtn->SetTrigger(trigHeldA);

	bgFileSelectionEntry = new GuiImageData(bg_game_selection_entry_png);

	for(int i = 0; i < FILE_PAGESIZE; ++i)
	{
		fileListBg[i] = new GuiImage(bgFileSelectionEntry);

		viewportText[i] = new GuiText(NULL, 20, (GXColor){0, 0, 0, 0xff});
		viewportText[i]->SetParent(this);
		viewportText[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		viewportText[i]->SetPosition(5, 0);
		viewportText[i]->SetMaxWidth(380);

		viewportButton[i] = new GuiButton(380, 26);
		viewportButton[i]->SetLabel(viewportText[i]);
		viewportButton[i]->SetImageOver(fileListBg[i]);
		viewportButton[i]->SetPosition(2, 26 * i + 3);
		viewportButton[i]->SetTrigger(trigA);
		viewportButton[i]->SetTrigger(trig2);
	}

	// Add elements to the window in the proper z-indexing order

	Append(bgFileSelectionImg);

	for(int i = 0; i < FILE_PAGESIZE; i++)
	{
		Append(viewportButton[i]);
	}

	Append(scrollbarImg);
	Append(arrowUpBtn);
	Append(arrowDownBtn);
	Append(scrollbarBoxBtn);
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

	for(int i = 0; i < FILE_PAGESIZE; i++)
	{
		delete viewportText[i];
		delete viewportButton[i];
	}
}

int GuiChatWindow::GetState()
{
	for(int i = 0; i < FILE_PAGESIZE; i++)
	{
		if(viewportButton[i]->GetState() == STATE_CLICKED)
		{
			// Please hold the particle physics jokes.
			state = STATE_CLICKED;
		}
	}

	return state;
}

void GuiChatWindow::TriggerUpdate()
{
	int newIndex = windowInfo.selIndex - windowInfo.pageIndex;

	char c[30];
	sprintf(c, "newIndex before: %d", newIndex);
	//InfoPrompt(c);

	if(newIndex >= FILE_PAGESIZE)
	{
		newIndex = FILE_PAGESIZE-1;
	}
	else if(newIndex < 0)
	{
		newIndex = 0;
	}

	sprintf(c, "newIndex after: %d", newIndex);
	//InfoPrompt(c);

	selectedItem = windowInfo.numEntries < FILE_PAGESIZE ? windowInfo.numEntries : FILE_PAGESIZE - 1;
	listChanged = true;
}

void GuiChatWindow::DrawTooltip()
{
}

extern GuiButton *readyBtn;

void writeReadyBtn(char *c)
{
	if(readyBtn != NULL)
	{
		readyBtn->SetLabel(new GuiText(c, 15, GXColor{0,0,0,255}));
	}
}

int i = 1,
		j = 1;

void GuiChatWindow::Update(GuiTrigger * t)
{
	char c[30];
	sprintf(c, "chan %d", t->chan);
	writeReadyBtn(c);

	/*
	if(i != -1)
	{
		char c[20];
		sprintf(c, "READY %d", i++);
		writeReadyBtn(c);
	}

	if(state == STATE_DISABLED || !t)
	{
		char c[20];
		sprintf(c, "return %d", j++);
		writeReadyBtn(c);
		i = -1;

		return;
	}*/

	int position = 0;
	int positionWiimote = 0;

	//if(!dirty)
	//{
		arrowUpBtn->Update(t);
		arrowDownBtn->Update(t);
		scrollbarBoxBtn->Update(t);
	//}

	// move the file listing to respond to wiimote cursor movement
	if( (listChanged || (scrollbarBoxBtn->GetState() == STATE_HELD
		&& scrollbarBoxBtn->GetStateChan() == t->chan
		&& t->wpad->ir.valid))
	&& windowInfo.numEntries > FILE_PAGESIZE)
	{
		scrollbarBoxBtn->SetPosition(0,0);
		positionWiimote = t->wpad->ir.y - 60 - scrollbarBoxBtn->GetTop();

		if(listChanged)
		{
			// If WriteLn() was called - either by the local user or because of an incoming message,
			// we trick Update() into thinking that the user has scrolled to the botton of the window.
			positionWiimote = scrollbarBoxBtn->GetMaxY();
		}
		else if(positionWiimote < scrollbarBoxBtn->GetMinY())
		{
			positionWiimote = scrollbarBoxBtn->GetMinY();
		}
		else if(positionWiimote > scrollbarBoxBtn->GetMaxY())
		{
			positionWiimote = scrollbarBoxBtn->GetMaxY();
		}

		windowInfo.pageIndex = (positionWiimote * windowInfo.numEntries)/156.0f - selectedItem;

		if(windowInfo.pageIndex <= 0)
		{
			windowInfo.pageIndex = 0;
		}
		else if(windowInfo.pageIndex+FILE_PAGESIZE >= windowInfo.numEntries)
		{
			windowInfo.pageIndex = windowInfo.numEntries-FILE_PAGESIZE;
		}

		listChanged = true;
		focus = false;
	}

	if(arrowDownBtn->GetState() == STATE_HELD && arrowDownBtn->GetStateChan() == t->chan)
	{
		//writeReadyBtn("down");

		t->wpad->btns_d |= WPAD_BUTTON_DOWN;

		if(!this->IsFocused())
		{
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
		}
	}
	else if(arrowUpBtn->GetState() == STATE_HELD && arrowUpBtn->GetStateChan() == t->chan)
	{
		//writeReadyBtn("up");

		t->wpad->btns_d |= WPAD_BUTTON_UP;

		if(!this->IsFocused())
		{
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
		}
	}

	// pad/joystick navigation
	if(!focus)
	{
		//writeReadyBtn("notFocus");
		// goto?!
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	//writeReadyBtn("here1");

	if(t->Right())
	{
		if(windowInfo.pageIndex < windowInfo.numEntries && windowInfo.numEntries > FILE_PAGESIZE)
		{
			windowInfo.pageIndex += FILE_PAGESIZE;

			if(windowInfo.pageIndex+FILE_PAGESIZE >= windowInfo.numEntries)
			{
				windowInfo.pageIndex = windowInfo.numEntries-FILE_PAGESIZE;
			}

			listChanged = true;
		}
	}
	else if(t->Left())
	{
		if(windowInfo.pageIndex > 0)
		{
			windowInfo.pageIndex -= FILE_PAGESIZE;

			if(windowInfo.pageIndex < 0)
			{
				windowInfo.pageIndex = 0;
			}

			listChanged = true;
		}
	}
	else if(t->Down())
	{
		if(windowInfo.pageIndex + selectedItem + 1 < windowInfo.numEntries)
		{
			if(selectedItem == FILE_PAGESIZE-1)
			{
				// move list down by 1
				++windowInfo.pageIndex;
				listChanged = true;
			}
			else if(viewportButton[selectedItem+1]->IsVisible())
			{
				viewportButton[selectedItem]->ResetState();
				viewportButton[++selectedItem]->SetState(STATE_SELECTED, t->chan);
			}
		}
	}
	else if(t->Up())
	{
		if(selectedItem == 0 &&	windowInfo.pageIndex + selectedItem > 0)
		{
			// move list up by 1
			--windowInfo.pageIndex;
			listChanged = true;
		}
		else if(selectedItem > 0)
		{
			viewportButton[selectedItem]->ResetState();
			viewportButton[--selectedItem]->SetState(STATE_SELECTED, t->chan);
		}
	}

	endNavigation:

	for(int i = 0; i < FILE_PAGESIZE; ++i)
	{
		if(listChanged || numEntries != windowInfo.numEntries)
		{
			if(windowInfo.pageIndex+i < windowInfo.numEntries)
			{
				if(viewportButton[i]->GetState() == STATE_DISABLED)
				{
					viewportButton[i]->SetState(STATE_DEFAULT);
				}

				viewportButton[i]->SetVisible(true);
				viewportText[i]->SetText(scrollbackBuffer[windowInfo.pageIndex+i].value);
			}
			else
			{
				viewportButton[i]->SetVisible(false);
				viewportButton[i]->SetState(STATE_DISABLED);
			}
		}

		if(i != selectedItem && viewportButton[i]->GetState() == STATE_SELECTED)
		{
			viewportButton[i]->ResetState();
		}
		else if(focus && i == selectedItem && viewportButton[i]->GetState() == STATE_DEFAULT)
		{
			viewportButton[selectedItem]->SetState(STATE_SELECTED, t->chan);
		}

		//if(!dirty)
		//{
		int currChan = t->chan;

		if(t->wpad->ir.valid && !viewportButton[i]->IsInside(t->wpad->ir.x, t->wpad->ir.y))
		{
			t->chan = -1;
		}

		viewportButton[i]->Update(t);
		t->chan = currChan;
		//}

		if(viewportButton[i]->GetState() == STATE_SELECTED)
		{
			selectedItem = i;
			windowInfo.selIndex = windowInfo.pageIndex + i;
		}
	}

	// update the location of the scroll box based on the position in the file list
	if(positionWiimote > 0)
	{
		position = positionWiimote; // follow wiimote cursor
		scrollbarBoxBtn->SetPosition(0,position+36);
	}
	else if(listChanged || numEntries != windowInfo.numEntries)
	{
		if(float((windowInfo.pageIndex<<1))/(float(FILE_PAGESIZE)) < 1.0)
		{
			position = 0;
		}
		else if(windowInfo.pageIndex+FILE_PAGESIZE >= windowInfo.numEntries)
		{
			position = 156;
		}
		else
		{
			position = 156 * (windowInfo.pageIndex + FILE_PAGESIZE/2) / (float)windowInfo.numEntries;
		}

		scrollbarBoxBtn->SetPosition(0,position+36);
	}

	listChanged = false;
	numEntries = windowInfo.numEntries;
	
	if(updateCB)
	{
		updateCB(this);
	}
}

void GuiChatWindow::Reset()
{
	windowInfo.numEntries = 0;
	windowInfo.selIndex = 0;
	windowInfo.pageIndex = 0;
	windowInfo.size = 0;
}

bool GuiChatWindow::WriteLn(const char *msg)
{
	/*char c[30];
	sprintf(c, "selIndex (%d) pageIndex (%d)", windowInfo.selIndex, windowInfo.pageIndex);
	InfoPrompt(c);*/

	if(msg == NULL || strlen(msg) <= 0)
	{
		return false;
	}

	if(windowInfo.size >= CHAT_SCROLLBACK_SIZE)
	{
		ErrorPrompt("Out of memory: too many files!");
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

			if( textDyn[linenum] == NULL)
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
		wcstombs(scrollbackBuffer[windowInfo.size].value, textDyn[i], 200);  // 200 is safely larger than textDyn[i]'s length
		windowInfo.size++;

		//if(windowInfo.numEntries < FILE_PAGESIZE)
		{
			windowInfo.numEntries++;
		}
	}

	//int viewportIdx = windowInfo.numEntries < FILE_PAGESIZE ? windowInfo.numEntries : FILE_PAGESIZE - 1;
	//viewportButton[viewportIdx]->SetState(STATE_SELECTED);
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
	//viewportButton[FILE_PAGESIZE - 1]->SetState(STATE_SELECTED);
	//TriggerUpdate();   // doesn't seem to do anything


	/*windowInfo.pageIndex = 9;
	selectedItem = 9;
	listChanged = true;*/


	/*
	if(windowInfo.pageIndex + selectedItem + 1 < windowInfo.numEntries)
	{
		if(selectedItem == FILE_PAGESIZE-1)
		{
			// move list down by 1
			++windowInfo.pageIndex;
			listChanged = true;
		}
		else if(viewportButton[selectedItem+1]->IsVisible())
		{
			viewportButton[selectedItem]->ResetState();
			viewportButton[++selectedItem]->SetState(STATE_SELECTED);
		}
	}
	*/

	/*arrowUpBtn->SetClickable(true);
	arrowDownBtn->SetClickable(true);
	arrowUpBtn->SetTrigger(trigHeldA);
	arrowDownBtn->SetTrigger(trigHeldA);*/

	return true;
}
