/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_filebrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include "filebrowser.h"

/**
 * Constructor for the GuiFileBrowser class.
 */
GuiFileBrowser::GuiFileBrowser(int w, int h)
{
	width = w;
	height = h;
	selectedItem = 0;
	selectable = true;
	listChanged = true; // trigger an initial list update
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	if(GCSettings.WiimoteOrientation)
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	bgGameSelection = new GuiImageData(bg_game_selection_png);
	bgGameSelectionImg = new GuiImage(bgGameSelection);
	bgGameSelectionImg->SetParent(this);
	bgGameSelectionImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	bgGameSelectionEntry = new GuiImageData(bg_game_selection_entry_png);
	gameFolder = new GuiImageData(folder_png);

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
	arrowUpBtn->SetTrigger(trigA);

	arrowDownBtn = new GuiButton(arrowDownImg->GetWidth(), arrowDownImg->GetHeight());
	arrowDownBtn->SetParent(this);
	arrowDownBtn->SetImage(arrowDownImg);
	arrowDownBtn->SetImageOver(arrowDownOverImg);
	arrowDownBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	arrowDownBtn->SetSelectable(false);
	arrowDownBtn->SetTrigger(trigA);

	scrollbarBoxBtn = new GuiButton(scrollbarBoxImg->GetWidth(), scrollbarBoxImg->GetHeight());
	scrollbarBoxBtn->SetParent(this);
	scrollbarBoxBtn->SetImage(scrollbarBoxImg);
	scrollbarBoxBtn->SetImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->SetSelectable(false);

	for(int i=0; i<PAGESIZE; i++)
	{
		gameListText[i] = new GuiText("Game",22, (GXColor){0, 0, 0, 0xff});
		gameListText[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		gameListText[i]->SetPosition(5,0);

		gameListBg[i] = new GuiImage(bgGameSelectionEntry);
		gameListFolder[i] = new GuiImage(gameFolder);

		gameList[i] = new GuiButton(380, 30);
		gameList[i]->SetParent(this);
		gameList[i]->SetLabel(gameListText[i]);
		gameList[i]->SetImageOver(gameListBg[i]);
		gameList[i]->SetPosition(2,30*i+3);
		gameList[i]->SetTrigger(trigA);
	}
}

/**
 * Destructor for the GuiFileBrowser class.
 */
GuiFileBrowser::~GuiFileBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete scrollbarBoxBtn;

	delete bgGameSelectionImg;
	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;
	delete scrollbarBoxImg;
	delete scrollbarBoxOverImg;

	delete bgGameSelection;
	delete bgGameSelectionEntry;
	delete gameFolder;
	delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;
	delete scrollbarBox;
	delete scrollbarBoxOver;

	delete trigA;

	for(int i=0; i<PAGESIZE; i++)
	{
		delete gameListText[i];
		delete gameList[i];
		delete gameListBg[i];
		delete gameListFolder[i];
	}
}

void GuiFileBrowser::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<PAGESIZE; i++)
		gameList[i]->ResetState();

	if(f == 1)
		gameList[selectedItem]->SetState(STATE_SELECTED);
}

void GuiFileBrowser::ResetState()
{
	state = STATE_DEFAULT;
	selectedItem = 0;

	for(int i=0; i<PAGESIZE; i++)
	{
		gameList[i]->ResetState();
	}
}

void GuiFileBrowser::TriggerUpdate()
{
	listChanged = true;
}

/**
 * Draw the button on screen
 */
void GuiFileBrowser::Draw()
{
	if(!this->IsVisible())
		return;

	bgGameSelectionImg->Draw();

	for(int i=0; i<PAGESIZE; i++)
	{
		gameList[i]->Draw();
	}

	scrollbarImg->Draw();
	arrowUpBtn->Draw();
	arrowDownBtn->Draw();
	scrollbarBoxBtn->Draw();

	this->UpdateEffects();
}

void GuiFileBrowser::Update(GuiTrigger * t)
{
	if(state == STATE_DISABLED || !t)
		return;

	// update the location of the scroll box based on the position in the file list
	int position = 136*(browser.pageIndex + selectedItem) / browser.numEntries;
	scrollbarBoxBtn->SetPosition(0,position+36);

	arrowUpBtn->Update(t);
	arrowDownBtn->Update(t);
	scrollbarBoxBtn->Update(t);

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(t->Right() || arrowDownBtn->GetState() == STATE_CLICKED)
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > PAGESIZE)
		{
			browser.pageIndex += PAGESIZE;
			if(browser.pageIndex+PAGESIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-PAGESIZE;
			listChanged = true;
		}
		arrowDownBtn->ResetState();
	}
	else if(t->Left() || arrowUpBtn->GetState() == STATE_CLICKED)
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
		arrowUpBtn->ResetState();
	}
	else if(t->Down())
	{
		if(browser.pageIndex + selectedItem + 1 < browser.numEntries)
		{
			if(selectedItem == PAGESIZE-1)
			{
				// move list down by 1
				browser.pageIndex++;
				listChanged = true;
			}
			else if(gameList[selectedItem+1]->IsVisible())
			{
				gameList[selectedItem]->ResetState();
				gameList[++selectedItem]->SetState(STATE_SELECTED);
			}
		}
	}
	else if(t->Up())
	{
		if(selectedItem == 0 &&	browser.pageIndex + selectedItem > 0)
		{
			// move list up by 1
			browser.pageIndex--;
			listChanged = true;
		}
		else if(selectedItem > 0)
		{
			gameList[selectedItem]->ResetState();
			gameList[--selectedItem]->SetState(STATE_SELECTED);
		}
	}

	endNavigation:

	for(int i=0; i<PAGESIZE; i++)
	{
		if(listChanged)
		{
			if(browser.pageIndex+i < browser.numEntries)
			{
				if(gameList[i]->GetState() == STATE_DISABLED)
					gameList[i]->SetState(STATE_DEFAULT);

				gameList[i]->SetVisible(true);

				gameListText[i]->SetText(browserList[browser.pageIndex+i].displayname);

				if(browserList[browser.pageIndex+i].isdir) // directory
				{
					gameList[i]->SetIcon(gameListFolder[i]);
					gameListText[i]->SetPosition(30,0);
				}
				else
				{
					gameList[i]->SetIcon(NULL);
					gameListText[i]->SetPosition(10,0);
				}
			}
			else
			{
				gameList[i]->SetVisible(false);
				gameList[i]->SetState(STATE_DISABLED);
			}
		}

		if(focus)
		{
			if(i != selectedItem && gameList[i]->GetState() == STATE_SELECTED)
				gameList[i]->ResetState();
			else if(i == selectedItem && gameList[i]->GetState() == STATE_DEFAULT)
				gameList[selectedItem]->SetState(STATE_SELECTED);
		}

		gameList[i]->Update(t);

		if(gameList[i]->GetState() == STATE_SELECTED)
		{
			selectedItem = i;
			browser.selIndex = browser.pageIndex + i;
		}
	}

	listChanged = false;

	if(updateCB)
		updateCB(this);
}
