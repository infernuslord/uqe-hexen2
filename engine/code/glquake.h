/*
 * $Header: /H2 Mission Pack/glquake.h 10    3/09/98 11:24p Mgummelt $
 */

// disable data conversion warnings

#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#include <windows.h>

// jkrige - glew.h
#include <GL\glew.h>
#include <GL\wglew.h>
// jkrige - glew.h

#include <gl\gl.h>
#include <gl\glu.h>

#include "glext.h" // jkrige - opengl extensions
//#include "glxext.h" // jkrige - opengl extensions
#include "wglext.h" // jkrige - windows opengl extensions


// jkrige - framebuffer object
#include "ph.h"
#include "gl_fbo.h"
#include "gl_bloom.h"
// jkrige - framebuffer object


void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);


// Function prototypes for the Texture Object Extension routines
typedef GLboolean (APIENTRY *ARETEXRESFUNCPTR)(GLsizei, const GLuint *,
                    const GLboolean *);
typedef void (APIENTRY *BINDTEXFUNCPTR)(GLenum, GLuint);
typedef void (APIENTRY *DELTEXFUNCPTR)(GLsizei, const GLuint *);
typedef void (APIENTRY *GENTEXFUNCPTR)(GLsizei, GLuint *);
typedef GLboolean (APIENTRY *ISTEXFUNCPTR)(GLuint);
typedef void (APIENTRY *PRIORTEXFUNCPTR)(GLsizei, const GLuint *,
                    const GLclampf *);
typedef void (APIENTRY *TEXSUBIMAGEPTR)(int, int, int, int, int, int, int, int, void *);
typedef int  (APIENTRY *FX_DISPLAY_MODE_EXT)(int);
typedef void (APIENTRY *FX_SET_PALETTE_EXT)( unsigned long * );
typedef void (APIENTRY *FX_MARK_PAL_TEXTURE_EXT)( void );

extern	BINDTEXFUNCPTR bindTexFunc;
extern	DELTEXFUNCPTR delTexFunc;
extern	TEXSUBIMAGEPTR TexSubImage2DFunc;
extern  FX_DISPLAY_MODE_EXT fxDisplayModeExtension;
extern  FX_SET_PALETTE_EXT fxSetPaletteExtension;
extern  FX_MARK_PAL_TEXTURE_EXT fxMarkPalTextureExtension;

// jkrige - gamma
extern BOOL  ( WINAPI * qwglGetDeviceGammaRamp3DFX)( HDC, LPVOID );
extern BOOL  ( WINAPI * qwglSetDeviceGammaRamp3DFX)( HDC, LPVOID );
// jkrige - gamma

// jkrige - anisotropic filtering
extern qboolean anisotropic_ext;
extern float maximumAnisotrophy;
// jkrige - anisotropic filtering


// jkrige - non power of two
extern qboolean npow2_ext;
// jkrige - non power of two


// jkrige - framebuffer multisample (bloom)
extern qboolean framebuffer_multisample_ext;
// jkrige - framebuffer multisample (bloom)


// jkrige - framebuffer object (bloom)
extern qboolean framebuffer_ext;
// jkrige - framebuffer object (bloom)


// jkrige - half float pixel (hdr bloom)
//extern qboolean halffloatpixel_ext;
// jkrige - half float pixel (hdr bloom)

typedef struct
{
	int		texnum;

	qboolean	tex_norm; // jkrige - normal mapping
	qboolean	tex_luma; // jkrige - luma textures

	char	identifier[64];
	int		width, height;
	qboolean	mipmap;
	int		lhcsum; // jkrige - memleak & texture mismatch
} gltexture_t;

//#define MAX_GLTEXTURES	2048
#define MAX_GLTEXTURES	4096 // jkrige - increased maximum number of opengl textures

extern gltexture_t	gltextures[MAX_GLTEXTURES];
extern int			numgltextures;

// jkrige - normal mapping
//#define GL_COMBINE		0x8570
//#define GL_COMBINE_RGB	0x8571
//#define GL_DOT3_RGB		0x86AE

#define	JK_NORM_TEX		(MAX_GLTEXTURES)
// jkrige - normal mapping

