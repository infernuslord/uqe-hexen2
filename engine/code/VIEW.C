// view.c -- player eye positioning

/*
 * $Header: /H2 Mission Pack/VIEW.C 4     3/18/98 11:34p Jmonroe $
 */

#include "quakedef.h"
#include "r_local.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t		lcd_x = {"lcd_x","0"};
cvar_t		lcd_yaw = {"lcd_yaw","0"};

cvar_t	scr_ofsx = {"scr_ofsx","0", false};
cvar_t	scr_ofsy = {"scr_ofsy","0", false};
cvar_t	scr_ofsz = {"scr_ofsz","0", false};

cvar_t	cl_rollspeed = {"cl_rollspeed", "200"};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0"};

cvar_t	cl_bob = {"cl_bob","0.02", false};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6", false};
cvar_t	cl_bobup = {"cl_bobup","0.5", false};

cvar_t	v_kicktime = {"v_kicktime", "0.5", false};
cvar_t	v_kickroll = {"v_kickroll", "0.6", false};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6", false};

cvar_t	v_iyaw_cycle = {"v_iyaw_cycle", "2", false};
cvar_t	v_iroll_cycle = {"v_iroll_cycle", "0.5", false};
cvar_t	v_ipitch_cycle = {"v_ipitch_cycle", "1", false};
cvar_t	v_iyaw_level = {"v_iyaw_level", "0.3", false};
cvar_t	v_iroll_level = {"v_iroll_level", "0.1", false};
cvar_t	v_ipitch_level = {"v_ipitch_level", "0.3", false};

cvar_t	v_idlescale = {"v_idlescale", "0", false};

cvar_t	crosshair = {"crosshair", "0", true};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;

extern	int			in_forward, in_forward2, in_back;
extern	cvar_t	sv_idealrollscale;


/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
vec3_t	forward, right, up;

float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;
	
	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fastfabs(side);
	
	value = cl_rollangle.value;
//	if (cl.inwater)
//		value *= 6;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;
	
	return side*sign;
	
}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (void)
{
	float	bob;
	float	cycle;
	
	cycle = cl.time - (int)(cl.time/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

	bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
//Con_Printf ("speed: %5.1f\n", Length(cl.velocity));
	bob = bob*0.3 + bob*0.7*sin(cycle);
	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	return bob;
	
}


//=============================================================================


cvar_t	v_centermove = {"v_centermove", "0.15", false};
cvar_t	v_centerspeed = {"v_centerspeed","500"};

cvar_t	v_centerrollspeed = {"v_centerrollspeed","125"};

void V_StartPitchDrift (void)
{
#if 1
	if (cl.laststop == cl.time)
	{
		return;		// something else is keeping it from drifting
	}
#endif
	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when 
===============
*/
void V_DriftPitch (void)
{
	float		delta, move;

	if (noclip_anglehack || !cl.onground || cls.demoplayback )
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if ( fastfabs(cl.cmd.forwardmove) < (cl.v.hasted*cl_forwardspeed.value)-10)
			cl.driftmove = 0;
		else
			cl.driftmove += host_frametime;
	
		if ( cl.driftmove > v_centermove.value)
		{
			V_StartPitchDrift ();
		}
		return;
	}
	
	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = host_frametime * cl.pitchvel;
	cl.pitchvel += host_frametime * v_centerspeed.value;
	
//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}


/*
===============
V_DriftRoll

Moves the client pitch angle towards cl.idealroll sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

===============
*/
void V_DriftRoll (void)
{
	float		delta, move;

	if (noclip_anglehack || cls.demoplayback )
	{
		return;
	}

	delta = cl.idealroll - cl.viewangles[ROLL];

	if (!delta)
	{
		cl.rollvel = 0;
		return;
	}


	move = host_frametime * cl.rollvel; 
	cl.rollvel += host_frametime * v_centerrollspeed.value;

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.rollvel = 0;
			move = delta;
		}
		cl.viewangles[ROLL] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.rollvel = 0;
			move = -delta;
		}
		cl.viewangles[ROLL] -= move;
	}
}




