/* daemon.c -- Server daemon
 * Created: Fri Feb 28 18:17:56 1997 by faith@cs.unc.edu
 * Revised: Wed May 21 22:27:35 1997 by faith@acm.org
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
 * $Id: daemon.c,v 1.15 1997/05/22 02:40:29 faith Exp $
 * 
 */

#include "dictd.h"
#include <ctype.h>
#include <setjmp.h>
#include "md5.h"
#include "regex.h"

static int          _dict_defines, _dict_matches;
static int          daemonS;
static const char   *daemonHostname;
static const char   *daemonIP;
static int          daemonPort;
static lst_Position databasePosition;
static char         daemonStamp[256];
static jmp_buf      env;

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

#define MAXARGCS 3
static struct {
   int        argc;
   const char *name[MAXARGCS];
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

static void *(lookup_command)( int argc, char **argv )
{
   int i, j;
   int err;
   
   for (i = 0; i < COMMANDS; i++) {
      if (argc >= commandInfo[i].argc) {
	 for (err = 0, j = 0; j < commandInfo[i].argc; j++) {
	    if (strcasecmp(argv[j], commandInfo[i].name[j])) {
	       err = 1;
	       break;
	    }
	 }
	 if (!err) return commandInfo[i].f;
      }
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
	 do {
	    db = lst_get_position( databasePosition );
	    databasePosition = lst_next_position( databasePosition );
	 } while (db && !db->available);
      }
      return db;
   } else {
      while (databasePosition) {
         db = lst_get_position( databasePosition );
	 databasePosition = lst_next_position( databasePosition );
         if (db && !strcmp(db->databaseName,name)) {
	    if (db->available) return db;
	    else               return NULL;
	 }
      }
      return NULL;
   }
}

static int count_databases( void )
{
   int count = 0;
   
   reset_databases();
   while (next_database("*")) ++count;
   return count;
}

static int daemon_check_list( const char *user, lst_List acl )
{
   lst_Position p;
   dictAccess   *a;
   char         regbuf[256];
   char         erbuf[100];
   int          err;
   const char   *s;
   char         *d;
   regex_t      re;

   if (!acl) return DICT_ALLOW;
   for (p = lst_init_position(acl); p; p = lst_next_position(p)) {
      a = lst_get_position(p);
      switch (a->type) {
      case DICT_DENY:
      case DICT_ALLOW:
      case DICT_AUTHONLY:
	 for (d = regbuf, s = a->spec; s && *s; ++s) {
	    switch (*s) {
	    case '*': *d++ = '.';  *d++ = '*'; break;
	    case '.': *d++ = '\\'; *d++ = '.'; break;
	    case '?': *d++ = '\\'; *d++ = '?'; break;
	    default:  *d++ = *s;               break;
	    }
	 }
	 *d = '\0';
	 if ((err = regcomp(&re, regbuf, REG_ICASE|REG_NOSUB))) {
	    regerror(err, &re, erbuf, sizeof(erbuf));
	    log_info( "regcomp(%s): %s\n", regbuf, erbuf );
	    return DICT_DENY;	/* Err on the side of safety */
	 }
	 if (!regexec(&re, daemonHostname, 0, NULL, 0)
	     || !regexec(&re, daemonIP, 0, NULL, 0)) {
	    PRINTF(DBG_AUTH,
		   ("Match %s with %s/%s\n",
		    a->spec,daemonHostname,daemonIP));
	    regfree(&re);
	    return a->type;
	 }
	 regfree(&re);
	 PRINTF(DBG_AUTH,
		("No match (%s with %s/%s)\n",
		 a->spec,daemonHostname,daemonIP));
	 break;
      case DICT_USER:
	 if (!strcmp(user,a->spec)) return DICT_ALLOW;
      case DICT_GROUP:
	 break;
      }
   }
   return DICT_DENY;
}