// jkrige - luma textures
#define	JK_LUMA_TEX		(MAX_GLTEXTURES * 2)
// jkrige - luma textures

#define INVERSE_PAL_R_BITS 6
#define INVERSE_PAL_G_BITS 6
#define INVERSE_PAL_B_BITS 6
#define INVERSE_PAL_TOTAL_BITS \
	( INVERSE_PAL_R_BITS + INVERSE_PAL_G_BITS + INVERSE_PAL_B_BITS )

extern unsigned char inverse_pal[(1<<INVERSE_PAL_TOTAL_BITS)+1];

extern	int		texture_extension_number;
//extern	int		normalmap_extension_number; // jkrige - normal mapping
extern	int		texture_mode;

extern	float	gldepthmin, gldepthmax;

#define MAX_EXTRA_TEXTURES 156   // 255-100+1
extern int			gl_extra_textures[MAX_EXTRA_TEXTURES];   // generic textures for models

void GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha);
void GL_Upload8 (byte *data, int width, int height, qboolean upload32, qboolean mipmap, qboolean alpha, int mode);
// jkrige - bytesperpixel
//int GL_LoadTexture (char *identifier, int width, int height, byte *data, int mipmap, int alpha, int mode);
int GL_LoadTexture (char *identifier, char *textype, int width, int height, byte *data, int mipmap, int alpha, int mode, int bytesperpixel);
// jkrige - bytesperpixel
int GL_LoadTransTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, byte Alpha);
int GL_FindTexture (char *identifier);

// jkrige - .lit colored lights
void GL_SetupLightmapFmt (qboolean check_cmdline);
// jkrige - .lit colored lights

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern glvert_t glv;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

extern	int glx, gly, glwidth, glheight;

// jkrige - glew.h
//extern	PROC glArrayElementEXT;
//extern	PROC glColorPointerEXT;
//extern	PROC glTexturePointerEXT;
//extern	PROC glVertexPointerEXT;
// jkrige - glew.h


// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define MAX_SKIN_HEIGHT 480

#define TILE_SIZE		128		// size of textures generated by R_GenTiledSurf

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01


void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base);

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	unsigned			width;
	unsigned			height;		// DEBUG only needed for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	byte				data[4];	// width*height elements
} surfcache_t;


typedef struct
{
	pixel_t		*surfdat;	// destination for generated surface
	int			rowbytes;	// destination logical width in bytes
	msurface_t	*surf;		// description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS];
							// adjust for lightmap levels for dynamic lighting
	texture_t	*texture;	// corrected for animating textures
	int			surfmip;	// mipmapped ratio of surface texels / world pixels
	int			surfwidth;	// in mipmapped texels
	int			surfheight;	// in mipmapped texels
} drawsurf_t;


// Changes to ptype_t must also be made in d_iface.h
typedef enum {
	pt_static,
	pt_grav,
	pt_fastgrav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2,
	pt_rain,
	pt_c_explode,
	pt_c_explode2,
	pt_spit,
	pt_fireball,
	pt_ice,
	pt_spell,
	pt_test,
	pt_quake,
	pt_rd,			// rider's death
	pt_vorpal,
	pt_setstaff,
	pt_magicmissile,
	pt_boneshard,
	pt_scarab,
	pt_acidball,
	pt_darken,
	pt_snow,
	pt_gravwell,
	pt_redfire
} ptype_t;

// Changes to rtype_t must also be made in glquake.h
typedef enum
{
   rt_rocket_trail = 0,
	rt_smoke,
	rt_blood,
	rt_tracer,
	rt_slight_blood,
	rt_tracer2,
	rt_voor_trail,
	rt_fireball,
	rt_ice,
	rt_spit,
	rt_spell,
	rt_vorpal,
	rt_setstaff,
	rt_magicmissile,
	rt_boneshard,
	rt_scarab,
	rt_acidball,
	rt_bloodshot,
} rt_type_t;

// !!! if this is changed, it must be changed in d_iface.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
// drivers never touch the following fields
	struct particle_s	*next;
	vec3_t		vel;
	vec3_t		min_org;
	vec3_t		max_org;
	float		ramp;
	float		die;
	byte		type;
	byte		flags;
	byte		count;
} particle_t;


