/* dict.h -- Header file for dict program
 * Created: Fri Dec  2 20:01:18 1994 by faith@cs.unc.edu
 * Revised: Wed Mar 26 13:28:08 1997 by faith@cs.unc.edu
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

#include "dictP.h"
#include "maa.h"
#include "zlib.h"

#include "net.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/wait.h>


				/* Configurable things */

#define DICT_DEFAULT_SERVICE    "2628"
#define DICT_QUEUE_DEPTH        10
#define DICT_CONFIG_FILE        "/etc/dict.conf"

				/* End of configurable things */

#define BUFFERSIZE 10240
#define DBG_VERBOSE     (0<<30|1<< 0) /* Verbose                           */
#define DBG_ZIP         (0<<30|1<< 1) /* Zip                               */
#define DBG_UNZIP       (0<<30|1<< 2) /* Unzip                             */
#define DBG_SEARCH      (0<<30|1<< 3) /* Search                            */
#define DBG_SCAN        (0<<30|1<< 4) /* Config file scan                  */
#define DBG_PARSE       (0<<30|1<< 5) /* Config file parse                 */
#define DBG_INIT        (0<<30|1<< 6) /* Database initialization           */
#define DBG_PORT        (0<<30|1<< 7) /* Log port number for connections   */
#define DBG_LEV         (0<<30|1<< 8) /* Levenshtein matching              */
#define DBG_NOFORK      (0<<30|1<< 9) /* Don't fork (single threaded)      */

#define DICT_UNKNOWN    0
#define DICT_TEXT       1
#define DICT_GZIP       2
#define DICT_DZIP       3

#define DICT_CACHE_SIZE 5

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
   const char    *optStart[257]; /* Optimized starting points */
} dictIndex;

typedef struct dictDatabase {
   const char *databaseName;
   const char *databaseShort;
   const char *dataFilename;
   const char *indexFilename;
   const char *filter;
   const char *prefilter;
   const char *postfilter;
   lst_List   acl;
   
   dictData   *data;
   dictIndex  *index;
} dictDatabase;

#define DICT_DENY  0
#define DICT_ALLOW 1
#define DICT_USER  0
#define DICT_GROUP 1
#define DICT_ADDR  2
#define DICT_NAME  3

typedef struct dictAccess {
   int        allow;		/* 1 = allow; 0 = deny */
   int        type;		/* user, group, hostaddr, hostname */
   const char *spec;
} dictAccess;

typedef struct dictConfig {
   lst_List   acl;              /* type dictAccess */
   lst_List   dbl;              /* type dictDatabase */
} dictConfig;

#define DICT_EXACT        1
#define DICT_PREFIX       2
#define DICT_SUBSTRING    3
#define DICT_REGEXP       4
#define DICT_SOUNDEX      5
#define DICT_LEVENSHTEIN  6


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
extern lst_List   dict_search_database( const char *word,
					dictDatabase *database, int strategy );
extern dictIndex  *dict_index_open( const char *filename );
extern void       dict_index_close( dictIndex *i );
extern void       dict_dump_list( lst_List list );
extern void       dict_destroy_list( lst_List list );

/* dictd.c */

extern const char *dict_get_hostname( void );
extern const char *dict_get_banner( void );
extern dictConfig *DictConfig;  /* GLOBAL VARIABLE */
extern int        _dict_comparisons; /* GLOBAL VARIABLE */

/* daemon.c */

extern int  dict_daemon( int s, struct sockaddr_in *csin, char ***argv0 );
extern void daemon_terminate( int signal, const char *name );

				/* dmalloc must be last */
#ifdef DMALLOC_FUNC_CHECK
# include "dmalloc.h"
#endif

#endif
