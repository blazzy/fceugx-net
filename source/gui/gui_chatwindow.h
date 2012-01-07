#include "gui.h"

#ifndef _GUI_CHATWINDOW_
#define _GUI_CHATWINDOW_

#define MAX_CHAT_MSG_LEN      255
#define CHAT_SCROLLBACK_SIZE  1000

struct WindowInfo
{
	int numEntries;  // # of entries in window
	int selIndex;    // currently selected index
	int pageIndex;   // starting index of page display
	int size;        // # of entries browerList has space allocated to store
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
		void Reset();
		bool WriteLn(const char *msg);
	private:
		GuiButton *arrowUpBtn;
		GuiButton *arrowDownBtn;
		GuiButton *scrollbarBoxBtn;

		GuiImage *bgFileSelectionImg;
		GuiImage *scrollbarImg;
		GuiImage *arrowDownImg;
		GuiImage *arrowDownOverImg;
		GuiImage *arrowUpImg;
		GuiImage *arrowUpOverImg;
		GuiImage *scrollbarBoxImg;
		GuiImage *scrollbarBoxOverImg;

		GuiImageData *bgFileSelection;
		GuiImageData *scrollbar;
		GuiImageData *arrowDown;
		GuiImageData *arrowDownOver;
		GuiImageData *arrowUp;
		GuiImageData *arrowUpOver;
		GuiImageData *scrollbarBox;
		GuiImageData *scrollbarBoxOver;

		GuiSound *btnSoundOver;
		GuiSound *btnSoundClick;

		GuiTrigger *trigA;
		GuiTrigger *trig2;
		GuiTrigger *trigHeldA;

		int selectedItem;
		int numEntries;
		bool listChanged;

		WindowInfo windowInfo;
		GuiText *viewport[FILE_PAGESIZE];
		Message scrollbackBuffer[CHAT_SCROLLBACK_SIZE];
};

#endif
