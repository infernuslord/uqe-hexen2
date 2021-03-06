/*  Copyright (C) 2000  Kevin Shanahan

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

/*
 * common/tyrlog.c
 */

//#include "tyrlog.h"
#include "cmdlib.h"
#include "hcc.h"
#include <stdio.h>

FILE      *logfile;
qboolean  log_ok;

void init_log(char *filename)
{
    log_ok = false;
    if ((logfile = fopen(filename,"w")))
        log_ok = true;
}

void close_log()
{
    if (log_ok)
        fclose(logfile);
}

void logprint(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    if (log_ok) {
        vfprintf(logfile, fmt, args);
    }
}

// jkrige - logerror
void logerror(const char *fmt, ...)
{
    va_list args;

	logprint ("************ ERROR ************\n");

    va_start(args, fmt);
    vprintf(fmt, args);
    if (log_ok) {
        vfprintf(logfile, fmt, args);
    }
	va_end (args);

	logprint ("\n");
	exit (1);
}
// jkrige - logerror

void logvprint(const char *fmt, va_list args)
{
    vprintf(fmt, args);
    if (log_ok) {
        vfprintf(logfile, fmt, args);
    }
}
