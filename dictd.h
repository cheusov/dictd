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
#include "zlib.h"
#include "codes.h"

#include "net.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/utsname.h>
#ifdef __osf__
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <sys/wait.h>
#include <grp.h>

				/* Configurable things */

#define DICT_DEFAULT_SERVICE     "2628"	/* Also in dict.h */
#define DICTD_CONFIG_NAME        "dictd.conf"
#define DICT_QUEUE_DEPTH         10
#define DICT_DEFAULT_DELAY       600 /* 10 minute timeout */
#define DICT_DAEMON_LIMIT        100 /* maximum simultaneous daemons */
#define DICT_PERSISTENT_PRESTART 3 /* not implemented */
#define DICT_PERSISTENT_LIMIT    5 /* not implemented */
#define DICT_SHORT_ENTRY_NAME    "00-database-short"
#define DICT_LONG_ENTRY_NAME     "00-database-long"
#define DICT_INFO_ENTRY_NAME     "00-database-info"
#define DICT_DEFAULT_STRATEGY    DICT_LEVENSHTEIN

				/* End of configurable things */


#define BUFFERSIZE 10240

#define DBG_VERBOSE     (0<<30|1<< 0) /* Verbose                            */
#define DBG_ZIP         (0<<30|1<< 1) /* Zip                                */
#define DBG_UNZIP       (0<<30|1<< 2) /* Unzip                              */
#define DBG_SEARCH      (0<<30|1<< 3) /* Search                             */
#define DBG_SCAN        (0<<30|1<< 4) /* Config file scan                   */
#define DBG_PARSE       (0<<30|1<< 5) /* Config file parse                  */
#define DBG_INIT        (0<<30|1<< 6) /* Database initialization            */
#define DBG_PORT        (0<<30|1<< 7) /* Log port number for connections    */
#define DBG_LEV         (0<<30|1<< 8) /* Levenshtein matching               */
#define DBG_AUTH        (0<<30|1<< 9) /* Debug authentication               */
#define DBG_NODETACH    (0<<30|1<<10) /* Don't detach as a background proc. */
#define DBG_NOFORK      (0<<30|1<<11) /* Don't fork (single threaded)       */
#define DBG_ALT         (0<<30|1<<12) /* altcompare()                      */

#define LOG_SERVER      (0<<30|1<< 0) /* Log server diagnostics             */
#define LOG_CONNECT     (0<<30|1<< 1) /* Log connection information         */
#define LOG_STATS       (0<<30|1<< 2) /* Log termination information        */
#define LOG_COMMAND     (0<<30|1<< 3) /* Log commands                       */
#define LOG_FOUND       (0<<30|1<< 4) /* Log words found                    */
#define LOG_NOTFOUND    (0<<30|1<< 5) /* Log words not found                */
#define LOG_CLIENT      (0<<30|1<< 6) /* Log client                         */
#define LOG_HOST        (0<<30|1<< 7) /* Log remote host name               */
#define LOG_TIMESTAMP   (0<<30|1<< 8) /* Log with timestamps                */
#define LOG_MIN         (0<<30|1<< 9) /* Log a few minimal things           */
#define LOG_AUTH        (0<<30|1<<10) /* Log authentication denials         */

#define DICT_LOG_TERM    0
#define DICT_LOG_DEFINE  1
#define DICT_LOG_MATCH   2
#define DICT_LOG_NOMATCH 3
#define DICT_LOG_CLIENT  4
#define DICT_LOG_TRACE   5
#define DICT_LOG_COMMAND 6
#define DICT_LOG_AUTH    7
#define DICT_LOG_CONNECT 8

#define DICT_UNKNOWN    0
#define DICT_TEXT       1
#define DICT_GZIP       2
#define DICT_DZIP       3

#define DICT_CACHE_SIZE 5

typedef struct dictStrategy {
   const char *name;
   const char *description;
   int        number;
} dictStrategy;

typedef struct dictCache {
   int           chunk;
   char          *inBuffer;
   int           stamp;
   int           count;
} dictCache;

typedef struct dictData {
   int           fd;		/* file descriptor */
   const char    *start;	/* start of mmap'd area */
   const char    *end;		/* end of mmap'd area */
   unsigned long size;		/* size of mmap */
   
   int           type;
   const char    *filename;
   z_stream      zStream;
   int           initialized;
   
   int           headerLength;
   int           method;
   int           flags;
   time_t        mtime;
   int           extraFlags;
   int           os;
   int           version;
   int           chunkLength;
   int           chunkCount;
   int           *chunks;
   unsigned long *offsets;	/* Sum-scan of chunks. */
   const char    *origFilename;
   const char    *comment;
   unsigned long crc;
   unsigned long length;
   unsigned long compressedLength;
   dictCache     cache[DICT_CACHE_SIZE];
} dictData;

