/* daemon.c -- Server daemon
 * Created: Fri Feb 28 18:17:56 1997 by faith@cs.unc.edu
 * Revised: Fri Mar 28 23:35:19 1997 by faith@cs.unc.edu
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
 * $Id: daemon.c,v 1.11 1997/03/31 01:53:27 faith Exp $
 * 
 */

#include "dictd.h"
#include <ctype.h>

static int          _dict_defines, _dict_matches;
static int          daemonS;
static const char   *daemonHostname;
static int          daemonPort;
static lst_Position databasePosition;

static void daemon_define( int argc, char **argv );
static void daemon_match( int argc, char **argv );
static void daemon_show_db( int argc, char **argv );
static void daemon_show_strat( int argc, char **argv );
static void daemon_show_info( int argc, char **argv );
static void daemon_show( int argc, char **argv );
static void daemon_client( int argc, char **argv );
static void daemon_auth( int argc, char **argv );
static void daemon_status( int argc, char **argv );
static void daemon_help( int argc, char **argv );
static void daemon_quit( int argc, char **argv );

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

static struct {
   int        argc;
   const char *name[];
   void       (*f)( int argc, char **argv );
} commandInfo[] = {
   { 1, {"define"},             daemon_define },
   { 1, {"match"},              daemon_match },
   { 2, {"show", "db"},         daemon_show_db },
   { 2, {"show", "databases"},  daemon_show_db },
   { 2, {"show", "strat"},      daemon_show_strat },
   { 2, {"show", "strategies"}, daemon_show_strat },
   { 2, {"show", "info"},       daemon_show_info },
   { 1, {"show"},               daemon_show },
   { 1, {"client"},             daemon_client },
   { 1, {"auth"},               daemon_auth },
   { 1, {"status"},             daemon_status },
   { 1, {"help"},               daemon_help },
   { 1, {"quit"},               daemon_quit },
};
#define COMMANDS (sizeof(commandInfo)/sizeof(commandInfo[0]))


static int lookup_strategy( const char *strategy )
{
   int i;
   
   for (i = 0; i < STRATEGIES; i++) {
      if (!strcasecmp(strategy, strategyInfo[i].name))
         return strategyInfo[i].number;
   }
   return -1;
}

static void *(lookup_command)( const char *command )
{
   int i, j;
   int err;
   
   for (i = 0; i < COMMANDS; i++) {
      for (er = 0, j = 0; j < commandInfo[i].argc; j++) {
	 if (strcasecmp(command, commandInfo[i].name)) {
	    err = 1;
	    break;
	 }
      }
      if (!err) return commandInfo[i].f;
   }
   return NULL;
}

static void reset_databases( void )
{
   databasePosition = lst_init_position( DictConfig->dbl );
}

static dictDatabase *next_database( const char *name )
{
   dictDatabase *db = NULL;

   if (!name) return NULL;

   if (!strcmp(name,"*")) {
      if (databasePosition) {
         db = lst_get_position( databasePosition );
         databasePosition = lst_next_position( databasePosition );
      }
      return db;
   } else {
      while (databasePosition) {
         db = lst_get_position( databasePosition );
         if (!strcmp(db->databaseName,name)) return db;
      }
      return NULL;
   }
}

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

   if (len > 500) {
      log_error( __FUNCTION__, "Buffer overflow (%d)\n", len );
      exit(0);
   }

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

void daemon_terminate( int sig, const char *name )
{
   tim_stop( "t" );
   close(daemonS);
   if (name) {
      daemon_log( "%s: d/m/c = %d/%d/%d; %0.3fr %0.3fu %0.3fs\n",
                  name,
                  _dict_defines,
                  _dict_matches,
                  _dict_comparisons,
                  tim_get_real( "t" ),
                  tim_get_user( "t" ),
                  tim_get_system( "t" ) );
   } else {
      daemon_log( "Signal %d: d/m/c = %d/%d/%d; %0.3fr %0.3fu %0.3fs\n",
                  sig,
                  _dict_defines,
                  _dict_matches,
                  _dict_comparisons,
                  tim_get_real( "t" ),
                  tim_get_user( "t" ),
                  tim_get_system( "t" ) );
   }
   if (sig) exit(sig+128);
   exit(0);
}


