// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"

// jkrige - mousewheel support
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL (WM_MOUSELAST+1)  // message that will be supported by the OS 
#endif

static UINT MSH_MOUSEWHEEL;
// jkrige - mousewheel support

// jkrige - moved to winquake.h
//#define MAX_MODE_LIST	30
// jkrige - moved to winquake.h

#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200

// jkrige - limit video modes
#define VID_MINWIDTH	320
#define VID_MINHEIGHT	240
// jkrige - limit video modes

#define VID_MAXWIDTH		10000
#define VID_MAXHEIGHT		10000

// jkrige - moved to winquake.h
//#define BASEWIDTH		320
//#define BASEHEIGHT		240 // jkrige - was 200
// jkrige - moved to winquake.h


#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

byte globalcolormap[VID_GRADES*256];

extern qboolean is_3dfx;
extern qboolean is_PowerVR;

// jkrige - moved to winquake.h
/*typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	// jkrige - increased maximum video mode descriptions
	char        modedesc[33];
	//char		modedesc[17];
	// jkrige - increased maximum video mode descriptions
} vmode_t;*/
// jkrige - moved to winquake.h

typedef struct {
	int			width;
	int			height;
} lmode_t;

// jkrige - removed lowrez modes
//lmode_t	lowresmodes[] = {
	//{320, 200}, // jkrige - removed lowrez modes
//	{320, 240},
	//{400, 300}, // jkrige - removed lowrez modes
	//{512, 384}, // jkrige - removed lowrez modes
//};
// jkrige - removed lowrez modes

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean		DDActive;
qboolean		scr_skipupdate;

// jkrige - scale2d
//static vmode_t	modelist[MAX_MODE_LIST];
vmode_t	modelist[MAX_MODE_LIST];
// jkrige - scale2d

static int		nummodes;

static vmode_t	*pcurrentmode;
static vmode_t	badmode;

static DEVMODE	gdevmode;
qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static int		windowed_mouse;
static HICON	hIcon;

FX_DISPLAY_MODE_EXT fxDisplayModeExtension;
FX_SET_PALETTE_EXT fxSetPaletteExtension;
//FX_MARK_PAL_TEXTURE_EXT fxMarkPalTextureExtension;

unsigned char inverse_pal[(1<<INVERSE_PAL_TOTAL_BITS)+1];

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow, dibwindow;

int			vid_modenum = NO_MODE;
int			vid_realmode;
int			vid_default = MODE_WINDOWED;
static int	windowed_default;

unsigned char	vid_curpal[256*3];
float RTint[256],GTint[256],BTint[256];

HGLRC	baseRC;
HDC		maindc;

glvert_t glv;

// jkrige - changed gl_ztrick default to 0
//		this will now default to zero, because of the compatibility problems,
//		Z fighting, and the fact that this doesn't necessarily give us
//		any performance boost on modern hardware, where it introduces a new
//		problem: if video page swaps are too frequent (130+ fps), the OpenGL
//		pipeline might not keep up with changing Z test function on each
//		frame update, sometimes resulting in flickering or "snow".
cvar_t	gl_ztrick = {"gl_ztrick","0"};
//cvar_t	gl_ztrick = {"gl_ztrick","1",true};
// jkrige - changed gl_ztrick default to 0

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];
unsigned	d_8to24TranslucentTable[256];

float		gldepthmin, gldepthmax;

modestate_t	modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

// jkrige - glew.h
//PROC glArrayElementEXT;
//PROC glColorPointerEXT;
//PROC glTexCoordPointerEXT;
//PROC glVertexPointerEXT;
// jkrige - glew.h


// jkrige - overbrights
cvar_t gl_overbright = {"gl_overbright", "1", true};
// jkrige - overbrights


//====================================

cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2","1", true};

// jkrige - windowed mouse
cvar_t		_windowed_mouse = {"_windowed_mouse","1", true};
//cvar_t		_windowed_mouse = {"_windowed_mouse","0", true};
// jkrige - windowed mouse

// jkrige - scale2d
cvar_t vid_scale2d = {"vid_scale2d", "1", true};
// jkrige - scale2d

// jkrige - non power of two
cvar_t gl_texture_non_power_of_two = {"gl_texture_non_power_of_two", "1", true};
// jkrige - non power of two

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

// direct draw software compatability stuff

void VID_HandlePause (qboolean pause)
{
}

void VID_ForceLockState (int lk)
{
}

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}

int VID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}


