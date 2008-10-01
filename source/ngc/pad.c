/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * pad.c
 *
 * Controller input
 ****************************************************************************/

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <math.h>
#include "driver.h"
#include "fceu.h"
#include "input.h"

#include "fceuconfig.h"
#include "pad.h"
#include "gcaudio.h"
#include "menu.h"
#include "fceustate.h"
#include "fceuram.h"
#include "filesel.h"

extern bool romLoaded;

// Original NES controller buttons
// All other pads are mapped to this
unsigned int nespadmap[] = {
	JOY_B, JOY_A,
	JOY_SELECT, JOY_START,
	JOY_UP, JOY_DOWN,
	JOY_LEFT, JOY_RIGHT,
	0 // insert coin for VS games, insert/eject/select disk for FDS
};

/*** Gamecube controller Padmap ***/
unsigned int gcpadmap[] = {
	PAD_BUTTON_B, PAD_BUTTON_A,
	PAD_TRIGGER_L, PAD_TRIGGER_R,
	PAD_BUTTON_UP, PAD_BUTTON_DOWN,
	PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
	PAD_TRIGGER_Z // insert coin for VS games
};
/*** Wiimote Padmap ***/
unsigned int wmpadmap[] = {
	WPAD_BUTTON_1, WPAD_BUTTON_2,
	WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
	WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT,
	WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
	WPAD_BUTTON_A // insert coin for VS games
};
/*** Classic Controller Padmap ***/
unsigned int ccpadmap[] = {
	WPAD_CLASSIC_BUTTON_Y, WPAD_CLASSIC_BUTTON_B,
	WPAD_CLASSIC_BUTTON_MINUS, WPAD_CLASSIC_BUTTON_PLUS,
	WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
	WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT,
	WPAD_CLASSIC_BUTTON_FULL_R // insert coin for VS games
};
/*** Nunchuk + wiimote Padmap ***/
unsigned int ncpadmap[] = {
	WPAD_NUNCHUK_BUTTON_C, WPAD_NUNCHUK_BUTTON_Z,
	WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
	WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
	WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
	WPAD_BUTTON_A // insert coin for VS games
};

static uint32 JSReturn = 0;
void *InputDPR;

INPUTC *zapperdata[2];
unsigned int myzappers[2][3];

extern INPUTC *FCEU_InitZapper(int w);

/****************************************************************************
 * Initialise Pads
 ****************************************************************************/
void InitialisePads()
{
	InputDPR = &JSReturn;
	FCEUI_SetInput(0, SI_GAMEPAD, InputDPR, 0);
	FCEUI_SetInput(1, SI_GAMEPAD, InputDPR, 0);

	ToggleFourScore(GCSettings.FSDisable);
	ToggleZapper(GCSettings.zapper);
}

void ToggleFourScore(int set)
{
	if(romLoaded)
		FCEUI_DisableFourScore(set);
}

void ToggleZapper(int set)
{
	if(romLoaded)
	{
		// set defaults
		zapperdata[0]=NULL;
		zapperdata[1]=NULL;
		myzappers[0][0]=myzappers[1][0]=128;
		myzappers[0][1]=myzappers[1][1]=120;
		myzappers[0][2]=myzappers[1][2]=0;

		// Default ports back to gamepad
		FCEUI_SetInput(0, SI_GAMEPAD, InputDPR, 0);
		FCEUI_SetInput(1, SI_GAMEPAD, InputDPR, 0);

		if(set)
		{
			// enable Zapper
			zapperdata[set-1] = FCEU_InitZapper(set-1);
			FCEUI_SetInput(set-1, SI_ZAPPER, myzappers[set-1], 1);
		}
	}
}

