#ifndef _GUI_TEXT_H_
#define _GUI_TEXT_H_

#include "ogc/gx.h"
#include "utils/FreeTypeGX.h"

#include "gui_element.h"

extern FreeTypeGX *fontSystem[];

enum
{
	SCROLL_NONE,
	SCROLL_HORIZONTAL
};

//!Display, manage, and manipulate text in the GUI
class GuiText : public GuiElement
{
	public:
		//!Constructor
		//!\param t Text
		//!\param s Font size
		//!\param c Font color
		GuiText(const char * t, int s, GXColor c);
		//!\overload
		//!Assumes SetPresets() has been called to setup preferred text attributes
		//!\param t Text
		GuiText(const char * t);
		//!Destructor
		~GuiText();
		//!Sets the text of the GuiText element
		//!\param t Text
		void SetText(const char * t);
		//!Sets the text of the GuiText element
		//!\param t UTF-8 Text
		void SetWText(wchar_t * t);
		//!Gets the translated text length of the GuiText element
		int GetLength();
		//!Sets up preset values to be used by GuiText(t)
		//!Useful when printing multiple text elements, all with the same attributes set
		//!\param sz Font size
		//!\param c Font color
		//!\param w Maximum width of texture image (for text wrapping)
		//!\param s Font size
		//!\param h Text alignment (horizontal)
		//!\param v Text alignment (vertical)
		void SetPresets(int sz, GXColor c, int w, u16 s, int h, int v);
		//!Sets the font size
		//!\param s Font size
		void SetFontSize(int s);
		//!Sets the maximum width of the drawn texture image
		//!\param w Maximum width
		void SetMaxWidth(int w);
		//!Gets the width of the text when rendered
		int GetTextWidth();
		//!Enables/disables text scrolling
		//!\param s Scrolling on/off
		void SetScroll(int s);
		//!Enables/disables text wrapping
		//!\param w Wrapping on/off
		//!\param width Maximum width (0 to disable)
		void SetWrap(bool w, int width = 0);
		//!Sets the font color
		//!\param c Font color
		void SetColor(GXColor c);
		//!Sets the FreeTypeGX style attributes
		//!\param s Style attributes
		void SetStyle(u16 s);
		//!Sets the text alignment
		//!\param hor Horizontal alignment (ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTRE)
		//!\param vert Vertical alignment (ALIGN_TOP, ALIGN_BOTTOM, ALIGN_MIDDLE)
		void SetAlignment(int hor, int vert);
		//!Updates the text to the selected language
		void ResetText();
		//!Constantly called to draw the text
		void Draw();
		//!Returns a copy of this object's backing array
		char *ToString();
	protected:
		GXColor color; //!< Font color
		wchar_t* text; //!< Translated Unicode text value
		wchar_t *textDyn[20]; //!< Text value, if max width, scrolling, or wrapping enabled
		int textDynNum; //!< Number of text lines
		char * origText; //!< Original text data (English)
		int size; //!< Font size
		int maxWidth; //!< Maximum width of the generated text object (for text wrapping)
		int textScroll; //!< Scrolling toggle
		int textScrollPos; //!< Current starting index of text string for scrolling
		int textScrollInitialDelay; //!< Delay to wait before starting to scroll
		int textScrollDelay; //!< Scrolling speed
		u16 style; //!< FreeTypeGX style attributes
		bool wrap; //!< Wrapping toggle
};

#endif
