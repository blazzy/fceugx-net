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
	bgFileSelectionImg->SetParent(this);
	bgFileSelectionImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

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

	Reset();
	WriteLn("[Player1] What packets through yonder socket breaks?  It is the east, and FCEUGX-net is the sun.");
	WriteLn("[Player3] Arise, fair sun, and kill the envious FCEUX,");
	WriteLn("[Player1] Who is already sick and pale with grief,");
	WriteLn("[Player1] That thou her netplay art far more fair than she:");
	WriteLn("[Player4] Be not her netplay, since she is envious");
	WriteLn("[Player4] Okay, you know what?  I have no idea what the hell I'm saying.");
	WriteLn("[Player2] Sing a song of sixpence");
	WriteLn("[Player1] A pocket full of rye");
	WriteLn("[Player3] Four and twenty blackbirds");
	WriteLn("[Player4] Baked in a pie");
	WriteLn("[Player1] When the pie was opened,");
	WriteLn("[Player2] The person about to eat it said \"What the @*%! is this?  Is this supposed to be some kind of sick joke?  I work hard all day ruling over this kingdom.  All I want is to be able to come home at the end of a hard day's work, eat and sit on my throne, but instead I've got to put up with THIS bull$#!%.\"");
	WriteLn("[Player4] Wasn't that a stupid thing to set before the king?");

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		viewport[i] = new GuiText(NULL, 20, (GXColor){0, 0, 0, 0xff});
		viewport[i]->SetParent(this);
		viewport[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
		viewport[i]->SetPosition(7, (26 * i) + 8);
	}
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

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		delete viewport[i];
	}
}

void GuiChatWindow::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		viewport[i]->ResetState();
	}

	if(f == 1)
	{
		viewport[selectedItem]->SetState(STATE_SELECTED);
	}
}

void GuiChatWindow::ResetState()
{
	state = STATE_DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		viewport[i]->ResetState();
	}
}

void GuiChatWindow::TriggerUpdate()
{
	int newIndex = windowInfo.selIndex-windowInfo.pageIndex;

	if(newIndex >= FILE_PAGESIZE)
	{
		newIndex = FILE_PAGESIZE-1;
	}
	else if(newIndex < 0)
	{
		newIndex = 0;
	}

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

	for(u32 i=0; i<FILE_PAGESIZE; ++i)
	{
		viewport[i]->Draw();
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
	if(scrollbarBoxBtn->GetState() == STATE_HELD
	&& scrollbarBoxBtn->GetStateChan() == t->chan
	&& t->wpad->ir.valid
	&& windowInfo.numEntries > FILE_PAGESIZE )
	{
		scrollbarBoxBtn->SetPosition(0,0);
		positionWiimote = t->wpad->ir.y - 60 - scrollbarBoxBtn->GetTop();

		if(positionWiimote < scrollbarBoxBtn->GetMinY())
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
		t->wpad->btns_d |= WPAD_BUTTON_DOWN;

		if(!this->IsFocused())
		{
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
		}
	}
	else if(arrowUpBtn->GetState() == STATE_HELD && arrowUpBtn->GetStateChan() == t->chan)
	{
		t->wpad->btns_d |= WPAD_BUTTON_UP;

		if(!this->IsFocused())
		{
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
		}
	}

	// pad/joystick navigation
	if(!focus)
	{
		// goto?!
		goto endNavigation; // skip navigation
		listChanged = false;
	}

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
			else if(viewport[selectedItem+1]->IsVisible())
			{
				viewport[selectedItem]->ResetState();
				viewport[++selectedItem]->SetState(STATE_SELECTED, t->chan);
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
			viewport[selectedItem]->ResetState();
			viewport[--selectedItem]->SetState(STATE_SELECTED, t->chan);
		}
	}

	endNavigation:

	for(int i = 0; i < FILE_PAGESIZE; ++i)
	{
		if(listChanged || numEntries != windowInfo.numEntries)
		{
			if(windowInfo.pageIndex+i < windowInfo.numEntries)
			{
				if(viewport[i]->GetState() == STATE_DISABLED)
				{
					viewport[i]->SetState(STATE_DEFAULT);
				}

				viewport[i]->SetVisible(true);
				viewport[i]->SetText(scrollbackBuffer[windowInfo.pageIndex+i].value);
			}
			else
			{
				viewport[i]->SetVisible(false);
				viewport[i]->SetState(STATE_DISABLED);
			}
		}

		if(i != selectedItem && viewport[i]->GetState() == STATE_SELECTED)
		{
			viewport[i]->ResetState();
		}
		else if(focus && i == selectedItem && viewport[i]->GetState() == STATE_DEFAULT)
		{
			viewport[selectedItem]->SetState(STATE_SELECTED, t->chan);
		}

		int currChan = t->chan;

		if(t->wpad->ir.valid && !viewport[i]->IsInside(t->wpad->ir.x, t->wpad->ir.y))
		{
			t->chan = -1;
		}

		viewport[i]->Update(t);
		t->chan = currChan;

		if(viewport[i]->GetState() == STATE_SELECTED)
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
		wcstombs(scrollbackBuffer[windowInfo.size].value, textDyn[i], 200 );  // 200 is safely larger than textDyn[i]'s length
		windowInfo.size++;
		windowInfo.numEntries++;
	}

	// Add a blank line between messages
	strcpy(scrollbackBuffer[windowInfo.size].value, "");
	windowInfo.size++;
	windowInfo.numEntries++;

	return true;
}
