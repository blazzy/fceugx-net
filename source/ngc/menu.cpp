/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * menu.c
 *
 * Main menu flow control
 ****************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "common.h"
#include "fceugx.h"
#include "fceuconfig.h"
#include "pad.h"
#include "gcvideo.h"
#include "filebrowser.h"
#include "gcunzip.h"
#include "networkop.h"
#include "memcardop.h"
#include "fileop.h"
#include "dvd.h"
#include "fceuram.h"
#include "fceustate.h"
#include "preferences.h"
#include "button_mapping.h"
#include "filelist.h"
#include "gui/gui.h"
#include "menu.h"
#include "fceuload.h"

#ifdef HW_RVL
static GuiImageData * pointer[4];
#endif

static GuiButton * btnLogo = NULL;
static GuiImage * gameScreenImg = NULL;
static GuiImage * bgImg = NULL;
static GuiImage * bgTopImg = NULL;
static GuiImage * bgBottomImg = NULL;
static GuiSound * bgMusic = NULL;
static GuiWindow * mainWindow = NULL;
static GuiText * settingText = NULL;
static int lastMenu = MENU_NONE;
static int mapMenuCtrl = 0;
static int mapMenuCtrlNES = 0;

static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;
static lwp_t progressthread = LWP_THREAD_NULL;
static int showProgress = 0;

static char progressTitle[100];
static char progressMsg[200];
static int progressDone = 0;
static int progressTotal = 0;

static void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

static void
HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(50);
}

/****************************************************************************
 * WindowPrompt
 ***************************************************************************/

int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);
	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetMaxWidth(430);

	GuiText btn1Txt(btn1Label, 24, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 24, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	CancelAction();
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		VIDEO_WaitVSync();

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}

/****************************************************************************
 * UpdateGUI
 ***************************************************************************/

/*static u32 arena1mem = 0;
static u32 arena2mem = 0;
static char mem[150] = { 0 };
static GuiText * memTxt = NULL;*/

static void *
UpdateGUI (void *arg)
{
	while(1)
	{
		if(guiHalt)
		{
			LWP_SuspendThread(guithread);
		}
		else
		{
			/*arena1mem = (u32)SYS_GetArena1Hi() - (u32)SYS_GetArena1Lo();
			#ifdef HW_RVL
			arena2mem = (u32)SYS_GetArena2Hi() - (u32)SYS_GetArena2Lo();
			#endif
			sprintf(mem, "A1: %u / A2: %u", arena1mem, arena2mem);
			if(memTxt) memTxt->SetText(mem);*/

			mainWindow->Draw();

			#ifdef HW_RVL
			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad.ir.valid)
					Menu_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
				DoRumble(i);
			}
			#endif

			Menu_Render();

			for(int i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);

			#ifdef HW_RVL
			if(updateFound)
			{
				updateFound = WindowPrompt(
					"Update Available",
					"An update is available!",
					"Update now",
					"Update later");
				if(updateFound)
					if(DownloadUpdate())
						ExitRequested = 1;
			}
			#endif

			if(ExitRequested || ShutdownRequested)
			{
				for(int a = 0; a < 255; a += 15)
				{
					mainWindow->Draw();
					Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, a},1);
					Menu_Render();
				}

				if(ExitRequested)
					ExitApp();
				#ifdef HW_RVL
				else if(ShutdownRequested)
					ShutdownWii();
				#endif
			}
		}
	}
	return NULL;
}

/****************************************************************************
 * ProgressWindow
 ***************************************************************************/

