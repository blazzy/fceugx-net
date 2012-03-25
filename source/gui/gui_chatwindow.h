/*!\mainpage libwiigui Documentation
 *
 * \section Introduction
 * libwiigui is a GUI library for the Wii, created to help structure the
 * design of a complicated GUI interface, and to enable an author to create
 * a sophisticated, feature-rich GUI. It was originally conceived and written
 * after I started to design a GUI for Snes9x GX, and found libwiisprite and
 * GRRLIB inadequate for the purpose. It uses GX for drawing, and makes use
 * of PNGU for displaying images and FreeTypeGX for text. It was designed to
 * be flexible and is easy to modify - don't be afraid to change the way it
 * works or expand it to suit your GUI's purposes! If you do, and you think
 * your changes might benefit others, please share them so they might be
 * added to the project!
 *
 * \section Quickstart
 * Start from the supplied template example. For more advanced uses, see the
 * source code for Snes9x GX, FCE Ultra GX, and Visual Boy Advance GX.
 *
 * \section Contact
 * If you have any suggestions for the library or documentation, or want to
 * contribute, please visit the libwiigui website:
 * http://code.google.com/p/libwiigui/
 *
 * \section Credits
 * LibWiiGUI was wholly designed and written by Tantric, not including this
 * class, which was written by Kurtis LoVerde.  Thanks to the authors of PNGU
 * and FreeTypeGX, of which this library makes use. Thanks also to the authors
 * of GRRLIB and libwiisprite for laying the foundations.
 *
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * kloverde    03/25/2012  New class
 */

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