void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
    RECT    rect;
    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qboolean VID_SetWindowedMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate, width, height;
	RECT			rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 "Hexen II",
		 "Hexen II",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(dibwindow, WindowRect.right - WindowRect.left,
				 WindowRect.bottom - WindowRect.top, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	// jkrige - scale2d
	//if (COM_CheckParm ("-scale2d")) {
	//	vid.height = vid.conheight = Scale2DHeight ;//modelist[modenum].height; // Scale2DHeight;
	//	vid.width = vid.conwidth =   Scale2DWidth  ;//modelist[modenum].width; //  Scale2DWidth ;
	//} else {
	//	vid.height = vid.conheight = modelist[modenum].height; // Scale2DHeight;
	//	vid.width = vid.conwidth =   modelist[modenum].width; //  Scale2DWidth ;
	//}

	if(vid_scale2d.value)
	{
		vid.width = vid.conwidth = conback->width = Scale2DWidth;
		vid.height = vid.conheight = conback->height = Scale2DHeight;
	}
	else
	{
		vid.width = vid.conwidth = conback->width = modelist[modenum].width;
		vid.height = vid.conheight = conback->height = modelist[modenum].height;
	}
	// jkrige - scale2d

	vid.numpages = 2;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}


qboolean VID_SetFullDIBMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate, width, height;
	RECT			rect;

	if (!leavecurrentmode)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen DIB mode");
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 "Hexen II",
		 "Hexen II",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	// jkrige - scale2d
	//if (COM_CheckParm ("-scale2d")) {
	//	vid.height = vid.conheight = Scale2DHeight ;//modelist[modenum].height; // Scale2DHeight;
	//	vid.width = vid.conwidth =   Scale2DWidth  ;//modelist[modenum].width; //  Scale2DWidth ;
	//} else {
	//	vid.height = vid.conheight = modelist[modenum].height; // Scale2DHeight;
	//	vid.width = vid.conwidth =   modelist[modenum].width; //  Scale2DWidth ;
	//}

	if(vid_scale2d.value)
	{
		vid.width = vid.conwidth = conback->width = Scale2DWidth;
		vid.height = vid.conheight = conback->height = Scale2DHeight;
	}
	else
	{
		vid.width = vid.conwidth = conback->width = modelist[modenum].width;
		vid.height = vid.conheight = conback->height = modelist[modenum].height;
	}
	// jkrige - scale2d

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}


int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp;
	qboolean		stat;
    MSG				msg;
	HDC				hdc;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode\n");
	}

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	Snd_ReleaseBuffer ();

	// jkrige - fmod sound system (music)
	//CDAudio_Pause ();
	FMOD_MusicPause();
	// jkrige - fmod sound system (music)

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();
	
	// jkrige - fmod sound system (music)
	//CDAudio_Resume ();
	FMOD_MusicResume();
	// jkrige - fmod sound system (music)

	Snd_AcquireBuffer ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_SafePrintf ("%s\n", VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


//====================================

BINDTEXFUNCPTR bindTexFunc;

#define TEXTURE_EXT_STRING "GL_EXT_texture_object"

#define FX_DISPLAY_MODE_EXT_STRING "gl3DfxDisplayModeEXT"

//#define FX_SET_PALETTE_EXT_STRING "gl3DfxSetPaletteEXT"
#define FX_SET_PALETTE_EXT_STRING "3DFX_set_global_palette"
#define VR_SET_PALETTE_EXT_STRING "POWERVR_set_global_palette"

//#define FX_MARK_PAL_TEXTURE_EXT_STRING "gl3DfxMarkPalettizedTextureEXT"

void Check3DfxDisplayModeExtension( void )
{
	char *tmp;
	qboolean display_mode_ext = false;

	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, FX_DISPLAY_MODE_EXT_STRING, strlen(FX_DISPLAY_MODE_EXT_STRING)) == 0)
			display_mode_ext = TRUE;
		tmp++;
	}

	fxDisplayModeExtension = NULL;
	if( !display_mode_ext )
		return;
	if ((fxDisplayModeExtension = (FX_DISPLAY_MODE_EXT)
		wglGetProcAddress((LPCSTR) FX_DISPLAY_MODE_EXT_STRING)) == NULL)
	{
		Sys_Error ("GetProcAddress for gl3DfxDisplayModeExt failed");
		return;
	}
}

void CheckSetPaletteExtension( void )
{
	char *tmp;
	qboolean set_palette_ext = false;
	char *search;

	fxSetPaletteExtension = NULL;
	
	search = NULL;
	if (is_3dfx) search = FX_SET_PALETTE_EXT_STRING;
	if (is_PowerVR) search = VR_SET_PALETTE_EXT_STRING;
	if (!search) return;


	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			//Con_Printf("Using palettized textures!\n");
			Con_Printf("...Using palettized textures\n");
			set_palette_ext = TRUE;
		}
		tmp++;
	}

	if( !set_palette_ext )
		return;
	if ((fxSetPaletteExtension = (FX_SET_PALETTE_EXT)
		wglGetProcAddress((LPCSTR) search)) == NULL)
	{
		Sys_Error ("GetProcAddress for gl3DfxSetPaletteEXT failed");
		return;
	}
}

