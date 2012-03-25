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

#ifndef _GUI_TOOLTIP_H_
#define _GUI_TOOLTIP_H_

#include "gui_element.h"
#include "gui_image.h"
#include "gui_text.h"

//!Display, manage, and manipulate tooltips in the GUI
class GuiTooltip : public GuiElement
{
	public:
		//!Constructor
		//!\param t Text
		GuiTooltip(const char *t);
		//!Destructor
		~GuiTooltip();
		//!Gets the element's current scale
		float GetScale();
		//!Sets the text of the GuiTooltip element
		//!\param t Text
		void SetText(const char * t);
		//!Constantly called to draw the GuiTooltip
		void DrawTooltip();

		time_t time1, time2; //!< Tooltip times

	protected:
		GuiImage leftImage; //!< Tooltip left image
		GuiImage tileImage; //!< Tooltip tile image
		GuiImage rightImage; //!< Tooltip right image
		GuiText *text; //!< Tooltip text
};

#endif
