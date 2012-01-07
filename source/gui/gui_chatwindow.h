#include "gui.h"

#ifndef _GUI_CHATWINDOW_
#define _GUI_CHATWINDOW_

#define MAX_CHAT_MSG_LEN		255
#define CHAT_SCROLLBACK_SIZE	1000

typedef struct
{
	char dir[MAXPATHLEN + 1]; // directory path of browserList
	int numEntries; // # of entries in browserList
	int selIndex; // currently selected index of browserList
	int pageIndex; // starting index of browserList page display
	int size; // # of entries browerList has space allocated to store
} BROWSERINFO_chat;

typedef struct
{
	char displayname[MAX_CHAT_MSG_LEN + 1];
} BROWSERENTRY_chat;


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
		bool Add(char *msg);
	private:
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
		int numEntries;
		bool listChanged;

		BROWSERINFO_chat browser_chat;
		BROWSERENTRY_chat browserList_chat[CHAT_SCROLLBACK_SIZE];
};

#endif