static int daemon_check_auth( const char *user )
{
   lst_Position p;
   lst_List     dbl = DictConfig->dbl;
   dictDatabase *db;
   
   switch (daemon_check_list( user, DictConfig->acl )) {
   default:
   case DICT_DENY:
      return 1;
   case DICT_AUTHONLY:
      if (!user) return 0;
   case DICT_ALLOW:
      for (p = lst_init_position(dbl); p; p = lst_next_position(p)) {
	 db = lst_get_position(p);
	 switch (daemon_check_list(user, db->acl)) {
	 case DICT_ALLOW: db->available = 1; continue;
	 default:         db->available = 0; continue;
	 }
      }
      break;
   }
   return 0;
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
   alarm(0);
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
   
   longjmp(env,1);
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
   return net_read( daemonS, buf, count );
}

static void daemon_ok( int code, const char *string, const char *timer )
{
   if (!timer) {
      daemon_printf("%d %s\n", code, string);
   } else {
      daemon_printf("%d %s [d/m/c = %d/%d/%d; %0.3fr %0.3fu %0.3fs]\n",
                    code,
                    string,
                    _dict_defines,
                    _dict_matches,
                    _dict_comparisons,
                    tim_get_real( timer ),
                    tim_get_user( timer ),
                    tim_get_system( timer ));
   }
}

