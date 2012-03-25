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
 * This library was wholly designed and written by Tantric. Thanks to the
 * authors of PNGU and FreeTypeGX, of which this library makes use. Thanks
 * also to the authors of GRRLIB and libwiisprite for laying the foundations.
 *
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * kloverde    03/25/2012  New file (moved out of gui.h)
 */

#ifndef _GUI_BUTTON_H_
#define _GUI_BUTTON_H_

#include "gui_element.h"
#include "gui_image.h"
#include "gui_text.h"
#include "gui_sound.h"
#include "gui_tooltip.h"
#include "gui_trigger.h"

#include "pad.h"   // externs

//!Display, manage, and manipulate buttons in the GUI. Buttons can have images, icons, text, and sound set (all of which are optional)
class GuiButton : public GuiElement
{
	public:
		//!Constructor
		//!\param w Width
		//!\param h Height
		GuiButton(int w = 0, int h = 0);
		//!Destructor
		~GuiButton();
		//!Sets the button's image
		//!\param i Pointer to GuiImage object
		void SetImage(GuiImage* i);
		//!Sets the button's image on over
		//!\param i Pointer to GuiImage object
		void SetImageOver(GuiImage* i);
		//!Sets the button's image on hold
		//!\param i Pointer to GuiImage object
		void SetImageHold(GuiImage* i);
		//!Sets the button's image on click
		//!\param i Pointer to GuiImage object
		void SetImageClick(GuiImage* i);
		//!Sets the button's icon
		//!\param i Pointer to GuiImage object
		void SetIcon(GuiImage* i);
		//!Returns the button's icon
		GuiImage* GetIcon();
		//!Sets the button's icon on over
		//!\param i Pointer to GuiImage object
		void SetIconOver(GuiImage* i);
		//!Sets the button's icon on hold
		//!\param i Pointer to GuiImage object
		void SetIconHold(GuiImage* i);
		//!Sets the button's icon on click
		//!\param i Pointer to GuiImage object
		void SetIconClick(GuiImage* i);
		//!Sets the button's label
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void SetLabel(GuiText* t, int n = 0);
		//!Sets the button's label on over (eg: different colored text)
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void SetLabelOver(GuiText* t, int n = 0);
		//!Sets the button's label on hold
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void SetLabelHold(GuiText* t, int n = 0);
		//!Sets the button's label on click
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void SetLabelClick(GuiText* t, int n = 0);
		//!Sets the sound to play on over
		//!\param s Pointer to GuiSound object
		void SetSoundOver(GuiSound * s);
		//!Sets the sound to play on hold
		//!\param s Pointer to GuiSound object
		void SetSoundHold(GuiSound * s);
		//!Sets the sound to play on click
		//!\param s Pointer to GuiSound object
		void SetSoundClick(GuiSound * s);
		//!Sets the tooltip for the button
		//!\param t Tooltip
		void SetTooltip(GuiTooltip * t);
		//!Constantly called to draw the GuiButton
		void Draw();
		//!Constantly called to draw the GuiButton's tooltip
		void DrawTooltip();
		//!Resets the text for all contained elements
		void ResetText();
		//!Constantly called to allow the GuiButton to respond to updated input data
		//!\param t Pointer to a GuiTrigger, containing the current input data from PAD/WPAD
		void Update(GuiTrigger * t);
	protected:
		GuiImage * image; //!< Button image (default)
		GuiImage * imageOver; //!< Button image for STATE_SELECTED
		GuiImage * imageHold; //!< Button image for STATE_HELD
		GuiImage * imageClick; //!< Button image for STATE_CLICKED
		GuiImage * icon; //!< Button icon (drawn after button image)
		GuiImage * iconOver; //!< Button icon for STATE_SELECTED
		GuiImage * iconHold; //!< Button icon for STATE_HELD
		GuiImage * iconClick; //!< Button icon for STATE_CLICKED
		GuiText * label[3]; //!< Label(s) to display (default)
		GuiText * labelOver[3]; //!< Label(s) to display for STATE_SELECTED
		GuiText * labelHold[3]; //!< Label(s) to display for STATE_HELD
		GuiText * labelClick[3]; //!< Label(s) to display for STATE_CLICKED
		GuiSound * soundOver; //!< Sound to play for STATE_SELECTED
		GuiSound * soundHold; //!< Sound to play for STATE_HELD
		GuiSound * soundClick; //!< Sound to play for STATE_CLICKED
		GuiTooltip * tooltip; //!< Tooltip to display on over
};

#endif
