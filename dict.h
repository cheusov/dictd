/* dict.h -- Header file for dict program
 * Created: Fri Dec  2 20:01:18 1994 by faith@cs.unc.edu
 * Revised: Wed Sep 25 20:55:04 1996 by faith@cs.unc.edu
 * Copyright 1994, 1995, 1996 Rickard E. Faith (faith@cs.unc.edu)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 1, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _DICT_H_
#define _DICT_H_

#include "maaP.h"

#define BUFFERSIZE 10240

extern void fmt_newline( void );
extern void fmt_new( const char *word );
extern void fmt_string( const char *string );
extern void fmt_flush( void );
extern void fmt_line( const char *line );
extern void fmt_def( const char *pos, int entry );
extern void fmt_open( const char *basename );
extern void fmt_close( void );

#endif