/*
============================================================================== 
 
						PALETTE FLASHES 
 
============================================================================== 
*/ 
 
 
cshift_t	cshift_empty = { {130,80,50}, 0 };
cshift_t	cshift_water = { {130,80,50}, 128 };
cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

// jkrige - gamma
#ifdef	GLQUAKE
cvar_t		gl_gamma = {"gamma", "2.4", true};
cvar_t		gl_overbrightbits = {"gl_overbrightbits", "0", true};
#else
cvar_t		v_gamma = {"gamma", "1", true};
#endif
// jkrige - gamma

byte		gammatable[256];	// palette is sent through this

#ifdef	GLQUAKE
byte		ramps[3][256];
float		v_blend[4];		// rgba 0.0 - 1.0
#endif	// GLQUAKE

void BuildGammaTable (float g)
{
	int		i, inf;
	
	if (g == 1.0)
	{
		for (i=0 ; i<256 ; i++)
			gammatable[i] = i;
		return;
	}
	
	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		gammatable[i] = inf;
	}
}

/*
=================
V_CheckGamma
=================
*/
qboolean V_CheckGamma (void)
{
	static float oldgammavalue;
	static float oldoverbrightvalue;

	// jkrige - gamma
#ifdef GLQUAKE
	if (gl_gamma.value == oldgammavalue && gl_overbrightbits.value == oldoverbrightvalue)
		return false;
	oldgammavalue = gl_gamma.value;
	oldoverbrightvalue = gl_overbrightbits.value;

	VG_SetColorMappings();
#else
	if (v_gamma.value == oldgammavalue)
		return false;
	oldgammavalue = v_gamma.value;
	
	BuildGammaTable (v_gamma.value);
#endif
	// jkrige - gamma

	vid.recalc_refdef = 1;				// force a surface cache flush

	return true;
}



/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right, up;
	entity_t	*ent;
	float	side;
	float	count;
	
	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)		
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

//
// calculate view angle kicks
//
	ent = &cl_entities[cl.viewentity];
	
	VectorSubtract (from, ent->origin, from);
	VectorNormalize (from);
	
	AngleVectors (ent->angles, forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll = count*side*v_kickroll.value;
	
	side = DotProduct (from, forward);
	v_dmg_pitch = count*side*v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

void V_DarkFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 0;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 0;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 0;
	cl.cshifts[CSHIFT_BONUS].percent = 255;
}