// jkrige - anisotropic filtering
qboolean anisotropic_ext = false;
float maximumAnisotrophy = 1.0f;

void CheckAnisotropicExtension( void )
{
	char *tmp;
	char *search;
	
	search = NULL;
	search = "GL_EXT_texture_filter_anisotropic";
	if (!search) return;


	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximumAnisotrophy);
			Con_Printf("...Using GL_EXT_texture_filter_anisotropic\n");
			anisotropic_ext = true;
		}
		tmp++;
	}

	if(!anisotropic_ext)
		Con_Printf("GL_EXT_texture_filter_anisotropic not found\n");
}
// jkrige - anisotropic filtering


// jkrige - non power of two
qboolean npow2_ext = false;

void CheckNonPowTwoExtension( void )
{
	unsigned char *tmp;
	char *search;
	
	search = NULL;
	search = "GL_ARB_texture_non_power_of_two";
	if (!search) return;


	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			Con_Printf("...Using GL_ARB_texture_non_power_of_two\n");
			npow2_ext = true;
		}
		tmp++;
	}

	if(!npow2_ext)
		Con_Printf("GL_ARB_texture_non_power_of_two not found\n");
}
// jkrige - non power of two


// jkrige - half float pixel (hdr bloom)
//qboolean halffloatpixel_ext = false;

/*void HalfFloatPixelExtension( void )
{
	unsigned char *tmp;
	char *search;
	
	search = NULL;
	search = "GL_ARB_half_float_pixel";
	if (!search) return;


	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			Con_Printf("...Using GL_ARB_half_float_pixel\n");
			halffloatpixel_ext = true;
		}
		tmp++;
	}

	if(!halffloatpixel_ext)
		Con_Printf("GL_ARB_half_float_pixel not found\n");
}*/
// jkrige - half float pixel (hdr bloom)


// jkrige - framebuffer multisample (bloom)
qboolean framebuffer_multisample_ext = false;

void CheckFramebufferMultisampleExtension( void )
{
	unsigned char *tmp;
	char *search;
	
	search = NULL;
	search = "GL_EXT_framebuffer_multisample";
	if (!search) return;


	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			Con_Printf("...Using GL_EXT_framebuffer_multisample\n");
			framebuffer_multisample_ext = true;
		}
		tmp++;
	}

	if(!framebuffer_multisample_ext)
		Con_Printf("GL_EXT_framebuffer_multisample not found\n");
}
// jkrige - framebuffer multisample (bloom)


// jkrige - framebuffer object (bloom)
qboolean framebuffer_ext = false;

void CheckFramebufferExtension( void )
{
	unsigned char *tmp;
	char *search;
	
	search = NULL;
	search = "GL_EXT_framebuffer_object";
	if (!search) return;


	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			Con_Printf("...Using GL_EXT_framebuffer_object\n");
			framebuffer_ext = true;
		}
		tmp++;
	}

	if(!framebuffer_ext)
		Con_Printf("GL_EXT_framebuffer_object not found\n");
}
// jkrige - framebuffer object (bloom)


/*
void Check3DfxMarkPaletteTextureExtension( void )
{
	char *tmp;
	qboolean found_ext = false;

	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, FX_SET_PALETTE_EXT_STRING, strlen(FX_SET_PALETTE_EXT_STRING)) == 0)
			found_ext = TRUE;
		tmp++;
	}

	fxMarkPalTextureExtension = NULL;
	if( !found_ext )
		return;
	if ((fxMarkPalTextureExtension = (FX_MARK_PAL_TEXTURE_EXT)
		wglGetProcAddress((LPCSTR) FX_MARK_PAL_TEXTURE_EXT_STRING)) == NULL)
	{
		Sys_Error ("GetProcAddress for fxMarkPalTextureExtension failed");
		return;
	}
}
*/

