/****************************************************************************
 * FCE Ultra 0.98.12
 * Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * gcvideo.cpp
 *
 * Video rendering
 ****************************************************************************/

#include <gccore.h>
#include <unistd.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/texconv.h>

#include "fceusupport.h"
#include "gcvideo.h"
#include "fceugx.h"
#include "videofilter.h"
#include "menu.h"
#include "pad.h"
#include "gui/gui.h"

int FDSTimer = 0;
u32 FrameTimer = 0;
int FDSSwitchRequested;

/*** External 2D Video ***/
/*** 2D Video Globals ***/
static GXRModeObj *vmode; // Graphics Mode Object
static unsigned int *xfb[2]; // Framebuffers
static int whichfb = 0; // Frame buffer toggle
int screenheight;
int screenwidth;
bool progressive = false;
static int currentVideoMode = -1; // -1 - not set, 0 - automatic, 1 - NTSC (480i), 2 - Progressive (480p), 3 - PAL (50Hz), 4 - PAL (60Hz)
static int oldRenderMode = -1; // set to GCSettings.render when changing (temporarily) to another mode

/*** 3D GX ***/
#define TEX_WIDTH 256
#define TEX_HEIGHT 240
#define DEFAULT_FIFO_SIZE ( 256 * 1024 )
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);
static u32 copynow = GX_FALSE;
static GXTexObj texobj;
static Mtx view;
static Mtx GXmodelView2D;

/*** Texture memory ***/
static unsigned char texturemem[TEX_WIDTH * TEX_HEIGHT * 4] ATTRIBUTE_ALIGN (32);
unsigned char filtermem[TEX_WIDTH * TEX_HEIGHT * 4] ATTRIBUTE_ALIGN (32);

static int UpdateVideo = 1;
static int vmode_60hz = 0;

u8 * gameScreenTex = NULL; // a GX texture screen capture of the game
u8 * gameScreenTex2 = NULL; // a GX texture screen capture of the game (copy)

#define HASPECT 256
#define VASPECT 240

static int fscale = 1;

// Need something to hold the PC palette
struct pcpal {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} pcpalette[256];

static unsigned int gcpalette[256];	// Much simpler GC palette
static unsigned short rgb565[256];	// Texture map palette

static long long prev;
static long long now;

extern "C"
{
long long gettime();
u32 diff_usec(long long start,long long end);
}

/* New texture based scaler */
typedef struct tagcamera
{
  guVector pos;
  guVector up;
  guVector view;
}
camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
	 Think of the output as a -80 x 80 by -60 x 60 graph.
***/
static s16 square[] ATTRIBUTE_ALIGN (32) =
{
  /*
   * X,   Y,  Z
   * Values set are for roughly 4:3 aspect
   */
   -HASPECT,  VASPECT, 0,	// 0
    HASPECT,  VASPECT, 0,	// 1
    HASPECT, -VASPECT, 0,	// 2
   -HASPECT, -VASPECT, 0	// 3
};


static camera cam = { {0.0F, 0.0F, 0.0F},
{0.0F, 0.5F, 0.0F},
{0.0F, 0.0F, -0.5F}
};

/***
*** Custom Video modes (used to emulate original console video modes)
***/

/** Original NES PAL Resolutions: **/

