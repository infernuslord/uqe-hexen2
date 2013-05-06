#include "qdata.h"

//qboolean	g_compress_pak;
qboolean	g_release;			// don't grab, copy output data to new tree
qboolean	g_pak;				// if true, copy to pak instead of release
char		g_releasedir[1024];	// c:\quake2\baseq2, etc
qboolean	g_archive;			// don't grab, copy source data to new tree
//qboolean	do3ds;
char		g_only[256];		// if set, only grab this cd
qboolean	g_skipmodel;		// set true when a cd is not g_only

//char		*ext_3ds = "3ds";
//char		*ext_tri= "tri";
//char		*trifileext;

/*
=======================================================

  PAK FILES

=======================================================
*/

unsigned Com_BlockChecksum (void *buffer, int length);

typedef struct
{
	char	name[56];
	int		filepos, filelen;
} packfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} packheader_t;

packfile_t		pfiles[16384];
FILE			*pakfile;
packfile_t		*pf;
packheader_t	pakheader;



/*
==============
BeginPak
==============
*/
void BeginPak (char *outname)
{
	if (!g_pak)
		return;

	pakfile = SafeOpenWrite (outname);

	// leave space for header
	SafeWrite (pakfile, &pakheader, sizeof(pakheader));
	
	pf = pfiles;
}


/*
==============
ReleaseFile

Filename should be gamedir reletive.
Either copies the file to the release dir, or adds it to
the pak file.
==============
*/
void ReleaseFile (char *filename)
{
	int		len;
	byte	*buf;
	char	source[1024];
	char	dest[1024];

	if (!g_release)
		return;

	sprintf (source, "%s%s", gamedir, filename);

	if (!g_pak)
	{	// copy it
		sprintf (dest, "%s/%s", g_releasedir, filename);
		logprint ("copying to %s\n", dest);
		QCopyFile (source, dest);
		return;
	}

	// pak it
	logprint ("paking %s\n", filename);
	if (strlen(filename) >= sizeof(pf->name))
		logerror ("Filename too long for pak: %s", filename); // jkrige - was Error()

	len = LoadFile (source, (void **)&buf);

	// jkrige - huffman (video.c)
	/*if (g_compress_pak && len < 4096*1024 )
	{
		cblock_t	in, out;
		cblock_t Huffman (cblock_t in);

		in.count = len;
		in.data = buf;

		out = Huffman (in);

		if (out.count < in.count)
		{
			printf ("   compressed from %i to %i\n", in.count, out.count);
			free (in.data);
			buf = out.data;
			len = out.count;
		}
		else
			free (out.data);
	}*/
	// jkrige - huffman (video.c)

	strcpy (pf->name, filename);
	pf->filepos = LittleLong(ftell(pakfile));
	pf->filelen = LittleLong(len);
	pf++;

	SafeWrite (pakfile, buf, len);

	free (buf);
}


/*
==============
FinishPak
==============
*/
void FinishPak (void)
{
	int		dirlen;
	int		d;
	int		i;
	unsigned	checksum;

	if (!g_pak)
		return;

	pakheader.id[0] = 'P';
	pakheader.id[1] = 'A';
	pakheader.id[2] = 'C';
	pakheader.id[3] = 'K';
	dirlen = (byte *)pf - (byte *)pfiles;
	pakheader.dirofs = LittleLong(ftell(pakfile));
	pakheader.dirlen = LittleLong(dirlen);
	
	checksum = Com_BlockChecksum ( (void *)pfiles, dirlen );

	SafeWrite (pakfile, pfiles, dirlen);

	i = ftell (pakfile);
	 
	fseek (pakfile, 0, SEEK_SET);
	SafeWrite (pakfile, &pakheader, sizeof(pakheader));
	fclose (pakfile);	
	
	d = pf - pfiles;
	logprint ("%i files packed in %i bytes\n",d, i);
	logprint ("checksum: 0x%x\n", checksum);
}


/*
===============
Cmd_File

This is only used to cause a file to be copied during a release
build (default.cfg, maps, etc)
===============
*/
void Cmd_File (void)
{
	GetToken (false);
	ReleaseFile (token);
}