void CheckTextureExtensions (void)
{
	char		*tmp;
	qboolean	texture_ext;
	HINSTANCE	hInstGL;

	texture_ext = FALSE;
	/* check for texture extension */
	tmp = (unsigned char *)glGetString(GL_EXTENSIONS);
	while (*tmp)
	{
		if (strncmp((const char*)tmp, TEXTURE_EXT_STRING, strlen(TEXTURE_EXT_STRING)) == 0)
			texture_ext = TRUE;
		tmp++;
	}

	if (!texture_ext || COM_CheckParm ("-gl11") )
	{
		hInstGL = LoadLibrary("opengl32.dll");
		
		if (hInstGL == NULL)
			Sys_Error ("Couldn't load opengl32.dll\n");

		bindTexFunc = (void *)GetProcAddress(hInstGL,"glBindTexture");

		if (!bindTexFunc)
			Sys_Error ("No texture objects!");
		return;
	}

/* load library and get procedure adresses for texture extension API */
	if ((bindTexFunc = (BINDTEXFUNCPTR)
		wglGetProcAddress((LPCSTR) "glBindTextureEXT")) == NULL)
	{
		Sys_Error ("GetProcAddress for BindTextureEXT failed");
		return;
	}
}

// jkrige - gamma
BOOL  ( WINAPI * qwglGetDeviceGammaRamp3DFX)( HDC, LPVOID );
BOOL  ( WINAPI * qwglSetDeviceGammaRamp3DFX)( HDC, LPVOID );

void Check3DFXGammaExtensions (void)
{
	char		*gl_extensions;
	HINSTANCE	hInstGL;

	gl_extensions = (unsigned char *)glGetString(GL_EXTENSIONS);

	// WGL_3DFX_gamma_control
	qwglGetDeviceGammaRamp3DFX = NULL;
	qwglSetDeviceGammaRamp3DFX = NULL;

	if ( strstr( gl_extensions, "WGL_3DFX_gamma_control" ) )
	{
		//if ( !r_ignorehwgamma->integer && r_ext_gamma_control->integer )
		//{
			qwglGetDeviceGammaRamp3DFX = ( BOOL ( WINAPI * )( HDC, LPVOID ) ) wglGetProcAddress((LPCSTR) "wglGetDeviceGammaRamp3DFX" );
			qwglSetDeviceGammaRamp3DFX = ( BOOL ( WINAPI * )( HDC, LPVOID ) ) wglGetProcAddress((LPCSTR) "wglSetDeviceGammaRamp3DFX" );

			if ( qwglGetDeviceGammaRamp3DFX && qwglSetDeviceGammaRamp3DFX )
			{
				//Con_Printf("using WGL_3DFX_gamma_control\n");
				Con_Printf("...Using WGL_3DFX_gamma_control\n");
			}
			else
			{
				qwglGetDeviceGammaRamp3DFX = NULL;
				qwglSetDeviceGammaRamp3DFX = NULL;
			}
		//}
		//else
		//{
		//	ri.Printf( PRINT_ALL, "...ignoring WGL_3DFX_gamma_control\n" );
		//}
	}
	else
	{
		Con_Printf("WGL_3DFX_gamma_control not found\n");
	}
}

#if 0
void CheckArrayExtensions (void)
{
	char		*tmp;

	/* check for texture extension */
	tmp = (unsigned char *)glGetString(GL_EXTENSIONS);
	while (*tmp)
	{
		if (strncmp((const char*)tmp, "GL_EXT_vertex_array", strlen("GL_EXT_vertex_array")) == 0)
		{
			if (
((glArrayElementEXT = wglGetProcAddress("glArrayElementEXT")) == NULL) ||
((glColorPointerEXT = wglGetProcAddress("glColorPointerEXT")) == NULL) ||
((glTexCoordPointerEXT = wglGetProcAddress("glTexCoordPointerEXT")) == NULL) ||
((glVertexPointerEXT = wglGetProcAddress("glVertexPointerEXT")) == NULL) )
			{
				Sys_Error ("GetProcAddress for vertex extension failed");
				return;
			}
			return;
		}
		tmp++;
	}

	Sys_Error ("Vertex array extension not present");
}
#endif