/* 240 lines progressive (PAL 50Hz) */
static GXRModeObj PAL_240p =
{
	VI_TVMODE_PAL_DS,       // viDisplayMode
	640,             // fbWidth
	240,             // efbHeight
	240,             // xfbHeight
	(VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
	(VI_MAX_HEIGHT_PAL - 480)/2,        // viYOrigin
	640,             // viWidth
	480,             // viHeight
	VI_XFBMODE_SF,   // xFBmode
	GX_FALSE,        // field_rendering
	GX_FALSE,        // aa

  // sample points arranged in increasing Y order
        {
                {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
                {6,6},{6,6},{6,6},  // pix 1
                {6,6},{6,6},{6,6},  // pix 2
                {6,6},{6,6},{6,6}   // pix 3
        },

  // vertical filter[7], 1/64 units, 6 bits each
        {
                 0,         // line n-1
                 0,         // line n-1
                21,         // line n
                22,         // line n
                21,         // line n
                 0,         // line n+1
                 0          // line n+1
        }
};

/** Original NES NTSC Resolutions: **/

/* 240 lines progressive (NTSC or PAL 60Hz) */
static GXRModeObj NTSC_240p =
{
	VI_TVMODE_EURGB60_DS,      // viDisplayMode
	640,             // fbWidth
	240,             // efbHeight
	240,             // xfbHeight
	(VI_MAX_WIDTH_NTSC - 640)/2,	// viXOrigin
	(VI_MAX_HEIGHT_NTSC - 480)/2,	// viYOrigin
	640,             // viWidth
	480,             // viHeight
	VI_XFBMODE_SF,   // xFBmode
	GX_FALSE,        // field_rendering
	GX_FALSE,        // aa

  // sample points arranged in increasing Y order
        {
                {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
                {6,6},{6,6},{6,6},  // pix 1
                {6,6},{6,6},{6,6},  // pix 2
                {6,6},{6,6},{6,6}   // pix 3
        },

  // vertical filter[7], 1/64 units, 6 bits each
        {
                  0,         // line n-1
                  0,         // line n-1
                 21,         // line n
                 22,         // line n
                 21,         // line n
                  0,         // line n+1
                  0          // line n+1
        }
};

/* TV Modes table */
static GXRModeObj *tvmodes[2] = {
	&NTSC_240p, &PAL_240p
};

/****************************************************************************
 * setFrameTimer()
 * change frame timings depending on whether ROM is NTSC or PAL
 ***************************************************************************/

static long long normaldiff;

void setFrameTimer()
{
	if (GCSettings.timing == 1) // PAL
		normaldiff = 20000; // 50hz
	else
		normaldiff = 16667; // 60hz

	prev = gettime();
}

static void SyncSpeed()
{
	now = gettime();
	while (diff_usec(prev, now) < normaldiff)
	{
		now = gettime();
		usleep(50);
	}
	prev = now;
}

/****************************************************************************
 * VideoThreading
 ***************************************************************************/
#define TSTACK 16384
static lwp_t vbthread = LWP_THREAD_NULL;
static unsigned char vbstack[TSTACK];

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank.
 *
 * Putting LWP to good use :)
 ***************************************************************************/
static void *
vbgetback (void *arg)
{
	while (1)
	{
		SyncSpeed();
		LWP_SuspendThread (vbthread);
	}

	return NULL;
}

/****************************************************************************
 * InitVideoThread
 ***************************************************************************/
void
InitVideoThread ()
{
	LWP_CreateThread (&vbthread, vbgetback, NULL, vbstack, TSTACK, 100);
}

/****************************************************************************
 * copy_to_xfb
 *
 * Stock code to copy the GX buffer to the current display mode.
 * Also increments the frameticker, as it's called for each vb.
 ***************************************************************************/
static inline void
copy_to_xfb (u32 arg)
{
	if (copynow == GX_TRUE)
	{
		GX_CopyDisp (xfb[whichfb], GX_TRUE);
		GX_Flush ();
		copynow = GX_FALSE;
	}

	FrameTimer++;

	// FDS switch disk requested - need to eject, select, and insert
	// but not all at once!
	if(FDSSwitchRequested)
	{
		switch(FDSSwitchRequested)
		{
			case 1:
				FDSSwitchRequested++;
				FCEUI_FDSInsert(); // eject disk
				FDSTimer = 0;
				break;
			case 2:
				if(FDSTimer > 60)
				{
					FDSSwitchRequested++;
					FDSTimer = 0;
					FCEUI_FDSSelect(); // select other side
					FCEUI_FDSInsert(); // insert disk
				}
				break;
			case 3:
				if(FDSTimer > 200)
				{
					FDSSwitchRequested = 0;
					FDSTimer = 0;
				}
				break;
		}
		FDSTimer++;
	}
}

/****************************************************************************
 * Scaler Support Functions
 ***************************************************************************/
static inline void
draw_init ()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	GX_SetNumTexGens (1);
	GX_SetNumChans (0);

	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache
}

static inline void
draw_vert (u8 pos, u8 c, f32 s, f32 t)
{
	GX_Position1x8 (pos);
	GX_Color1x8 (c);
	GX_TexCoord2f32 (s, t);
}

static inline void
draw_square (Mtx v)
{
	Mtx m;			// model matrix.
	Mtx mv;			// modelview matrix.

	guMtxIdentity (m);
	guMtxTransApply (m, m, 0, 0, -100);
	guMtxConcat (v, m, mv);

	GX_LoadPosMtxImm (mv, GX_PNMTX0);
	GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
	draw_vert (0, 0, 0.0, 0.0);
	draw_vert (1, 0, 1.0, 0.0);
	draw_vert (2, 0, 1.0, 1.0);
	draw_vert (3, 0, 0.0, 1.0);
	GX_End ();
}

/****************************************************************************
 * StartGX
 *
 * This function initialises GX and sets it up for use
 ***************************************************************************/
static void
StartGX ()
{
	GXColor background = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
	memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
	GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear (background, 0x00ffffff);

	GX_SetDispCopyGamma (GX_GM_1_0);
	GX_SetCullMode (GX_CULL_NONE);
}

/****************************************************************************
 * StopGX
 *
 * Stops GX (when exiting)
 ***************************************************************************/
void StopGX()
{
	GX_AbortFrame();
	GX_Flush();

	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
}

/****************************************************************************
 * UpdateScaling
 *
 * This function updates the quad aspect ratio.
 ***************************************************************************/
static inline void
UpdateScaling()
{
	int xscale, yscale;

	fscale = GetFilterScale((RenderFilter)GCSettings.FilterMethod);

	// update scaling
	if (GCSettings.render == 0)	// original render mode
	{
		if (GCSettings.FilterMethod != FILTER_NONE)
		{
			xscale = TEX_WIDTH;
			yscale = TEX_HEIGHT;
		}
		else // no filtering
		{
			fscale = 1;
			xscale = 640 / 2; // use GX scaler instead VI
			yscale = TEX_HEIGHT / 2;
		}
	}
	else // unfiltered and filtered mode
	{
		xscale = vmode->fbWidth / 2;
		yscale = vmode->efbHeight / 2;
	}

	// aspect ratio scaling (change width scale)
	// yes its pretty cheap and ugly, but its easy!
	if (GCSettings.widescreen)
		xscale = (3*xscale)/4;

	xscale *= GCSettings.ZoomLevel;
	yscale *= GCSettings.ZoomLevel;

	// update vertex position matrix
	square[0] = square[9] = (-xscale) + GCSettings.xshift;
	square[3] = square[6] = (xscale) + GCSettings.xshift;
	square[1] = square[4] = (yscale) - GCSettings.yshift;
	square[7] = square[10] = (-yscale) - GCSettings.yshift;
	DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!
	draw_init ();
}

/****************************************************************************
 * SetupVideoMode
 *
 * Finds the optimal video mode, or uses the user-specified one
 * Also configures original video modes
 ***************************************************************************/
static void SetupVideoMode()
{
	if(currentVideoMode == GCSettings.videomode)
		return; // no need to do anything

	// choose the desired video mode
	switch(GCSettings.videomode)
	{
		case 1: // NTSC (480i)
			vmode = &TVNtsc480IntDf;
			break;
		case 2: // Progressive (480p)
			vmode = &TVNtsc480Prog;
			break;
		case 3: // PAL (50Hz)
			vmode = &TVPal574IntDfScale;
			break;
		case 4: // PAL (60Hz)
			vmode = &TVEurgb60Hz480IntDf;
			break;
		default:
			vmode = VIDEO_GetPreferredMode(NULL);

			#ifdef HW_DOL
			/* we have component cables, but the preferred mode is interlaced
			 * why don't we switch into progressive?
			 * on the Wii, the user can do this themselves on their Wii Settings */
			if(VIDEO_HaveComponentCable())
				vmode = &TVNtsc480Prog;
			#endif

			// use hardware vertical scaling to fill screen
			if(vmode->viTVMode >> 2 == VI_PAL)
				vmode = &TVPal574IntDfScale;
			break;
	}

	// configure original modes
	switch (vmode->viTVMode >> 2)
	{
		case VI_PAL:
			// 576 lines (PAL 50Hz)
			vmode_60hz = 0;

			// Original Video modes (forced to PAL 50Hz)
			// set video signal mode
			NTSC_240p.viTVMode = VI_TVMODE_PAL_DS;
			NTSC_240p.viYOrigin = (VI_MAX_HEIGHT_PAL - 480)/2;
			break;

		case VI_NTSC:
			// 480 lines (NTSC 60Hz)
			vmode_60hz = 1;

			// Original Video modes (forced to NTSC 60Hz)
			// set video signal mode
			PAL_240p.viTVMode = VI_TVMODE_NTSC_DS;
			PAL_240p.viYOrigin = (VI_MAX_HEIGHT_NTSC - 480)/2;
			NTSC_240p.viTVMode = VI_TVMODE_NTSC_DS;
			break;

		default:
			// 480 lines (PAL 60Hz)
			vmode_60hz = 1;

			// Original Video modes (forced to PAL 60Hz)
			// set video signal mode
			PAL_240p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
			PAL_240p.viYOrigin = (VI_MAX_HEIGHT_NTSC - 480)/2;
			NTSC_240p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
			break;
	}

	// check for progressive scan
	if (vmode->viTVMode == VI_TVMODE_NTSC_PROG)
		progressive = true;
	else
		progressive = false;

	#ifdef HW_RVL
	// widescreen fix
	if(CONF_GetAspectRatio() == CONF_ASPECT_16_9)
	{
		vmode->viWidth = VI_MAX_WIDTH_PAL;
	}
	#endif

	currentVideoMode = GCSettings.videomode;
}

/****************************************************************************
 * InitGCVideo
 *
 * This function MUST be called at startup.
 * - also sets up menu video mode
 ***************************************************************************/
void
InitGCVideo ()
{
	SetupVideoMode();

	// configure VI
	VIDEO_Configure (vmode);

	screenheight = 480;
	screenwidth = 640;

	// Allocate the video buffers
	xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

	// A console is always useful while debugging.
	//console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

	// Clear framebuffers etc.
	VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer (xfb[0]);

	// video callbacks
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)copy_to_xfb);

	VIDEO_SetBlack (FALSE);
	VIDEO_Flush ();
	VIDEO_WaitVSync ();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync ();

	copynow = GX_FALSE;

	StartGX ();
	InitLUTs();	// init LUTs for hq2x
	InitVideoThread ();
	// Finally, the video is up and ready for use :)
}