static void
ProgressWindow(char *title, char *msg)
{
	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiImageData progressbarOutline(progressbar_outline_png);
	GuiImage progressbarOutlineImg(&progressbarOutline);
	progressbarOutlineImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarOutlineImg.SetPosition(25, 40);

	GuiImageData progressbarEmpty(progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarEmptyImg.SetPosition(25, 40);
	progressbarEmptyImg.SetTile(100);

	GuiImageData progressbar(progressbar_png);
	GuiImage progressbarImg(&progressbar);
	progressbarImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarImg.SetPosition(25, 40);

	GuiImageData throbber(throbber_png);
	GuiImage throbberImg(&throbber);
	throbberImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	throbberImg.SetPosition(0, 40);

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);
	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	if(showProgress == 1)
	{
		promptWindow.Append(&progressbarEmptyImg);
		promptWindow.Append(&progressbarImg);
		promptWindow.Append(&progressbarOutlineImg);
	}
	else
	{
		promptWindow.Append(&throbberImg);
	}

	usleep(400000); // wait to see if progress flag changes soon
	if(!showProgress)
		return;

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	float angle = 0;
	u32 count = 0;

	while(showProgress)
	{
		VIDEO_WaitVSync();

		if(showProgress == 1)
		{
			progressbarImg.SetTile(100*progressDone/progressTotal);
		}
		else if(showProgress == 2)
		{
			if(count % 5 == 0)
			{
				angle+=45;
				if(angle >= 360)
					angle = 0;
				throbberImg.SetAngle(angle);
			}
			count++;
		}
	}

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

static void * ProgressThread (void *arg)
{
	while(1)
	{
		if(!showProgress)
			LWP_SuspendThread (progressthread);

		ProgressWindow(progressTitle, progressMsg);
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 ***************************************************************************/

void
InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
	LWP_CreateThread (&progressthread, ProgressThread, NULL, NULL, 0, 40);
}

/****************************************************************************
 * Progress Bar
 *
 * Show the user what's happening
 ***************************************************************************/

void
CancelAction()
{
	showProgress = 0;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(progressthread))
		usleep(50);
}

void
ShowProgress (const char *msg, int done, int total)
{
	if(total < (256*1024))
		return;
	else if(done > total) // this shouldn't happen
		done = total;

	if(done/total > 0.99)
		done = total;

	if(showProgress != 1)
		CancelAction(); // wait for previous progress window to finish

	strncpy(progressMsg, msg, 200);
	sprintf(progressTitle, "Please Wait");
	showProgress = 1;
	progressTotal = total;
	progressDone = done;
	LWP_ResumeThread (progressthread);
}

void
ShowAction (const char *msg)
{
	if(showProgress != 2)
		CancelAction(); // wait for previous progress window to finish

	strncpy(progressMsg, msg, 200);
	sprintf(progressTitle, "Please Wait");
	showProgress = 2;
	progressDone = 0;
	progressTotal = 0;
	LWP_ResumeThread (progressthread);
}

void ErrorPrompt(const char *msg)
{
	WindowPrompt("Error", msg, "OK", NULL);
}

int ErrorPromptRetry(const char *msg)
{
	return WindowPrompt("Error", msg, "Retry", "Cancel");
}

void InfoPrompt(const char *msg)
{
	WindowPrompt("Information", msg, "OK", NULL);
}

void AutoSave()
{
	if (GCSettings.AutoSave == 1)
	{
		SaveRAMAuto(GCSettings.SaveMethod, SILENT);
	}
	else if (GCSettings.AutoSave == 2)
	{
		if (WindowPrompt("Save", "Save State?", "Save", "Don't Save") )
			SaveStateAuto(GCSettings.SaveMethod, NOTSILENT);
	}
	else if (GCSettings.AutoSave == 3)
	{
		if (WindowPrompt("Save", "Save RAM and State?", "Save", "Don't Save") )
		{
			SaveRAMAuto(GCSettings.SaveMethod, NOTSILENT);
			SaveStateAuto(GCSettings.SaveMethod, NOTSILENT);
		}
	}
}

static void OnScreenKeyboard(char * var)
{
	int save = -1;

	GuiKeyboard keyboard(var);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 24, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		VIDEO_WaitVSync();

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		strncpy(var, keyboard.kbtextstr, 100);
		var[100] = 0;
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

static int
SettingWindow(const char * title, GuiWindow * w)
{
	int save = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);

	GuiText okBtnTxt("OK", 24, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(20, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-20, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&okBtn);
	promptWindow.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->Append(w);
	mainWindow->ChangeFocus(w);
	ResumeGui();

	while(save == -1)
	{
		VIDEO_WaitVSync();

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->Remove(w);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return save;
}

/****************************************************************************
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/

static void WindowCredits(void * ptr)
{
	if(btnLogo->GetState() != STATE_CLICKED)
		return;

	btnLogo->ResetState();

	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindow(screenwidth,screenheight);
	GuiWindow creditsWindowBox(580,448);
	creditsWindowBox.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	creditsWindowBox.Append(&creditsBoxImg);

	int numEntries = 23;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 30, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=32;

	txt[i] = new GuiText("Official Site: http://code.google.com/p/fceugc/", 20, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=40;

	txt[i]->SetPresets(22, (GXColor){0, 0, 0, 255}, 0,
			FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP, ALIGN_LEFT, ALIGN_TOP);

	txt[i] = new GuiText("Coding & menu design");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Tantric");
	txt[i]->SetPosition(320,y); i++; y+=24;
	txt[i] = new GuiText("Menu artwork");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("the3seashells");
	txt[i]->SetPosition(320,y); i++; y+=24;
	txt[i] = new GuiText("Menu sound");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Peter de Man");
	txt[i]->SetPosition(320,y); i++; y+=48;

	txt[i] = new GuiText("FCE Ultra GX GameCube");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("SoftDev, askot,");
	txt[i]->SetPosition(320,y); i++; y+=24;
	txt[i] = new GuiText("dsbomb, others");
	txt[i]->SetPosition(320,y); i++; y+=24;
	txt[i] = new GuiText("FCE Ultra");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Xodnizel");
	txt[i]->SetPosition(320,y); i++; y+=24;
	txt[i] = new GuiText("Original FCE");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("BERO");
	txt[i]->SetPosition(320,y); i++; y+=24;

	txt[i] = new GuiText("libogc / devkitPPC");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("shagkur & wintermute");
	txt[i]->SetPosition(320,y); i++; y+=24;
	txt[i] = new GuiText("FreeTypeGX");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Armin Tamzarian");
	txt[i]->SetPosition(320,y); i++; y+=48;

	txt[i]->SetPresets(18, (GXColor){0, 0, 0, 255}, 0,
		FTGX_JUSTIFY_CENTER | FTGX_ALIGN_TOP, ALIGN_CENTRE, ALIGN_TOP);

	txt[i] = new GuiText("This software is open source and may be copied,");
	txt[i]->SetPosition(0,y); i++; y+=20;
	txt[i] = new GuiText("distributed, or modified under the terms of the");
	txt[i]->SetPosition(0,y); i++; y+=20;
	txt[i] = new GuiText("GNU General Public License (GPL) Version 2.");
	txt[i]->SetPosition(0,y); i++; y+=20;

	for(i=0; i < numEntries; i++)
		creditsWindowBox.Append(txt[i]);

	creditsWindow.Append(&creditsWindowBox);

	while(!exit)
	{
		if(gameScreenImg)
			gameScreenImg->Draw();
		else
			bgImg->Draw();

		bgBottomImg->Draw();
		bgTopImg->Draw();
		creditsWindow.Draw();

		for(i=3; i >= 0; i--)
		{
			#ifdef HW_RVL
			if(userInput[i].wpad.ir.valid)
				Menu_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48,
					96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
			DoRumble(i);
			#endif
		}

		Menu_Render();

		for(i=0; i < 4; i++)
		{
			if(userInput[i].wpad.btns_d || userInput[i].pad.btns_d)
				exit = true;
		}
	}

	// clear buttons pressed
	for(i=0; i < 4; i++)
	{
		userInput[i].wpad.btns_d = 0;
		userInput[i].pad.btns_d = 0;
	}

	for(i=0; i < numEntries; i++)
		delete txt[i];
}

/****************************************************************************
 * MenuGameSelection
 ***************************************************************************/

static int MenuGameSelection()
{
	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	// populate initial directory listing
	if(OpenGameList() <= 0)
	{
		int choice = WindowPrompt(
		"Error",
		"Games directory is inaccessible on selected load device.",
		"Retry",
		"Check Settings");

		if(choice)
			return MENU_GAMESELECTION;
		else
			return MENU_SETTINGS_FILE;
	}

	int menu = MENU_NONE;

	GuiText titleTxt("Choose Game", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData iconHome(icon_home_png);
	GuiImageData iconSettings(icon_settings_png);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText settingsBtnTxt("Settings", 24, (GXColor){0, 0, 0, 255});
	GuiImage settingsBtnIcon(&iconSettings);
	settingsBtnIcon.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	settingsBtnIcon.SetPosition(20,0);
	GuiImage settingsBtnImg(&btnOutline);
	GuiImage settingsBtnImgOver(&btnOutlineOver);
	GuiButton settingsBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	settingsBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	settingsBtn.SetPosition(100, -35);
	settingsBtn.SetLabel(&settingsBtnTxt);
	settingsBtn.SetIcon(&settingsBtnIcon);
	settingsBtn.SetImage(&settingsBtnImg);
	settingsBtn.SetImageOver(&settingsBtnImgOver);
	settingsBtn.SetSoundOver(&btnSoundOver);
	settingsBtn.SetTrigger(&trigA);
	settingsBtn.SetEffectGrow();

	GuiText exitBtnTxt("Exit", 24, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnIcon(&iconHome);
	exitBtnIcon.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	exitBtnIcon.SetPosition(20,0);
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	exitBtn.SetPosition(-100, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetIcon(&exitBtnIcon);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetTrigger(&trigHome);
	exitBtn.SetEffectGrow();

	GuiWindow buttonWindow(screenwidth, screenheight);
	buttonWindow.Append(&settingsBtn);
	buttonWindow.Append(&exitBtn);

	GuiFileBrowser gameBrowser(424, 248);
	gameBrowser.SetPosition(50, 108);

	HaltGui();
	btnLogo->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	btnLogo->SetPosition(-30, 30);
	mainWindow->Append(&titleTxt);
	mainWindow->Append(&gameBrowser);
	mainWindow->Append(&buttonWindow);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync();

		// update gameWindow based on arrow buttons
		// set MENU_EXIT if A button pressed on a game
		for(int i=0; i<PAGESIZE; i++)
		{
			if(gameBrowser.gameList[i]->GetState() == STATE_CLICKED)
			{
				gameBrowser.gameList[i]->ResetState();
				// check corresponding browser entry
				if(browserList[browser.selIndex].isdir || IsSz())
				{
					bool res;

					if(IsSz())
						res = BrowserLoadSz(GCSettings.LoadMethod);
					else
						res = BrowserChangeFolder(GCSettings.LoadMethod);

					if(res)
					{
						gameBrowser.ResetState();
						gameBrowser.gameList[0]->SetState(STATE_SELECTED);
						gameBrowser.TriggerUpdate();
					}
					else
					{
						menu = MENU_GAMESELECTION;
						break;
					}
				}
				else
				{
					#ifdef HW_RVL
					ShutoffRumble();
					#endif
					mainWindow->SetState(STATE_DISABLED);
					if(BrowserLoadFile(GCSettings.LoadMethod))
						menu = MENU_EXIT;
					mainWindow->SetState(STATE_DEFAULT);
				}
			}
		}

		if(settingsBtn.GetState() == STATE_CLICKED)
			menu = MENU_SETTINGS;
		else if(exitBtn.GetState() == STATE_CLICKED)
			ExitRequested = 1;
	}
	HaltGui();
	mainWindow->Remove(&titleTxt);
	mainWindow->Remove(&buttonWindow);
	mainWindow->Remove(&gameBrowser);
	return menu;
}

static void ControllerWindowUpdate(void * ptr, int dir)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.Controller += dir;

		if(GCSettings.Controller == CTRL_PAD) // skip
			GCSettings.Controller += dir;

		if(GCSettings.Controller > CTRL_LENGTH-1)
			GCSettings.Controller = 0;
		else if(GCSettings.Controller < 0)
			GCSettings.Controller = CTRL_LENGTH-1;

		settingText->SetText(ctrlName[GCSettings.Controller]);
		b->ResetState();
	}
}

static void ControllerWindowLeftClick(void * ptr) { ControllerWindowUpdate(ptr, -1); }
static void ControllerWindowRightClick(void * ptr) { ControllerWindowUpdate(ptr, +1); }

static void ControllerWindow()
{
	GuiWindow * w = new GuiWindow(250,250);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ControllerWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ControllerWindowRightClick);

	settingText = new GuiText(ctrlName[GCSettings.Controller], 24, (GXColor){0, 0, 0, 255});

	int currentController = GCSettings.Controller;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(settingText);

	if(!SettingWindow("Controller",w))
		GCSettings.Controller = currentController; // undo changes

	delete(w);
	delete(settingText);
}

/****************************************************************************
 * MenuGame
 ***************************************************************************/

static int MenuGame()
{
	int menu = MENU_NONE;

	GuiText titleTxt((char *)romFilename, 24, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_close_png);
	GuiImageData btnCloseOutlineOver(button_close_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData battery(battery_png);
	GuiImageData batteryRed(battery_red_png);
	GuiImageData batteryBar(battery_bar_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText saveBtnTxt("Save", 24, (GXColor){0, 0, 0, 255});
	GuiImage saveBtnImg(&btnLargeOutline);
	GuiImage saveBtnImgOver(&btnLargeOutlineOver);
	GuiButton saveBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	saveBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	saveBtn.SetPosition(50, 120);
	saveBtn.SetLabel(&saveBtnTxt);
	saveBtn.SetImage(&saveBtnImg);
	saveBtn.SetImageOver(&saveBtnImgOver);
	saveBtn.SetSoundOver(&btnSoundOver);
	saveBtn.SetTrigger(&trigA);
	saveBtn.SetEffectGrow();

	GuiText loadBtnTxt("Load", 24, (GXColor){0, 0, 0, 255});
	GuiImage loadBtnImg(&btnLargeOutline);
	GuiImage loadBtnImgOver(&btnLargeOutlineOver);
	GuiButton loadBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	loadBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	loadBtn.SetPosition(0, 120);
	loadBtn.SetLabel(&loadBtnTxt);
	loadBtn.SetImage(&loadBtnImg);
	loadBtn.SetImageOver(&loadBtnImgOver);
	loadBtn.SetSoundOver(&btnSoundOver);
	loadBtn.SetTrigger(&trigA);
	loadBtn.SetEffectGrow();

	GuiText resetBtnTxt("Reset", 24, (GXColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnLargeOutline);
	GuiImage resetBtnImgOver(&btnLargeOutlineOver);
	GuiButton resetBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	resetBtn.SetPosition(-50, 120);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);
	resetBtn.SetEffectGrow();

	GuiText controllerBtnTxt("Controller", 24, (GXColor){0, 0, 0, 255});
	GuiImage controllerBtnImg(&btnLargeOutline);
	GuiImage controllerBtnImgOver(&btnLargeOutlineOver);
	GuiButton controllerBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	controllerBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	controllerBtn.SetPosition(-125, 250);
	controllerBtn.SetLabel(&controllerBtnTxt);
	controllerBtn.SetImage(&controllerBtnImg);
	controllerBtn.SetImageOver(&controllerBtnImgOver);
	controllerBtn.SetSoundOver(&btnSoundOver);
	controllerBtn.SetTrigger(&trigA);
	controllerBtn.SetEffectGrow();

	/*GuiText cheatsBtnTxt("Cheats", 24, (GXColor){0, 0, 0, 255});
	GuiImage cheatsBtnImg(&btnLargeOutline);
	GuiImage cheatsBtnImgOver(&btnLargeOutlineOver);
	GuiButton cheatsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	cheatsBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	cheatsBtn.SetPosition(125, 250);
	cheatsBtn.SetLabel(&cheatsBtnTxt);
	cheatsBtn.SetImage(&cheatsBtnImg);
	cheatsBtn.SetImageOver(&cheatsBtnImgOver);
	cheatsBtn.SetSoundOver(&btnSoundOver);
	cheatsBtn.SetTrigger(&trigA);
	cheatsBtn.SetEffectGrow();*/

	GuiText mainmenuBtnTxt("Main Menu", 24, (GXColor){0, 0, 0, 255});
	GuiImage mainmenuBtnImg(&btnOutline);
	GuiImage mainmenuBtnImgOver(&btnOutlineOver);
	GuiButton mainmenuBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	mainmenuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	mainmenuBtn.SetPosition(0, -35);
	mainmenuBtn.SetLabel(&mainmenuBtnTxt);
	mainmenuBtn.SetImage(&mainmenuBtnImg);
	mainmenuBtn.SetImageOver(&mainmenuBtnImgOver);
	mainmenuBtn.SetSoundOver(&btnSoundOver);
	mainmenuBtn.SetTrigger(&trigA);
	mainmenuBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-30, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	#ifdef HW_RVL
	int i = 0;
	char txt[3];
	GuiText * batteryTxt[4];
	GuiImage * batteryImg[4];
	GuiImage * batteryBarImg[4];
	GuiButton * batteryBtn[4];

	for(i=0; i < 4; i++)
	{
		if(i == 0)
			sprintf(txt, "P %d", i+1);
		else
			sprintf(txt, "P%d", i+1);

		batteryTxt[i] = new GuiText(txt, 22, (GXColor){255, 255, 255, 255});
		batteryTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i]->SetPosition(30, 0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->SetTile(0);
		batteryBarImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryBarImg[i]->SetPosition(34, 0);

		batteryBtn[i] = new GuiButton(70, 20);
		batteryBtn[i]->SetLabel(batteryTxt[i]);
		batteryBtn[i]->SetImage(batteryImg[i]);
		batteryBtn[i]->SetIcon(batteryBarImg[i]);
		batteryBtn[i]->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		batteryBtn[i]->SetRumble(false);
		batteryBtn[i]->SetAlpha(150);
	}

	batteryBtn[0]->SetPosition(45, -65);
	batteryBtn[1]->SetPosition(135, -65);
	batteryBtn[2]->SetPosition(45, -40);
	batteryBtn[3]->SetPosition(135, -40);
	#endif

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&saveBtn);
	w.Append(&loadBtn);
	w.Append(&resetBtn);
	w.Append(&controllerBtn);
	//w.Append(&cheatsBtn);

	#ifdef HW_RVL
	w.Append(batteryBtn[0]);
	w.Append(batteryBtn[1]);
	w.Append(batteryBtn[2]);
	w.Append(batteryBtn[3]);
	#endif

	w.Append(&mainmenuBtn);
	w.Append(&closeBtn);

	btnLogo->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btnLogo->SetPosition(-30, -40);
	mainWindow->Append(&w);

	if(lastMenu == MENU_NONE)
	{
		bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		mainmenuBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		#ifdef HW_RVL
		batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		#endif

		saveBtn.SetEffect(EFFECT_FADE, 15);
		loadBtn.SetEffect(EFFECT_FADE, 15);
		resetBtn.SetEffect(EFFECT_FADE, 15);
		controllerBtn.SetEffect(EFFECT_FADE, 15);
		//cheatsBtn.SetEffect(EFFECT_FADE, 15);

		AutoSave();
	}

	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync();

		#ifdef HW_RVL
		int level;
		for(i=0; i < 4; i++)
		{
			if(WPAD_Probe(i, NULL) == WPAD_ERR_NONE) // controller connected
			{
				level = (userInput[i].wpad.battery_level / 100.0) * 4;
					batteryBarImg[i]->SetTile(level);

				if(level == 0)
					batteryImg[i]->SetImage(&batteryRed);
				else
					batteryImg[i]->SetImage(&battery);

				batteryBtn[i]->SetAlpha(255);
			}
			else // controller not connected
			{
				batteryBarImg[i]->SetTile(0);
				batteryImg[i]->SetImage(&battery);
				batteryBtn[i]->SetAlpha(150);
			}
		}
		#endif

		if(saveBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_SAVE;
		}
		else if(loadBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_LOAD;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			PowerNES();
			menu = MENU_EXIT;
		}
		else if(controllerBtn.GetState() == STATE_CLICKED)
		{
			ControllerWindow();
		}
		/*else if(cheatsBtn.GetState() == STATE_CLICKED)
		{
			cheatsBtn.ResetState();
			if(Cheat.num_cheats > 0)
				menu = MENU_GAME_CHEATS;
			else
				InfoPrompt("Cheats file not found!");
		}*/
		else if(mainmenuBtn.GetState() == STATE_CLICKED)
		{
			if(gameScreenImg)
			{
				mainWindow->Remove(gameScreenImg);
				delete gameScreenImg;
				gameScreenImg = NULL;
			}
			if(gameScreenTex)
			{
				free(gameScreenTex);
				gameScreenTex = NULL;
			}
			bgImg->SetVisible(true);
			#ifndef NO_SOUND
			bgMusic->Play(); // startup music
			#endif
			menu = MENU_GAMESELECTION;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			mainmenuBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			#ifdef HW_RVL
			batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			#endif

			saveBtn.SetEffect(EFFECT_FADE, -15);
			loadBtn.SetEffect(EFFECT_FADE, -15);
			resetBtn.SetEffect(EFFECT_FADE, -15);
			controllerBtn.SetEffect(EFFECT_FADE, -15);
			//cheatsBtn.SetEffect(EFFECT_FADE, -15);

			usleep(150000); // wait for effects to finish
		}
	}

	HaltGui();

	#ifdef HW_RVL
	for(i=0; i < 4; i++)
	{
		delete batteryTxt[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryBtn[i];
	}
	#endif

	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuGameSaves
 ***************************************************************************/

static int MenuGameSaves(int action)
{
	int menu = MENU_NONE;
	int ret;
	int i, len, len2;
	int j = 0;
	SaveList saves;
	char filepath[1024];
	int method = GCSettings.SaveMethod;

	if(method == METHOD_AUTO)
		autoSaveMethod(NOTSILENT);

	if(!ChangeInterface(method, NOTSILENT))
		return MENU_GAME;

	GuiText titleTxt(NULL, 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	if(action == 0)
		titleTxt.SetText("Load Game");
	else
		titleTxt.SetText("Save Game");

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_close_png);
	GuiImageData btnCloseOutlineOver(button_close_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-30, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	w.Append(&closeBtn);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	memset(&saves, 0, sizeof(saves));

	if(method == METHOD_MC_SLOTA)
	{
		ParseMCDirectory(CARD_SLOTA);
	}
	else if(method == METHOD_MC_SLOTB)
	{
		ParseMCDirectory(CARD_SLOTB);
	}
	else
	{
		strncpy(browser.dir, GCSettings.SaveFolder, 200);
		ParseDirectory(GCSettings.SaveMethod);
	}

	for(i=0; i < browser.numEntries; i++)
	{
		len = strlen(romFilename);
		len2 = strlen(browserList[i].filename);

		if(len > 26 && (method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB))
			len = 26; // memory card filenames are a maximum of 32 chars

		// find matching files
		if(len2 > 5 && strncmp(browserList[i].filename, romFilename, len) == 0)
		{
			if(strncmp(&browserList[i].filename[len2-4], ".sav", 4) == 0)
				saves.type[j] = FILE_RAM;
			else if(strncmp(&browserList[i].filename[len2-4], ".fcs", 4) == 0)
				saves.type[j] = FILE_STATE;
			else
				saves.type[j] = -1;

			if(saves.type[j] != -1)
			{
				int n = -1;
				char tmp[256];
				strncpy(tmp, browserList[i].filename, 255);
				tmp[len2-4] = 0;

				if(len2 - len == 7)
					n = atoi(&tmp[len2-5]);
				else if(len2 - len == 6)
					n = atoi(&tmp[len2-6]);

				if(n > 0 && n < MAX_SAVES)
					saves.files[saves.type[j]][n] = 1;

				strncpy(saves.filename[j], browserList[i].filename, 255);

				if(method != METHOD_MC_SLOTA && method != METHOD_MC_SLOTB)
				{
					if(saves.type[j] == FILE_STATE)
					{
						char scrfile[1024];
						sprintf(scrfile, "%s/%s.png", GCSettings.SaveFolder, tmp);

						AllocSaveBuffer();
						if(LoadFile(scrfile, GCSettings.SaveMethod, SILENT))
							saves.previewImg[j] = new GuiImageData(savebuffer);
						FreeSaveBuffer();
					}
					struct tm * timeinfo = localtime(&browserList[j].mtime);
					strftime(saves.date[j], 20, "%a %b %d", timeinfo);
					strftime(saves.time[j], 10, "%I:%M %p", timeinfo);
				}
				j++;
			}
		}
	}

	saves.length = j;

	if(saves.length == 0 && action == 0)
	{
		InfoPrompt("No game saves found.");
		menu = MENU_GAME;
	}

	GuiSaveBrowser saveBrowser(552, 248, &saves, action);
	saveBrowser.SetPosition(0, 108);
	saveBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	mainWindow->Append(&saveBrowser);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		ret = saveBrowser.GetClickedSave();

		// load or save game
		if(ret >= 0)
		{
			int result = 0;

			if(action == 0) // load
			{
				MakeFilePath(filepath, saves.type[ret], method, saves.filename[ret]);
				switch(saves.type[ret])
				{
					case FILE_RAM:
						result = LoadRAM(filepath, GCSettings.SaveMethod, NOTSILENT);
						break;
					case FILE_STATE:
						result = LoadState(filepath, GCSettings.SaveMethod, NOTSILENT);
						break;
				}
				if(result)
					menu = MENU_EXIT;
			}
			else // save
			{
				if(ret == 0) // new SRAM
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_RAM][i] == 0)
							break;

					if(i < 100)
					{
						MakeFilePath(filepath, FILE_RAM, method, romFilename, i);
						SaveRAM(filepath, GCSettings.SaveMethod, NOTSILENT);
						menu = MENU_GAME_SAVE;
					}
				}
				else if(ret == 1) // new Snapshot
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_STATE][i] == 0)
							break;

					if(i < 100)
					{
						MakeFilePath(filepath, FILE_STATE, method, romFilename, i);
						SaveState(filepath, GCSettings.SaveMethod, NOTSILENT);
						menu = MENU_GAME_SAVE;
					}
				}
				else // overwrite SRAM/Snapshot
				{
					MakeFilePath(filepath, saves.type[ret-2], method, saves.filename[ret-2]);
					switch(saves.type[ret-2])
					{
						case FILE_RAM:
							SaveRAM(filepath, GCSettings.SaveMethod, NOTSILENT);
							break;
						case FILE_STATE:
							SaveState(filepath, GCSettings.SaveMethod, NOTSILENT);
							break;
					}
					menu = MENU_GAME_SAVE;
				}
			}
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			backBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);

			w.SetEffect(EFFECT_FADE, -15);

			usleep(150000); // wait for effects to finish
		}
	}

	HaltGui();

	for(i=0; i < saves.length; i++)
		if(saves.previewImg[i])
			delete saves.previewImg[i];

	mainWindow->Remove(&saveBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuGameCheats
 ***************************************************************************/
/*
static int MenuGameCheats()
{
	int menu = MENU_NONE;
	int ret;
	u16 i = 0;
	OptionList options;

	for(i=0; i < Cheat.num_cheats; i++)
		sprintf (options.name[i], "%s", Cheat.c[i].name);

	options.length = i;

	GuiText titleTxt("Cheats", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_close_png);
	GuiImageData btnCloseOutlineOver(button_close_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-30, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	w.Append(&closeBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		for(i=0; i < Cheat.num_cheats; i++)
			sprintf (options.value[i], "%s", Cheat.c[i].enabled == true ? "On" : "Off");

		ret = optionBrowser.GetClickedOption();

		if(Cheat.c[ret].enabled)
			S9xDisableCheat(ret);
		else
			S9xEnableCheat(ret);

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 35);
			backBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 35);

			w.SetEffect(EFFECT_FADE, -15);

			usleep(150000); // wait for effects to finish
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}
*/
/****************************************************************************
 * MenuSettings
 ***************************************************************************/

static int MenuSettings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Settings", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText mappingBtnTxt("Button Mappings", 24, (GXColor){0, 0, 0, 255});
	mappingBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage mappingBtnImg(&btnLargeOutline);
	GuiImage mappingBtnImgOver(&btnLargeOutlineOver);
	GuiButton mappingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	mappingBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	mappingBtn.SetPosition(50, 120);
	mappingBtn.SetLabel(&mappingBtnTxt);
	mappingBtn.SetImage(&mappingBtnImg);
	mappingBtn.SetImageOver(&mappingBtnImgOver);
	mappingBtn.SetSoundOver(&btnSoundOver);
	mappingBtn.SetTrigger(&trigA);
	mappingBtn.SetEffectGrow();

	GuiText videoBtnTxt("Video", 24, (GXColor){0, 0, 0, 255});
	videoBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage videoBtnImg(&btnLargeOutline);
	GuiImage videoBtnImgOver(&btnLargeOutlineOver);
	GuiButton videoBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	videoBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	videoBtn.SetPosition(0, 120);
	videoBtn.SetLabel(&videoBtnTxt);
	videoBtn.SetImage(&videoBtnImg);
	videoBtn.SetImageOver(&videoBtnImgOver);
	videoBtn.SetSoundOver(&btnSoundOver);
	videoBtn.SetTrigger(&trigA);
	videoBtn.SetEffectGrow();

	GuiText savingBtnTxt1("Saving", 24, (GXColor){0, 0, 0, 255});
	GuiText savingBtnTxt2("&", 18, (GXColor){0, 0, 0, 255});
	GuiText savingBtnTxt3("Loading", 24, (GXColor){0, 0, 0, 255});
	savingBtnTxt1.SetPosition(0, -20);
	savingBtnTxt3.SetPosition(0, +20);
	GuiImage savingBtnImg(&btnLargeOutline);
	GuiImage savingBtnImgOver(&btnLargeOutlineOver);
	GuiButton savingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	savingBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	savingBtn.SetPosition(-50, 120);
	savingBtn.SetLabel(&savingBtnTxt1, 0);
	savingBtn.SetLabel(&savingBtnTxt2, 1);
	savingBtn.SetLabel(&savingBtnTxt3, 2);
	savingBtn.SetImage(&savingBtnImg);
	savingBtn.SetImageOver(&savingBtnImgOver);
	savingBtn.SetSoundOver(&btnSoundOver);
	savingBtn.SetTrigger(&trigA);
	savingBtn.SetEffectGrow();

	GuiText menuBtnTxt("Menu", 24, (GXColor){0, 0, 0, 255});
	menuBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage menuBtnImg(&btnLargeOutline);
	GuiImage menuBtnImgOver(&btnLargeOutlineOver);
	GuiButton menuBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	menuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	menuBtn.SetPosition(-125, 250);
	menuBtn.SetLabel(&menuBtnTxt);
	menuBtn.SetImage(&menuBtnImg);
	menuBtn.SetImageOver(&menuBtnImgOver);
	menuBtn.SetSoundOver(&btnSoundOver);
	menuBtn.SetTrigger(&trigA);
	menuBtn.SetEffectGrow();

	GuiText networkBtnTxt("Network", 24, (GXColor){0, 0, 0, 255});
	networkBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage networkBtnImg(&btnLargeOutline);
	GuiImage networkBtnImgOver(&btnLargeOutlineOver);
	GuiButton networkBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	networkBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	networkBtn.SetPosition(125, 250);
	networkBtn.SetLabel(&networkBtnTxt);
	networkBtn.SetImage(&networkBtnImg);
	networkBtn.SetImageOver(&networkBtnImgOver);
	networkBtn.SetSoundOver(&btnSoundOver);
	networkBtn.SetTrigger(&trigA);
	networkBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiText resetBtnTxt("Reset Settings", 24, (GXColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnOutline);
	GuiImage resetBtnImgOver(&btnOutlineOver);
	GuiButton resetBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	resetBtn.SetPosition(-100, -35);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);
	resetBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&mappingBtn);
	w.Append(&videoBtn);
	w.Append(&savingBtn);
	w.Append(&menuBtn);