static void daemon_write( const char *buf, int len )
{
   int left = len;
   int count;
   
   while (left) {
      if ((count = write(daemonS, buf, left)) != left) {
	 if (count <= 0) {
#if HAVE_STRERROR
            log_error( __FUNCTION__,
                       "Error writing %d of %d bytes:"
                       " retval = %d, errno = %d (%s)\n",
                       left, len, count, errno, strerror(errno) );
#else
            log_error( __FUNCTION__,
                       "Error writing %d of %d bytes:"
                       " retval = %d, errno = %d\n",
                       left, len, count, errno );
#endif
            daemon_terminate( 0, __FUNCTION__ );
         }
      }
      left -= count;
   }
}

static void daemon_crlf( char *d, const char *s, int dot )
{
   while (*s) {
      if (*s == '\n') {
	 *d++ = '\r';
	 *d++ = '\n';
	 if (dot && s[1] == '.' && s[2] != '\n')
	    *d++ = '.'; /* double first dot on line */
	 ++s;
      } else {
	 *d++ = *s++;
      }
   }
   if (dot) {
      *d++ = '.';
      *d++ = '\r';
      *d++ = '\n';
   }
   *d = '\0';
}

static void daemon_printf( const char *format, ... )
{
   va_list ap;
   char    buf[BUFFERSIZE];
   char    *pt;
   int     len;

   va_start( ap, format );
   vsprintf( buf, format, ap );
   va_end( ap );
   if ((len = strlen( buf )) >= BUFFERSIZE) {
      log_error( __FUNCTION__, "Buffer overflow: %d\n", len );
      daemon_terminate( 0, __FUNCTION__ );
   }

   pt = alloca(2*len);
   daemon_crlf(pt, buf, 0);
   daemon_write(pt, strlen(pt));
}

