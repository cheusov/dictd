/* daemon.c -- Server daemon
 * Created: Fri Feb 28 18:17:56 1997 by faith@cs.unc.edu
 * Revised: Mon Mar 10 12:12:48 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
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
 * $Id: daemon.c,v 1.6 1997/03/10 21:46:54 faith Exp $
 * 
 */

#include <ctype.h>
#include "dictd.h"

static int          totalMatches;
static int          daemonS;
static const char   *daemonHostname;
static int          daemonPort;

static struct {
   const char *name;
   const char *description;
   int        number;
} strategyInfo[] = {
   {"exact",     "Match words exactly",                        DICT_EXACT },
   {"prefix",    "Match prefixes",                             DICT_PREFIX },
   {"substring", "Match substring occurring anywhere in word", DICT_SUBSTRING},
   {"regexp",    "Regular expression match",                   DICT_REGEXP },
   {"soundex",   "Match using SOUNDEX algorithm",              DICT_SOUNDEX },
   {"lev", "Match words within Levenshtein distance one", DICT_LEVENSHTEIN },
};

#define STRATEGIES (sizeof(strategyInfo)/sizeof(strategyInfo[0]))

static void daemon_status( void );

static void daemon_log( const char *format, ... )
{
   va_list ap;
   char    buf[512];
   char    buf2[3*512];
   int     len;
   char    *s, *d;
   int     c;

   if (dbg_test(DBG_PORT))
      sprintf( buf, "%s:%d ", daemonHostname, daemonPort );
   else
      sprintf( buf, "%s ", daemonHostname );
      
   len = strlen( buf );
   
   va_start( ap, format );
   vsprintf( buf+len, format, ap );
   va_end( ap );
   len = strlen( buf );

   if (len > 500)
      err_internal( __FUNCTION__, "Buffer overflow (%d)\n", len );

   for (s = buf, d = buf2; *s; s++) {
      c = (unsigned char)*s;
      if (c == '\t')      *d++ = ' ';
      else if (c == '\n') *d++ = c;
      else {
	 if (c > 128)       { *d++ = 'M'; *d++ = '-';    c -= 128; }
	 if (c < 32)        { *d++ = '^'; *d++ = c + 64;           }
	 else if (c == 127) { *d++ = '^'; *d++ = '?';              }
	 else                 *d++ = c;
      }
   }
   *d = '\0';

   log_info( "%s", buf2 );
}

static int daemon_quit( int clean, const char *function )
{
   if (clean) daemon_status();
   else       tim_stop( "daemon" );
   
   close(daemonS);
   daemon_log( "%s/%s %d %0.3fr %0.3fu %0.3fs\n",
	       clean ? "quit" : "abend",
	       function,
	       totalMatches,
	       tim_get_real( "daemon" ),
	       tim_get_user( "daemon" ),
	       tim_get_system( "daemon" ) );
   exit(0);
}

static void daemon_write( const char *format, ... )
{
   va_list ap;
   char    buf[1024];
   int     len;

   va_start( ap, format );
   vsprintf( buf, format, ap );
   va_end( ap );
   len = strlen( buf );

   if (write(daemonS, buf, len) != len)
      daemon_quit( 0, __FUNCTION__ );
}


static int dump_def( const void *datum, void *arg )
{
   char         *buf;
   dictWord     *dw = (dictWord *)datum;
   dictDatabase *db = (dictDatabase *)arg;
   int          len;

   buf = dict_data_read( db->data, dw->start, dw->end,
			 db->prefilter, db->postfilter );
   len = strlen( buf );

   daemon_write( "definition \"%s\" \"%s\" \"%s\" %d\n",
		 dw->word,
		 db->databaseName,
		 db->databaseShort,
		 len );
   if (write(daemonS, buf, len) != len)
      daemon_quit( 0, __FUNCTION__ );

   xfree( buf );
   return 0;
}