//int		texture_mode = GL_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int		texture_extension_number = 1;
//int		normalmap_extension_number = 1; // jkrige - normal mapping

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	// jkrige - glew.h
	glewInit();
	// jkrige - glew.h

	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);

	// jkrige - don't display extension list
	gl_extensions = glGetString (GL_EXTENSIONS);
	//Con_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);
	// jkrige - don't display extension list
	Con_Printf("------------------------------------\n");

	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4))
	{
		is_3dfx = true;
	}

	if (!Q_strncasecmp ((char *)gl_renderer, "PowerVR PCX1",12) ||
		!Q_strncasecmp ((char *)gl_renderer, "PowerVR PCX2",12))
	{
		is_PowerVR = true;
	}

	CheckTextureExtensions ();

    fxDisplayModeExtension = NULL;
	fxSetPaletteExtension = NULL;
	//fxMarkPalTextureExtension = NULL;
	Check3DfxDisplayModeExtension();
	Check3DFXGammaExtensions (); // jkrige - gamma
	CheckSetPaletteExtension();
	CheckAnisotropicExtension(); // jkrige - anisotropic filtering
	CheckNonPowTwoExtension();   // jkrige - non power of two
	//HalfFloatPixelExtension();   // jkrige - half float pixel (hdr bloom)
	CheckFramebufferMultisampleExtension(); // jkrige - framebuffer multisample (bloom)
	CheckFramebufferExtension(); // jkrige - framebuffer object (bloom)
	//Check3DfxMarkPaletteTextureExtension();	

	// jkrige - clear color changed to black (used to be red)
    glClearColor(0.0, 0.0, 0.0, 0.0);
	//glClearColor (1,0,0,0);
	// jkrige - clear color changed to black (used to be red)
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	// Pa3PyX: to avoid clipping of smaller fonts/graphics
    glAlphaFunc(GL_GREATER, 0.632);
	//glAlphaFunc(GL_GREATER, 0.666);

	// jkrige - brush z-fighting
	glPolygonOffset(0.05, 25);
	// jkrige - brush z-fighting

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

#if 0
	CheckArrayExtensions ();

	glEnable (GL_VERTEX_ARRAY_EXT);
	glEnable (GL_TEXTURE_COORD_ARRAY_EXT);
	glVertexPointerEXT (3, GL_FLOAT, 0, 0, &glv.x);
	glTexCoordPointerEXT (2, GL_FLOAT, 0, 0, &glv.s);
	glColorPointerEXT (3, GL_FLOAT, 0, 0, &glv.r);
#endif

	// jkrige - gamma
	VG_CheckHardwareGamma();
	VG_SetColorMappings();
	// jkrige - gamma
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	extern cvar_t gl_clear;

	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
	if (!scr_skipupdate)
		SwapBuffers(maindc);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if ((int)_windowed_mouse.value != windowed_mouse)
		{
			if (_windowed_mouse.value)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			windowed_mouse = (int)_windowed_mouse.value;
		}
	}
}


int ColorIndex[16] =
{
	0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231
};

unsigned ColorPercent[16] =
{
	25, 51, 76, 102, 114, 127, 140, 153, 165, 178, 191, 204, 216, 229, 237, 247
};

static int ConvertTrueColorToPal( unsigned char *true_color, unsigned char *palette )
{
	int i;
	long min_dist;
	int min_index;
	long r, g, b;

	min_dist = 256 * 256 + 256 * 256 + 256 * 256;
	min_index = -1;
	r = ( long )true_color[0];
	g = ( long )true_color[1];
	b = ( long )true_color[2];

	for( i = 0; i < 256; i++ )
	{
		long palr, palg, palb, dist;
		long dr, dg, db;

		palr = palette[3*i];
		palg = palette[3*i+1];
		palb = palette[3*i+2];
		dr = palr - r;
		dg = palg - g;
		db = palb - b;
		dist = dr * dr + dg * dg + db * db;
		if( dist < min_dist )
		{
			min_dist = dist;
			min_index = i;
		}
	}
	return min_index;
}

void	VID_CreateInversePalette( unsigned char *palette )
{
#ifdef DO_BUILD
	FILE *FH;

	long r, g, b;
	long index = 0;
	unsigned char true_color[3];
	static qboolean been_here = false;

	if( been_here )
		return;

	been_here = true;
	
	for( r = 0; r < ( 1 << INVERSE_PAL_R_BITS ); r++ )
	{
		for( g = 0; g < ( 1 << INVERSE_PAL_G_BITS ); g++ )
		{
			for( b = 0; b < ( 1 << INVERSE_PAL_B_BITS ); b++ )
			{
				true_color[0] = ( unsigned char )( r << ( 8 - INVERSE_PAL_R_BITS ) );
				true_color[1] = ( unsigned char )( g << ( 8 - INVERSE_PAL_G_BITS ) );
				true_color[2] = ( unsigned char )( b << ( 8 - INVERSE_PAL_B_BITS ) );
				inverse_pal[index] = ConvertTrueColorToPal( true_color, palette );
				index++;
			}
		}
	}

	FH = fopen("data1\\gfx\\invpal.lmp","wb");
	fwrite(inverse_pal,1,sizeof(inverse_pal),FH);
	fclose(FH);
#else
	COM_LoadStackFile ("gfx/invpal.lmp",inverse_pal,sizeof(inverse_pal));
#endif
}