#ifdef HW_RVL
	w.Append(&networkBtn);
#endif

	w.Append(&backBtn);
	w.Append(&resetBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		if(mappingBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS;
		}
		else if(videoBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_VIDEO;
		}
		else if(savingBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_FILE;
		}
		else if(menuBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MENU;
		}
		else if(networkBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_NETWORK;
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESELECTION;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			resetBtn.ResetState();

			int choice = WindowPrompt(
				"Reset Settings",
				"Are you sure that you want to reset your settings?",
				"Yes",
				"No");

			if(choice == 1)
				DefaultSettings();
		}
	}

	if(menu == MENU_GAMESELECTION)
		SavePrefs(NOTSILENT);

	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuSettingsMappings
 ***************************************************************************/

static int MenuSettingsMappings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Settings - Button Mappings", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText nesBtnTxt("NES Controller", 24, (GXColor){0, 0, 0, 255});
	nesBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage nesBtnImg(&btnLargeOutline);
	GuiImage nesBtnImgOver(&btnLargeOutlineOver);
	GuiButton nesBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	nesBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	nesBtn.SetPosition(-125, 120);
	nesBtn.SetLabel(&nesBtnTxt);
	nesBtn.SetImage(&nesBtnImg);
	nesBtn.SetImageOver(&nesBtnImgOver);
	nesBtn.SetSoundOver(&btnSoundOver);
	nesBtn.SetTrigger(&trigA);
	nesBtn.SetEffectGrow();

	GuiText zapperBtnTxt("Zapper", 24, (GXColor){0, 0, 0, 255});
	zapperBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage zapperBtnImg(&btnLargeOutline);
	GuiImage zapperBtnImgOver(&btnLargeOutlineOver);
	GuiButton zapperBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	zapperBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	zapperBtn.SetPosition(125, 120);
	zapperBtn.SetLabel(&zapperBtnTxt);
	zapperBtn.SetImage(&zapperBtnImg);
	zapperBtn.SetImageOver(&zapperBtnImgOver);
	zapperBtn.SetSoundOver(&btnSoundOver);
	zapperBtn.SetTrigger(&trigA);
	zapperBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&nesBtn);
	w.Append(&zapperBtn);

	w.Append(&backBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		if(nesBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlNES = CTRL_PAD;
		}
		else if(zapperBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlNES = CTRL_ZAPPER;
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

static int MenuSettingsMappingsController()
{
	int menu = MENU_NONE;
	char menuTitle[100];
	char menuSubtitle[100];

	sprintf(menuTitle, "Settings - Button Mappings");
	GuiText titleTxt(menuTitle, 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,30);

	sprintf(menuSubtitle, "%s", ctrlName[mapMenuCtrlNES]);
	GuiText subtitleTxt(menuSubtitle, 22, (GXColor){255, 255, 255, 255});
	subtitleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	subtitleTxt.SetPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText gamecubeBtnTxt("GameCube Controller", 24, (GXColor){0, 0, 0, 255});
	gamecubeBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage gamecubeBtnImg(&btnLargeOutline);
	GuiImage gamecubeBtnImgOver(&btnLargeOutlineOver);
	GuiButton gamecubeBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	gamecubeBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	gamecubeBtn.SetPosition(-125, 120);
	gamecubeBtn.SetLabel(&gamecubeBtnTxt);
	gamecubeBtn.SetImage(&gamecubeBtnImg);
	gamecubeBtn.SetImageOver(&gamecubeBtnImgOver);
	gamecubeBtn.SetSoundOver(&btnSoundOver);
	gamecubeBtn.SetTrigger(&trigA);
	gamecubeBtn.SetEffectGrow();

	GuiText wiimoteBtnTxt("Wiimote", 24, (GXColor){0, 0, 0, 255});
	GuiImage wiimoteBtnImg(&btnLargeOutline);
	GuiImage wiimoteBtnImgOver(&btnLargeOutlineOver);
	GuiButton wiimoteBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	wiimoteBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	wiimoteBtn.SetPosition(125, 120);
	wiimoteBtn.SetLabel(&wiimoteBtnTxt);
	wiimoteBtn.SetImage(&wiimoteBtnImg);
	wiimoteBtn.SetImageOver(&wiimoteBtnImgOver);
	wiimoteBtn.SetSoundOver(&btnSoundOver);
	wiimoteBtn.SetTrigger(&trigA);
	wiimoteBtn.SetEffectGrow();

	GuiText classicBtnTxt("Classic Controller", 24, (GXColor){0, 0, 0, 255});
	classicBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage classicBtnImg(&btnLargeOutline);
	GuiImage classicBtnImgOver(&btnLargeOutlineOver);
	GuiButton classicBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	classicBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	classicBtn.SetPosition(-125, 250);
	classicBtn.SetLabel(&classicBtnTxt);
	classicBtn.SetImage(&classicBtnImg);
	classicBtn.SetImageOver(&classicBtnImgOver);
	classicBtn.SetSoundOver(&btnSoundOver);
	classicBtn.SetTrigger(&trigA);
	classicBtn.SetEffectGrow();

	GuiText nunchukBtnTxt("Wiimote Nunchuk", 24, (GXColor){0, 0, 0, 255});
	nunchukBtnTxt.SetMaxWidth(btnLargeOutline.GetWidth()-30);
	GuiImage nunchukBtnImg(&btnLargeOutline);
	GuiImage nunchukBtnImgOver(&btnLargeOutlineOver);
	GuiButton nunchukBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	nunchukBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	nunchukBtn.SetPosition(125, 250);
	nunchukBtn.SetLabel(&nunchukBtnTxt);
	nunchukBtn.SetImage(&nunchukBtnImg);
	nunchukBtn.SetImageOver(&nunchukBtnImgOver);
	nunchukBtn.SetSoundOver(&btnSoundOver);
	nunchukBtn.SetTrigger(&trigA);
	nunchukBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&subtitleTxt);

	w.Append(&gamecubeBtn);
#ifdef HW_RVL
	w.Append(&wiimoteBtn);

	if(mapMenuCtrlNES == CTRL_PAD)
	{
		w.Append(&nunchukBtn);
		w.Append(&classicBtn);
	}
#endif
	w.Append(&backBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		if(wiimoteBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_WIIMOTE;
		}
		else if(nunchukBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_NUNCHUK;
		}
		else if(classicBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_CLASSIC;
		}
		else if(gamecubeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_GCPAD;
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * ButtonMappingWindow
 ***************************************************************************/

static u32
ButtonMappingWindow()
{
	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt("Button Mapping", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);

	char msg[200];

	switch(mapMenuCtrl)
	{
		case CTRLR_GCPAD:
			#ifdef HW_RVL
			sprintf(msg, "Press any button on the GameCube Controller now. Press Home or the C-Stick in any direction to cancel.");
			#else
			sprintf(msg, "Press any button on the GameCube Controller now. Press the C-Stick in any direction to cancel.");
			#endif
			break;
		case CTRLR_WIIMOTE:
			sprintf(msg, "Press any button on the Wiimote now. Press Home to cancel.");
			break;
		case CTRLR_CLASSIC:
			sprintf(msg, "Press any button on the Classic Controller now. Press Home to cancel.");
			break;
		case CTRLR_NUNCHUK:
			sprintf(msg, "Press any button on the Wiimote or Nunchuk now. Press Home to cancel.");
			break;
	}

	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetMaxWidth(430);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	u32 pressed = 0;

	while(pressed == 0)
	{
		VIDEO_WaitVSync();

		if(mapMenuCtrl == CTRLR_GCPAD)
		{
			pressed = userInput[0].pad.btns_d;


			if(userInput[0].pad.substickX < -70 ||
					userInput[0].pad.substickX > 70 ||
					userInput[0].pad.substickY < -70 ||
										userInput[0].pad.substickY > 70)
				pressed = WPAD_BUTTON_HOME;

			if(userInput[0].wpad.btns_d == WPAD_BUTTON_HOME)
				pressed = WPAD_BUTTON_HOME;
		}
		else
		{
			pressed = userInput[0].wpad.btns_d;

			// always allow Home button to be pressed to cancel
			if(pressed != WPAD_BUTTON_HOME)
			{
				switch(mapMenuCtrl)
				{
					case CTRLR_WIIMOTE:
						if(pressed > 0x1000)
							pressed = 0; // not a valid input
						break;

					case CTRLR_CLASSIC:
						if(userInput[0].wpad.exp.type != WPAD_EXP_CLASSIC)
							pressed = 0; // not a valid input
						else if(pressed <= 0x1000)
							pressed = 0; // not a valid input
						break;

					case CTRLR_NUNCHUK:
						if(userInput[0].wpad.exp.type != WPAD_EXP_NUNCHUK)
							pressed = 0; // not a valid input
						break;
				}
			}
		}
	}

	if(pressed == WPAD_BUTTON_HOME
		|| pressed == WPAD_CLASSIC_BUTTON_HOME)
		pressed = 0;

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();

	return pressed;
}

static int MenuSettingsMappingsMap()
{
	int menu = MENU_NONE;
	int ret,i,j;
	OptionList options;

	char menuTitle[100];
	char menuSubtitle[100];
	sprintf(menuTitle, "Settings - Button Mappings");

	GuiText titleTxt(menuTitle, 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,30);

	sprintf(menuSubtitle, "%s - %s", ctrlName[mapMenuCtrlNES], ctrlrName[mapMenuCtrl]);
	GuiText subtitleTxt(menuSubtitle, 22, (GXColor){255, 255, 255, 255});
	subtitleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	subtitleTxt.SetPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	i=0;

	switch(mapMenuCtrlNES)
	{
		case CTRL_PAD:
			sprintf(options.name[i++], "B");
			sprintf(options.name[i++], "A");
			sprintf(options.name[i++], "B (Rapid)");
			sprintf(options.name[i++], "A (Rapid)");
			sprintf(options.name[i++], "Select");
			sprintf(options.name[i++], "Start");
			sprintf(options.name[i++], "Up");
			sprintf(options.name[i++], "Down");
			sprintf(options.name[i++], "Left");
			sprintf(options.name[i++], "Right");
			sprintf(options.name[i++], "Insert Coin / Switch Disk");
			options.length = i;
			break;
		case CTRL_ZAPPER:
			sprintf(options.name[i++], "Fire");
			sprintf(options.name[i++], "Insert Coin");
			options.length = i;
			break;
	};

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(180);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	mainWindow->Append(&subtitleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		for(i=0; i < options.length; i++)
		{
			options.value[i][0] = 0;

			for(j=0; j < ctrlr_def[mapMenuCtrl].num_btns; j++)
			{
				if(btnmap[mapMenuCtrlNES][mapMenuCtrl][i] ==
					ctrlr_def[mapMenuCtrl].map[j].btn)
				{
					sprintf(options.value[i], ctrlr_def[mapMenuCtrl].map[j].name);
					break;
				}
			}
		}

		ret = optionBrowser.GetClickedOption();

		if(ret >= 0)
		{
			u32 pressed = ButtonMappingWindow(); // get a button selection from user

			if (pressed > 0)
				btnmap[mapMenuCtrlNES][mapMenuCtrl][ret] = pressed; // update mapping
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS_CTRL;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	mainWindow->Remove(&subtitleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsVideo
 ***************************************************************************/

static void ScreenZoomWindowUpdate(void * ptr, float amount)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.ZoomLevel += amount;

		char zoom[10];
		sprintf(zoom, "%.2f%%", GCSettings.ZoomLevel*100);
		settingText->SetText(zoom);
		b->ResetState();
	}
}

static void ScreenZoomWindowLeftClick(void * ptr) { ScreenZoomWindowUpdate(ptr, -0.01); }
static void ScreenZoomWindowRightClick(void * ptr) { ScreenZoomWindowUpdate(ptr, +0.01); }

static void ScreenZoomWindow()
{
	GuiWindow * w = new GuiWindow(250,250);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenZoomWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenZoomWindowRightClick);

	settingText = new GuiText(NULL, 22, (GXColor){0, 0, 0, 255});
	char zoom[10];
	sprintf(zoom, "%.2f%%", GCSettings.ZoomLevel*100);
	settingText->SetText(zoom);

	float currentZoom = GCSettings.ZoomLevel;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(settingText);

	if(!SettingWindow("Screen Zoom",w))
		GCSettings.ZoomLevel = currentZoom; // undo changes

	delete(w);
	delete(settingText);
}

static void ScreenPositionWindowUpdate(void * ptr, int x, int y)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.xshift += x;
		GCSettings.yshift += y;

		char shift[10];
		sprintf(shift, "%i, %i", GCSettings.xshift, GCSettings.yshift);
		settingText->SetText(shift);
		b->ResetState();
	}
}

static void ScreenPositionWindowLeftClick(void * ptr) { ScreenPositionWindowUpdate(ptr, -1, 0); }
static void ScreenPositionWindowRightClick(void * ptr) { ScreenPositionWindowUpdate(ptr, +1, 0); }
static void ScreenPositionWindowUpClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, -1); }
static void ScreenPositionWindowDownClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, +1); }

