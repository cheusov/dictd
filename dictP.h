
/* dictP.h -- 
 * Created: Fri Mar  7 10:54:05 1997 by faith@dict.org
 * Copyright 1997, 1999, 2000 Rickard E. Faith (faith@dict.org)
 * Copyright 2002-2008 Aleksey Cheusov (vle@gmx.net)
 *
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
 */

#ifndef _DICTP_H_
#define _DICTP_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if  defined(__INTERIX) || defined(__OPENNT)
#ifndef _ALL_SOURCE
#define _ALL_SOURCE
#endif /* _ALL_SOURCE */
#endif /* __OPENNT */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wctype.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#if !HAVE_DAEMON
extern int daemon(int nochdir, int noclose);
#endif

#include <wchar.h>

#if !HAVE_ISWALNUM
extern int iswalnum__ (wint_t wc);
#else
#define iswalnum__ iswalnum
#endif

#if !HAVE_ISWSPACE
extern int iswspace__ (wint_t wc);
#else
#define iswspace__ iswspace
#endif

#if !HAVE_TOWLOWER
extern wint_t towlower__ (wint_t wc);
#else
#define towlower__ towlower
#endif

#include <stddef.h>

#include <langinfo.h>

#ifndef SYSTEM_UTF8_FUNCS
#define MB_CUR_MAX__ 6
#else
#define MB_CUR_MAX__ MB_CUR_MAX
#endif

#include <wchar.h>

#if !HAVE_STRLCPY
extern size_t strlcpy (char *s, const char * wc, size_t size);
#endif

#if !HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
#endif

#if !HAVE_WCRTOMB
extern size_t wcrtomb__ (char *s, wchar_t wc, mbstate_t *ps);
#else
#define wcrtomb__ wcrtomb
#endif

#if !HAVE_WCTOMB
extern int wctomb__ (char *s, wchar_t wc);
#else
#define wctomb__ wctomb
#endif

#if !HAVE_MBRLEN
extern size_t mbrlen__ (const char *s, size_t n, mbstate_t *ps);
#else
#define mbrlen__ mbrlen
#endif

#if !HAVE_MBRTOWC
extern size_t mbrtowc__ (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
#else
#define mbrtowc__ mbrtowc
#endif

#if !HAVE_MBSTOWCS
extern size_t mbstowcs__ (wchar_t *dest, const char *src, size_t n);
#else
#define mbstowcs__ mbstowcs
#endif

#if !HAVE_SETENV
extern int setenv(const char *name, const char *value, int overwrite);
#endif

#if !HAVE_MBTOWC
extern int mbtowc__ (wchar_t *pwc, const char *s, size_t n);
#else
#define mbtowc__ mbtowc
#endif

#if !HAVE_WCWIDTH
#define wcwidth__(x) (1)
#endif

#ifdef USE_PLUGIN
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
#include <time.h>

/* Include some standard header files. */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

/* We actually need a few non-ANSI C things... */
#if defined(__STRICT_ANSI__)
#if !HAVE_FILENO
extern int      fileno( FILE *stream );
#endif
extern FILE     *fdopen( int fildes, const char *mode );
#endif

#include <sys/resource.h>

/* Provide assert() */
#include <assert.h>

/* Provide stdarg support */
#include <stdarg.h>

/* Provide networking stuff */
#include <sys/wait.h>
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#include <netdb.h>
#include <netinet/in.h>

/* Provide mmap stuff */
#include <sys/mman.h>

#include <limits.h>

/* Handle getopt correctly */
#include <getopt.h>

				/* Local stuff */
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define HAVE_UTF8 1

#endif /* _DICTP_H_ */