//====================================================


extern	entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys, c_sky_polys;


//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;
extern	int	currenttexture;

// jkrige - texture mode
extern	int	particletexture_linear;
extern	int	particletexture_point;
// jkrige - texture mode

extern	int	playertextures;

extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
//extern	cvar_t	r_shadows; // jkrige - removed alias shadows
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_wholeframe;

// jkrige - fix dynamic light shine through
extern	cvar_t	r_dynamic_sidemark;
// jkrige - fix dynamic light shine through

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_texsort;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
// jkrige - disabled tjunction removal
//extern	cvar_t	gl_keeptjunctions;
//extern	cvar_t	gl_reporttjunctions;
// jkrige - disabled tjunction removal
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;
//extern	cvar_t	gl_fullskybox; // jkrige - skybox
extern	cvar_t	gl_skytype; // jkrige - skyshader


// jkrige - .lit colored lights
extern	int		gl_coloredstatic;
// jkrige - .lit colored lights

extern	int		gl_lightmap_format;
extern	int		gl_solid_format;
extern	int		gl_alpha_format;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;

// jkrige - texture mode
extern	cvar_t	gl_texturemode;
// jkrige - texture mode

// jkrige - .lit colored lights
extern	cvar_t	gl_coloredlight;
//extern	cvar_t	gl_colored_dynamic_lights;
//extern	cvar_t	gl_extra_dynamic_lights;
extern	cvar_t	gl_lightmapfmt;
// jkrige - .lit colored lights

// jkrige - normal mapping
extern	cvar_t	gl_normalmap_generate;
extern	cvar_t	gl_normalmap_render;
// jkrige - normal mapping

// jkrige - luma textures
extern	cvar_t	gl_lumatex_render;
// jkrige - luma textures

extern	int			mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);

byte *playerTranslation;

// jkrige - overbrights
extern cvar_t gl_overbright;
// jkrige - overbrights


/*
 * $Log: /H2 Mission Pack/glquake.h $
 * 
 * 10    3/09/98 11:24p Mgummelt
 * 
 * 9     3/05/98 7:54p Jmonroe
 * fixed startRain, optimized particle struct
 * 
 * 8     1/27/98 10:57a Plipo
 * 
 * 20    9/18/97 2:34p Rlove
 * 
 * 19    9/17/97 1:27p Rlove
 * 
 * 18    9/17/97 11:11a Rlove
 * 
 * 17    7/15/97 4:09p Rjohnson
 * New particle effect
 * 
 * 16    7/03/97 12:22p Rjohnson
 * Fix for reading in the inverse palette
 * 
 * 15    6/16/97 4:20p Rjohnson
 * Added a sky poly count
 * 
 * 14    6/14/97 1:59p Rjohnson
 * Updated the fps display in the gl size, as well as meshing directories
 * 
 * 13    6/12/97 9:02a Rlove
 * New vorpal particle effect
 * 
 * 12    6/02/97 3:42p Gmctaggart
 * GL Catchup
 * 
 * 11    5/30/97 11:42a Rjohnson
 * Added new effect type for the rider's death
 * 
 * 10    5/23/97 3:05p Rjohnson
 * Update to effects / particle types
 * 
 * 9     4/30/97 11:19a Rjohnson
 * Added a particle type from the other version
 * 
 * 8     4/04/97 4:10p Rjohnson
 * Added proper transparent particles for the gl version
 * 
 * 7     3/22/97 5:19p Rjohnson
 * Changed the stone drawing flag to just a generic way based upon the
 * skin number
 * 
 * 6     3/22/97 3:21p Rjohnson
 * Moved the glpic structure to this header
 * 
 * 5     3/13/97 10:53p Rjohnson
 * New function to upload a texture with a specific alpha value
 * 
 * 4     3/07/97 1:11p Rjohnson
 * Added the rocket trail types
 * 
 * 3     3/07/97 1:06p Rjohnson
 * Id Updates
 * 
 * 2     2/20/97 12:13p Rjohnson
 * Code fixes for id update
 */