typedef struct dictIndex {
   int           fd;		 /* file descriptor */
   const char    *start;	 /* start of mmap'd area */
   const char    *end;		 /* end of mmap'd area */
   unsigned long size;		 /* size of mmap */
   const char    *optStart[UCHAR_MAX+2]; /* Optimized starting points */
   unsigned long headwords;	 /* computed number of headwords */
} dictIndex;

typedef struct dictDatabase {
   const char *databaseName;
   const char *databaseShort;
   const char *databaseInfoPointer;
   const char *dataFilename;
   const char *indexFilename;
   const char *filter;
   const char *prefilter;
   const char *postfilter;
   lst_List   acl;
   int        available;	/* if user has authenticated for database */
   
   dictData   *data;
   dictIndex  *index;
} dictDatabase;

#define DICT_DENY     0
#define DICT_ALLOW    1
#define DICT_AUTHONLY 2
#define DICT_USER     3
#define DICT_GROUP    4		/* Not implemented */
#define DICT_MATCH    5         /* For IP matching routines */
#define DICT_NOMATCH  6         /* For IP matching routines */

typedef struct dictAccess {
   int        type;		/* deny, allow, accessonly, user, group */
   const char *spec;
} dictAccess;

typedef struct dictConfig {
   lst_List      acl;		/* type dictAccess */
   lst_List      dbl;		/* type dictDatabase */
   hsh_HashTable usl;		/* username/shared-secret list */
   const char    *site;
} dictConfig;

#define DICT_EXACT        1	/* Exact */
#define DICT_PREFIX       2	/* Prefix */
#define DICT_SUBSTRING    3	/* Substring */
#define DICT_SUFFIX       4	/* Suffix */
#define DICT_RE           5	/* POSIX 1003.2 (modern) regular expressions */
#define DICT_REGEXP       6	/* old (basic) regular expresions */
#define DICT_SOUNDEX      7	/* Soundex */
#define DICT_LEVENSHTEIN  8	/* Levenshtein */


typedef struct dictWord {
   const char    *word;
   unsigned long start;
   unsigned long end;
   dictDatabase  *database;
} dictWord;

typedef struct dictToken {
   const char   *string;
   int          integer;
   src_Type     src;
} dictToken;

extern dictData *dict_data_open( const char *filename, int computeCRC );
extern void     dict_data_close( dictData *data );
extern void     dict_data_print_header( FILE *str, dictData *data );
extern int      dict_data_zip( const char *inFilename, const char *outFilename,
			       const char *preFilter, const char *postFilter );
extern char     *dict_data_read( dictData *data,
				 unsigned long start, unsigned long end,
				 const char *preFilter,
				 const char *postFilter );
extern int      dict_data_filter( char *buffer, int *len, int maxLength,
				  const char *filter );


extern const char *dict_index_search( const char *word, dictIndex *idx );
extern int        dict_search_database( lst_List l,
					const char *word,
					dictDatabase *database, int strategy );
extern dictIndex  *dict_index_open( const char *filename );
extern void       dict_index_close( dictIndex *i );
extern void       dict_dump_list( lst_List list );
extern void       dict_destroy_list( lst_List list );

/* dictd.c */

extern void       dict_initsetproctitle( int argc, char **argv, char **envp );
extern void       dict_setproctitle( const char *format, ... );
extern const char *dict_format_time( double t );
extern const char *dict_get_hostname( void );
extern const char *dict_get_banner( int shortFlag );

extern dictConfig *DictConfig;  /* GLOBAL VARIABLE */
extern int        _dict_comparisons; /* GLOBAL VARIABLE */
extern int        _dict_forks;	/* GLOBAL VARIABLE */

/* daemon.c */

extern int  dict_daemon( int s, struct sockaddr_in *csin, char ***argv0,
			 int delay, int error );
extern void daemon_terminate( int sig, const char *name );
extern int get_strategies_count ();
extern const dictStrategy *get_strategies ();
extern int lookup_strategy( const char *strategy );

/* */
extern int        utf8_mode;

				/* dmalloc must be last */
#ifdef DMALLOC_FUNC_CHECK
# include "dmalloc.h"
#endif

#endif
