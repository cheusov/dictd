/* dictd.h -- Header file for dict program
 * Created: Fri Dec  2 20:01:18 1994 by faith@dict
 * Revised: Mon Apr 22 15:47:26 2002 by faith@dict.org
 * Copyright 1994-2000, 2002 Rickard E. Faith (faith@dict.org)
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

#ifndef _DICTD_H_
#define _DICTD_H_

#include "dictP.h"
#include "maa.h"
#include "codes.h"
#include "defs.h"

#include "net.h"
#include <errno.h>

#include <netdb.h>
#include <signal.h>
/*
#ifdef __osf__
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <sys/wait.h>
#include <grp.h>
#include <arpa/inet.h>
*/

extern void       dict_disable_strat (dictDatabase *db, const char* strat);

extern void       dict_dump_list( lst_List list );
extern void       dict_destroy_list( lst_List list );

extern int        dict_destroy_datum( const void *datum );

#ifdef USE_PLUGIN
extern int        dict_plugin_open (dictDatabase *db);
extern void       dict_plugin_close (dictDatabase *db);
#endif

/* dictd.c */

extern void       dict_initsetproctitle( int argc, char **argv, char **envp );
extern void       dict_setproctitle( const char *format, ... );
extern const char *dict_format_time( double t );
extern const char *dict_get_hostname( void );
extern const char *dict_get_banner( int shortFlag );

extern dictConfig *DictConfig;  /* GLOBAL VARIABLE */
extern int        _dict_comparisons; /* GLOBAL VARIABLE */
extern int        _dict_forks;	/* GLOBAL VARIABLE */
extern const char *locale;

extern const char *locale;
extern       int   inetd;    /* 1 if --inetd is applied, 0 otherwise */

/*
  If the filename doesn't start with / or .,
  it is prepended with DICT_DIR
*/
extern const char *postprocess_dict_filename (const char *fn);
/*
  If the filename doesn't start with / or .,
  it is prepended with PLUGIN_DIR
*/
extern const char *postprocess_plugin_filename (const char *fn);

/* daemon.c */

extern int  dict_daemon( int s, struct sockaddr_in *csin, char ***argv0,
			 int delay, int error );
extern int  dict_inetd( char ***argv0, int delay, int error );
extern void daemon_terminate( int sig, const char *name );

/* */
extern int        utf8_mode;

				/* dmalloc must be last */
#ifdef DMALLOC_FUNC_CHECK
# include "dmalloc.h"
#endif

#endif