void V_WhiteFlash_f(void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 255;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 255;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 255;
	cl.cshifts[CSHIFT_BONUS].percent = 255;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	switch (contents)
	{
	case CONTENTS_EMPTY:
	case CONTENTS_SOLID:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift(void)
{
/*	if (cl.items & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else if (cl.items & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	}
*/
	if((int)cl.v.artifact_active&ARTFLAG_DIVINE_INTERVENTION)
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = 255;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = 255;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = 255;
		cl.cshifts[CSHIFT_BONUS].percent = 256;
	}

	if((int)cl.v.artifact_active&ARTFLAG_FROZEN)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 20;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 70;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 65;
	}
	else if((int)cl.v.artifact_active&ARTFLAG_STONED)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 205;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 205;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 205;
		//cl.cshifts[CSHIFT_POWERUP].percent = 80;
		cl.cshifts[CSHIFT_POWERUP].percent = 11000;
	}
	else if((int)cl.v.artifact_active&ART_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100;
	}
	else if((int)cl.v.artifact_active&ART_INVINCIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else
	{
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
}

/*
=============
V_CalcBlend
=============
*/
#ifdef	GLQUAKE
void V_CalcBlend (void)
{
	float	r, g, b, a, a2;
	int		j;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

	for (j=0 ; j<NUM_CSHIFTS ; j++)	
	{
		if(cl.cshifts[j].percent > 10000)
		{ // Set percent for grayscale
			cl.cshifts[j].percent = 80;
		}

		a2 = cl.cshifts[j].percent/255.0;
		if (!a2)
			continue;
		a = a + a2*(1-a);
//Con_Printf ("j:%i a:%f\n", j, a);
		a2 = a2/a;
		r = r*(1-a2) + cl.cshifts[j].destcolor[0]*a2;
		g = g*(1-a2) + cl.cshifts[j].destcolor[1]*a2;
		b = b*(1-a2) + cl.cshifts[j].destcolor[2]*a2;
	}

	v_blend[0] = r/255.0;
	v_blend[1] = g/255.0;
	v_blend[2] = b/255.0;
	v_blend[3] = a;
	if (v_blend[3] > 1)
		v_blend[3] = 1;
	if (v_blend[3] < 0)
		v_blend[3] = 0;
}
#endif

/*
=============
V_UpdatePalette
=============
*/
#ifdef	GLQUAKE
void V_UpdatePalette (void)
{
	int		i, j;
	qboolean	new;
	byte	*basepal, *newpal;
	byte	pal[768];
	float	r,g,b,a;
	int		ir, ig, ib;
	qboolean force;

	V_CalcPowerupCshift ();
	
	new = false;
	
	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			new = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				new = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}
	
// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	force = V_CheckGamma ();
	if (!new && !force)
		return;

	V_CalcBlend ();

	a = v_blend[3];
	r = 255*v_blend[0]*a;
	g = 255*v_blend[1]*a;
	b = 255*v_blend[2]*a;

	a = 1-a;
	for (i=0 ; i<256 ; i++)
	{
		ir = i*a + r;
		ig = i*a + g;
		ib = i*a + b;
		if (ir > 255)
			ir = 255;
		if (ig > 255)
			ig = 255;
		if (ib > 255)
			ib = 255;

		ramps[0][i] = gammatable[ir];
		ramps[1][i] = gammatable[ig];
		ramps[2][i] = gammatable[ib];
	}

	basepal = host_basepal;
	newpal = pal;
	
	for (i=0 ; i<256 ; i++)
	{
		ir = basepal[0];
		ig = basepal[1];
		ib = basepal[2];
		basepal += 3;
		
		newpal[0] = ramps[0][ir];
		newpal[1] = ramps[1][ig];
		newpal[2] = ramps[2][ib];
		newpal += 3;
	}

	VID_ShiftPalette (pal);	
}
#else	// !GLQUAKE
void V_UpdatePalette (void)
{
	int		i, j;
	qboolean	new;
	byte	*basepal, *newpal;
	byte	pal[768];
	int		r,g,b;
	qboolean force;

	V_CalcPowerupCshift ();
	
	new = false;
	
	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			new = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				new = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}
	
// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	force = V_CheckGamma ();
	if (!new && !force)
		return;
			
	basepal = host_basepal;
	newpal = pal;
	
	for (i=0 ; i<256 ; i++)
	{
		r = basepal[0];
		g = basepal[1];
		b = basepal[2];
		basepal += 3;
	
		for (j=0 ; j<NUM_CSHIFTS ; j++)	
		{
			if(cl.cshifts[j].percent > 10000)
			{ // Create a grayscale
				r = g = b = (r*76+g*141+b*38)/256;
			}
			else
			{
				r += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[0]-r))>>8;
				g += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[1]-g))>>8;
				b += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[2]-b))>>8;
			}
		}
		
		newpal[0] = gammatable[r];
		newpal[1] = gammatable[g];
		newpal[2] = gammatable[b];
		newpal += 3;
	}

	VID_ShiftPalette (pal);	
}
#endif	// !GLQUAKE


