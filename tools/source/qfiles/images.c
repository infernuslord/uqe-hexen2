


/*
==============
Cmd_Mip

$mip filename x y width height <OPTIONS>
must be multiples of sixteen
SURF_WINDOW
==============
*/
void Cmd_Mip (void)
{
	int             x,y,xl,yl,xh,yh,w,h;
	byte            *screen_p, *source;
	int             linedelta;
	miptex_t		*qtex;
	int				miplevel, mipstep;
	int				xx, yy, pix;
	int				count;
	int				flags, value, contents;
	mipparm_t		*mp;
	char			lumpname[64];
	byte			*lump_p;
	char			filename[1024];
	char			animname[64];

	GetToken (false);
	strcpy (lumpname, token);
	
	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);

	if ( (w & 15) || (h & 15) )
		Error ("line %i: miptex sizes must be multiples of 16", scriptline);

	flags = 0;
	contents = 0;
	value = 0;

	animname[0] = 0;

	// get optional flags and values
	while (TokenAvailable ())
	{
		GetToken (false);
	
		for (mp=mipparms ; mp->name ; mp++)
		{
			if (!strcmp(mp->name, token))
			{
				switch (mp->type)
				{
				case pt_animvalue:
					GetToken (false);	// specify the next animation frame
					strcpy (animname, token);
					break;
				case pt_flags:
					flags |= mp->flags;
					break;
				case pt_contents:
					contents |= mp->flags;
					break;
				case pt_flagvalue:
					flags |= mp->flags;
					GetToken (false);	// specify the light value
					value = atoi(token);
					break;
				}
				break;
			}
		}
		if (!mp->name)
			Error ("line %i: unknown parm %s", scriptline, token);
	}

	sprintf (filename, "%stextures/%s/%s.wal", gamedir, mip_prefix, lumpname);
	if (g_release)
		return;	// textures are only released by $maps

	xh = xl+w;
	yh = yl+h;

	qtex = malloc (sizeof(miptex_t) + w*h*2);
	memset (qtex, 0, sizeof(miptex_t));

	qtex->width = LittleLong(w);
	qtex->height = LittleLong(h);
	qtex->flags = LittleLong(flags);
	qtex->contents = LittleLong(contents);
	qtex->value = LittleLong(value);
	sprintf (qtex->name, "%s/%s", mip_prefix, lumpname);
	if (animname[0])
		sprintf (qtex->animname, "%s/%s", mip_prefix, animname);
	
	lump_p = (byte *)(&qtex->value+1);
	
	screen_p = byteimage + yl*byteimagewidth + xl;
	linedelta = byteimagewidth - w;

	source = lump_p;
	qtex->offsets[0] = LittleLong(lump_p - (byte *)qtex);

	for (y=yl ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
		{
			pix = *screen_p++;
			if (pix == 255)
				pix = 1;		// should never happen
			*lump_p++ = pix;
		}
		screen_p += linedelta;
	}
	
//
// subsample for greater mip levels
//
	d_red = d_green = d_blue = 0;	// no distortion yet

	for (miplevel = 1 ; miplevel<4 ; miplevel++)
	{
		qtex->offsets[miplevel] = LittleLong(lump_p - (byte *)qtex);
		
		mipstep = 1<<miplevel;
		for (y=0 ; y<h ; y+=mipstep)
		{

			for (x = 0 ; x<w ; x+= mipstep)
			{
				count = 0;
				for (yy=0 ; yy<mipstep ; yy++)
					for (xx=0 ; xx<mipstep ; xx++)
					{
						pixdata[count] = source[ (y+yy)*w + x + xx ];
						count++;
					}
				*lump_p++ = AveragePixels (count);
			}	
		}
	}

//
// dword align the size
//
	while ((int)lump_p&3)
		*lump_p++ = 0;

//
// write it out
//
	printf ("writing %s\n", filename);
	SaveFile (filename, (byte *)qtex, lump_p - (byte *)qtex);

	free (qtex);
}