static int dump_def( const void *datum, void *arg )
{
   char         *buf;
   dictWord     *dw = (dictWord *)datum;
   dictDatabase *db = (dictDatabase *)arg;

   buf = dict_data_read( db->data, dw->start, dw->end,
			 db->prefilter, db->postfilter );

   daemon_printf( "%d \"%s\" %s \"%s\" - text follows\n",
		  CODE_DEFINITION_FOLLOWS,
                  dw->word,
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
   daemon_printf( ".\n" );
}

static void daemon_banner( void )
{
   time_t         t;

   time(&t);

   sprintf( daemonStamp, "<%d.%d.%ld@%s>", 
	    _dict_forks,
	    getpid(),
	    t,
	    net_hostname() );
   daemon_printf( "%d %s %s %s\n",
		  CODE_HELLO,
                  net_hostname(),
		  dict_get_banner(),
		  daemonStamp );
}

static void daemon_define( int argc, char **argv )
{
   lst_List       list;
   dictDatabase   *db;
   int            matches = 0;
   char           *word;
   const char     *databaseName;
   int            none = 1;

   switch (argc) {
   case 2:  databaseName = "*";     word = argv[1]; break;
   case 3:  databaseName = argv[1]; word = argv[2]; break;
   default:
      daemon_printf( "% syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );
      return;
   }

   reset_databases();
   while ((db = next_database(databaseName))) {
      none = 0;
      list = dict_search_database( word, db, DICT_EXACT );
      if (list && (matches = lst_length(list)) > 0) {
	 _dict_defines += matches;
	 daemon_printf( "%d %d definitions retrieved - definitions follow\n",
			CODE_DEFINITIONS_FOUND,
			matches );
	 daemon_dump_defs( list, db );
	 daemon_ok( CODE_DEFINITIONS_FINISHED, "ok", "c" );
	 daemon_log("define %s \"%s\" %d\n", db->databaseName, word, matches);
      } else {
	 if (*databaseName == '*') continue; /* keep searching */
	 goto nomatch;
      }
      if (list) dict_destroy_list( list );
      if (matches) return;
   }
   
   if (none || *databaseName != '*') {
      daemon_printf( "%d invalid database, use SHOW DB for list\n",
		     CODE_INVALID_DB );
      return;
   }

 nomatch:
   daemon_ok( CODE_NO_MATCH, "no match", "c" );
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
   int            none = 1;

   switch (argc) {
   case 2:  databaseName = "*";     strategy = "lev";   word = argv[1]; break;
   case 3:  databaseName = "*";     strategy = argv[1]; word = argv[2]; break;
   case 4:  databaseName = argv[1]; strategy = argv[2]; word = argv[3]; break;
   default:
      daemon_printf( "%d syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );
      return;
   }

   if ((strategyNumber = lookup_strategy(strategy)) < 0) {
      daemon_printf( "%d invalid strategy, use SHOW STRAT for a list\n",
		     CODE_INVALID_STRATEGY );
      return;
   }

   reset_databases();
   while ((db = next_database(databaseName))) {
      none = 0;
      list = dict_search_database( word, db, strategyNumber );
      if (list && (matches = lst_length(list)) > 0) {
	 _dict_matches += matches;
	 daemon_printf( "%d %d matches found - text follow\n",
			CODE_MATCHES_FOUND, matches );
	 daemon_dump_matches( list );
	 daemon_ok( CODE_MATCHES_FINISHED, "ok", "c" );
	 daemon_log("match %s %s \"%s\" %d\n",
		    db->databaseName, strategy, word, matches);
      } else {
	 if (*databaseName == '*') continue; /* keep searching */
	 goto nomatch;
      }
      if (list) dict_destroy_list( list );
      if (matches) return;
   }
   
   if (none || *databaseName != '*') {
      daemon_printf( "%d invalid database, use SHOW DB for list\n",
		     CODE_INVALID_DB );
      return;
   }

 nomatch:
   daemon_ok( CODE_NO_MATCH, "no match for ", "c" );
   daemon_log( "nomatch %s %s \"%s\"\n", databaseName, strategy, word );
}

static void daemon_show_db( int argc, char **argv )
{
   int          count;
   dictDatabase *db;
   
   if (argc != 2) {
      daemon_printf( "%d syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );
      return;
   }

   if (!(count = count_databases())) {
      daemon_printf( "%d no databases present\n", CODE_NO_DATABASES );
   } else {
      daemon_printf( "%d %d databases present - text follows\n",
		     CODE_DATABASE_LIST, count );
      reset_databases();
      while ((db = next_database("*"))) {
	 daemon_printf( "%s \"%s\"\n",
			db->databaseName, db->databaseShort );
      }
      daemon_printf( ".\n" );
   }
}

static void daemon_show_strat( int argc, char **argv )
{
   int i;
   
   if (argc != 2) {
      daemon_printf( "%d syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );
      return;
   }

   if (!STRATEGIES) {
      daemon_printf( "%d no strategies available\n", CODE_NO_STRATEGIES );
   } else {
      daemon_printf( "%d %d databases present - text follows\n",
		     CODE_STRATEGY_LIST, STRATEGIES );
      for (i = 0; i < STRATEGIES; i++) {
	 daemon_printf( "%s \"%s\"\n",
			strategyInfo[i].name, strategyInfo[i].description );
      }
      daemon_printf( ".\n" );
   }
}

static void daemon_show_info( int argc, char **argv )
{
   if (argc != 3) {
      daemon_printf( "%d syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );
      return;
   }
}

static void daemon_show( int argc, char **argv )
{
   daemon_printf( "%d syntax error, illegal parameters\n",
		  CODE_ILLEGAL_PARAM );
}

static void daemon_client( int argc, char **argv )
{
   if (argc != 2)
      daemon_printf( "%d syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );

   switch (argc) {
   case 0:
   case 1:
      break;			/* do nothing */
   case 2:
      daemon_log( "client: %.80s\n", argv[1] );
      break;
   case 3:
      daemon_log( "client: %.80s %.80s\n", argv[1], argv[2] );
      break;
   default:
      daemon_log( "client: %.80s %.80s %.80s\n", argv[1], argv[2], argv[3] );
      break;
   }
   if (argc == 2) daemon_ok( CODE_OK, "ok", NULL );
}

static void daemon_auth( int argc, char **argv )
{
   char              *buf;
   hsh_HashTable     h = DictConfig->usl;
   const char        *secret;
   struct MD5Context ctx;
   unsigned char     digest[16];
   char              hex[33];
   int               i;
   
   if (argc != 3)
      daemon_printf( "%d syntax error, illegal parameters\n",
		     CODE_ILLEGAL_PARAM );
   if (!h || !(secret = hsh_retrieve(h, argv[1]))) {
      daemon_printf( "%d auth denied\n", CODE_AUTH_DENIED );
      return;
   }
   
   buf = alloca(strlen(daemonStamp) + strlen(secret) + 10);
   sprintf( buf, "%s%s", daemonStamp, secret );

   MD5Init(&ctx);
   MD5Update(&ctx, buf, strlen(buf));
   MD5Final(digest, &ctx);

   for (i = 0; i < 16; i++) sprintf( hex+2*i, "%02x", digest[i] );
   hex[32] = '\0';

   PRINTF(DBG_AUTH,("Got %s expected %s\n", argv[2], hex ));

   if (strcmp(hex,argv[2])) {
      daemon_printf( "%d auth denied\n", CODE_AUTH_DENIED );
   } else {
      daemon_printf( "%d authenticated\n", CODE_AUTH_OK );
      daemon_check_auth( argv[1] );
   }
}

static void daemon_status( int argc, char **argv )
{
   daemon_ok( CODE_STATUS, "status", "t" );
}

static void daemon_help( int argc, char **argv )
{
   daemon_printf( "%d help text follows\n", CODE_HELP );
   daemon_text(
    "DEFINE database word -- look up word in database\n"
    "DEFINE word          -- look up word in all databases until found\n"
    "MATCH database strategy word -- match word in database using strategy\n"
    "MATCH strategy word          -- match word in all databases until found\n"
    "MATCH word                   -- match word in using lev strategy\n"
    "SHOW DB              -- list all accessible databases\n"
    "SHOW DATABASES       -- list all accessible databases\n"
    "SHOW STRAT           -- list available matching strategies\n"
    "SHOW STRATEGIES      -- list available matching strategies\n"
    "SHOW INFO database   -- provide information about the database\n"
    "CLIENT info          -- identify client to server\n"
    "AUTH user string     -- provide authentication information\n"
    "STATUS               -- display timing information\n"
    "HELP                 -- display this help information\n"
    "QUIT                 -- terminate connection\n"
   );
}

static void daemon_quit( int argc, char **argv )
{
   daemon_ok( CODE_GOODBYE, "bye", "t" );
   daemon_terminate( 0, __FUNCTION__ );
}

int dict_daemon( int s, struct sockaddr_in *csin, char ***argv0, int delay,
		 int error )
{
   char           buf[4096];
   int            count;
   struct hostent *h;
   arg_List       cmdline;
   int            argc;
   char           **argv;
   void           (*command)(int, char **);
      
   if (setjmp(env)) return 0;
   
   daemonPort = ntohs(csin->sin_port);
   daemonIP   = str_find( inet_ntoa(csin->sin_addr) );
   if ((h = gethostbyaddr((void *)&csin->sin_addr,
			  sizeof(csin->sin_addr), csin->sin_family))) {
      daemonHostname = str_find( h->h_name );
   } else
      daemonHostname = daemonIP;

   daemonS           = s;
   
   _dict_defines     = 0;
   _dict_matches     = 0;
   _dict_comparisons = 0;

   tim_start( "t" );
   daemon_log( "connected\n" );

   if (error) {
      daemon_printf( "%d temporarily unavailable\n",
		     CODE_TEMPORARILY_UNAVAILABLE );
      daemon_terminate( 0, "temporarily unavailable" );
   }

   if (daemon_check_auth( NULL )) {
      daemon_printf( "%d access denied\n", CODE_ACCESS_DENIED );
      daemon_terminate( 0, "access denied" );
   }
	      
   daemon_banner();

   alarm(delay);
   while ((count = daemon_read( buf, 4000 )) >= 0) {
      alarm(0);
      tim_start( "c" );
      if (!count) {
#if 0
         daemon_ok( CODE_OK, "ok", "c" );
#endif
	 continue;
      }
      
      cmdline = arg_argify(buf,0);
      arg_get_vector( cmdline, &argc, &argv );
      if ((command = lookup_command(argc,argv))) {
	 command(argc, argv);
      } else {
	 daemon_printf( "%d unknown command\n", CODE_SYNTAX_ERROR );
      }
      arg_destroy(cmdline);
      alarm(delay);
   }
   printf( "%d %d\n", count, errno );
   daemon_terminate( 0, "disconnect" );
   return 0;
}