/* 
============================================================================== 
 
						VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{	
	float	yaw, pitch, move;
	static float oldyaw = 0;
	static float oldpitch = 0;
	
	yaw = r_refdef.viewangles[YAW];
	pitch = -r_refdef.viewangles[PITCH];

	yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;
	pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;
	move = host_frametime*20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}
	
	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}
	
	oldyaw = yaw;
	oldpitch = pitch;

	cl.viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
	cl.viewent.angles[PITCH] = - (r_refdef.viewangles[PITCH] + pitch);

	cl.viewent.angles[ROLL] -= v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	cl.viewent.angles[PITCH] -= v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	cl.viewent.angles[YAW] -= v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
	entity_t	*ent;
	
	ent = &cl_entities[cl.viewentity];

// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

	if (r_refdef.vieworg[0] < ent->origin[0] - 14)
		r_refdef.vieworg[0] = ent->origin[0] - 14;
	else if (r_refdef.vieworg[0] > ent->origin[0] + 14)
		r_refdef.vieworg[0] = ent->origin[0] + 14;

	if (r_refdef.vieworg[1] < ent->origin[1] - 14)
		r_refdef.vieworg[1] = ent->origin[1] - 14;
	else if (r_refdef.vieworg[1] > ent->origin[1] + 14)
		r_refdef.vieworg[1] = ent->origin[1] + 14;

	if (r_refdef.vieworg[2] < ent->origin[2] - 0)
		r_refdef.vieworg[2] = ent->origin[2] - 0;
	else if (r_refdef.vieworg[2] > ent->origin[2] + 86)
		r_refdef.vieworg[2] = ent->origin[2] + 86;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float		side;
		
	side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		r_refdef.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
		r_refdef.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
		v_dmg_time -= host_frametime;
	}

	if (cl.v.health <= 0)
	{
		r_refdef.viewangles[ROLL] = 80;	// dead view angle
		return;
	}
}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (void)
{
	entity_t	*ent, *view;
	float		old;

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->origin, r_refdef.vieworg);
	VectorCopy (ent->angles, r_refdef.viewangles);
	view->model = NULL;
	r_refdef.vieworg[2] += cl.viewheight;

// allways idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle ();
	v_idlescale.value = old;
}

/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (void)
{
	entity_t	*ent, *view;
	int			i;
	vec3_t		forward, right, up;
	vec3_t		angles;
	float		bob;
	static float oldz = 0;

	// jkrige - new view weapon bobbing
	float cycle;
	float cl_bob1cycle = 1.6f;
	float cl_bob2cycle = 0.80f;
	float cl_bob1 = 0.0040f;
	float cl_bob2 = 0.0020f;
	float cl_bob1up = 0.0002f;
	float cl_bob2smooth = 0.005f;
	float cl_bobfall = 0.05f;
	float cl_bobfallcycle = 3.0f;
	float cl_bobfallminspeed = 200.0f;
	float cl_bobmodel_speed = 7.0f;
	double xyspeed;
	double bobfall;
	// jkrige - new view weapon bobbing

	if (!cl.v.cameramode)
	{		
		V_DriftPitch ();
		V_DriftRoll ();
	}

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;
	

// transform the view offset by the model's matrix to get the offset from
// model origin for the view
	ent->angles[YAW] = cl.viewangles[YAW];	// the model should face
										// the view dir
	ent->angles[PITCH] = -cl.viewangles[PITCH];	// the model should face
										// the view dir
										
	
	if (cl.v.movetype != MOVETYPE_FLY)
		bob = V_CalcBob ();
	else  // no bobbing when you fly
		bob = 1;

	
// refresh position
	VectorCopy (ent->origin, r_refdef.vieworg);
	r_refdef.vieworg[2] += cl.viewheight + bob;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef.vieworg[0] += 1.0/32;
	r_refdef.vieworg[1] += 1.0/32;
	r_refdef.vieworg[2] += 1.0/32;

	VectorCopy (cl.viewangles, r_refdef.viewangles);
	V_CalcViewRoll ();
	V_AddIdle ();

// offsets
	angles[PITCH] = -ent->angles[PITCH];	// because entity pitches are
											//  actually backward
	angles[YAW] = ent->angles[YAW];
	angles[ROLL] = ent->angles[ROLL];

	AngleVectors (angles, forward, right, up);

	for (i=0 ; i<3 ; i++)
	{
		r_refdef.vieworg[i] += scr_ofsx.value*forward[i] + scr_ofsy.value*right[i] + scr_ofsz.value*up[i];
	}
	
	
	V_BoundOffsets ();
		
// set up gun position
	VectorCopy (cl.viewangles, view->angles);
	
	CalcGunAngle ();

	VectorCopy (ent->origin, view->origin);
	view->origin[2] += cl.viewheight;

	// jkrige - new view weapon bobbing
	xyspeed = bound (0, sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]), 400); // bounded XY speed, used by several effects below

	// vertical view bobbing code
	{
		view->origin[2] += bob;

		// LordHavoc: this code is *weird*, but not replacable (I think it
		// should be done in QC on the server, but oh well, quake is quake)
		// LordHavoc: figured out bobup: the time at which the sin is at 180
		// degrees (which allows lengthening or squishing the peak or valley)
		cycle = cl.time / cl_bob1cycle;
		cycle -= (int) cycle;

		if (cycle < cl_bob1up)
			cycle = sin(M_PI * cycle / cl_bob1up);
		else
			cycle = sin(M_PI + M_PI * (cycle-cl_bob1up)/(1.0 - cl_bob1up));

		// bob is proportional to velocity in the xy plane (don't count Z, or jumping messes it up)
		bob = xyspeed * bound(0, cl_bob1, 0.05);
		bob = bob*0.3 + bob*0.7*cycle;
		view->origin[2] += bob;
	}

	// horizontal view bobbing code
	{
		vec3_t bob2vel;
		float side, front;

		cycle = cl.time / cl_bob2cycle;
		cycle -= (int) cycle;

		if (cycle < 0.5)
			cycle = cos(M_PI * cycle / 0.5); // cos looks better here with the other view bobbing using sin
		else
			cycle = cos(M_PI + M_PI * (cycle-0.5)/0.5);
		bob = bound(0, cl_bob2, 0.05) * cycle;

		// this value slowly decreases from 1 to 0 when we stop touching the ground.
		// The cycle is later multiplied with it so the view smooths back to normal
		if (cl.onground /*&& !cl.cmd.jump*/) // also block the effect while the jump button is pressed, to avoid twitches when bunny-hopping
			cl.bob2_smooth = 1;
		else
		{
			if(cl.bob2_smooth > 0)
				cl.bob2_smooth -= bound(0, cl_bob2smooth, 1);
			else
				cl.bob2_smooth = 0;
		}

		// calculate the front and side of the player between the X and Y axes
		AngleVectors(view->angles, forward, right, up);
		// now get the speed based on those angles. The bounds should match the same value as xyspeed's
		side = bound(-400, DotProduct (cl.velocity, right) * cl.bob2_smooth, 400);
		front = bound(-400, DotProduct (cl.velocity, forward) * cl.bob2_smooth, 400);
		VectorScale(forward, bob, forward);
		VectorScale(right, bob, right);
		// we use side with forward and front with right, so the bobbing goes
		// to the side when we walk forward and to the front when we strafe
		VectorMAMAM(side, forward, front, right, 0, up, bob2vel);
		view->origin[0] += bob2vel[0];
		view->origin[1] += bob2vel[1];
	}

	// fall bobbing code, causes the view to swing down and back up when touching the ground
	{
		if (!cl.onground && (r_viewleaf && r_viewleaf->contents > CONTENTS_WATER)) // && !cl.inwater
		{
			if(cl.bobfall_swing == 0)
			{
				cl.bobfall_speed = bound(-400, cl.velocity[2], 0) * bound(0, cl_bobfall, 0.1);

				if (cl.velocity[2] < -cl_bobfallminspeed)
					cl.bobfall_swing = 1;
				else
					cl.bobfall_swing = 0; // TODO really?
			}
		}
		else
		{
			cl.bobfall_swing = max(0, cl.bobfall_swing - cl_bobfallcycle * (cl.time - cl.oldtime));

			bobfall = sin(M_PI * cl.bobfall_swing) * cl.bobfall_speed;
			r_refdef.vieworg[2] += bobfall * 0.92f;
			view->origin[2] += bobfall;
		}
	}

	// gun model bobbing code
	{
		// calculate for swinging gun model
		// the gun bobs when running on the ground, but doesn't bob when you're in the air.
		// Sajt: I tried to smooth out the transitions between bob and no bob, which works
		// for the most part, but for some reason when you go through a message trigger or
		// pick up an item or anything like that it will momentarily jolt the gun.
		//vec3_t forward, right, up;
		float bspeed;
		float s;
		float t;

		s = cl.time * cl_bobmodel_speed;
		if (cl.onground)
		{
			if (cl.time - cl.hitgroundtime < 0.2)
			{
				// just hit the ground, speed the bob back up over the next 0.2 seconds
				t = cl.time - cl.hitgroundtime;
				t = bound(0, t, 0.2);
				t *= 5;
			}
			else
				t = 1;
		}
		else
		{
			// recently left the ground, slow the bob down over the next 0.2 seconds
			t = cl.time - cl.lastongroundtime;
			t = 0.2 - bound(0, t, 0.2);
			t *= 5;
		}

		bspeed = xyspeed * 0.01f;
		AngleVectors (view->angles, forward, right, up);
		bob = bspeed * cl_bob2 * sin (s) * t;
		VectorMA (view->origin, bob, right, view->origin);
		bob = bspeed * cl_bob1up * cos (s * 2) * t;
		VectorMA (view->origin, bob, up, view->origin);
	}

	/*for (i=0 ; i<3 ; i++)
	{
		// jkrige - view weapon bobbing decreased
		//view->origin[i] += forward[i]*bob*0.4;
		view->origin[i] += forward[i]*bob*0.195;
		// jkrige - view weapon bobbing decreased

//		view->origin[i] += right[i]*bob*0.4;
//		view->origin[i] += up[i]*bob*0.8;
	}
	view->origin[2] += bob;*/
	// jkrige - new view weapon bobbing

	