/*
===============
PackDirectory_r

===============
*/
#ifdef _WIN32
#include "io.h"
void PackDirectory_r (char *dir)
{
	struct _finddata_t fileinfo;
	int		handle;
	char	dirstring[1024];
	char	filename[1024];

	sprintf (dirstring, "%s%s/*.*", gamedir, dir);

	handle = _findfirst (dirstring, &fileinfo);
	if (handle == -1)
		return;

	do
	{
		sprintf (filename, "%s/%s", dir, fileinfo.name);
		if (fileinfo.attrib & _A_SUBDIR)
		{	// directory
			if (fileinfo.name[0] != '.')	// don't pak . and ..
				PackDirectory_r (filename);
			continue;
		}
		// copy or pack the file
		ReleaseFile (filename);		
	} while (_findnext( handle, &fileinfo ) != -1);

	_findclose (handle);
}
#else

#include <sys/types.h>
#ifdef NeXT
#include <sys/dir.h>
#else
#include <sys/dirent.h>
#endif

void PackDirectory_r (char *dir)
{
#ifdef NeXT
	struct direct **namelist, *ent;
#else
	struct dirent **namelist, *ent;
#endif
	int		count;
	struct stat st;
	int			i;
	int			len;
	char		fullname[1024];
	char		dirstring[1024];
	char		*name;
	
	sprintf (dirstring, "%s%s", gamedir, dir);
	count = scandir(dirstring, &namelist, NULL, NULL);
	
	for (i=0 ; i<count ; i++)
	{
		ent = namelist[i];	
		name = ent->d_name;

		if (name[0] == '.')
			continue;
	
		sprintf (fullname, "%s/%s", dir, name);
		sprintf (dirstring, "%s%s/%s", gamedir, dir, name);
		
		if (stat (dirstring, &st) == -1)
			logerror ("fstating %s", pf->name); // jkrige - was Error()
		if (st.st_mode & S_IFDIR)
		{	// directory
			PackDirectory_r (fullname);
			continue;
		}

		// copy or pack the file
		ReleaseFile (fullname);		
	}
}
#endif


/*
===============
Cmd_Dir

This is only used to cause a directory to be copied during a
release build (sounds, etc)
===============
*/
void Cmd_Dir (void)
{
	GetToken (false);
	PackDirectory_r (token);	
}

//========================================================================

#define	MAX_RTEX	16384
int		numrtex;
char	rtex[MAX_RTEX][64];

void ReleaseTexture (char *name)
{
	int		i;
	char	path[1024];

	for (i=0 ; i<numrtex ; i++)
		if (!Q_strcasecmp(name, rtex[i]))
			return;

	if (numrtex == MAX_RTEX)
		logerror ("numrtex == MAX_RTEX"); // jkrige - was Error()

	strcpy (rtex[i], name);
	numrtex++;

	sprintf (path, "textures/%s.wal", name);
	ReleaseFile (path);
}

/*
===============
Cmd_Maps

Only relevent for release and pak files.
Releases the .bsp files for the maps, and scans all of the files to
build a list of all textures used, which are then released.
===============
*/
/*void Cmd_Maps (void)
{
	char	map[1024];
	int		i;

	while (TokenAvailable ())
	{
		GetToken (false);
		sprintf (map, "maps/%s.bsp", token);
		ReleaseFile (map);

		if (!g_release)
			continue;

		// get all the texture references
		sprintf (map, "%smaps/%s.bsp", gamedir, token);
		LoadBSPFileTexinfo (map);
		for (i=0 ; i<numtexinfo ; i++)
			ReleaseTexture (texinfo[i].texture);
	}
}*/


//==============================================================