s8 WPAD_StickX(u8 chan,u8 right)
{
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (right == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (right == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * sin((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}

s8 WPAD_StickY(u8 chan, u8 right)
{
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (right == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

			case WPAD_EXP_CLASSIC:
			if (right == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * cos((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}

// hold zapper cursor positions
int pos_x = 0;
int pos_y = 0;

void UpdateCursorPosition (int pad)
{
	#define ZAPPERPADCAL 20

	// gc left joystick
	signed char pad_x = PAD_StickX (pad);
	signed char pad_y = PAD_StickY (pad);

	if (pad_x > ZAPPERPADCAL){
		pos_x += (pad_x*1.0)/ZAPPERPADCAL;
		if (pos_x > 256) pos_x = 256;
	}
	if (pad_x < -ZAPPERPADCAL){
		pos_x -= (pad_x*-1.0)/ZAPPERPADCAL;
		if (pos_x < 0) pos_x = 0;
	}

	if (pad_y < -ZAPPERPADCAL){
		pos_y += (pad_y*-1.0)/ZAPPERPADCAL;
		if (pos_y > 224) pos_y = 224;
	}
	if (pad_y > ZAPPERPADCAL){
		pos_y -= (pad_y*1.0)/ZAPPERPADCAL;
		if (pos_y < 0) pos_y = 0;
	}

#ifdef HW_RVL
	struct ir_t ir;		// wiimote ir
	WPAD_IR(pad, &ir);
	if (ir.valid)
	{
		pos_x = (ir.x * 256) / 640;
		pos_y = (ir.y * 224) / 480;
	}
	else
	{
		signed char wm_ax = WPAD_StickX (pad, 0);
		signed char wm_ay = WPAD_StickY (pad, 0);

		if (wm_ax > ZAPPERPADCAL){
			pos_x += (wm_ax*1.0)/ZAPPERPADCAL;
			if (pos_x > 256) pos_x = 256;
		}
		if (wm_ax < -ZAPPERPADCAL){
			pos_x -= (wm_ax*-1.0)/ZAPPERPADCAL;
			if (pos_x < 0) pos_x = 0;
		}

		if (wm_ay < -ZAPPERPADCAL){
			pos_y += (wm_ay*-1.0)/ZAPPERPADCAL;
			if (pos_y > 224) pos_y = 224;
		}
		if (wm_ay > ZAPPERPADCAL){
			pos_y -= (wm_ay*1.0)/ZAPPERPADCAL;
			if (pos_y < 0) pos_y = 0;
		}
	}
#endif
}

/****************************************************************************
 * Convert GC Joystick Readings to JOY
 ****************************************************************************/
u8 PADTUR = 2;

unsigned char DecodeJoy( unsigned short pad )
{
	signed char pad_x = PAD_StickX (pad);
	signed char pad_y = PAD_StickY (pad);
	u32 jp = PAD_ButtonsHeld (pad);
	unsigned char J = 0;

	#ifdef HW_RVL
	signed char wm_ax = 0;
	signed char wm_ay = 0;
	u32 wp = 0;
	wm_ax = WPAD_StickX ((u8)pad, 0);
	wm_ay = WPAD_StickY ((u8)pad, 0);
	wp = WPAD_ButtonsHeld (pad);

	u32 exp_type;
	if ( WPAD_Probe(pad, &exp_type) != 0 ) exp_type = WPAD_EXP_NONE;
	#endif

	/***
	Gamecube Joystick input
	***/
	// Is XY inside the "zone"?
	if (pad_x * pad_x + pad_y * pad_y > PADCAL * PADCAL)
	{
		if (pad_x > 0 && pad_y == 0) J |= JOY_RIGHT;
		if (pad_x < 0 && pad_y == 0) J |= JOY_LEFT;
		if (pad_x == 0 && pad_y > 0) J |= JOY_UP;
		if (pad_x == 0 && pad_y < 0) J |= JOY_DOWN;

		if (pad_x != 0 && pad_y != 0)
		{
			if ((float)pad_y / pad_x >= -2.41421356237 && (float)pad_y / pad_x < 2.41421356237)
			{
				if (pad_x >= 0)
					J |= JOY_RIGHT;
				else
					J |= JOY_LEFT;
			}

			if ((float)pad_x / pad_y >= -2.41421356237 && (float)pad_x / pad_y < 2.41421356237)
			{
				if (pad_y >= 0)
					J |= JOY_UP;
				else
					J |= JOY_DOWN;
			}
		}
	}
#ifdef HW_RVL
	/***
	Wii Joystick (classic, nunchuk) input
	***/
	// Is XY inside the "zone"?
	if (wm_ax * wm_ax + wm_ay * wm_ay > PADCAL * PADCAL)
	{
		/*** we don't want division by zero ***/
		if (wm_ax > 0 && wm_ay == 0)
			J |= JOY_RIGHT;
		if (wm_ax < 0 && wm_ay == 0)
			J |= JOY_LEFT;
		if (wm_ax == 0 && wm_ay > 0)
			J |= JOY_UP;
		if (wm_ax == 0 && wm_ay < 0)
			J |= JOY_DOWN;

		if (wm_ax != 0 && wm_ay != 0)
		{

			/*** Recalc left / right ***/
			float t;

			t = (float) wm_ay / wm_ax;
			if (t >= -2.41421356237 && t < 2.41421356237)
			{
				if (wm_ax >= 0)
					J |= JOY_RIGHT;
				else
					J |= JOY_LEFT;
			}

			/*** Recalc up / down ***/
			t = (float) wm_ax / wm_ay;
			if (t >= -2.41421356237 && t < 2.41421356237)
			{
				if (wm_ay >= 0)
					J |= JOY_UP;
				else
					J |= JOY_DOWN;
			}
		}
	}
#endif

	/*** Report pressed buttons (gamepads) ***/
	int i;

	for (i = 0; i < MAXJP; i++)
	{
		if ( (jp & gcpadmap[i])											// gamecube controller
		#ifdef HW_RVL
		|| ( (exp_type == WPAD_EXP_NONE) && (wp & wmpadmap[i]) )	// wiimote
		|| ( (exp_type == WPAD_EXP_CLASSIC) && (wp & ccpadmap[i]) )	// classic controller
		|| ( (exp_type == WPAD_EXP_NUNCHUK) && (wp & ncpadmap[i]) )	// nunchuk + wiimote
		#endif
		)
		{
			if(nespadmap[i] > 0)
				J |= nespadmap[i];
			else
			{
				if(nesGameType == 4) // FDS
				{
					/* these commands shouldn't be issued in parallel but this
					 * allows us to only map one button for both!
					 * the gamer must just have to press the button twice */
					FCEUI_FDSInsert(0); // eject / insert disk
					FCEUI_FDSSelect(); // select other side
				}
				else
					FCEU_DoSimpleCommand(0x07); // insert coin for VS Games
			}
		}
	}

	// zapper enabled
	if(GCSettings.zapper)
	{
		int z = GCSettings.zapper-1; // NES port # (0 or 1)

		myzappers[z][2] = 0; // reset trigger to not pressed

		// is trigger pressed?
		if ( (jp & PAD_BUTTON_A) // gamecube controller
		#ifdef HW_RVL
		|| (wp & WPAD_BUTTON_A)	// wiimote
		|| (wp & WPAD_BUTTON_B)
		|| (wp & WPAD_CLASSIC_BUTTON_A) // classic controller
		#endif
		)
		{
			// report trigger press
			myzappers[z][2] |= 2;
		}

		// cursor position
		UpdateCursorPosition(0); // update cursor for wiimote 1
		myzappers[z][0] = pos_x;
		myzappers[z][1] = pos_y;

		// Report changes to FCE Ultra
		zapperdata[z]->Update(z,myzappers[z],0);
	}

	return J;
}

void GetJoy()
{
    JSReturn = 0; // reset buttons pressed
	unsigned char pad[4];
    short i;

    s8 gc_px = PAD_SubStickX (0);
    u32 jp = PAD_ButtonsHeld (0); // gamecube controller button info

    #ifdef HW_RVL
    s8 wm_sx = WPAD_StickX (0,1);
    u32 wm_pb = WPAD_ButtonsHeld (0); // wiimote / expansion button info
    #endif

    // request to go back to menu
    if ((gc_px < -70) || (jp & PAD_BUTTON_START)
    #ifdef HW_RVL
    		 || (wm_pb & WPAD_BUTTON_HOME)
    		 || (wm_pb & WPAD_CLASSIC_BUTTON_HOME)
    		 || (wm_sx < -70)
    #endif
    )
	{
    	StopAudio();

    	if (GCSettings.AutoSave == 1)
    	{
    		SaveRAM(GCSettings.SaveMethod, SILENT);
    	}
    	else if (GCSettings.AutoSave == 2)
    	{
			SaveState(GCSettings.SaveMethod, SILENT);
    	}
    	else if(GCSettings.AutoSave == 3)
    	{
    		SaveRAM(GCSettings.SaveMethod, SILENT);
    		SaveState(GCSettings.SaveMethod, SILENT);
    	}

    	MainMenu(4);
	}
	else
	{
		for (i = 0; i < 4; i++)
			pad[i] = DecodeJoy(i);

		JSReturn = pad[0] | pad[1] << 8 | pad[2] << 16 | pad[3] << 24;
	}
}