// fudge position around to keep amount of weapon visible
// roughly equal with different FOV

#if 0
	if (cl.model_precache[cl.stats[STAT_WEAPON]] && strcmp (cl.model_precache[cl.stats[STAT_WEAPON]]->name,  "progs/v_shot2.mdl"))
#endif

	// jkrige - viewsize decreased
	/*if (scr_viewsize.value >= 110) // jkrige - was == 110 (viewmodel)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 100)
		view->origin[2] += 2;
	else if (scr_viewsize.value == 90)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 80)
		view->origin[2] += 0.5;*/
	view->origin[2] += 2.2;
	// jkrige - viewsize decreased

	view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->frame = cl.stats[STAT_WEAPONFRAME];
	if (!view->colorshade)
	{
		view->colormap = vid.colormap;
	}
	else
	{
#ifdef GLQUAKE
		view->colormap = vid.colormap;
#else
		view->colormap = globalcolormap;
#endif
	}

	// Place weapon in powered up mode
	if ((ent->drawflags & MLS_MASKIN) == MLS_POWERMODE)
		view->drawflags = (view->drawflags & MLS_MASKOUT) | MLS_POWERMODE;
	else
		view->drawflags = (view->drawflags & MLS_MASKOUT) | 0;

// set up the refresh position
	VectorAdd (r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);

