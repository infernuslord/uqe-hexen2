
//**************************************************************************
//**
//** hcc.c
//**
//** $Header: /H2 Mission Pack/Tools/hcc/hcc.C 9     3/24/98 3:38p Jmonroe $
//**
//** Hash table modifications based on fastqcc by Jonathan Roy
//** (roy@atlantic.net).
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "hcc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void BeginCompilation(void);
static qboolean FinishCompilation(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int hcc_OptimizeImmediates;
int hcc_OptimizeNameTable;
qboolean hcc_WarningsActive;
qboolean hcc_ShowUnrefFuncs;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

char		sourcedir[1024];
char		destfile[1024];

float		pr_globals[MAX_REGS];
int			numpr_globals;

char		strings[MAX_STRINGS];
int			strofs;

dstatement_t	statements[MAX_STATEMENTS];
int			numstatements;
int			statement_linenums[MAX_STATEMENTS];

dfunction_t	functions[MAX_FUNCTIONS];
int			numfunctions;

ddef_t		globals[MAX_GLOBALS];
int			numglobaldefs;

ddef_t		fields[MAX_FIELDS];
int			numfielddefs;

char		precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
int			precache_sounds_block[MAX_SOUNDS];
int			numsounds;

char		precache_models[MAX_MODELS][MAX_DATA_PATH];
int			precache_models_block[MAX_SOUNDS];
int			nummodels;

char		precache_files[MAX_FILES][MAX_DATA_PATH];
int			precache_files_block[MAX_SOUNDS];
int			numfiles;

// CODE --------------------------------------------------------------------

/*
============
WriteFiles

  Generates files.dat, which contains all of the
  data files actually used by the game, to be
  processed by qfiles.exe
============
*/
void WriteFiles (void)
{
	FILE	*f;
	int		i;
	char	filename[1024];

	sprintf (filename, "%sfiles.dat", sourcedir);
	f = fopen (filename, "w");
	if (!f)
		logerror ("Couldn't open %s", filename); // jkrige - was MS_Error()

	fprintf (f, "%i\n", numsounds);
	for (i=0 ; i<numsounds ; i++)
		fprintf (f, "%i %s\n", precache_sounds_block[i],
			precache_sounds[i]);

	fprintf (f, "%i\n", nummodels);
	for (i=0 ; i<nummodels ; i++)
		fprintf (f, "%i %s\n", precache_models_block[i],
			precache_models[i]);

	fprintf (f, "%i\n", numfiles);
	for (i=0 ; i<numfiles ; i++)
		fprintf (f, "%i %s\n", precache_files_block[i],
			precache_files[i]);

	fclose (f);
}


// CopyString returns an offset from the string heap
int	CopyString (char *str)
{
	int		old;

	if (!strcmpi(str,"gfx/palette.lmp"))
	{
		old=0;
	}

	old = strofs;
	strcpy (strings+strofs, str);
	strofs += strlen(str)+1;
	return old;
}

void PrintStrings (void)
{
	int		i, l, j;
	
	for (i=0 ; i<strofs ; i += l)
	{
		l = strlen(strings+i) + 1;
		logprint ("%5i : ",i);
		for (j=0 ; j<l ; j++)
		{
			if (strings[i+j] == '\n')
			{
				putchar ('\\');
				putchar ('n');
			}
			else
				putchar (strings[i+j]);
		}
		logprint ("\n");
	}
}

void PrintFunctions (void)
{
	int		i,j;
	dfunction_t	*d;
	
	for (i=0 ; i<numfunctions ; i++)
	{
		d = &functions[i];
		logprint ("%s : %s : %i %i (", strings + d->s_file, strings + d->s_name, d->first_statement, d->parm_start);
		for (j=0 ; j<d->numparms ; j++)
			logprint ("%i ",d->parm_size[j]);
		logprint (")\n");
	}
}

void PrintFields (void)
{
	int		i;
	ddef_t	*d;
	
	for (i=0 ; i<numfielddefs ; i++)
	{
		d = &fields[i];
		logprint ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}

void PrintGlobals (void)
{
	int		i;
	ddef_t	*d;
	
	for (i=0 ; i<numglobaldefs ; i++)
	{
		d = &globals[i];
		logprint ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}


void InitData (void)
{
	int		i;
	
	numstatements = 1;
	strofs = 1;
	numfunctions = 1;
	numglobaldefs = 1;
	numfielddefs = 1;
	
	def_ret.ofs = OFS_RETURN;
	for (i=0 ; i<MAX_PARMS ; i++)
		def_parms[i].ofs = OFS_PARM0 + 3*i;
}

//==========================================================================
//
// WriteData
//
//==========================================================================

void WriteData(int crc)
{
	def_t *def;
	ddef_t *dd;
	dprograms_t progs;
	FILE *h;

	int localName;

	if(hcc_OptimizeNameTable)
	{
		localName = CopyString("LCL+");
	}
	for(def = pr.def_head.next; def; def = def->next)
	{
		if(def->type->type == ev_field)
		{
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = strofs; // The name gets copied below
			dd->ofs = G_INT(def->ofs);
		}
		dd = &globals[numglobaldefs];
		numglobaldefs++;
		dd->type = def->type->type;
		if(!def->initialized
			&& def->type->type != ev_function
			&& def->type->type != ev_field
			&& def->scope == NULL)
		{
			if (strncmp (def->name,"STR_",4)!=0)	//str_ is a special case string constant
				dd->type |= DEF_SAVEGLOBGAL;
		}

		if(hcc_OptimizeNameTable && ((def->scope != NULL) ||
			(!(dd->type&DEF_SAVEGLOBGAL)&&(def->type->type < ev_field))))
		{
			dd->s_name = localName;
		}
		else
		{
			dd->s_name = CopyString(def->name);
		}
		dd->ofs = def->ofs;
	}

//PrintStrings ();
//PrintFunctions ();
//PrintFields ();
//PrintGlobals ();

strofs = (strofs+3)&~3;

	logprint("object file %s\n", destfile);
	logprint("      registers: %-6d / %-6d (%6d)\n", numpr_globals,
		MAX_REGS, numpr_globals*sizeof(float));
	logprint("     statements: %-10d / %-10d (%10)\n", numstatements,
		MAX_STATEMENTS, numstatements*sizeof(dstatement_t));
	logprint("      functions: %-6d / %-6d (%6d)\n", numfunctions,
		MAX_FUNCTIONS, numfunctions*sizeof(dfunction_t));
	logprint("    global defs: %-6d / %-6d (%6d)\n", numglobaldefs,
		MAX_GLOBALS, numglobaldefs*sizeof(ddef_t));
	logprint("     field defs: %-6d / %-6d (%6d)\n", numfielddefs,
		MAX_FIELDS, numfielddefs*sizeof(ddef_t));
	logprint("    string heap: %-6d / %-6d\n", strofs, MAX_STRINGS);
	logprint("  entity fields: %d\n", pr.size_fields);

	h = MS_SafeOpenWrite (destfile);
	MS_SafeWrite (h, &progs, sizeof(progs));

	progs.ofs_strings = ftell (h);
	progs.numstrings = strofs;
	MS_SafeWrite (h, strings, strofs);

	progs.ofs_statements = ftell (h);
	progs.numstatements = numstatements;
	MS_SafeWrite (h, statements, numstatements*sizeof(dstatement_t));

	progs.ofs_functions = ftell (h);
	progs.numfunctions = numfunctions;
	MS_SafeWrite (h, functions, numfunctions*sizeof(dfunction_t));

	progs.ofs_globaldefs = ftell (h);
	progs.numglobaldefs = numglobaldefs;
	MS_SafeWrite (h, globals, numglobaldefs*sizeof(ddef_t));

	progs.ofs_fielddefs = ftell (h);
	progs.numfielddefs = numfielddefs;
	MS_SafeWrite (h, fields, numfielddefs*sizeof(ddef_t));

	progs.ofs_globals = ftell (h);
	progs.numglobals = numpr_globals;
	MS_SafeWrite (h, pr_globals, numpr_globals*4);

	logprint("     total size: %d\n", (int)ftell(h));

	progs.entityfields = pr.size_fields;
	progs.version = PROG_VERSION;
	progs.crc = crc;

	fseek(h, 0, SEEK_SET);
	MS_SafeWrite(h, &progs, sizeof(progs));
	fclose(h);
}

/*
===============
PR_String

Returns a string suitable for printing (no newlines, max 60 chars length)
===============
*/
char *PR_String (char *string)
{
	static char buf[80];
	char	*s;
	
	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (s == buf + sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
		}
		else
			*s++ = *string;
		string++;
		if (s - buf > 60)
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}



def_t	*PR_DefForFieldOfs (gofs_t ofs)
{
	def_t	*d;
	
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (d->type->type != ev_field)
			continue;
		if (*((int *)&pr_globals[d->ofs]) == ofs)
			return d;
	}
	logerror ("PR_DefForFieldOfs: couldn't find %i",ofs); // jkrige - was MS_Error()
	return NULL;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (etype_t type, void *val)
{
	static char	line[256];
	def_t		*def;
	dfunction_t	*f;
	
	switch (type)
	{
	case ev_string:
		sprintf (line, "%s", PR_String(strings + *(int *)val));
		break;
	case ev_entity:	
		sprintf (line, "entity %i", *(int *)val);
		break;
	case ev_function:
		f = functions + *(int *)val;
		if (!f)
			sprintf (line, "undefined function");
		else
			sprintf (line, "%s()", strings + f->s_name);
		break;
	case ev_field:
		def = PR_DefForFieldOfs ( *(int *)val );
		sprintf (line, ".%s", def->name);
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		sprintf (line, "%5.1f", *(float *)val);
		break;
	case ev_vector:
		sprintf (line, "'%5.1f %5.1f %5.1f'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PR_GlobalStringNoContents (gofs_t ofs)
{
	int		i;
	def_t	*def;
	void	*val;
	static char	line[128];
	
	val = (void *)&pr_globals[ofs];
	def = pr_global_defs[ofs];
	if (!def)
//		MS_Error ("PR_GlobalString: no def for %i", ofs);
		sprintf (line,"%i(?)", ofs);
	else
		sprintf (line,"%i(%s)", ofs, def->name);
	
	i = strlen(line);
	for ( ; i<16 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}

char *PR_GlobalString (gofs_t ofs)
{
	char	*s;
	int		i;
	def_t	*def;
	void	*val;
	static char	line[128];
	
	val = (void *)&pr_globals[ofs];
	def = pr_global_defs[ofs];
	if (!def)
		return PR_GlobalStringNoContents(ofs);
	if (def->initialized && def->type->type != ev_function)
	{
		s = PR_ValueString (def->type->type, &pr_globals[ofs]);
		sprintf (line,"%i(%s)", ofs, s);
	}
	else
		sprintf (line,"%i(%s)", ofs, def->name);
	
	i = strlen(line);
	for ( ; i<16 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}

/*
============
PR_PrintOfs
============
*/
void PR_PrintOfs (gofs_t ofs)
{
	logprint ("%s\n",PR_GlobalString(ofs));
}

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement (dstatement_t *s)
{
	int		i;

	logprint ("%4i : %4i : %s ", (int)(s - statements),
		statement_linenums[s-statements], pr_opcodes[s->op].opname);
	i = strlen(pr_opcodes[s->op].opname);
	for ( ; i<10 ; i++)
		logprint (" ");
		
	if (s->op == OP_IF || s->op == OP_IFNOT)
		logprint ("%sbranch %i",PR_GlobalString(s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		logprint ("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		logprint ("%s",PR_GlobalString(s->a));
		logprint ("%s", PR_GlobalStringNoContents(s->b));
	}
	else if ( (unsigned)(s->op - OP_SWITCH_F) < 5)
	{
		logprint ("%sbranch %i",PR_GlobalString(s->a),s->b);
	}
	else if (s->op == OP_CASE)
	{
		logprint ("of %i branch %i",s->a, s->b);
	}
	else
	{
		if (s->a)
			logprint ("%s",PR_GlobalString(s->a));
		if (s->b)
			logprint ("%s",PR_GlobalString(s->b));
		if (s->c)
			logprint ("%s", PR_GlobalStringNoContents(s->c));
	}
	logprint ("\n");
}

//==========================================================================
//
// BeginCompilation
//
// Called before compiling a batch of files, clears the pr struct.
//
//==========================================================================

static void BeginCompilation(void)
{
	int i;

	numpr_globals = RESERVED_OFS;
	pr.def_tail = &pr.def_head;
	for(i = 0; i < RESERVED_OFS; i++)
	{
		pr_global_defs[i] = &def_void;
	}

	// Link the function type in so state forward declarations match
	// proper type.
	pr.types = &type_function;
	type_function.next = NULL;
	pr_error_count = 0;
}

//==========================================================================
//
// FinishCompilation
//
// Called after all files are compiled to check for errors.  Returns
// false if errors were detected.
//
//==========================================================================

static qboolean FinishCompilation(void)
{
	def_t *d;
	qboolean errors;
	qboolean globals_done = false;

	errors = false;
	for(d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_globals"))
			globals_done = true;
			
		if(d->type->type == ev_function && !d->scope)
		{
			if(!d->initialized)
			{ // Prototype, but no code
				logprint("function '%s' was not defined\n", d->name);
				errors = true;
			}
			if(hcc_ShowUnrefFuncs && (d->referenceCount == 0) && globals_done)
			{ // Function never used
				logprint("unreferenced function '%s'\n", d->name);
			}
		}
	}
	return !errors;
}

/*
============
PR_WriteProgdefs

Writes the global and entity structures out
Returns a crc of the header, to be stored in the progs file for comparison
at load time.
============
*/
int	PR_WriteProgdefs (char *filename)
{
	def_t	*d;
	FILE	*f;
	unsigned short		crc;
	int		c;
	
	logprint ("writing %s\n", filename);
	f = fopen (filename, "w");
	
// print global vars until the first field is defined
	fprintf (f,"\n/* generated by hcc, do not modify */\n\ntypedef struct\n{\tint\tpad[%i];\n", RESERVED_OFS);
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (!strcmp (d->name, "end_sys_globals"))
			break;
			
		switch (d->type->type)
		{
		case ev_float:
			fprintf (f, "\tfloat\t%s;\n",d->name);
			break;
		case ev_vector:
			fprintf (f, "\tvec3_t\t%s;\n",d->name);
			d=d->next->next->next;	// skip the elements
			break;
		case ev_string:
			fprintf (f,"\tstring_t\t%s;\n",d->name);
			break;
		case ev_function:
			fprintf (f,"\tfunc_t\t%s;\n",d->name);
			break;
		case ev_entity:
			fprintf (f,"\tint\t%s;\n",d->name);
			break;
		default:
			fprintf (f,"\tint\t%s;\n",d->name);
			break;
		}
	}
	fprintf (f,"} globalvars_t;\n\n");

// print all fields
	fprintf (f,"typedef struct\n{\n");
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (!strcmp (d->name, "end_sys_fields"))
			break;
			
		if (d->type->type != ev_field)
			continue;
			
		switch (d->type->aux_type->type)
		{
		case ev_float:
			fprintf (f,"\tfloat\t%s;\n",d->name);
			break;
		case ev_vector:
			fprintf (f,"\tvec3_t\t%s;\n",d->name);
			d=d->next->next->next;	// skip the elements
			break;
		case ev_string:
			fprintf (f,"\tstring_t\t%s;\n",d->name);
			break;
		case ev_function:
			fprintf (f,"\tfunc_t\t%s;\n",d->name);
			break;
		case ev_entity:
			fprintf (f,"\tint\t%s;\n",d->name);
			break;
		default:
			fprintf (f,"\tint\t%s;\n",d->name);
			break;
		}
	}
	fprintf (f,"} entvars_t;\n\n");
	
	fclose (f);
	
// do a crc of the file
	MS_CRCInit(&crc);
	f = fopen (filename, "r+");
	while ((c = fgetc(f)) != EOF)
		MS_CRCProcessByte(&crc, (byte)c);
		
	fprintf (f,"#define PROGHEADER_CRC %i\n", crc);
	fclose (f);

	return crc;
}


void PrintFunction (char *name)
{
	int		i;
	dstatement_t	*ds;
	dfunction_t		*df;
	
	for (i=0 ; i<numfunctions ; i++)
		if (!strcmp (name, strings + functions[i].s_name))
			break;
	if (i==numfunctions)
		logerror ("No function names \"%s\"", name); // jkrige - was MS_Error()
	df = functions + i;	
	
	logprint ("Statements for %s:\n", name);
	ds = statements + df->first_statement;
	while (1)
	{
		PR_PrintStatement (ds);
		if (!ds->op)
			break;
		ds++;
	}
}

//==========================================================================
//
// main
//
//==========================================================================

void main(int argc, char **argv)
{
	char *src;
	char *src2;
	char filename[1024],infile[1024];
	int p, crc;
	double start, stop;
	int registerCount;
	int registerSize;
	int statementCount;
	int statementSize;
	int functionCount;
	int functionSize;
	int fileInfo;
	int quiet;

	// jkrige
	SetConsoleTitle("UQE Hexen II HCC");

	init_log("uqehx2hcc.log");
	logprint("UQEHX2HCC 1.16 -- based on HCC by Raven Software\n");
    logprint("================================================\n");
	logprint("Modified by Jacques Krige\n");
	logprint("Build: %s, %s\n", __TIME__, __DATE__);
    logprint("http://www.corvinstein.com\n");
	logprint("================================================\n\n");
	// jkrige

	start = MS_GetTime();

	myargc = argc;
	myargv = argv;

	if(MS_CheckParm("-?") || MS_CheckParm("-help"))
	{
		logprint(" -oi			  : Optimize Immediates\n");
		logprint(" -on			  : Optimize Name Table\n");
		logprint(" -quiet           : Quiet mode\n");
		logprint(" -fileinfo        : Show object sizes per file\n");
		logprint(" -src <directory> : Specify source directory\n");
		logprint(" -name <source>   : Specify the name of the .src file\n");
		return;
	}

	p = MS_CheckParm("-src");
	if(p && p < argc-1 )
	{
		strcpy(sourcedir, argv[p+1]);
		strcat(sourcedir, "/");
		logprint("Source directory: %s\n", sourcedir);
	}
	else
	{
		strcpy(sourcedir, "");
	}

	p = MS_CheckParm("-name");
	if(p && p < argc-1 )
	{
		strcpy(infile, argv[p+1]);
		logprint("Input file: %s\n", infile);
	}
	else
	{
		strcpy(infile, "progs.src");
	}

	InitData();
	LX_Init();
	CO_Init();
	EX_Init();

	sprintf(filename, "%s%s", sourcedir,infile);
	MS_LoadFile(filename, (void *)&src);

	src = MS_Parse(src);
	if(!src)
	{
		logerror ("No destination filename.  HCC -help for info.\n"); // jkrige - was MS_Error()
	}
	strcpy(destfile, ms_Token);

	BeginCompilation();

	hcc_OptimizeImmediates = MS_CheckParm("-oi");
	hcc_OptimizeNameTable = MS_CheckParm("-on");
	hcc_WarningsActive = MS_CheckParm("-nowarnings") ? false : true;
	hcc_ShowUnrefFuncs = MS_CheckParm("-urfunc") ? true : false;

	fileInfo = MS_CheckParm("-fileinfo");
	quiet = MS_CheckParm("-quiet");

	do
	{
		src = MS_Parse(src);
		if(!src)
		{
			break;
		}
		registerCount = numpr_globals;
		statementCount = numstatements;
		functionCount = numfunctions;
		sprintf(filename, "%s%s", sourcedir, ms_Token);
		if(!quiet)
		{
			logprint("compiling %s\n", filename);
		}
		MS_LoadFile(filename, (void *)&src2);
		if(!CO_CompileFile(src2, filename))
		{
			exit(1);
		}
		if(!quiet && fileInfo)
		{
			registerCount = numpr_globals-registerCount;
			registerSize = registerCount*sizeof(float);
			statementCount = numstatements-statementCount;
			statementSize = statementCount*sizeof(dstatement_t);
			functionCount = numfunctions-functionCount;
			functionSize = functionCount*sizeof(dfunction_t);
			logprint("      registers: %d (%d)\n", registerCount,
				registerSize);
			logprint("     statements: %d (%d)\n", statementCount,
				statementSize);
			logprint("      functions: %d (%d)\n", functionCount,
				functionSize);
			logprint("     total size: %d\n",
				registerSize+statementSize+functionSize);
		}
	} while(1);

	if(!FinishCompilation())
	{
		logerror ("compilation errors"); // jkrige - was MS_Error()
	}

	p = MS_CheckParm("-asm");
	if(p)
	{
		for(p++; p < argc; p++)
		{
			if(argv[p][0] == '-')
			{
				break;
			}
			PrintFunction(argv[p]);
		}
	}

	// write progdefs.h
	crc = PR_WriteProgdefs("progdefs.h");

	// write data file
	WriteData(crc);

	// write files.dat
	WriteFiles();
	logprint(" precache_sound: %d\n", numsounds);
	logprint(" precache_model: %d\n", nummodels);
	logprint("  precache_file: %d\n", numfiles);

	stop = MS_GetTime();
	logprint("\n%d seconds elapsed.\n", (int)(stop-start));
	exit(0);
}
