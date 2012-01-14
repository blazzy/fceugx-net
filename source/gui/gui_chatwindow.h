#include "gui.h"

#ifndef _GUI_CHATWINDOW_
#define _GUI_CHATWINDOW_

#define MAX_CHAT_MSG_LEN      255
#define CHAT_SCROLLBACK_SIZE  1000

struct WindowInfo
{
	int  numEntries;  // # of entries in viewport
	int  selIndex;    // currently selected index
	int  pageIndex;   // starting index of page display (scrollbackBuffer[x] currently displayed in viewport[0])
	int  size;        // # of messages in scrollbackBuffer
};

struct Message
{
	char value[MAX_CHAT_MSG_LEN + 1];
};

class GuiChatWindow : public GuiWindow
{
	public:
		GuiChatWindow(int w, int h);
		~GuiChatWindow();
		int  GetState();
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void DrawTooltip();
		void TriggerUpdate();
		void Update(GuiTrigger *t);
		void Reset();
		bool WriteLn(const char *msg);
		void SetVisible(bool vis);

		GuiImage *fileListBg[FILE_PAGESIZE];
		GuiImageData *bgFileSelectionEntry;
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
		Message scrollbackBuffer[CHAT_SCROLLBACK_SIZE];
		GuiText *viewportText[FILE_PAGESIZE];
	public: GuiButton *viewportButton[FILE_PAGESIZE];

		bool dirty;
};

#endif
