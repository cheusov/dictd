
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
 * $Id: dictP.h,v 1.12 2003/09/04 17:57:02 cheusov Exp $
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
#if defined(__svr4__) && defined(__sgi__) && !HAVE_ALLOCA_H /* IRIX */
# undef HAVE_ALLOCA_H
# define HAVE_ALLOCA_H 1
#endif

#if HAVE_ALLOCA_H
# include <alloca.h>
#else
# ifdef _AIX
# pragma alloca
# else
#  ifndef alloca /* predefined by HP cc +Olibcalls */
#  if !defined(__svr4__) && !defined(__sgi__)	/* not on IRIX */
    char *alloca ();
#  endif
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

#if !HAVE_SNPRINTF
extern int snprintf(char *str, size_t size, const char *format, ...);
#endif

#if !HAVE_VSNPRINTF
#include <stdarg.h>
extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

#if !HAVE_INET_ATON
#define inet_aton(a,b) (b)->s_addr = inet_addr(a)
#endif

#if HAVE_WINT_T
#include <wchar.h>
#else
typedef unsigned int wint_t;
#endif

#if HAVE_WCHAR_T
#include <stddef.h>
#else
typedef unsigned int wchar_t;
#endif

#if HAVE_MBSTATE_T
#include <wchar.h>
#else
typedef char mbstate_t;
#endif

#if !HAVE_STRLCPY
extern size_t strlcpy (char *s, const char * wc, size_t size);
#endif

#if !HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
#endif

#if !HAVE_WCRTOMB
extern size_t wcrtomb (char *s, wchar_t wc, mbstate_t *ps);
#endif

#if !HAVE_WCTOMB
extern int wctomb (char *s, wchar_t wc);
#endif

#if !HAVE_MBRLEN
extern size_t mbrlen (const char *s, size_t n, mbstate_t *ps);
#endif

#if !HAVE_MBRTOWC
extern size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
#endif

#if !HAVE_MBSTOWCS
extern size_t mbstowcs (wchar_t *dest, const char *src, size_t n);
#endif

#if !HAVE_MBTOWC
extern int mbtowc (wchar_t *pwc, const char *s, size_t n);
#endif

#if USE_PLUGIN
# if HAVE_DLFCN_H
#  include <dlfcn.h>
   typedef void *  lt_dlhandle;
#  define lt_dlsym dlsym
#  define lt_dlopen(filename) dlopen(filename, RTLD_NOW)
#  define lt_dlclose dlclose
#  define lt_dlerror dlerror
# else
#  include <ltdl.h>
# endif
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

#if HAVE_ISWALNUM && HAVE_ISWSPACE && HAVE_TOWLOWER
#ifdef HAVE_UTF8
#undef HAVE_UTF8
#endif

#define HAVE_UTF8 1
#endif

#endif /* _DICTP_H_ */