static void ScreenPositionWindow()
{
	GuiWindow * w = new GuiWindow(150,150);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	w->SetPosition(0, -10);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiTrigger trigUp;
	trigUp.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP, PAD_BUTTON_UP);

	GuiTrigger trigDown;
	trigDown.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN, PAD_BUTTON_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenPositionWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenPositionWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	arrowUpBtn.SetImage(&arrowUpImg);
	arrowUpBtn.SetImageOver(&arrowUpOverImg);
	arrowUpBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	arrowUpBtn.SetTrigger(0, &trigA);
	arrowUpBtn.SetTrigger(1, &trigUp);
	arrowUpBtn.SetSelectable(false);
	arrowUpBtn.SetUpdateCallback(ScreenPositionWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	arrowDownBtn.SetImage(&arrowDownImg);
	arrowDownBtn.SetImageOver(&arrowDownOverImg);
	arrowDownBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	arrowDownBtn.SetTrigger(0, &trigA);
	arrowDownBtn.SetTrigger(1, &trigDown);
	arrowDownBtn.SetSelectable(false);
	arrowDownBtn.SetUpdateCallback(ScreenPositionWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	settingText = new GuiText(NULL, 22, (GXColor){0, 0, 0, 255});
	char shift[10];
	sprintf(shift, "%i, %i", GCSettings.xshift, GCSettings.yshift);
	settingText->SetText(shift);

	int currentX = GCSettings.xshift;
	int currentY = GCSettings.yshift;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(&arrowUpBtn);
	w->Append(&arrowDownBtn);
	w->Append(&screenPositionImg);
	w->Append(settingText);

	if(!SettingWindow("Screen Position",w))
	{
		GCSettings.xshift = currentX; // undo changes
		GCSettings.yshift = currentY;
	}

	delete(w);
	delete(settingText);
}

static int MenuSettingsVideo()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	OptionList options;

	sprintf(options.name[i++], "Rendering");
	sprintf(options.name[i++], "Scaling");
	sprintf(options.name[i++], "Cropping");
	sprintf(options.name[i++], "Palette");
	sprintf(options.name[i++], "Timing");
	sprintf(options.name[i++], "Screen Zoom");
	sprintf(options.name[i++], "Screen Position");
	options.length = i;

	GuiText titleTxt("Settings - Video", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		// don't allow original render mode if progressive video mode detected
		if (GCSettings.render==0 && progressive)
			GCSettings.render++;

		if (GCSettings.render == 0)
			sprintf (options.value[0], "Original");
		else if (GCSettings.render == 1)
			sprintf (options.value[0], "Filtered");
		else if (GCSettings.render == 2)
			sprintf (options.value[0], "Unfiltered");

		if(GCSettings.widescreen)
			sprintf (options.value[1], "16:9 Correction");
		else
			sprintf (options.value[1], "Default");

		switch(GCSettings.hideoverscan)
		{
			case 0: sprintf (options.value[2], "Video Cropping Off"); break;
			case 1: sprintf (options.value[2], "Video Cropping Vertical"); break;
			case 2: sprintf (options.value[2], "Video Cropping Horizontal"); break;
			case 3: sprintf (options.value[2], "Video Cropping Both"); break;
		}

		sprintf (options.value[3], "%s",
			GCSettings.currpal ? palettes[GCSettings.currpal-1].name : "Default");

		sprintf (options.value[4], "%s", GCSettings.timing == true ? " PAL" : "NTSC");
		sprintf (options.value[5], "%.2f%%", GCSettings.ZoomLevel*100);
		sprintf (options.value[6], "%d, %d", GCSettings.xshift, GCSettings.yshift);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.render++;
				if (GCSettings.render > 2)
					GCSettings.render = 0;
				break;

			case 1:
				GCSettings.widescreen ^= 1;
				break;

			case 2:
				GCSettings.hideoverscan++;
				if (GCSettings.hideoverscan > 3)
					GCSettings.hideoverscan = 0;
				break;

			case 3: // palette
				if ( ++GCSettings.currpal > MAXPAL )
					GCSettings.currpal = 0;
				break;

			case 4: // timing
				GCSettings.timing ^= 1;
				FCEUI_SetVidSystem(GCSettings.timing); // causes a small 'pop' in the audio
				break;

			case 5:
				ScreenZoomWindow();
				break;

			case 6:
				ScreenPositionWindow();
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsFile
 ***************************************************************************/

static int MenuSettingsFile()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	OptionList options;
	sprintf(options.name[i++], "Load Method");
	sprintf(options.name[i++], "Load Folder");
	sprintf(options.name[i++], "Save Method");
	sprintf(options.name[i++], "Save Folder");
	sprintf(options.name[i++], "Auto Load");
	sprintf(options.name[i++], "Auto Save");
	sprintf(options.name[i++], "Verify MC Saves");
	options.length = i;

	GuiText titleTxt("Settings - Saving & Loading", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(180);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		// some load/save methods are not implemented - here's where we skip them
		// they need to be skipped in the order they were enumerated in snes9xGX.h

		// no USB ports on GameCube
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_USB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_USB)
			GCSettings.SaveMethod++;
		#endif

		// saving to DVD is impossible
		if(GCSettings.SaveMethod == METHOD_DVD)
			GCSettings.SaveMethod++;

		// disable SMB in GC mode (stalls out)
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_SMB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_SMB)
			GCSettings.SaveMethod++;
		#endif

		// disable MC saving in Wii mode - does not work for some reason!
		#ifdef HW_RVL
		if(GCSettings.SaveMethod == METHOD_MC_SLOTA)
			GCSettings.SaveMethod++;
		if(GCSettings.SaveMethod == METHOD_MC_SLOTB)
			GCSettings.SaveMethod++;
		options.name[6][0] = 0;
		#endif

		// correct load/save methods out of bounds
		if(GCSettings.LoadMethod > 4)
			GCSettings.LoadMethod = 0;
		if(GCSettings.SaveMethod > 6)
			GCSettings.SaveMethod = 0;

		if (GCSettings.LoadMethod == METHOD_AUTO) sprintf (options.value[0],"Auto");
		else if (GCSettings.LoadMethod == METHOD_SD) sprintf (options.value[0],"SD");
		else if (GCSettings.LoadMethod == METHOD_USB) sprintf (options.value[0],"USB");
		else if (GCSettings.LoadMethod == METHOD_DVD) sprintf (options.value[0],"DVD");
		else if (GCSettings.LoadMethod == METHOD_SMB) sprintf (options.value[0],"Network");

		sprintf (options.value[1], "%s", GCSettings.LoadFolder);

		if (GCSettings.SaveMethod == METHOD_AUTO) sprintf (options.value[2],"Auto");
		else if (GCSettings.SaveMethod == METHOD_SD) sprintf (options.value[2],"SD");
		else if (GCSettings.SaveMethod == METHOD_USB) sprintf (options.value[2],"USB");
		else if (GCSettings.SaveMethod == METHOD_SMB) sprintf (options.value[2],"Network");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTA) sprintf (options.value[2],"MC Slot A");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTB) sprintf (options.value[2],"MC Slot B");

		sprintf (options.value[3], "%s", GCSettings.SaveFolder);

		if (GCSettings.AutoLoad == 0) sprintf (options.value[4],"Off");
		else if (GCSettings.AutoLoad == 1) sprintf (options.value[4],"SRAM");
		else if (GCSettings.AutoLoad == 2) sprintf (options.value[4],"Snapshot");

		if (GCSettings.AutoSave == 0) sprintf (options.value[5],"Off");
		else if (GCSettings.AutoSave == 1) sprintf (options.value[5],"SRAM");
		else if (GCSettings.AutoSave == 2) sprintf (options.value[5],"Snapshot");
		else if (GCSettings.AutoSave == 3) sprintf (options.value[5],"Both");

		sprintf (options.value[6], "%s", GCSettings.VerifySaves == true ? "On" : "Off");

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.LoadMethod ++;
				break;

			case 1:
				OnScreenKeyboard(GCSettings.LoadFolder);
				break;

			case 2:
				GCSettings.SaveMethod ++;
				break;

			case 3:
				OnScreenKeyboard(GCSettings.SaveFolder);
				break;

			case 4:
				GCSettings.AutoLoad ++;
				if (GCSettings.AutoLoad > 2)
					GCSettings.AutoLoad = 0;
				break;

			case 5:
				GCSettings.AutoSave ++;
				if (GCSettings.AutoSave > 3)
					GCSettings.AutoSave = 0;
				break;

			case 6:
				GCSettings.VerifySaves ^= 1;
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsMenu
 ***************************************************************************/