// smooth out stair step ups
if (cl.onground && ent->origin[2] - oldz > 0)
{
	float steptime;
	
	steptime = cl.time - cl.oldtime;
	if (steptime < 0)
//FIXME		I_Error ("steptime < 0");
		steptime = 0;

	oldz += steptime * 80;

	if (oldz > ent->origin[2])
		oldz = ent->origin[2];
	if (ent->origin[2] - oldz > 12)
		oldz = ent->origin[2] - 12;
	r_refdef.vieworg[2] += oldz - ent->origin[2];
	view->origin[2] += oldz - ent->origin[2];
}
else
	oldz = ent->origin[2];

	// jkrige - removed chase
	//if (chase_active.value)
		//Chase_Update ();
	// jkrige - removed chase
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
extern vrect_t	scr_vrect;

void V_RenderView (void)
{
	if (con_forcedup)
		return;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_Set ("scr_ofsx", "0");
		Cvar_Set ("scr_ofsy", "0");
		Cvar_Set ("scr_ofsz", "0");
	}

	if (cl.intermission)
	{	// intermission / finale rendering
		V_CalcIntermissionRefdef ();	
	}
	else
	{
		if (!cl.paused /* && (sv.maxclients > 1 || key_dest == key_game) */ )
			V_CalcRefdef ();
	}

	R_PushDlights ();

	if (lcd_x.value)
	{
		//
		// render two interleaved views
		//
		int		i;

		vid.rowbytes <<= 1;
		vid.aspect *= 0.5;

		r_refdef.viewangles[YAW] -= lcd_yaw.value;
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] -= right[i]*lcd_x.value;
		R_RenderView ();

		vid.buffer += vid.rowbytes>>1;

		R_PushDlights ();

		r_refdef.viewangles[YAW] += lcd_yaw.value*2;
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] += 2*right[i]*lcd_x.value;
		R_RenderView ();

		vid.buffer -= vid.rowbytes>>1;

		r_refdef.vrect.height <<= 1;

		vid.rowbytes >>= 1;
		vid.aspect *= 2;
	}
	else
	{
		R_RenderView ();
	}