/****************************************************************************
 * ResetVideo_Emu
 *
 * Reset the video/rendering mode for the emulator rendering
****************************************************************************/
void
ResetVideo_Emu ()
{
	SetupVideoMode();
	GXRModeObj *rmode = vmode; // same mode as menu
	Mtx44 p;

	// change current VI mode if using original render mode
	if (GCSettings.render == 0)
		rmode = tvmodes[GCSettings.timing];

	// reconfigure VI
	VIDEO_Configure (rmode);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
		while (VIDEO_GetNextField())
			VIDEO_WaitVSync();

	VIDEO_SetPreRetraceCallback(NULL);

	GXColor background = {0, 0, 0, 255};
	GX_SetCopyClear (background, 0x00ffffff);

	// reconfigure GX
	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (GCSettings.render == 1) ? GX_TRUE : GX_FALSE, rmode->vfilter);	// deflicker ON only for filtered mode

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);

	guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 100, 1000); // matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	// set aspect ratio
	draw_init ();
	UpdateScaling();

	// reinitialize texture
	GX_InvalidateTexAll ();
	GX_InitTexObj (&texobj, texturemem, TEX_WIDTH*fscale, TEX_HEIGHT*fscale, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);	// initialize the texture obj we are going to use
	if (!(GCSettings.render&1))
		GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1); // original/unfiltered video mode: force texture filtering OFF
	GX_LoadTexObj (&texobj, GX_TEXMAP0);
	memset(texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2); // clear texture memory
}

/****************************************************************************
 * RenderFrame
 *
 * Render a single frame
 ****************************************************************************/