static void daemon_text( const char *text )
{
   char *pt = alloca( 2*strlen(text) );

   daemon_crlf(pt, text, 1);
   daemon_write(pt, strlen(pt));
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

static void daemon_ok( const char *string, const char *timer )
{
   time_t t;
   char   buf[1024];
   int    len;

   tim_stop( timer );
   time(&t);
      
   sprintf(buf,
	   "%s [d/m/c = %d/%d/%d; %0.3fr %0.3fu %0.3fs; %24.24s]\n",
	   string,
	   _dict_defines,
	   _dict_matches,
	   _dict_comparisons,
	   tim_get_real( timer ),
	   tim_get_user( timer ),
	   tim_get_system( timer ),
	   ctime(&t));

   len = strlen( buf );
   daemon_write(buf, len);
}

static int dump_def( const void *datum, void *arg )
{
   char         *buf;
   dictWord     *dw = (dictWord *)datum;
   dictDatabase *db = (dictDatabase *)arg;

   buf = dict_data_read( db->data, dw->start, dw->end,
			 db->prefilter, db->postfilter );

   daemon_printf( "251 %s \"%s\" - text follows\n",
		  db->databaseName,
		  db->databaseShort );
   daemon_text(buf);
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

   daemon_printf( "%s \"%s\"\n", dw->database->databaseName, dw->word );
   return 0;
}

static void daemon_dump_matches( lst_List list )
{
   lst_iterate( list, dump_match );
}

static void daemon_banner( void )
{
   time_t         t;

   time(&t);
   
   daemon_printf( "220 %s %s <%d.%d.%ld@%s>\n",
                  net_hostname(),
		  dict_get_banner(),
		  _dict_forks,
		  getpid(),
		  t,
		  net_hostname());
}

static void daemon_define( int argc, char **argv )
{
   lst_List       list;
   dictDatabase   *db;
   int            matches = 0;
   char           *word;
   const char     *databaseName;

   switch (argc) {
   case 2:  databaseName = "*";     word = argv[1]; break;
   case 3:  databaseName = argv[1]; word = argv[2]; break;
   default:
      daemon_printf( "501 syntax error, illegal parameters\n" );
      return;
   }

   reset_databases();
   while ((db = next_database(databaseName))) {
      list = dict_search_database( word, db, DICT_EXACT );
      if (list && (matches = lst_length(list)) > 0) {
	 daemon_printf( "250 %d definitions retrieved - definitions follow\n",
			matches );
	 daemon_dump_defs( list, db );
	 daemon_ok( "259 ok", "c" );
	 daemon_log("define %s \"%s\" %d\n", db->databaseName, word, matches);
      } else {
	 if (*databaseName == '*') continue; /* keep searching */
	 goto nomatch;
      }
      if (list) dict_destroy_list( list );
      _dict_defines += matches;
      if (matches) return;
   }
   
   if (*databaseName != '*') {
      daemon_printf( "550 invalid database, use SHOW DB for list\n" );
      return;
   }

 nomatch:
   daemon_printf( "552 nomatch for %s \"%s\"\n", databaseName, word );
   daemon_log( "nomatch %s exact \"%s\"\n", databaseName, word );
}

static void daemon_match( int argc, char **argv )
{
   lst_List       list;
   dictDatabase   *db;
   int            matches = 0;
   char           *word;
   const char     *databaseName;
   const char     *strategy;
   int            strategyNumber;

   switch (argc) {
   case 2:  databaseName = "*";     strategy = "lev";   word = argv[1]; break;
   case 3:  databaseName = "*";     strategy = argv[2]; word = argv[2]; break;
   case 4:  databaseName = argv[1]; strategy = argv[2]; word = argv[3]; break;
   default:
      daemon_printf( "501 syntax error, illegal parameters\n" );
      return;
   }

   if ((strategyNumber = lookup_strategy(strategy)) < 0) {
      daemon_printf( "551 invalid strategy, use SHOW STRAT for a list\n" );
      return;
   }

   reset_databases();
   while ((db = next_database(databaseName))) {
      list = dict_search_database( word, db, strategyNumber );
      if (list && (matches = lst_length(list)) > 0) {
	 daemon_printf( "250 %d matches found - text follow\n", matches );
	 daemon_dump_matches( list );
	 daemon_ok( "259 ok", "c" );
	 daemon_log("match %s %s \"%s\" %d\n",
		    db->databaseName, strategy, word, matches);
      } else {
	 if (*databaseName == '*') continue; /* keep searching */
	 goto nomatch;
      }
      if (list) dict_destroy_list( list );
      _dict_matches += matches;
      if (matches) return;
   }
   
   if (*databaseName != '*') {
      daemon_printf( "550 invalid database, use SHOW DB for list\n" );
      return;
   }

 nomatch:
   daemon_printf( "552 no match for %s %s \"%s\"\n",
		  databaseName, strategy, word );
   daemon_log( "nomatch %s %s \"%s\"\n", databaseName, strategy, word );
}

static void daemon_show_db( int argc, char **argv )
{
   int          count;
   dictDatabase *db;
   
   if (argc != 2) {
      daemon_printf( "501 syntax error, illegal parameters\n" );
      return;
   }

   if (!(count = lst_length( DictConfig->dbl ))) {
      daemon_printf( "554 no databases present\n" );
   } else {
      daemon_printf( "210 %d databases present - text follows\n", count );
      reset_databases();
      while ((db = next_database("*"))) {
	 daemon_printf( "%s \"%s\"\n", db->databaseName, db->databaseShort );
      }
      daemon_printf( ".\n" );
   }
}

static void daemon_show_strat( int argc, char **argv )
{
   int          count;
   dictDatabase *db;
   
   if (argc != 2) {
      daemon_printf( "501 syntax error, illegal parameters\n" );
      return;
   }

   if (!(count = lst_length( DictConfig->dbl ))) {
      daemon_printf( "554 no databases present\n" );
   } else {
      daemon_printf( "210 %d databases present - text follows\n", count );
      reset_databases();
      while ((db = next_database("*"))) {
	 daemon_printf( "%s \"%s\"\n", db->databaseName, db->databaseShort );
      }
      daemon_printf( ".\n" );
   }
}

static void daemon_show_info( int argc, char **argv )
{
   if (argc != 3) {
      daemon_printf( "501 syntax error, illegal parameters\n" );
      return;
   }
}

static void daemon_show( int argc, char **argv )
{
   daemon_printf( "501 syntax error, illegal parameters\n" );
}

static void daemon_client( int argc, char **argv )
{
}

static void daemon_auth( int argc, char **argv )
{
}

static void daemon_status( int argc, char **argv )
{
   daemon_ok( "213 status", "t" );
}

static void daemon_help( int argc, char **argv )
{
   daemon_printf( "100 help text follows\n" );
   daemon_text( "define word\n" );
}

static void daemon_quit( int argc, char **argv )
{
   daemon_ok( "221 bye", "t" );
   daemon_terminate( 0, __FUNCTION__ );
}


#if 0
static int daemon_show( const char *what )
{
   int          count;
   int          i;
   dictDatabase *db;
   
   if (!strcmp(what,"databases") || !strcmp(what,"db")) {
      count = lst_length( DictConfig->dbl );
      daemon_printf( "databases %d\n", count );
      for (i = 1; i <= count; i++) {
	 db = lst_nth_get( DictConfig->dbl, i );
	 daemon_printf( "database %s \"%s\"\n",
			db->databaseName, db->databaseShort );
      }
      daemon_status("259 ok", "c");
      return count;
   } else if (!strcmp(what,"strategies") || !strcmp(what,"strat")) {
      daemon_printf( "strategies %d\n", STRATEGIES );
      for (i = 0; i < STRATEGIES; i++) {
	 daemon_printf( "strategy %s \"%s\"\n",
			strategyInfo[i].name,
			strategyInfo[i].description );
      }
      daemon_status("259 ok", "c");
      return STRATEGIES;
   }
   
   daemon_printf("500 Illegal show command" );
   return 0;
}
#endif

#if 0
static int daemon_define( const char *word, const char *databaseName )
{
#if 0
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
	    daemon_printf( "definitions %d\n", matches );
	    daemon_dump_defs( list, db );
	    daemon_status();
	 } else {
            if (!databaseName) continue; /* keep searching */
	    daemon_error( 0, "No match" );
	 }
	 if (list) dict_destroy_list( list );
	 daemon_log( "define %s \"%.80s\" %d\n",
		     db->databaseName, word, matches );
	 return matches;
      }
   }
   daemon_log( "no match \"%.80s\" %s\n", word, databaseName );
   daemon_define("unknown",NULL);
   if (databaseName)
      daemon_error( 0, "Unknown database \"%s\"", databaseName );
   else 
      daemon_error( 0, "No match" );
   return 0;
