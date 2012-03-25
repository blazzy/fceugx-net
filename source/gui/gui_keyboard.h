#ifndef _GUI_KEYBOARD_H_
#define _GUI_KEYBOARD_H_

#include "gui_window.h"
#include "gui_trigger.h"
#include "gui_text.h"
#include "gui_image.h"
#include "gui_text.h"
#include "gui_button.h"
#include "gui_imagedata.h"
#include "gui_sound.h"
#include "gui_trigger.h"

#include "filelist.h"    // global images

#define MAX_KEYBOARD_DISPLAY	32

typedef struct _keytype {
	char ch, chShift;
} Key;

//!On-screen keyboard
class GuiKeyboard : public GuiWindow
{
	public:
		GuiKeyboard(char * t, u32 m);
		~GuiKeyboard();
		void Update(GuiTrigger * t);
		char kbtextstr[256];
	protected:
		u32 kbtextmaxlen;
		int shift;
		int caps;
		GuiText * kbText;
		GuiImage * keyTextboxImg;
		GuiText * keyCapsText;
		GuiImage * keyCapsImg;
		GuiImage * keyCapsOverImg;
		GuiButton * keyCaps;
		GuiText * keyShiftText;
		GuiImage * keyShiftImg;
		GuiImage * keyShiftOverImg;
		GuiButton * keyShift;
		GuiText * keyBackText;
		GuiImage * keyBackImg;
		GuiImage * keyBackOverImg;
		GuiButton * keyBack;
		GuiImage * keySpaceImg;
		GuiImage * keySpaceOverImg;
		GuiButton * keySpace;
		GuiButton * keyBtn[4][11];
		GuiImage * keyImg[4][11];
		GuiImage * keyImgOver[4][11];
		GuiText * keyTxt[4][11];
		GuiImageData * keyTextbox;
		GuiImageData * key;
		GuiImageData * keyOver;
		GuiImageData * keyMedium;
		GuiImageData * keyMediumOver;
		GuiImageData * keyLarge;
		GuiImageData * keyLargeOver;
		GuiSound * keySoundOver;
		GuiSound * keySoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trig2;
		Key keys[4][11]; // two chars = less space than one pointer
};

#endif