static int MenuSettingsMenu()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	OptionList options;

	sprintf(options.name[i++], "Exit Action");
	sprintf(options.name[i++], "Wiimote Orientation");
	sprintf(options.name[i++], "Music Volume");
	sprintf(options.name[i++], "Sound Effects Volume");
	options.length = i;

	GuiText titleTxt("Settings - Menu", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		#ifdef HW_RVL
		if (GCSettings.ExitAction == 1)
			sprintf (options.value[0], "Return to Wii Menu");
		else if (GCSettings.ExitAction == 2)
			sprintf (options.value[0], "Power off Wii");
		else
			sprintf (options.value[0], "Return to Loader");

		if (GCSettings.WiimoteOrientation == 0)
			sprintf (options.value[1], "Vertical");
		else if (GCSettings.WiimoteOrientation == 1)
			sprintf (options.value[1], "Horizontal");
		#else // GameCube
		if(GCSettings.ExitAction > 1)
			GCSettings.ExitAction = 0;
		if (GCSettings.ExitAction == 0)
			sprintf (options.value[0], "Return to Loader");
		else
			sprintf (options.value[0], "Reboot");

		options.name[1][0] = 0; // Wiimote
		options.name[2][0] = 0; // Music
		options.name[3][0] = 0; // Sound Effects
		#endif

		if(GCSettings.MusicVolume > 0)
			sprintf(options.value[2], "%d%%", GCSettings.MusicVolume);
		else
			sprintf(options.value[2], "Mute");

		if(GCSettings.SFXVolume > 0)
			sprintf(options.value[3], "%d%%", GCSettings.SFXVolume);
		else
			sprintf(options.value[3], "Mute");

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.ExitAction++;
				if(GCSettings.ExitAction > 2)
					GCSettings.ExitAction = 0;
				break;
			case 1:
				GCSettings.WiimoteOrientation ^= 1;
				break;
			case 2:
				GCSettings.MusicVolume += 10;
				if(GCSettings.MusicVolume > 100)
					GCSettings.MusicVolume = 0;
				bgMusic->SetVolume(GCSettings.MusicVolume);
				break;
			case 3:
				GCSettings.SFXVolume += 10;
				if(GCSettings.SFXVolume > 100)
					GCSettings.SFXVolume = 0;
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsNetwork
 ***************************************************************************/

static int MenuSettingsNetwork()
{
	int menu = MENU_NONE;
#ifdef HW_RVL
	int ret;
	int i = 0;
	OptionList options;
	sprintf(options.name[i++], "SMB Share IP");
	sprintf(options.name[i++], "SMB Share Name");
	sprintf(options.name[i++], "SMB Share Username");
	sprintf(options.name[i++], "SMB Share Password");
	options.length = i;

	GuiText titleTxt("Settings - Network", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		strncpy (options.value[0], GCSettings.smbip, 15);
		strncpy (options.value[1], GCSettings.smbshare, 19);
		strncpy (options.value[2], GCSettings.smbuser, 19);
		strncpy (options.value[3], GCSettings.smbpwd, 19);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				OnScreenKeyboard(GCSettings.smbip);
				break;

			case 1:
				OnScreenKeyboard(GCSettings.smbshare);
				break;

			case 2:
				OnScreenKeyboard(GCSettings.smbuser);
				break;

			case 3:
				OnScreenKeyboard(GCSettings.smbpwd);
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
#endif
	return menu;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/

void
MainMenu (int menu)
{
	int currentMenu = menu;
	lastMenu = MENU_NONE;

	#ifdef HW_RVL
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);
	#endif

	mainWindow = new GuiWindow(screenwidth, screenheight);

	bgImg = new GuiImage(screenwidth, screenheight, (GXColor){175, 200, 215, 255});
	bgImg->ColorStripe(10);
	mainWindow->Append(bgImg);

	if(gameScreenTex)
	{
		gameScreenImg = new GuiImage(gameScreenTex, screenwidth, screenheight);
		gameScreenImg->SetAlpha(192);
		//gameScreenImg->SetStripe(100);
		gameScreenImg->ColorStripe(30);
		mainWindow->Append(gameScreenImg);
		bgImg->SetVisible(false);
	}

	GuiTrigger trigA;
	if(GCSettings.WiimoteOrientation)
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData bgTop(bg_top_png);
	bgTopImg = new GuiImage(&bgTop);
	GuiImageData bgBottom(bg_bottom_png);
	bgBottomImg = new GuiImage(&bgBottom);
	bgBottomImg->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	GuiImageData logo(logo_png);
	GuiImage logoImg(&logo);
	GuiImageData logoOver(logo_over_png);
	GuiImage logoImgOver(&logoOver);
	GuiText logoTxt(APPVERSION, 18, (GXColor){255, 255, 255, 255});
	logoTxt.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	btnLogo = new GuiButton(logoImg.GetWidth(), logoImg.GetHeight());
	btnLogo->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	btnLogo->SetPosition(-30, 30);
	btnLogo->SetImage(&logoImg);
	btnLogo->SetImageOver(&logoImgOver);
	btnLogo->SetLabel(&logoTxt);
	btnLogo->SetTrigger(&trigA);
	btnLogo->SetUpdateCallback(WindowCredits);

	mainWindow->Append(bgTopImg);
	mainWindow->Append(bgBottomImg);
	mainWindow->Append(btnLogo);

	/*// memory usage - for debugging
	memTxt = new GuiText(NULL, 18, (GXColor){255, 255, 255, 255});
	memTxt->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	memTxt->SetPosition(20, 20);
	mainWindow->Append(memTxt);*/

	if(currentMenu == MENU_GAMESELECTION)
		ResumeGui();

	// Load preferences
	if(!LoadPrefs())
		SavePrefs(SILENT);

	#ifndef NO_SOUND
	bgMusic = new GuiSound(bg_music_ogg, bg_music_ogg_size, SOUND_OGG);
	bgMusic->SetVolume(GCSettings.MusicVolume);
	if(currentMenu == MENU_GAMESELECTION) bgMusic->Play(); // startup music
	#endif

	while(currentMenu != MENU_EXIT || !romLoaded)
	{
		switch (currentMenu)
		{
			case MENU_GAMESELECTION:
				currentMenu = MenuGameSelection();
				break;
			case MENU_GAME:
				currentMenu = MenuGame();
				break;
			case MENU_GAME_LOAD:
				currentMenu = MenuGameSaves(0);
				break;
			case MENU_GAME_SAVE:
				currentMenu = MenuGameSaves(1);
				break;
			/*case MENU_GAME_CHEATS:
				currentMenu = MenuGameCheats();
				break;*/
			case MENU_SETTINGS:
				currentMenu = MenuSettings();
				break;
			case MENU_SETTINGS_MAPPINGS:
				currentMenu = MenuSettingsMappings();
				break;
			case MENU_SETTINGS_MAPPINGS_CTRL:
				currentMenu = MenuSettingsMappingsController();
				break;
			case MENU_SETTINGS_MAPPINGS_MAP:
				currentMenu = MenuSettingsMappingsMap();
				break;
			case MENU_SETTINGS_VIDEO:
				currentMenu = MenuSettingsVideo();
				break;
			case MENU_SETTINGS_FILE:
				currentMenu = MenuSettingsFile();
				break;
			case MENU_SETTINGS_MENU:
				currentMenu = MenuSettingsMenu();
				break;
			case MENU_SETTINGS_NETWORK:
				currentMenu = MenuSettingsNetwork();
				break;
			default: // unrecognized menu
				currentMenu = MenuGameSelection();
				break;
		}
		lastMenu = currentMenu;
	}

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	CancelAction();
	HaltGui();

	#ifndef NO_SOUND
	bgMusic->Stop();
	delete bgMusic;
	#endif

	//delete memTxt;
	delete btnLogo;
	delete bgImg;
	delete bgTopImg;
	delete bgBottomImg;
	delete mainWindow;

	#ifdef HW_RVL
	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];
	#endif

	mainWindow = NULL;

	if(gameScreenImg)
	{
		delete gameScreenImg;
		gameScreenImg = NULL;
	}
	if(gameScreenTex)
	{
		free(gameScreenTex);
		gameScreenTex = NULL;
	}
	if(gameScreenTex2)
	{
		free(gameScreenTex2);
		gameScreenTex2 = NULL;
	}
}