static void daemon_dump_defs( lst_List list, dictDatabase *db )
{
   lst_iterate_arg( list, dump_def, db );
}

static int dump_match( const void *datum )
{
   dictWord     *dw = (dictWord *)datum;

   daemon_write( "%s\n", dw->word );
   return 0;
}

static void daemon_dump_matches( lst_List list )
{
   lst_iterate( list, dump_match );
}

static void daemon_status( void )
{
   time_t t;
   char   buf[1024];
   int    len;

   tim_stop( "daemon" );
   time(&t);

   sprintf(buf,
	   "status %0.3fr %0.3fu %0.3fs (%24.24s)\n",
	   tim_get_real( "daemon" ),
	   tim_get_user( "daemon" ),
	   tim_get_system( "daemon" ),
	   ctime(&t));
   len = strlen( buf );
   if (write(daemonS, buf, len) != len)
      daemon_quit( 0, __FUNCTION__ );
}

static void daemon_error( int number, const char *format, ... )
{
   va_list ap;
   char    buf[1024];
   int     len;

   sprintf( buf, "error %d ", number );
   len = strlen( buf );
   
   va_start( ap, format );
   vsprintf( buf+len, format, ap );
   va_end( ap );
   len = strlen( buf );

   if (*(buf+len-1) != '\n') {
      sprintf( buf+len, "\n" );
      len = strlen( buf );
   }

   if (write(daemonS, buf, len) != len)
      daemon_quit( 0, __FUNCTION__ );
}

static int daemon_read( char *buf, int count )
{
   int  len = 0;
   int  n;
   char c;
   char *pt = buf;

   *pt = '\0';
   while ((n = read( daemonS, &c, 1 )) > 0) {
      switch (c) {
      case '\n': *pt = '\0';       return len;
      case '\r':                   break;
      default:   *pt++ = c; ++len; break;
      }
   }
   if (!n) return len;
   return n;
}

static int daemon_banner( void )
{
   time_t         t;

   time(&t);
   
   daemon_write( "server 1 %s <%ld.%ld@%s>\n",
		 dict_get_banner(),
		 (long)getpid(),
		 t,
		 net_hostname() );
   return 0;
}

static int daemon_show( const char *what )
{
   int          count;
   int          i;
   dictDatabase *db;
   
   if (!strcmp(what,"databases") || !strcmp(what,"db")) {
      count = lst_length( DictConfig->dbl );
      daemon_write( "databases %d\n", count );
      for (i = 1; i <= count; i++) {
	 db = lst_nth_get( DictConfig->dbl, i );
	 daemon_write( "database %s \"%s\"\n",
		       db->databaseName, db->databaseShort );
      }
      daemon_status();
      return count;
   } else if (!strcmp(what,"strategies") || !strcmp(what,"strat")) {
      daemon_write( "strategies %d\n", STRATEGIES );
      for (i = 0; i < STRATEGIES; i++) {
	 daemon_write( "strategy %s \"%s\"\n",
		       strategyInfo[i].name,
		       strategyInfo[i].description );
      }
      daemon_status();
      return STRATEGIES;
   }
   
   daemon_error( 0, "Illegal show command" );
   return 0;
}

static int daemon_define( const char *word, const char *databaseName )
{
   lst_List       list;
   dictDatabase   *db;
   int            i;
   int            matches = 0;
   int            count = lst_length( DictConfig->dbl );

				/* FIXME! Iterative search... */
   for (i = 1; i <= count; i++) {
      db = lst_nth_get( DictConfig->dbl, i );
      if (!databaseName || !strcmp( db->databaseName, databaseName )) {
	 list = dict_search_database( word, db, DICT_EXACT );
	 if (list && (matches = lst_length(list)) > 0) {
	    daemon_write( "definitions %d\n", matches );
	    daemon_dump_defs( list, db );
	    daemon_status();
	 } else {
	    daemon_error( 0, "No match" );
	 }
	 if (list) dict_destroy_list( list );
	 daemon_log( "define %s \"%.80s\" %d\n",
		     db->databaseName, word, matches );
	 return matches;
      }
   }
   daemon_error( 0, "Unknown database \"%s\"", databaseName );
   return 0;
}