void VID_Download3DfxPalette( void )
{
	unsigned long fxPalette[256];
	int i;

	for( i = 0; i < 256; i++ )
	{
		fxPalette[i] = 0xff000000 | 
			( (  d_8to24table[i] & 0x000000ff ) << 16 ) |
			( (  d_8to24table[i] & 0x0000ff00 ) ) |
			( (  d_8to24table[i] & 0x00ff0000 ) >> 16 );

//		fxPalette[i] = i<<16; // 0x00rrggbb
	}
	if( fxSetPaletteExtension )
		fxSetPaletteExtension( fxPalette );
}

void VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	int		r,g,b,v;
	int		i,c,p;
	unsigned	*table;
	
	
	if( is_3dfx || is_PowerVR)
		VID_CreateInversePalette( palette );

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}

	d_8to24table[255] &= 0xffffff;	// 255 is transparent

	if( is_3dfx || is_PowerVR )
		VID_Download3DfxPalette();

	pal = palette;
	table = d_8to24TranslucentTable;

	for (i=0; i<16;i++)
	{
		c = ColorIndex[i]*3;

		r = pal[c];
		g = pal[c+1];
		b = pal[c+2];

		for(p=0;p<16;p++)
		{
			v = (ColorPercent[15-p]<<24) + (r<<0) + (g<<8) + (b<<16);
			//v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
			*table++ = v;

			RTint[i*16+p] = ((float)r) / ((float)ColorPercent[15-p]) ;
			GTint[i*16+p] = ((float)g) / ((float)ColorPercent[15-p]);
			BTint[i*16+p] = ((float)b) / ((float)ColorPercent[15-p]);
		}
	}
}

BOOL	gammaworks;
void	VID_ShiftPalette (unsigned char *palette)
{
	extern	byte ramps[3][256];
	
//	VID_SetPalette (palette);

//	gammaworks = SetDeviceGammaRamp (maindc, ramps);
}


void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}


void	VID_Shutdown (void)
{
   	HGLRC hRC;
   	HDC	  hDC;

	if (vid_initialized)
	{
    	hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

		// jkrige - gamma
		VG_RestoreGamma();
		// jkrige - gamma

    	wglMakeCurrent(NULL, NULL);

    	if (hRC)
    	    wglDeleteContext(hRC);

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate(false, false);
	}
}


//==========================================================================


BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGL
	|  PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
	32,				// 32-bit z-buffer	
	0,				// no stencil buffer
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

	// jkrige
	//PIXELFORMATDESCRIPTOR pfd;
	//ZeroMemory(&pfd, sizeof(pfd));
    //pfd.nSize = sizeof(pfd);
    //pfd.nVersion = 1;
    //pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    //pfd.iPixelType = PFD_TYPE_RGBA;
    //pfd.cColorBits = 24; /*(flags & OS_ALPHA) ? 32 : 24;*/
    //pfd.cAlphaBits = 0; /*(flags & OS_ALPHA) ? 8  : 0;*/
    //pfd.cDepthBits = 0; /*(flags & OS_DEPTH) ? 16 : 0;*/
    //pfd.cStencilBits = 0;
    //pfd.iLayerType = PFD_MAIN_PLANE;
	// jkrige

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}



byte        scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
					}; 

byte        shiftscantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '!',    '@',    '#',    '$',    '%',    '^', 
	'&',    '*',    '(',    ')',    '_',    '+',    K_BACKSPACE, 9, // 0 
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I', 
	'O',    'P',    '{',    '}',    13 ,    K_CTRL,'A',  'S',      // 1 
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':', 
	'"' ,    '~',    K_SHIFT,'|',  'Z',    'X',    'C',    'V',      // 2 
	'B',    'N',    'M',    '<',    '>',    '?',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10,0  ,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'_',K_LEFTARROW,'%',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
					}; 


/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	return scantokey[key];
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
    HDC			hdc;
    int			i, t;
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}


/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LONG    lRet = 1;
	int		fwKeys, xPos, yPos, fActive, fMinimized, temp;

	// jkrige - mousewheel support
	if ( uMsg == MSH_MOUSEWHEEL )
	{
		if ( ( ( int ) wParam ) > 0 )
		{
			Key_Event( K_MWHEELUP, true);
			Key_Event( K_MWHEELUP, false);
		}
		else
		{
			Key_Event( K_MWHEELDOWN, true);
			Key_Event( K_MWHEELDOWN, false);
		}
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}
	// jkrige - mousewheel support

    switch (uMsg)
    {
		// jkrige - mousewheel support
		case WM_MOUSEWHEEL:
			/*
			** this chunk of code theoretically only works under NT4 and Win98
			** since this message doesn't exist under Win95
			*/
			if ( ( short ) HIWORD( wParam ) > 0 )
			{
				Key_Event( K_MWHEELUP, true);
				Key_Event( K_MWHEELUP, false);
			}
			else
			{
				Key_Event( K_MWHEELDOWN, true);
				Key_Event( K_MWHEELDOWN, false);
			}
			break;
		// jkrige - mousewheel support

		case WM_CREATE:
			// jkrige - mousewheel support
			MSH_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG");
			// jkrige - mousewheel support
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (MapKey(lParam), true);
			break;
			
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);

			break;


    	case WM_SIZE:
            break;

   	    case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
			if (dibwindow)
				DestroyWindow (dibwindow);

            PostQuitMessage (0);
        }
        break;

		// jkrige - fmod sound system (music)
