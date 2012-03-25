#ifndef _GUI_CHATWINDOW_
#define _GUI_CHATWINDOW_

#define MAX_CHAT_MSG_LEN 255
#define CHAT_SCROLLBACK_SIZE 1000

#include "gui.h"
#include "menu.h"

struct WindowInfo
{
	int numEntries; // # of entries in viewport
	int selIndex; // currently selected index
	int pageIndex; // starting index of page display (scrollbackBuffer[x] currently displayed in viewport[0])
	int size; // # of messages in scrollbackBuffer
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
		void Draw();
		void DrawTooltip();
		void TriggerUpdate();
		void Update(GuiTrigger * t);
		GuiButton * fileList[FILE_PAGESIZE];

		bool IsKeyboardHotspotClicked();
		bool WriteLn(const char *msg);
	protected:
		GuiText * fileListText[FILE_PAGESIZE];
		GuiImage * fileListBg[FILE_PAGESIZE];
		GuiImage * fileListIcon[FILE_PAGESIZE];

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