void RenderFrame(unsigned char *XBuf)
{
	// Ensure previous vb has complete
	while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
		usleep (50);

	// swap framebuffers
	whichfb ^= 1;

	// video has changed
	if(UpdateVideo)
	{
		UpdateVideo = 0;
		ResetVideo_Emu(); // reset video to emulator rendering settings
	}

	int width, height;

	u8 borderheight = 0;
	u8 borderwidth = 0;

	// 0 = off, 1 = vertical, 2 = horizontal, 3 = both
	if(GCSettings.hideoverscan == 1 || GCSettings.hideoverscan == 3)
		borderheight = 8;
	if(GCSettings.hideoverscan >= 2)
		borderwidth = 8;

	if (GCSettings.FilterMethod != FILTER_NONE)
	{
		FilterMethod ((uint8*) XBuf, 272, (uint8*) filtermem, TEX_WIDTH*fscale*2, TEX_WIDTH, TEX_HEIGHT);
		MakeTexture565((char *)filtermem, (char *) texturemem, TEX_WIDTH*fscale, TEX_HEIGHT*fscale);
	}
	else
	{
		u16 *texture = (unsigned short *)texturemem + (borderheight << 8) + (borderwidth << 2);
		u8 *src1 = XBuf + (borderheight << 8) + borderwidth;
		u8 *src2 = XBuf + (borderheight << 8) + borderwidth + 256;
		u8 *src3 = XBuf + (borderheight << 8) + borderwidth + 512;
		u8 *src4 = XBuf + (borderheight << 8) + borderwidth + 768;

		// fill the texture
		for (height = 0; height < 240 - (borderheight << 1); height += 4)
		{
			for (width = 0; width < 256 - (borderwidth << 1); width += 4)
			{
				// Row one
				*texture++ = rgb565[*src1++];
				*texture++ = rgb565[*src1++];
				*texture++ = rgb565[*src1++];
				*texture++ = rgb565[*src1++];

				// Row two
				*texture++ = rgb565[*src2++];
				*texture++ = rgb565[*src2++];
				*texture++ = rgb565[*src2++];
				*texture++ = rgb565[*src2++];

				// Row three
				*texture++ = rgb565[*src3++];
				*texture++ = rgb565[*src3++];
				*texture++ = rgb565[*src3++];
				*texture++ = rgb565[*src3++];

				// Row four
				*texture++ = rgb565[*src4++];
				*texture++ = rgb565[*src4++];
				*texture++ = rgb565[*src4++];
				*texture++ = rgb565[*src4++];
			}
			src1 += 768 + (borderwidth << 1); // line 4*N
			src2 += 768 + (borderwidth << 1); // line 4*(N+1)
			src3 += 768 + (borderwidth << 1); // line 4*(N+2)
			src4 += 768 + (borderwidth << 1); // line 4*(N+3)

			texture += (borderwidth << 3);
		}
	}

	// load texture into GX
	DCFlushRange(texturemem, TEX_WIDTH * TEX_HEIGHT * 4);

	// clear texture objects
	GX_InvalidateTexAll();

	// render textured quad
	draw_square(view);
	GX_DrawDone();

	if(ScreenshotRequested)
	{
		if(GCSettings.render == 0) // we can't take a screenshot in Original mode
		{
			oldRenderMode = 0;
			GCSettings.render = 2; // switch to unfiltered mode
			UpdateVideo = 1; // request the switch
		}
		else
		{
			ScreenshotRequested = 0;
			TakeScreenshot();
			if(oldRenderMode != -1)
			{
				GCSettings.render = oldRenderMode;
				oldRenderMode = -1;
			}
			ConfigRequested = 1;
		}
	}

	// EFB is ready to be copied into XFB
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();

	copynow = GX_TRUE;

	// Return to caller, don't waste time waiting for vb
	LWP_ResumeThread (vbthread);
}

/****************************************************************************
 * Zoom Functions
 ***************************************************************************/
void
zoom (float speed)
{
	if (GCSettings.ZoomLevel > 1)
		GCSettings.ZoomLevel += (speed / -100.0);
	else
		GCSettings.ZoomLevel += (speed / -200.0);

	if (GCSettings.ZoomLevel < 0.5)
		GCSettings.ZoomLevel = 0.5;
	else if (GCSettings.ZoomLevel > 2.0)
		GCSettings.ZoomLevel = 2.0;

	UpdateVideo = 1;	// update video
}

void
zoom_reset ()
{
	GCSettings.ZoomLevel = 1.0;
	UpdateVideo = 1;	// update video
}

/****************************************************************************
 * TakeScreenshot
 *
 * Copies the current screen into a GX texture
 ***************************************************************************/