#ifndef UQE_FMOD_CDAUDIO
		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;
#endif
		// jkrige - fmod sound system (music)

    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}


/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}

	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{

	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%dx%d)", modelist[MODE_FULLSCREEN_DEFAULT].width, modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[100]; // jkrige - might be low
	//static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "Desktop resolution (%dx%d)", modelist[MODE_FULLSCREEN_DEFAULT].width, modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{

	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int		t, modenum;
	
	modenum = atoi (Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}

void VID_Switch_f (void)
{
	int newmode;

	newmode = atoi (Cmd_Argv(1));
	if( !fxDisplayModeExtension )
		return;

//	fxDisplayModeExtension( newmode );
	fxDisplayModeExtension( 0 );
	Sleep( 2 );
	fxDisplayModeExtension( 1 );
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
	char		*pinfo;
	vmode_t		*pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}


void VID_InitDIB (HINSTANCE hInstance)
{
	WNDCLASS		wc;
	HDC				hdc;
	int				i;

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "Hexen II";

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height= atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	sprintf (modelist[0].modedesc, "%dx%d", modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}


/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode;
	int		i, modenum, cmodes, originalnummodes, existingmode, numlowresmodes;
	int		j, bpp, done;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		// jkrige - limit video modes
		//if ((devmode.dmBitsPerPel >= 15) && (nummodes < MAX_MODE_LIST))
		if ((devmode.dmBitsPerPel == 32) &&
			//(devmode.dmPelsWidth >= VID_MINWIDTH) &&
			//(devmode.dmPelsHeight >= VID_MINHEIGHT) &&
			(
				(devmode.dmPelsWidth == VID_MINWIDTH && devmode.dmPelsHeight == VID_MINHEIGHT) || (devmode.dmPelsWidth >= 640 && devmode.dmPelsHeight >= 480)
			) &&
			(devmode.dmPelsWidth <= VID_MAXWIDTH) &&
			(devmode.dmPelsHeight <= VID_MAXHEIGHT) &&
			(nummodes < MAX_MODE_LIST))
		// jkrige - limit video modes
		{
			devmode.dmFields = DM_BITSPERPEL |
							   DM_PELSWIDTH |
							   DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;

				// jkrige - limit video modes
				//sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);
				sprintf (modelist[nummodes].modedesc, "%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight);
				// jkrige - limit video modes


			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;

						// jkrige - limit video modes
						//sprintf (modelist[nummodes].modedesc, "%dx%dx%d", modelist[nummodes].width, modelist[nummodes].height, modelist[nummodes].bpp);
						sprintf (modelist[nummodes].modedesc, "%dx%d", modelist[nummodes].width, modelist[nummodes].height);
						// jkrige - limit video modes
					}
				}

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

	// jkrige - limit video modes
// see if there are any low-res modes that aren't being reported
	/*numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]); // jkrige - limit video modes
	bpp = 16;
	done = 0;

	do
	{
		for (j=0 ; (j<numlowresmodes) && (nummodes < MAX_MODE_LIST) ; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
						 devmode.dmPelsWidth, devmode.dmPelsHeight,
						 devmode.dmBitsPerPel);

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}
		switch (bpp)
		{
			case 16:
				bpp = 32;
				break;

			case 32:
				bpp = 24;
				break;

			case 24:
				done = 1;
				break;
		}
	} while (!done);
	*/

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}