#else
   daemon_status("259 ok","c");
#endif
   return 0;
}
#endif

#if 0
static int daemon_match( const char *word,
			 const char *strategy, const char *databaseName )
{
#if 0
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
		  daemon_printf( "matches %d\n", matches );
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
#else
   daemon_status("259 ok","c");
#endif
   return 0;
}
#endif

int dict_daemon( int s, struct sockaddr_in *csin, char ***argv0 )
{
   char           buf[4096];
   int            count;
   struct hostent *h;
   arg_List       cmdline;
   int            argc;
   char           **argv;
   const char     *hostname;
   int            port;
   void           (*command)(int, char **);
      
   port     = ntohs(csin->sin_port);
   hostname = str_find( inet_ntoa(csin->sin_addr) );
   if ((h = gethostbyaddr((void *)&csin->sin_addr,
			  sizeof(csin->sin_addr), csin->sin_family))) {
      hostname = str_find( h->h_name );
   }

   daemonS           = s;
   daemonHostname    = hostname;
   daemonPort        = port;
   
   _dict_defines     = 0;
   _dict_matches     = 0;
   _dict_comparisons = 0;
   
   tim_start( "t" );
   daemon_log( "connected\n" );

	      
   daemon_banner();

   while ((count = daemon_read( buf, 4000 )) >= 0) {
      tim_start( "c" );
      if (!count) {
#if 0
         daemon_ok( "259 ok", "c" );
#endif
	 continue;
      }
      
      cmdline = arg_argify(buf,0);
      arg_get_vector( cmdline, &argc, &argv );
      if ((command = lookup_command(argv[0]))) {
	 command(argc, argv);
      } else {
	 daemon_printf( "500 unknown command\n" );
      }
      arg_destroy(cmdline);
   }
   daemon_terminate( 0, "disconnect" );
   return 0;
}
