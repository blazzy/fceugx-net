#ifndef _GUI_SAVEBROWSER_H_
#define _GUI_SAVEBROWSER_H_

#include "gui_element.h"
#include "gui_imagedata.h"
#include "gui_button.h"
#include "gui_image.h"
#include "gui_imagedata.h"
#include "gui_sound.h"
#include "gui_trigger.h"

#define SAVELISTSIZE 			6
#define MAX_SAVES 				100

typedef struct _savelist {
	int length;
	char filename[MAX_SAVES+1][256];
	GuiImageData * previewImg[MAX_SAVES+1];
	char date[MAX_SAVES+1][20];
	char time[MAX_SAVES+1][10];
	int type[MAX_SAVES+1];
	int files[2][MAX_SAVES+1];
} SaveList;

//!Display a list of game save files, with screenshots and file information
class GuiSaveBrowser : public GuiElement
{
	public:
		GuiSaveBrowser(int w, int h, SaveList * l, int a);
		~GuiSaveBrowser();
		int GetClickedSave();
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void Update(GuiTrigger * t);
	protected:
		int selectedItem;
		int action;
		int listOffset;
		SaveList * saves;
		GuiButton * saveBtn[SAVELISTSIZE];
		GuiText * saveDate[SAVELISTSIZE];
		GuiText * saveTime[SAVELISTSIZE];
		GuiText * saveType[SAVELISTSIZE];

		GuiImage * saveBgImg[SAVELISTSIZE];
		GuiImage * saveBgOverImg[SAVELISTSIZE];
		GuiImage * savePreviewImg[SAVELISTSIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;

		GuiImageData * gameSave;
		GuiImageData * gameSaveOver;
		GuiImageData * gameSaveBlank;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trig2;

		bool saveBtnLastOver[SAVELISTSIZE];
};

#endif