static int daemon_match( const char *word,
			 const char *strategy, const char *databaseName )
{
   lst_List       list;
   dictDatabase   *db;
   int            i, j;
   int            matches = 0;

				/* FIXME! Iterative search... */
   for (i = 1; i <= lst_length( DictConfig->dbl ); i++) {
      db = lst_nth_get( DictConfig->dbl, i );
      if (!databaseName || !strcmp( db->databaseName, databaseName )) {
	 for (j = 0; j < STRATEGIES; j++) {
	    if (!strcmp(strategyInfo[j].name,strategy)) {
	       list = dict_search_database( word, db,
					    strategyInfo[j].number);
	       if (list && (matches = lst_length(list)) > 0) {
		  daemon_write( "matches %d\n", matches );
		  daemon_dump_matches( list );
		  daemon_status();
	       } else {
		  daemon_error( 0, "No match" );
	       }
	       if (list) dict_destroy_list( list );
	       daemon_log( "match %s \"%.80s\" %s %d\n",
			   db->databaseName, word, strategyInfo[j].name,
			   matches );
	       return matches;
	    }
	 }
	 daemon_error( 0, "Unknown strategy \"%s\"", strategy );
	 return 0;
      }
   }
   daemon_error( 0, "Unknown database \"%s\"", databaseName );
   return 0;
}

int dict_daemon( int s, struct sockaddr_in *csin )
{
   char           buf[4096];
   int            count;
   struct hostent *h;
   arg_List       cmdline;
   int            argc;
   char           **argv;
   const char     *hostname;
   int            port;
      
   port     = ntohs(csin->sin_port);
   hostname = str_find( inet_ntoa(csin->sin_addr) );
   if ((h = gethostbyaddr((void *)&csin->sin_addr,
			  sizeof(csin->sin_addr), csin->sin_family))) {
      hostname = str_find( h->h_name );
   }

   daemonS        = s;
   daemonHostname = hostname;
   daemonPort     = port;
   
   tim_start( "daemon" );
   daemon_log( "connected\n" );

   daemon_banner();

   while ((count = daemon_read( buf, 4000 )) >= 0) {
      if (!count) {
	 daemon_status();
	 continue;
      }
      cmdline = arg_argify(buf);
      arg_get_vector( cmdline, &argc, &argv );
      
      if (argc == 2 && !strcmp("define",argv[0])) {
	 totalMatches += daemon_define( argv[1], NULL );
      } else if (argc == 3 && !strcmp("define",argv[0])) {
	 totalMatches += daemon_define( argv[1], argv[2] );
      } else if (argc == 3 && !strcmp("match",argv[0])) {
	 daemon_match( argv[1], argv[2], NULL );
      } else if (argc == 4 && !strcmp("match",argv[0])) {
	 daemon_match( argv[1], argv[2], argv[3] );
      } else if (argc == 2 && !strcmp("show",argv[0])) {
	 daemon_show( argv[1] );
      } else if (argc == 1 && !strcmp("quit",argv[0])) {
	 daemon_quit( 1, __FUNCTION__ );
	 return 0;
      } else {
	 daemon_error( 0, "Illegal command: \"%s\"\n", buf );
	 daemon_log( "illegal %.80s\n", buf );
      }
      arg_destroy(cmdline);
   }
   tim_stop( "daemon" );
   daemon_log( "disconnect %d %0.3fr %0.3fu %0.3fs\n",
	       totalMatches,
	       tim_get_real( "daemon" ),
	       tim_get_user( "daemon" ),
	       tim_get_system( "daemon" ) );
   return 0;
}
