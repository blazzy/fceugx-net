#ifndef _GUI_CHATWINDOW_H_
#define _GUI_CHATWINDOW_H_

#include "gui_element.h"
#include "gui_trigger.h"
#include "gui_button.h"
#include "gui_image.h"
#include "gui_imagedata.h"
#include "gui_sound.h"
#include "gui_trigger.h"
#include "gui_window.h"

#include "menu.h"

#define MAX_CHAT_MSG_LEN     255
#define CHAT_SCROLLBACK_SIZE 1000
#define CHAT_PAGESIZE        10

struct WindowInfo
{
	int numEntries; // # of entries in viewport
	int selIndex;   // currently selected index
	int pageIndex;  // starting index of page display (scrollbackBuffer[x] currently displayed in viewport[0])
	int size;       // # of messages in scrollbackBuffer
	int selectedItem;
};

struct Message
{
	char value[MAX_CHAT_MSG_LEN + 1];
};

class GuiChatWindow : public GuiElement
{
	public:
		GuiChatWindow(int w, int h);
		~GuiChatWindow();
		void ResetState();
		void SetFocus(int f);
		void Draw();;
		void DrawTooltip();
		void TriggerUpdate();
		void Update(GuiTrigger * t);
		GuiButton * fileList[CHAT_PAGESIZE];

		bool IsKeyboardHotspotClicked();
		bool WriteLn(const char *msg);
	//protected:
		GuiText * fileListText[CHAT_PAGESIZE];
		GuiImage * fileListBg[CHAT_PAGESIZE];
		GuiImage * fileListIcon[CHAT_PAGESIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiImage * bgFileSelectionImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;
		GuiImage * scrollbarBoxImg;
		GuiImage * scrollbarBoxOverImg;

		GuiImageData * bgFileSelection;
		GuiImageData * bgFileSelectionEntry;
		GuiImageData * iconFolder;
		GuiImageData * iconSD;
		GuiImageData * iconUSB;
		GuiImageData * iconDVD;
		GuiImageData * iconSMB;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;
		GuiImageData * scrollbarBox;
		GuiImageData * scrollbarBoxOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trig2;
		GuiTrigger * trigHeldA;

		int selectedItem;
		bool listChanged;

		WindowInfo windowInfo;
		Message scrollbackBuffer[CHAT_SCROLLBACK_SIZE];
};

#endif