void TakeScreenshot()
{
	int texSize = vmode->fbWidth * vmode->efbHeight * 4;

	if(gameScreenTex) free(gameScreenTex);
	gameScreenTex = (u8 *)memalign(32, texSize);
	if(gameScreenTex == NULL) return;
	GX_SetTexCopySrc(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetTexCopyDst(vmode->fbWidth, vmode->efbHeight, GX_TF_RGBA8, GX_FALSE);
	GX_CopyTex(gameScreenTex, GX_FALSE);
	GX_PixModeSync();
	DCFlushRange(gameScreenTex, texSize);

	#ifdef HW_RVL
	if(gameScreenTex2) free(gameScreenTex2);
	gameScreenTex2 = (u8 *)memalign(32, texSize);
	if(gameScreenTex2 == NULL) return;
	GX_CopyTex(gameScreenTex2, GX_FALSE);
	GX_PixModeSync();
	DCFlushRange(gameScreenTex2, texSize);
	#endif
}

/****************************************************************************
 * ResetVideo_Menu
 *
 * Reset the video/rendering mode for the menu
****************************************************************************/
void
ResetVideo_Menu ()
{
	Mtx44 p;
	f32 yscale;
	u32 xfbHeight;

	SetupVideoMode();
	VIDEO_Configure (vmode);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
		while (VIDEO_GetNextField())
			VIDEO_WaitVSync();

	VIDEO_SetPreRetraceCallback((VIRetraceCallback)UpdatePads);

	// clears the bg to color and clears the z buffer
	GXColor background = {0, 0, 0, 255};
	GX_SetCopyClear (background, 0x00ffffff);

	yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (vmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_InvVtxCache ();
	GX_InvalidateTexAll();

	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc (GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_TRUE);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	guMtxIdentity(GXmodelView2D);
	guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);

	guOrtho(p,0,479,0,639,0,300);
	GX_LoadProjectionMtx(p, GX_ORTHOGRAPHIC);

	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
}

/****************************************************************************
 * Menu_Render
 *
 * Renders everything current sent to GX, and flushes video
 ***************************************************************************/
void Menu_Render()
{
	whichfb ^= 1; // flip framebuffer
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[whichfb],GX_TRUE);
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

/****************************************************************************
 * Menu_DrawImg
 *
 * Draws the specified image on screen using GX
 ***************************************************************************/
void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[],
	f32 degrees, f32 scaleX, f32 scaleY, u8 alpha)
{
	if(data == NULL)
		return;

	GXTexObj texObj;

	GX_InitTexObj(&texObj, data, width,height, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_InvalidateTexAll();

	GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	Mtx m,m1,m2, mv;
	width *=.5;
	height*=.5;
	guMtxIdentity (m1);
	guMtxScaleApply(m1,m1,scaleX,scaleY,1.0);
	guVector axis = (guVector) {0 , 0, 1 };
	guMtxRotAxisDeg (m2, &axis, degrees);
	guMtxConcat(m2,m1,m);

	guMtxTransApply(m,m, xpos+width,ypos+height,0);
	guMtxConcat (GXmodelView2D, m, mv);
	GX_LoadPosMtxImm (mv, GX_PNMTX0);

	GX_Begin(GX_QUADS, GX_VTXFMT0,4);
	GX_Position3f32(-width, -height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(width, -height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(width, height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(-width, height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(0, 1);
	GX_End();
	GX_DrawDone();
	GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

/****************************************************************************
 * Menu_DrawRectangle
 *
 * Draws a rectangle at the specified coordinates using GX
 ***************************************************************************/
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, GXColor color, u8 filled)
{
	u8 fmt;
	long n;
	int i;
	f32 x2 = x+width;
	f32 y2 = y+height;
	guVector v[] = {{x,y,0.0f}, {x2,y,0.0f}, {x2,y2,0.0f}, {x,y2,0.0f}, {x,y,0.0f}};

	if(!filled)
	{
		fmt = GX_LINESTRIP;
		n = 5;
	}
	else
	{
		fmt = GX_TRIANGLEFAN;
		n = 4;
	}

	GX_Begin(fmt, GX_VTXFMT0, n);
	for(i=0; i<n; i++)
	{
		GX_Position3f32(v[i].x, v[i].y,  v[i].z);
		GX_Color4u8(color.r, color.g, color.b, color.a);
	}
	GX_End();
	GX_DrawDone();
}

/****************************************************************************
 * rgbcolor
 *
 * Support routine for gcpalette
 ****************************************************************************/

static unsigned int rgbcolor(unsigned char r1, unsigned char g1, unsigned char b1,
        unsigned char r2, unsigned char g2, unsigned char b2) {
    int y1,cb1,cr1,y2,cb2,cr2,cb,cr;

    y1=(299*r1+587*g1+114*b1)/1000;
    cb1=(-16874*r1-33126*g1+50000*b1+12800000)/100000;
    cr1=(50000*r1-41869*g1-8131*b1+12800000)/100000;

    y2=(299*r2+587*g2+114*b2)/1000;
    cb2=(-16874*r2-33126*g2+50000*b2+12800000)/100000;
    cr2=(50000*r2-41869*g2-8131*b2+12800000)/100000;

    cb=(cb1+cb2) >> 1;
    cr=(cr1+cr2) >> 1;

    return ((y1 << 24) | (cb << 16) | (y2 << 8) | cr);
}

/****************************************************************************
 * SetPalette
 *
 * A shadow copy of the palette is maintained, in case the NES Emu kernel
 * requests a copy.
 ****************************************************************************/
void FCEUD_SetPalette(unsigned char index, unsigned char r, unsigned char g,
        unsigned char b) {
    /*** Make PC compatible copy ***/
    pcpalette[index].r = r;
    pcpalette[index].g = g;
    pcpalette[index].b = b;

    /*** Generate Gamecube palette ***/
    gcpalette[index] = rgbcolor(r,g,b,r,g,b);

    /*** Generate RGB565 texture palette ***/
    rgb565[index] = ((r & 0xf8) << 8) |
        ((g & 0xfc) << 3) |
        ((b & 0xf8) >> 3);
}

/****************************************************************************
 * GetPalette
 ****************************************************************************/
void FCEUD_GetPalette(unsigned char i, unsigned char *r, unsigned char *g,
        unsigned char *b) {
    *r = pcpalette[i].r;
    *g = pcpalette[i].g;
    *b = pcpalette[i].b;
}

void SetPalette()
{
	if ( GCSettings.currpal == 0 )
	{
		// Do palette reset
		FCEU_ResetPalette();
	}
	else
	{
		// Now setup this palette
		unsigned char i,r,g,b;

		for ( i = 0; i < 64; i++ )
		{
			r = palettes[GCSettings.currpal-1].data[i] >> 16;
			g = ( palettes[GCSettings.currpal-1].data[i] & 0xff00 ) >> 8;
			b = ( palettes[GCSettings.currpal-1].data[i] & 0xff );
			FCEUD_SetPalette( i, r, g, b);
			FCEUD_SetPalette( i+64, r, g, b);
			FCEUD_SetPalette( i+128, r, g, b);
			FCEUD_SetPalette( i+192, r, g, b);
		}
	}
}

struct st_palettes palettes[] = {
    /* The default NES palette must be the first entry in the array */
    { "loopy", "Loopy's NES palette",
        { 0x757575, 0x271b8f, 0x0000ab, 0x47009f,
            0x8f0077, 0xab0013, 0xa70000, 0x7f0b00,
            0x432f00, 0x004700, 0x005100, 0x003f17,
            0x1b3f5f, 0x000000, 0x000000, 0x000000,
            0xbcbcbc, 0x0073ef, 0x233bef, 0x8300f3,
            0xbf00bf, 0xe7005b, 0xdb2b00, 0xcb4f0f,
            0x8b7300, 0x009700, 0x00ab00, 0x00933b,
            0x00838b, 0x000000, 0x000000, 0x000000,
            0xffffff, 0x3fbfff, 0x5f97ff, 0xa78bfd,
            0xf77bff, 0xff77b7, 0xff7763, 0xff9b3b,
            0xf3bf3f, 0x83d313, 0x4fdf4b, 0x58f898,
            0x00ebdb, 0x000000, 0x000000, 0x000000,
            0xffffff, 0xabe7ff, 0xc7d7ff, 0xd7cbff,
            0xffc7ff, 0xffc7db, 0xffbfb3, 0xffdbab,
            0xffe7a3, 0xe3ffa3, 0xabf3bf, 0xb3ffcf,
            0x9ffff3, 0x000000, 0x000000, 0x000000 }
    },
    { "quor", "Quor's palette from Nestra 0.63",
        { 0x3f3f3f, 0x001f3f, 0x00003f, 0x1f003f,
            0x3f003f, 0x3f0020, 0x3f0000, 0x3f2000,
            0x3f3f00, 0x203f00, 0x003f00, 0x003f20,
            0x003f3f, 0x000000, 0x000000, 0x000000,
            0x7f7f7f, 0x405f7f, 0x40407f, 0x5f407f,
            0x7f407f, 0x7f4060, 0x7f4040, 0x7f6040,
            0x7f7f40, 0x607f40, 0x407f40, 0x407f60,
            0x407f7f, 0x000000, 0x000000, 0x000000,
            0xbfbfbf, 0x809fbf, 0x8080bf, 0x9f80bf,
            0xbf80bf, 0xbf80a0, 0xbf8080, 0xbfa080,
            0xbfbf80, 0xa0bf80, 0x80bf80, 0x80bfa0,
            0x80bfbf, 0x000000, 0x000000, 0x000000,
            0xffffff, 0xc0dfff, 0xc0c0ff, 0xdfc0ff,
            0xffc0ff, 0xffc0e0, 0xffc0c0, 0xffe0c0,
            0xffffc0, 0xe0ffc0, 0xc0ffc0, 0xc0ffe0,
            0xc0ffff, 0x000000, 0x000000, 0x000000 }
    },
    { "chris", "Chris Covell's NES palette",
        { 0x808080, 0x003DA6, 0x0012B0, 0x440096,
            0xA1005E, 0xC70028, 0xBA0600, 0x8C1700,
            0x5C2F00, 0x104500, 0x054A00, 0x00472E,
            0x004166, 0x000000, 0x050505, 0x050505,
            0xC7C7C7, 0x0077FF, 0x2155FF, 0x8237FA,
            0xEB2FB5, 0xFF2950, 0xFF2200, 0xD63200,
            0xC46200, 0x358000, 0x058F00, 0x008A55,
            0x0099CC, 0x212121, 0x090909, 0x090909,
            0xFFFFFF, 0x0FD7FF, 0x69A2FF, 0xD480FF,
            0xFF45F3, 0xFF618B, 0xFF8833, 0xFF9C12,
            0xFABC20, 0x9FE30E, 0x2BF035, 0x0CF0A4,
            0x05FBFF, 0x5E5E5E, 0x0D0D0D, 0x0D0D0D,
            0xFFFFFF, 0xA6FCFF, 0xB3ECFF, 0xDAABEB,
            0xFFA8F9, 0xFFABB3, 0xFFD2B0, 0xFFEFA6,
            0xFFF79C, 0xD7E895, 0xA6EDAF, 0xA2F2DA,
            0x99FFFC, 0xDDDDDD, 0x111111, 0x111111 }
    },
    { "matt", "Matthew Conte's NES palette",
        { 0x808080, 0x0000bb, 0x3700bf, 0x8400a6,
            0xbb006a, 0xb7001e, 0xb30000, 0x912600,
            0x7b2b00, 0x003e00, 0x00480d, 0x003c22,
            0x002f66, 0x000000, 0x050505, 0x050505,
            0xc8c8c8, 0x0059ff, 0x443cff, 0xb733cc,
            0xff33aa, 0xff375e, 0xff371a, 0xd54b00,
            0xc46200, 0x3c7b00, 0x1e8415, 0x009566,
            0x0084c4, 0x111111, 0x090909, 0x090909,
            0xffffff, 0x0095ff, 0x6f84ff, 0xd56fff,
            0xff77cc, 0xff6f99, 0xff7b59, 0xff915f,
            0xffa233, 0xa6bf00, 0x51d96a, 0x4dd5ae,
            0x00d9ff, 0x666666, 0x0d0d0d, 0x0d0d0d,
            0xffffff, 0x84bfff, 0xbbbbff, 0xd0bbff,
            0xffbfea, 0xffbfcc, 0xffc4b7, 0xffccae,
            0xffd9a2, 0xcce199, 0xaeeeb7, 0xaaf7ee,
            0xb3eeff, 0xdddddd, 0x111111, 0x111111 }
    },
    { "pasofami", "Palette from PasoFami/99",
        { 0x7f7f7f, 0x0000ff, 0x0000bf, 0x472bbf,
            0x970087, 0xab0023, 0xab1300, 0x8b1700,
            0x533000, 0x007800, 0x006b00, 0x005b00,
            0x004358, 0x000000, 0x000000, 0x000000,
            0xbfbfbf, 0x0078f8, 0x0058f8, 0x6b47ff,
            0xdb00cd, 0xe7005b, 0xf83800, 0xe75f13,
            0xaf7f00, 0x00b800, 0x00ab00, 0x00ab47,
            0x008b8b, 0x000000, 0x000000, 0x000000,
            0xf8f8f8, 0x3fbfff, 0x6b88ff, 0x9878f8,
            0xf878f8, 0xf85898, 0xf87858, 0xffa347,
            0xf8b800, 0xb8f818, 0x5bdb57, 0x58f898,
            0x00ebdb, 0x787878, 0x000000, 0x000000,
            0xffffff, 0xa7e7ff, 0xb8b8f8, 0xd8b8f8,
            0xf8b8f8, 0xfba7c3, 0xf0d0b0, 0xffe3ab,
            0xfbdb7b, 0xd8f878, 0xb8f8b8, 0xb8f8d8,
            0x00ffff, 0xf8d8f8, 0x000000, 0x000000 }
    },
    { "crashman", "CrashMan's NES palette",
        { 0x585858, 0x001173, 0x000062, 0x472bbf,
            0x970087, 0x910009, 0x6f1100, 0x4c1008,
            0x371e00, 0x002f00, 0x005500, 0x004d15,
            0x002840, 0x000000, 0x000000, 0x000000,
            0xa0a0a0, 0x004499, 0x2c2cc8, 0x590daa,
            0xae006a, 0xb00040, 0xb83418, 0x983010,
            0x704000, 0x308000, 0x207808, 0x007b33,
            0x1c6888, 0x000000, 0x000000, 0x000000,
            0xf8f8f8, 0x267be1, 0x5870f0, 0x9878f8,
            0xff73c8, 0xf060a8, 0xd07b37, 0xe09040,
            0xf8b300, 0x8cbc00, 0x40a858, 0x58f898,
            0x00b7bf, 0x787878, 0x000000, 0x000000,
            0xffffff, 0xa7e7ff, 0xb8b8f8, 0xd8b8f8,
            0xe6a6ff, 0xf29dc4, 0xf0c0b0, 0xfce4b0,
            0xe0e01e, 0xd8f878, 0xc0e890, 0x95f7c8,
            0x98e0e8, 0xf8d8f8, 0x000000, 0x000000 }
    },
    { "mess", "palette from MESS NES driver",
        { 0x747474, 0x24188c, 0x0000a8, 0x44009c,
            0x8c0074, 0xa80010, 0xa40000, 0x7c0800,
            0x402c00, 0x004400, 0x005000, 0x003c14,
            0x183c5c, 0x000000, 0x000000, 0x000000,
            0xbcbcbc, 0x0070ec, 0x2038ec, 0x8000f0,
            0xbc00bc, 0xe40058, 0xd82800, 0xc84c0c,
            0x887000, 0x009400, 0x00a800, 0x009038,
            0x008088, 0x000000, 0x000000, 0x000000,
            0xfcfcfc, 0x3cbcfc, 0x5c94fc, 0x4088fc,
            0xf478fc, 0xfc74b4, 0xfc7460, 0xfc9838,
            0xf0bc3c, 0x80d010, 0x4cdc48, 0x58f898,
            0x00e8d8, 0x000000, 0x000000, 0x000000,
            0xfcfcfc, 0xa8e4fc, 0xc4d4fc, 0xd4c8fc,
            0xfcc4fc, 0xfcc4d8, 0xfcbcb0, 0xfcd8a8,
            0xfce4a0, 0xe0fca0, 0xa8f0bc, 0xb0fccc,
            0x9cfcf0, 0x000000, 0x000000, 0x000000 }
    },
    { "zaphod-cv", "Zaphod's VS Castlevania palette",
        { 0x7f7f7f, 0xffa347, 0x0000bf, 0x472bbf,
            0x970087, 0xf85898, 0xab1300, 0xf8b8f8,
            0xbf0000, 0x007800, 0x006b00, 0x005b00,
            0xffffff, 0x9878f8, 0x000000, 0x000000,
            0xbfbfbf, 0x0078f8, 0xab1300, 0x6b47ff,
            0x00ae00, 0xe7005b, 0xf83800, 0x7777ff,
            0xaf7f00, 0x00b800, 0x00ab00, 0x00ab47,
            0x008b8b, 0x000000, 0x000000, 0x472bbf,
            0xf8f8f8, 0xffe3ab, 0xf87858, 0x9878f8,
            0x0078f8, 0xf85898, 0xbfbfbf, 0xffa347,
            0xc800c8, 0xb8f818, 0x7f7f7f, 0x007800,
            0x00ebdb, 0x000000, 0x000000, 0xffffff,
            0xffffff, 0xa7e7ff, 0x5bdb57, 0xe75f13,
            0x004358, 0x0000ff, 0xe7005b, 0x00b800,
            0xfbdb7b, 0xd8f878, 0x8b1700, 0xffe3ab,
            0x00ffff, 0xab0023, 0x000000, 0x000000 }
    },
    { "zaphod-smb", "Zaphod's VS SMB palette",
        { 0x626a00, 0x0000ff, 0x006a77, 0x472bbf,
            0x970087, 0xab0023, 0xab1300, 0xb74800,
            0xa2a2a2, 0x007800, 0x006b00, 0x005b00,
            0xffd599, 0xffff00, 0x009900, 0x000000,
            0xff66ff, 0x0078f8, 0x0058f8, 0x6b47ff,
            0x000000, 0xe7005b, 0xf83800, 0xe75f13,
            0xaf7f00, 0x00b800, 0x5173ff, 0x00ab47,
            0x008b8b, 0x000000, 0x91ff88, 0x000088,
            0xf8f8f8, 0x3fbfff, 0x6b0000, 0x4855f8,
            0xf878f8, 0xf85898, 0x595958, 0xff009d,
            0x002f2f, 0xb8f818, 0x5bdb57, 0x58f898,
            0x00ebdb, 0x787878, 0x000000, 0x000000,
            0xffffff, 0xa7e7ff, 0x590400, 0xbb0000,
            0xf8b8f8, 0xfba7c3, 0xffffff, 0x00e3e1,
            0xfbdb7b, 0xffae00, 0xb8f8b8, 0xb8f8d8,
            0x00ff00, 0xf8d8f8, 0xffaaaa, 0x004000 }
    },
    { "vs-drmar", "VS Dr. Mario palette",
        { 0x5f97ff, 0x000000, 0x000000, 0x47009f,
            0x00ab00, 0xffffff, 0xabe7ff, 0x000000,
            0x000000, 0x000000, 0x000000, 0x000000,
            0xe7005b, 0x000000, 0x000000, 0x000000,
            0x5f97ff, 0x000000, 0x000000, 0x000000,
            0x000000, 0x8b7300, 0xcb4f0f, 0x000000,
            0xbcbcbc, 0x000000, 0x000000, 0x000000,
            0x000000, 0x000000, 0x000000, 0x000000,
            0x00ebdb, 0x000000, 0x000000, 0x000000,
            0x000000, 0xff9b3b, 0x000000, 0x000000,
            0x83d313, 0x000000, 0x3fbfff, 0x000000,
            0x0073ef, 0x000000, 0x000000, 0x000000,
            0x00ebdb, 0x000000, 0x000000, 0x000000,
            0x000000, 0x000000, 0xf3bf3f, 0x000000,
            0x005100, 0x000000, 0xc7d7ff, 0xffdbab,
            0x000000, 0x000000, 0x000000, 0x000000 }
    },
    { "vs-cv", "VS Castlevania palette",
        { 0xaf7f00, 0xffa347, 0x008b8b, 0x472bbf,
            0x970087, 0xf85898, 0xab1300, 0xf8b8f8,
            0xf83800, 0x007800, 0x006b00, 0x005b00,
            0xffffff, 0x9878f8, 0x00ab00, 0x000000,
            0xbfbfbf, 0x0078f8, 0xab1300, 0x6b47ff,
            0x000000, 0xe7005b, 0xf83800, 0x6b88ff,
            0xaf7f00, 0x00b800, 0x6b88ff, 0x00ab47,
            0x008b8b, 0x000000, 0x000000, 0x472bbf,
            0xf8f8f8, 0xffe3ab, 0xf87858, 0x9878f8,
            0x0078f8, 0xf85898, 0xbfbfbf, 0xffa347,
            0x004358, 0xb8f818, 0x7f7f7f, 0x007800,
            0x00ebdb, 0x000000, 0x000000, 0xffffff,
            0xffffff, 0xa7e7ff, 0x5bdb57, 0x6b88ff,
            0x004358, 0x0000ff, 0xe7005b, 0x00b800,
            0xfbdb7b, 0xffa347, 0x8b1700, 0xffe3ab,
            0xb8f818, 0xab0023, 0x000000, 0x007800 }
    },
    /* The default VS palette must be the last entry in the array */
    { "vs-smb", "VS SMB/VS Ice Climber palette",
        { 0xaf7f00, 0x0000ff, 0x008b8b, 0x472bbf,
            0x970087, 0xab0023, 0x0000ff, 0xe75f13,
            0xbfbfbf, 0x007800, 0x5bdb57, 0x005b00,
            0xf0d0b0, 0xffe3ab, 0x00ab00, 0x000000,
            0xbfbfbf, 0x0078f8, 0x0058f8, 0x6b47ff,
            0x000000, 0xe7005b, 0xf83800, 0xf87858,
            0xaf7f00, 0x00b800, 0x6b88ff, 0x00ab47,
            0x008b8b, 0x000000, 0x000000, 0x3fbfff,
            0xf8f8f8, 0x006b00, 0x8b1700, 0x9878f8,
            0x6b47ff, 0xf85898, 0x7f7f7f, 0xe7005b,
            0x004358, 0xb8f818, 0x0078f8, 0x58f898,
            0x00ebdb, 0xfbdb7b, 0x000000, 0x000000,
            0xffffff, 0xa7e7ff, 0xb8b8f8, 0xf83800,
            0xf8b8f8, 0xfba7c3, 0xffffff, 0x00ffff,
            0xfbdb7b, 0xffa347, 0xb8f8b8, 0xb8f8d8,
            0xb8f818, 0xf8d8f8, 0x000000, 0x007800 }
    }
};
