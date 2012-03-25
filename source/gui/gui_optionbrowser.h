#ifndef _GUI_OPTIONBROWSER_H_
#define _GUI_OPTIONBROWSER_H_

#include "gui_element.h"
#include "gui_trigger.h"
#include "gui_text.h"
#include "gui_button.h"
#include "gui_image.h"
#include "gui_imagedata.h"
#include "gui_sound.h"
#include "gui_trigger.h"

#define MAX_OPTIONS 			150
#define OPTION_BROWSER_PAGESIZE 8

typedef struct _optionlist {
	int length;
	char name[MAX_OPTIONS][100];
	char value[MAX_OPTIONS][50];
} OptionList;

//!Display a list of menu options
class GuiOptionBrowser : public GuiElement
{
	public:
		GuiOptionBrowser(int w, int h, OptionList * l);
		~GuiOptionBrowser();
		void SetCol1Position(int x);
		void SetCol2Position(int x);
		int FindMenuItem(int c, int d);
		int GetClickedOption();
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void TriggerUpdate();
		void ResetText();
		void Update(GuiTrigger * t);
		GuiText * optionVal[OPTION_BROWSER_PAGESIZE];
	protected:
		int optionIndex[OPTION_BROWSER_PAGESIZE];
		GuiButton * optionBtn[OPTION_BROWSER_PAGESIZE];
		GuiText * optionTxt[OPTION_BROWSER_PAGESIZE];
		GuiImage * optionBg[OPTION_BROWSER_PAGESIZE];

		int selectedItem;
		int listOffset;
		OptionList * options;

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiImage * bgOptionsImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;

		GuiImageData * bgOptions;
		GuiImageData * bgOptionsEntry;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trig2;

		bool listChanged;
};

#endif