/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int		i, existingmode;
	int		basenummodes, width, height, bpp, findbpp, done;
	byte	*ptmp;
	char	gldir[MAX_OSPATH];
	HDC		hdc;
	DEVMODE	devmode;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&gl_ztrick);

	// jkrige - scale2d
	Cvar_RegisterVariable (&vid_scale2d);
	// jkrige - scale2d

	// jkrige - non power of two
	Cvar_RegisterVariable (&gl_texture_non_power_of_two);
	// jkrige - non power of two

	// jkrige - overbrights
	Cvar_RegisterVariable (&gl_overbright);
	// jkrige - overbrights


	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_switch", VID_Switch_f);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB (global_hInstance);

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = atoi(com_argv[COM_CheckParm("-mode")+1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				modelist[MODE_FULLSCREEN_DEFAULT].width = GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height = GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
				{
					width = atoi(com_argv[COM_CheckParm("-width")+1]);
				}
				else
				{
					width = 640;
				}

				// jkrige - limit video modes
				/*if (COM_CheckParm("-bpp"))
				{
					bpp = atoi(com_argv[COM_CheckParm("-bpp")+1]);
					findbpp = 0;
				}
				else
				{
					//bpp = 15;
					bpp = 16; // jkrige - change bpp from 16 to 32 default: was 15
					findbpp = 1;
				}*/
				bpp = 16;
				findbpp = 1;
				// jkrige - limit video modes

				if (COM_CheckParm("-height"))
					height = atoi(com_argv[COM_CheckParm("-height")+1]);
				//else
				//	height = 480; // jkrige - change default height

			// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;

					// jkrige - limit video modes
					//sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);
					sprintf (modelist[nummodes].modedesc, "%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight);
					// jkrige - limit video modes

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp))
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
					{
						nummodes++;
					}
				}

				done = 0;

				do
				{
					// jkrige - moved above width/height check
					//if (!done)
					//{
						if (findbpp)
						{
							switch (bpp)
							{
							// jkrige - limit video modes
							//case 15:
							//	bpp = 16;
							//	break;
							// jkrige - limit video modes
							case 16:
								bpp = 32;
								break;
							case 32:
								bpp = 24;
								break;
							case 24:
								done = 1;
								break;
							}
						}
						//else
						//{
						//	done = 1;
						//}
					//}
					// jkrige - moved above width/height check

					if (COM_CheckParm("-height"))
					{
						height = atoi(com_argv[COM_CheckParm("-height")+1]);

						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) &&
								(modelist[i].height == height) &&
								(modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp))
							//if ((modelist[i].width == width) && (modelist[i].bpp == bpp) && (modelist[i].height == height)) // jkrige - change default height
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					// jkrige - moved above width/height check
					/*if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15:
								bpp = 16;
								break;
							case 16:
								bpp = 32;
								break;
							case 32:
								bpp = 24;
								break;
							case 24:
								done = 1;
								break;
							}
						}
						else
						{
							done = 1;
						}
					}*/
					// jkrige - moved above width/height check
				} while (!done);

				if (!vid_default)
				{
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	DestroyWindow (hwnd_dialog);

	VID_SetMode (vid_default, palette);

    maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

    baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("wglCreateContect failed");
    if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();

	// jkrige - mdl meshing removal
	//sprintf (gldir, "%s/glhexen", com_gamedir);
	//Sys_mkdir (gldir);
	//sprintf (gldir, "%s/glhexen/boss", com_gamedir);
	//Sys_mkdir (gldir);
	//sprintf (gldir, "%s/glhexen/puzzle", com_gamedir);
	//Sys_mkdir (gldir);
	// jkrige - mdl meshing removal

	vid_realmode = vid_modenum;

	VID_SetPalette (palette);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");

	// jkrige - scale2d
	COM_SetScale2D();
	// jkrige - scale2d
}


//========================================================
// Video menu stuff
//========================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 4) // jkrige - was 2, now 4
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			lnummodes, i, j, k, column, row, dup, dupmode;
	char		temp[100];
	vmode_t		*pv;

	ScrollTitle("gfx/menu/title7.lmp");

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();
	
	for (i=1 ; (i<lnummodes) && (vid_wmodes < MAX_MODEDESCS) ; i++)
	{
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;

	}

	if (vid_wmodes > 0)
	{
		// jkrige - limit video modes
		//M_Print (2*8, 60+0*8, "Fullscreen Modes (WIDTHxHEIGHTxBPP)");
		M_Print (4*8, 60+0*8, "Fullscreen Modes  (WIDTHxHEIGHT)");
		// jkrige - limit video modes

		column = 8;
		row = 60+2*8;

		for (i=0 ; i<vid_wmodes ; i++)
		{
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13*8;

			if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
			{
				column = 8;
				row += 8;
			}
		}
	}

	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*2, "Video modes must be set from the");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*3, "command line with -width <width>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*4, "and -height <height>");
	// jkrige - limit video modes
	//M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*5, "and -bpp <bits-per-pixel>");
	// jkrige - limit video modes
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6, "Select windowed mode with -window");
}


/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("raven/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
	}
}

void D_ShowLoadingSize(void)
{
	if (!vid_initialized)
		return;

	glDrawBuffer  (GL_FRONT);

	SCR_DrawLoading();

	glDrawBuffer  (GL_BACK);
}