/*
===============
ParseScript
===============
*/
void ParseScript (void)
{
	while (1)
	{
		do
		{	// look for a line starting with a $ command
			GetToken (true);
			if (endofscript)
				return;
			if (token[0] == '$')
				break;				
			while (TokenAvailable())
				GetToken (false);
		} while (1);
	
		//
		// model commands
		//
		//if (!strcmp (token, "$modelname"))
		//	Cmd_Modelname ();
		//else if (!strcmp (token, "$base"))
		//	Cmd_Base ();
		//else if (!strcmp (token, "$cd"))
		//	Cmd_Cd ();
		//else if (!strcmp (token, "$origin"))
		//	Cmd_Origin ();
		//else if (!strcmp (token, "$scale"))
		//	Cmd_ScaleUp ();
		//else if (!strcmp (token, "$frame"))
		//	Cmd_Frame ();
		//else if (!strcmp (token, "$skin"))
		//	Cmd_Skin ();
		//else if (!strcmp (token, "$skinsize"))
		//	Cmd_Skinsize ();
		//
		// sprite commands
		//
		//else if (!strcmp (token, "$spritename"))
		//	Cmd_SpriteName ();

		if (!strcmp (token, "$load")) // jkrige - changed "if" starting position
			Cmd_Load ();
		//else if (!strcmp (token, "$spriteframe"))
		//	Cmd_SpriteFrame ();
		//
		// image commands
		//
		else if (!strcmp (token, "$grab"))
			Cmd_Grab ();
		else if (!strcmp (token, "$raw"))
			Cmd_Raw ();
		else if (!strcmp (token, "$colormap"))
			Cmd_Colormap ();
		else if (!strcmp (token, "$mippal"))
			Cmd_Mippal ();
		else if (!strcmp (token, "$mipdir"))
			Cmd_Mipdir ();
		else if (!strcmp (token, "$mip"))
			Cmd_Mip ();
		else if (!strcmp (token, "$environment"))
			Cmd_Environment ();
		//
		// video
		//
		//else if (!strcmp (token, "$video"))
		//	Cmd_Video ();
		//
		// misc
		//
		else if (!strcmp (token, "$file"))
			Cmd_File ();
		else if (!strcmp (token, "$dir"))
			Cmd_Dir ();
		//else if (!strcmp (token, "$maps"))
		//	Cmd_Maps ();
		//else if (!strcmp (token, "$alphalight"))
		//	Cmd_Alphalight ();
		//else if (!strcmp (token, "$inverse16table" ))
		//	Cmd_Inverse16Table();
		else
			logerror ("bad command %s\n", token); // jkrige - was Error()
	}
}

//=======================================================

/*
==============
main
==============
*/
int main (int argc, char **argv)
{
	static	int		i;		// VC4.2 compiler bug if auto...
	char	path[1024];

	// jkrige
	SetConsoleTitle("UQE Hexen II WAL");

	init_log("uqehx2wal.log");
	logprint("UQEHX2WAL 1.16 -- based on QDATA by John Carmack\n");
    logprint("================================================\n");
	logprint("Modified by Jacques Krige\n");
	logprint("Build: %s, %s\n", __TIME__, __DATE__);
    logprint("http://www.corvinstein.com\n");
	logprint("================================================\n\n");
	// jkrige

	ExpandWildcards (&argc, &argv);

	for (i=1 ; i<argc ; i++)
	{
		if (!strcmp(argv[i], "-archive"))
		{
			// -archive f:/quake2/release/dump_11_30
			archive = true;
			strcpy (archivedir, argv[i+1]);
			logprint ("Archiving source to: %s\n", archivedir);
			i++;
		}
		else if (!strcmp(argv[i], "-release"))
		{
			g_release = true;
			strcpy (g_releasedir, argv[i+1]);
			logprint ("Copy output to: %s\n", g_releasedir);
			i++;
		}
		//else if (!strcmp(argv[i], "-compress")) // jkrige
		//{
		//	g_compress_pak = true;
		//	printf ("Compressing pakfile\n");
		//}
		else if (!strcmp(argv[i], "-pak"))
		{
			g_release = true;
			g_pak = true;
			logprint ("Building pakfile: %s\n", argv[i+1]);
			BeginPak (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i], "-only"))
		{
			strcpy (g_only, argv[i+1]);
			logprint ("Only grabbing %s\n", g_only);
			i++;
		}
		//else if (!strcmp(argv[i], "-3ds")) // jkrige
		//{
		//	do3ds = true;
		//	printf ("loading .3ds files\n");
		//}
		else if (argv[i][0] == '-')
			logerror ("uqehx2wal: Unknown option \"%s\"", argv[i]);
		else
			break;
	}

	// jkrige
	//if (i >= argc)
	//	Error ("usage: qgrab [-archive <directory>] [-release <directory>] [-only <model>] [-3ds] file.qgr");
	if (i >= argc)
		logerror ("usage: uqehx2wal [-archive <directory>] [-release <directory>] [-only <model>] file.hgr"); // jkrige - was Error()
	// jkrige

	//if (do3ds)
	//	trifileext = ext_3ds;
	//else
	//	trifileext = ext_tri;

	for ( ; i<argc ; i++)
	{
		logprint ("--------------- %s ---------------\n", argv[i]);
		// load the script
		strcpy (path, argv[i]);
		DefaultExtension (path, ".hdt"); // jkrige - was .qdt
		SetQdirFromPath (path);
		LoadScriptFile (ExpandArg(path));
		
		//
		// parse it
		//
		ParseScript ();

		// write out the last model
		//FinishModel ();
		//FinishSprite ();
	}

	if (g_pak)
		FinishPak ();

	return 0;
}