#ifndef GLQUAKE
	if (crosshair.value)
		Draw_Character (scr_vrect.x + scr_vrect.width/2, scr_vrect.y + scr_vrect.height/2, '+');
#endif
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f);	
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("df", V_DarkFlash_f);
	Cmd_AddCommand ("wf", V_WhiteFlash_f);

	Cmd_AddCommand ("centerview", V_StartPitchDrift);

	Cvar_RegisterVariable (&lcd_x);
	Cvar_RegisterVariable (&lcd_yaw);

	Cvar_RegisterVariable (&v_centermove);
	Cvar_RegisterVariable (&v_centerspeed);
	Cvar_RegisterVariable (&v_centerrollspeed);

	Cvar_RegisterVariable (&v_iyaw_cycle);
	Cvar_RegisterVariable (&v_iroll_cycle);
	Cvar_RegisterVariable (&v_ipitch_cycle);
	Cvar_RegisterVariable (&v_iyaw_level);
	Cvar_RegisterVariable (&v_iroll_level);
	Cvar_RegisterVariable (&v_ipitch_level);

	Cvar_RegisterVariable (&v_idlescale);
	Cvar_RegisterVariable (&crosshair);

	Cvar_RegisterVariable (&scr_ofsx);
	Cvar_RegisterVariable (&scr_ofsy);
	Cvar_RegisterVariable (&scr_ofsz);
	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);	
	
	BuildGammaTable (1.0);	// no gamma yet
	// jkrige - gamma
#ifdef GLQUAKE
	Cvar_RegisterVariable (&gl_gamma);
	Cvar_RegisterVariable (&gl_overbrightbits);
#else
	Cvar_RegisterVariable (&v_gamma);
#endif
	// jkrige - gamma
}


/*
 * $Log: /H2 Mission Pack/VIEW.C $
 * 
 * 4     3/18/98 11:34p Jmonroe
 * fixed gl renderheight in intermission, fixed bottom plaque draw, added
 * credit cd track
 * 
 * 3     2/27/98 11:53p Jweier
 * 
 * 2     2/04/98 12:53a Jmonroe
 * added fastfabs
 * 
 * 21    10/03/97 4:07p Rjohnson
 * Fix for look spring
 * 
 * 20    9/02/97 7:47p Rjohnson
 * Update
 * 
 * 19    9/01/97 6:24p Rlove
 * 
 * 18    8/14/97 4:28p Rjohnson
 * added Dark Flash
 * 
 * 17    8/11/97 6:47p Rjohnson
 * Fix for color tinting of view model
 * 
 * 16    8/11/97 2:52p Rlove
 * 
 * 15    8/09/97 5:09a Bgokey
 * 
 * 14    8/09/97 4:07a Bgokey
 * 
 * 13    8/09/97 3:39a Bgokey
 * 
 * 12    8/09/97 3:34a Bgokey
 * 
 * 11    8/09/97 2:15a Bgokey
 * 
 * 10    5/01/97 5:06p Rlove
 * New camera
 * 
 * 9     4/25/97 6:59p Rjohnson
 * Expanded the view limits by a small amount
 * 
 * 8     4/25/97 5:54p Rjohnson
 * Adjustment for id's view offset and our models
 * 
 * 7     4/04/97 5:40p Rlove
 * 
 * 6     4/04/97 3:07p Rjohnson
 * Networking updates and corrections
 * 
 * 5     3/07/97 2:34p Rjohnson
 * Id Updates
 * 
 * 4     2/18/97 5:07p Rjohnson
 * Id Updates
 */
