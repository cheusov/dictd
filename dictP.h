
/* dictP.h -- 
 * Created: Fri Mar  7 10:54:05 1997 by faith@dict.org
 * Revised: Fri Dec 22 06:06:33 2000 by faith@dict.org
 * Copyright 1997, 1999, 2000 Rickard E. Faith (faith@dict.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
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
 * $Id: dictP.h,v 1.6 2002/09/27 16:57:44 cheusov Exp $
 * 
 */

#ifndef _DICTP_H_
#define _DICTP_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef __GNUC__
#define __FUNCTION__ __FILE__
#endif

/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
# define alloca __builtin_alloca
#else
# if defined(__svr4__) && defined(__sgi__) && !HAVE_ALLOCA_H /* IRIX */
#  undef HAVE_ALLOCA_H
#  define HAVE_ALLOCA_H 1
# endif
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
#   if !defined(__svr4__) && !defined(__sgi__)	/* not on IRIX */
char *alloca ();
#   endif
#   endif
#  endif
# endif
#endif

/* Get string functions */
#if STDC_HEADERS
# include <string.h>
#else
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if !HAVE_STRDUP
extern char *strdup( const char * );
#endif

#if !HAVE_STRTOL
extern long strtol( const char *, char **, int );
#endif

#if !HAVE_STRTOUL
extern unsigned long int strtoul( const char *, char **, int );
#endif

/* Get time functions */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

/* Include some standard header files. */
#include <stdio.h>
#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
# include <stdlib.h>
#endif

/* Always use local (libmaa) getopt */
#include <getopt.h>

/* We actually need a few non-ANSI C things... */
#if defined(__STRICT_ANSI__)
extern char     *strdup( const char * );
extern int      fileno( FILE *stream );
extern FILE     *fdopen( int fildes, const char *mode );
extern void     bcopy( const void *src, void *dest, int n );
extern long int random( void );
extern void     srandom( unsigned int );
#endif

#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

/* Provide assert() */
#include <assert.h>

/* Provide stdarg support */
#include <stdarg.h>

/* Provide networking stuff */
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* Provide mmap stuff */
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

/* System dependent declarations: Many brain damaged systems don't provide
declarations for standard library calls.  We provide them here for
situations that we know about. */
#include "decl.h"

#if HAVE_LIMITS_H
#include <limits.h>
#endif

				/* Local stuff */
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#endif
