/* dictd.c -- 
 * Created: Fri Feb 21 20:09:09 1997 by faith@cs.unc.edu
 * Revised: Sun Jan 18 18:40:01 1998 by faith@acm.org
 * Copyright 1997, 1998 Rickard E. Faith (faith@acm.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: dictd.c,v 1.33 1998/01/19 03:37:20 faith Exp $
 * 
 */

#include "dictd.h"
#include "servparse.h"

#define PERSISTENT 0		/* DO *NOT* CHANGE!!!!!  Should be 0.
				   I didn't have time to implement the
                                   persistent-daemon support.  It doesn't
                                   work.  Don't turn it on.  */

#define MAXPROCTITLE 2048       /* Maximum amount of proc title we'll use. */
#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))

#if HAVE_SEMGET
#if PERSISTENT
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#endif
#else
#undef PERSISTENT
#define PERSISTENT 0
#endif

extern int        yy_flex_debug;
static int        _dict_daemon;
static int        _dict_persistent;
static int        _dict_daemon_count;
static int        _dict_daemon_limit        = DICT_DAEMON_LIMIT;
static int        _dict_markTime;
static char       *_dict_argvstart;
static int        _dict_argvlen;

#if PERSISTENT
static int        _dict_persistent_count;
static int        _dict_persistent_limit    = DICT_PERSISTENT_LIMIT;
static int        _dict_persistent_prestart = DICT_PERSISTENT_PRESTART;
static pid_t      *_dict_persistent_pids;
static int        _dict_sem;
#endif

       int        _dict_forks;
       dictConfig *DictConfig;


void dict_initsetproctitle( int argc, char **argv, char **envp )
{
   int i;

   _dict_argvstart = argv[0];
   
   for (i = 0; envp[i]; i++) continue;
   if (i)
      _dict_argvlen = envp[i-1] + strlen(envp[i-1]) - _dict_argvstart;
   else
      _dict_argvlen = argv[argc-1] + strlen(argv[argc-1]) - _dict_argvstart;
   argv[1] = NULL;
}

void dict_setproctitle( const char *format, ... )
{
   va_list ap;
   int     len;
   char    buf[MAXPROCTITLE];

   va_start( ap, format );
   vsprintf( buf, format, ap );
   va_end( ap );
   if ((len = strlen(buf)) > MAXPROCTITLE-1)
      err_fatal( __FUNCTION__, "buffer overflow (%d)\n", len );
   buf[ MIN(_dict_argvlen,MAXPROCTITLE) - 1 ] = '\0';
   strcpy( _dict_argvstart, buf );
   memset( _dict_argvstart+len, ' ', _dict_argvlen-len );
}

const char *dict_format_time( double t )
{
   static int  current = 0;
   static char buf[10][128];	/* Rotate 10 buffers */
   static char *this;
   long int    s, m, h, d;

   this = buf[current];
   if (++current >= 10) current = 0;

   if (t < 600) {
      sprintf( this, "%0.3f", t );
   } else {
      s = (long int)t;
      d = s / (3600*24);
      s -= d * 3600 * 24;
      h = s / 3600;
      s -= h * 3600;
      m = s / 60;
      s -= m * 60;

      if (d)
	 sprintf( this, "%ld+%02ld:%02ld:%02ld", d, h, m, s );
      else if (h)
	 sprintf( this, "%02ld:%02ld:%02ld", h, m, s );
      else
	 sprintf( this, "%02ld:%02ld", m, s );
   }

   return this;
}

static void reaper( int dummy )
{
#if defined(__sparc__) && defined(__svr4__)
   int        status;
#else
   union wait status;
#endif
   pid_t      pid;
   int        flag = 0;

   while ((pid = wait3(&status, WNOHANG, NULL)) > 0) {
#if PERSISTENT
      int        i;
      
      for (i = 0; i < _dict_persistent_limit; i++) {
	 if (_dict_persistent_pids[i] == pid) {
	    _dict_persistent_pids[i] = 0;
	    ++flag;
	    break;
	 }
      }

      if (flag) {
	 --_dict_persistent_count;
      } else
#endif
	 --_dict_daemon_count;
      
      if (flg_test(LOG_SERVER))
         log_info( ":I: Reaped %d%s%s%s\n",
                   pid,
                   flag ? " [persistent]": "",
                   _dict_daemon ? " IN CHILD": "",
                   _dict_persistent ? " IN PERSISTENT DAEMON": "" );
   }
}

#if PERSISTENT
static int start_persistent( void )
{
   pid_t pid;
   int   i;
   
   ++_dict_forks;
   switch ((pid = fork())) {
   case 0:
      ++_dict_daemon;
      ++_dict_persistent;
      break;
   case -1:
      log_info( ":E: Unable to fork daemon\n" );
      alarm(10);		/* Can't use sleep() here */
      pause();
      break;
   default:
      ++_dict_persistent_count;
      if (flg_test(LOG_SERVER))
         log_info( ":I: Forked %d, daemon %d\n", pid, _dict_persistent_count );
      for (i = 0; i < _dict_persistent_limit; i++) {
	 if (!_dict_persistent_pids[i]) {
	    _dict_persistent_pids[i] = pid;
	    break;
	 }
      }
      break;
   }
   return pid;
}
#endif

static int start_daemon( void )
{
   pid_t pid;
   
   ++_dict_forks;
   switch ((pid = fork())) {
   case 0:
      ++_dict_daemon;
      break;
   case -1:
      log_info( ":E: Unable to fork daemon\n" );
      alarm(10);		/* Can't use sleep() here */
      pause();
      break;
   default:
      ++_dict_daemon_count;
      if (flg_test(LOG_SERVER)) log_info( ":I: Forked %d\n", pid );
      break;
   }
   return pid;
}

static void handler( int sig )
{
   const char *name = NULL;
   time_t     t;
   
   switch (sig) {
   case SIGHUP:  name = "SIGHUP";  break;
   case SIGINT:  name = "SIGINT";  break;
   case SIGQUIT: name = "SIGQUIT"; break;
   case SIGILL:  name = "SIGILL";  break;
   case SIGTRAP: name = "SIGTRAP"; break;
   case SIGTERM: name = "SIGTERM"; break;
   case SIGPIPE: name = "SIGPIPE"; break;
   case SIGALRM: name = "SIGALRM"; break;
   }

   if (_dict_daemon) {
      daemon_terminate( sig, name );
   } else {
      tim_stop( "dictd" );
      if (sig == SIGALRM && _dict_markTime > 0) {
	 time(&t);
	 log_info( ":T: %24.24s; %d/%d %sr %su %ss\n",
		   ctime(&t),
		   _dict_daemon_count,
		   _dict_forks,
		   dict_format_time( tim_get_real( "dictd" ) ),
		   dict_format_time( tim_get_user( "dictd" ) ),
		   dict_format_time( tim_get_system( "dictd" ) ) );
	 alarm(_dict_markTime);
	 return;
      }
      if (name) {
	 log_info( ":I: %s: c/f = %d/%d; %sr %su %ss\n",
		   name,
		   _dict_comparisons,
		   _dict_forks,
		   dict_format_time( tim_get_real( "dictd" ) ),
		   dict_format_time( tim_get_user( "dictd" ) ),
		   dict_format_time( tim_get_system( "dictd" ) ) );
      } else {
	 log_info( ":I: Signal %d: c/f = %d/%d; %sr %su %ss\n",
		   sig,
		   _dict_comparisons,
		   _dict_forks,
		   dict_format_time( tim_get_real( "dictd" ) ),
		   dict_format_time( tim_get_user( "dictd" ) ),
		   dict_format_time( tim_get_system( "dictd" ) ) );
      }
   }
   if (!dbg_test(DBG_NOFORK) || sig != SIGALRM)
      exit(sig+128);
}

static void setsig( int sig, void (*f)(int) )
{
   struct sigaction   sa;
   
   sa.sa_handler = f;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sigaction(sig, &sa, NULL);
}

struct access_print_struct {
   FILE *s;
   int  offset;
};

static int access_print( const void *datum, void *arg )
{
   dictAccess                 *a     = (dictAccess *)datum;
   struct access_print_struct *aps   = (struct access_print_struct *)arg;
   FILE                       *s     = aps->s;
   int                        offset = aps->offset;
   int                        i;
   const char                 *desc;

   for (i = 0; i < offset; i++) fputc( ' ', s );
   switch (a->type) {
   case DICT_DENY:     desc = "deny";     break;
   case DICT_ALLOW:    desc = "allow";    break;
   case DICT_AUTHONLY: desc = "authonly"; break;
   case DICT_USER:     desc = "user";     break;
   case DICT_GROUP:    desc = "group";    break; /* Not implemented. */
   default:            desc = "unknown";  break;
   }
   fprintf( s, "%s %s\n", desc, a->spec );
   return 0;
}

static void acl_print( FILE *s, lst_List l, int offset)
{
   struct access_print_struct aps;
   int                        i;

   aps.s      = s;
   aps.offset = offset + 3;
   
   for (i = 0; i < offset; i++) fputc( ' ', s );
   fprintf( s, "access {\n" );
   lst_iterate_arg( l, access_print, &aps );
   for (i = 0; i < offset; i++) fputc( ' ', s );
   fprintf( s, "}\n" );
}

static int user_print( const void *key, const void *datum, void *arg )
{
   const char *username = (const char *)key;
   const char *secret   = (const char *)datum;
   FILE       *s        = (FILE *)arg;

   if (dbg_test(DBG_AUTH))
      fprintf( s, "user %s %s\n", username, secret );
   else
      fprintf( s, "user %s *\n", username );
   return 0;
}

static int config_print( const void *datum, void *arg )
{
   dictDatabase *db = (dictDatabase *)datum;
   FILE         *s  = (FILE *)arg;

   fprintf( s, "database %s {\n", db->databaseName );
   if (db->dataFilename)
      fprintf( s, "   data       %s\n", db->dataFilename );
   if (db->indexFilename)
      fprintf( s, "   index      %s\n", db->indexFilename );
   if (db->filter)
      fprintf( s, "   filter     %s\n", db->filter ); /* Not implemented. */
   if (db->prefilter)
      fprintf( s, "   prefilter  %s\n", db->prefilter );
   if (db->postfilter)
      fprintf( s, "   postfilter %s\n", db->postfilter );
   if (db->databaseShort)
      fprintf( s, "   name       %s\n", db->databaseShort );
   if (db->acl) acl_print( s, db->acl, 3 );
   fprintf( s, "}\n" );
   return 0;
}

static void dict_config_print( FILE *stream, dictConfig *c )
{
   FILE *s = stream ? stream : stderr;

   if (c->acl) acl_print( s, c->acl, 0 );
   lst_iterate_arg( c->dbl, config_print, s );
   if (c->usl) hsh_iterate_arg( c->usl, user_print, s );
}

static const char *get_entry_info( dictDatabase *db, const char *entryName )
{
   dictWord *dw;
   lst_List list = lst_create();
   char     *pt;
   
   if (!dict_search_database( list, entryName, db, DICT_EXACT )) {
      lst_destroy( list );
      return NULL;
   }
      
   dw = lst_nth_get( list, 1 );
				/* Don't ever free this */
   pt = dict_data_read( db->data, dw->start, dw->end,
			db->prefilter, db->postfilter );
   pt += strlen(entryName) + 1;
   while (*pt == ' ' || *pt == '\t') ++pt;
   pt[ strlen(pt) - 1 ] = '\0';
   dict_destroy_list( list );
   return pt;
}

static int init_database( const void *datum )
{
   dictDatabase *db = (dictDatabase *)datum;

   db->index = dict_index_open( db->indexFilename );
   db->data  = dict_data_open( db->dataFilename, 0 );

   if (!db->databaseShort)
      db->databaseShort = get_entry_info( db, DICT_SHORT_ENTRY_NAME );
   else if (*db->databaseShort == '@')
      db->databaseShort = get_entry_info( db, db->databaseShort + 1 );
   if (!db->databaseShort) db->databaseShort = str_find( db->databaseName );
   
   log_info( ":I: %-12.12s %s\n",
	     db->databaseName, db->databaseShort );
   PRINTF(DBG_INIT,
	  ("%s \"%s\" initialized\n",db->databaseName,db->databaseShort));
   return 0;
}

static int log_database_info( const void *datum )
{
   dictDatabase  *db = (dictDatabase *)datum;
   const char    *pt;
   unsigned long headwords = 0;

   for (pt = db->index->start; pt < db->index->end; pt++)
      if (*pt == '\n') ++headwords;
   db->index->headwords = headwords;
   
   log_info( ":I: %-12.12s %12lu %12lu %12lu %12lu\n",
	     db->databaseName, headwords,
	     db->index->size, db->data->size, db->data->length );

   return 0;
}

static void dict_init_databases( dictConfig *c )
{
   lst_iterate( c->dbl, init_database );
   lst_iterate( c->dbl, log_database_info );
}

static int dump_def( const void *datum, void *arg )
{
   char         *buf;
   dictWord     *dw = (dictWord *)datum;
   dictDatabase *db = (dictDatabase *)arg;

   buf = dict_data_read( db->data, dw->start, dw->end,
			 db->prefilter, db->postfilter );
   printf( "%s\n", buf );
   xfree( buf );
   return 0;
}

static void dict_dump_defs( lst_List list, dictDatabase *db )
{
   lst_iterate_arg( list, dump_def, db );
}

static const char *id_string( const char *id )
{
   static char buffer[BUFFERSIZE];
   arg_List    a;
   char        *pt, *dot;

   sprintf( buffer, "%s", DICT_VERSION );
   pt = buffer + strlen( buffer );

   a = arg_argify( id, 0 );
   if (arg_count(a) >= 2) {
      if ((dot = strchr( arg_get(a, 2), '.' )))
	 sprintf( pt, ".%s", dot+1 );
      else
	 sprintf( pt, ".%s", arg_get( a, 2 ) );
   }
   arg_destroy( a );
   
   return buffer;
}

const char *dict_get_banner( int shortFlag )
{
   static char    *shortBuffer = NULL;
   static char    *longBuffer = NULL;
   const char     *id = "$Id: dictd.c,v 1.33 1998/01/19 03:37:20 faith Exp $";
   struct utsname uts;
   
   if (shortFlag && shortBuffer) return shortBuffer;
   if (!shortFlag && longBuffer) return longBuffer;
   
   uname( &uts );
   
   shortBuffer = xmalloc(256);
   sprintf( shortBuffer, "%s %s", err_program_name(), id_string( id ) );
   
   longBuffer = xmalloc(256);
   sprintf( longBuffer,
	    "%s %s/rf on %s %s", err_program_name(), id_string( id ),
	    uts.sysname,
	    uts.release );
   
   if (shortFlag) return shortBuffer;
   return longBuffer;
}

static void banner( void )
{
   fprintf( stderr, "%s\n", dict_get_banner(0) );
   fprintf( stderr,
	    "Copyright 1997, 1998 Rickard E. Faith (faith@cs.unc.edu)\n" );
}

static void license( void )
{
   static const char *license_msg[] = {
     "",
     "This program is free software; you can redistribute it and/or modify it",
     "under the terms of the GNU General Public License as published by the",
     "Free Software Foundation; either version 1, or (at your option) any",
     "later version.",
     "",
     "This program is distributed in the hope that it will be useful, but",
     "WITHOUT ANY WARRANTY; without even the implied warranty of",
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU",
     "General Public License for more details.",
     "",
     "You should have received a copy of the GNU General Public License along",
     "with this program; if not, write to the Free Software Foundation, Inc.,",
     "675 Mass Ave, Cambridge, MA 02139, USA.",
   0 };
   const char        **p = license_msg;
   
   banner();
   while (*p) fprintf( stderr, "   %s\n", *p++ );
}
    
static void help( void )
{
   static const char *help_msg[] = {
      "-h --help             give this help",
      "   --license          display software license",
      "-v --verbose          verbose mode",
      "-V --version          display version number",
      "-p --port <port>      port number",
      "   --delay <seconds>  client timeout in seconds",
      "   --depth <length>   TCP/IP queue depth",
      "   --limit <children> maximum simultaneous children",
      "-c --config <file>    configuration file",
      "-l --log <option>     select logging option",
      "-s --syslog           log via syslog(3)",
      "-L --logfile <file>   log via specified file",
      "-m --mark <minutes>   how often should a timestamp be logged",
      "-d --debug <option>   select debug option",
      "-t --test <word>      self test -- lookup word",
      "   --ftest <file>     self test -- lookup all words in file",
      "-f --force            force startup even if daemon running",
      0 };
   const char        **p = help_msg;

   banner();
   while (*p) fprintf( stderr, "%s\n", *p++ );
}

static void set_minimal( void )
{
   flg_set(flg_name(LOG_FOUND));
   flg_set(flg_name(LOG_NOTFOUND));
   flg_set(flg_name(LOG_STATS));
   flg_set(flg_name(LOG_CLIENT));
   flg_set("-min");
}

int main( int argc, char **argv, char **envp )
{
   int                childSocket;
   int                masterSocket;
   struct sockaddr_in csin;
   int                c;
   time_t             startTime;
   int                alen         = sizeof(csin);
   const char         *service     = DICT_DEFAULT_SERVICE;
   const char         *configFile  = DICT_CONFIG_PATH DICTD_CONFIG_NAME;
   int                detach       = 1;
   const char         *testWord    = NULL;
   const char         *testFile    = NULL;
   const char         *logFile     = NULL;
   int                delay        = DICT_DEFAULT_DELAY;
   int                depth        = DICT_QUEUE_DEPTH;
   int                useSyslog    = 0;
   int                logOptions   = 0;
   int                forceStartup = 0;
#if PERSISTENT
   struct sembuf      sembuf;
#endif
   struct option      longopts[]  = {
      { "verbose",  0, 0, 'v' },
      { "version",  0, 0, 'V' },
      { "debug",    1, 0, 'd' },
      { "port",     1, 0, 'p' },
      { "config",   1, 0, 'c' },
      { "help",     0, 0, 'h' },
      { "license",  0, 0, 500 },
      { "test",     1, 0, 't' },
      { "ftest",    1, 0, 501 },
      { "log",      1, 0, 'l' },
      { "syslog",   0, 0, 's' },
      { "mark",     1, 0, 'm' },
      { "delay",    1, 0, 502 },
      { "depth",    1, 0, 503 },
      { "limit",    1, 0, 504 },
      { "force",    1, 0, 'f' },
#if PERSISTENT
      { "perlimit", 1, 0, 505 },
      { "prestart", 1, 0, 506 },
#endif
      { 0,          0, 0,  0  }
   };

   maa_init(argv[0]);

   flg_register( LOG_SERVER,    "server" );
   flg_register( LOG_CONNECT,   "connect" );
   flg_register( LOG_STATS,     "stats" );
   flg_register( LOG_COMMAND,   "command" );
   flg_register( LOG_FOUND,     "found" );
   flg_register( LOG_NOTFOUND,  "notfound" );
   flg_register( LOG_CLIENT,    "client" );
   flg_register( LOG_HOST,      "host" );
   flg_register( LOG_TIMESTAMP, "timestamp" );
   flg_register( LOG_MIN,       "min" );

   dbg_register( DBG_VERBOSE,  "verbose" );
   dbg_register( DBG_SCAN,     "scan" );
   dbg_register( DBG_PARSE,    "parse" );
   dbg_register( DBG_SEARCH,   "search" );
   dbg_register( DBG_INIT,     "init" );
   dbg_register( DBG_PORT,     "port" );
   dbg_register( DBG_LEV,      "lev" );
   dbg_register( DBG_AUTH,     "auth" );
   dbg_register( DBG_NODETACH, "nodetach" );
   dbg_register( DBG_NOFORK,   "nofork" );

   while ((c = getopt_long( argc, argv,
			    "vVd:p:c:hL:t:l:sm:f", longopts, NULL )) != EOF)
      switch (c) {
                                /* Remember to copy optarg since we're
                                   going to destroy argv soon... */
      case 'v': dbg_set( "verbose" );                     break;
      case 'V': banner(); exit(1);                        break;
      case 'd': dbg_set( optarg );                        break;
      case 'p': service = str_copy(optarg);               break;
      case 'c': configFile = str_copy(optarg);            break;
      case 't': testWord = str_copy(optarg);              break;
      case 'L': logFile = str_copy(optarg);               break;
      case 's': ++useSyslog;                              break;
      case 'm': _dict_markTime = 60*atoi(optarg);         break;
      case 'f': ++forceStartup;                           break;
      case 'l':
	 ++logOptions;
	 flg_set( optarg );
	 if (flg_test(LOG_MIN)) set_minimal();
	 break;
      case 500: license(); exit(1);                       break;
      case 501: testFile = str_copy(optarg);              break;
      case 502: delay = atoi(optarg);                     break;
      case 503: depth = atoi(optarg);                     break;
      case 504: _dict_daemon_limit = atoi(optarg);        break;
#if PERSISTENT
      case 505: _dict_persistent_limit = atoi(optarg);    break;
      case 506: _dict_persistent_prestart = atoi(optarg); break;
#endif
      case 'h':
      default:  help(); exit(1);                          break;
      }

#if PERSISTENT
   if (_dict_persistent_prestart > _dict_persistent_limit)
      _dict_persistent_prestart = _dict_persistent_limit;
#endif
   if (dbg_test(DBG_NOFORK))    dbg_set_flag( DBG_NODETACH);
   if (dbg_test(DBG_NODETACH))  detach = 0;
   if (dbg_test(DBG_PARSE))     prs_set_debug(1);
   if (dbg_test(DBG_SCAN))      yy_flex_debug = 1;
   else                         yy_flex_debug = 0;

   DictConfig = xmalloc(sizeof(struct dictConfig));
   memset( DictConfig, 0, sizeof( struct dictConfig ) );
   if (!access(configFile,R_OK)) prs_file_nocpp( configFile );

   if (testWord) {		/* stand-alone test mode */
      lst_List list = lst_create();

      dict_config_print( NULL, DictConfig );
      dict_init_databases( DictConfig );
      if (dict_search_database( list,
				testWord,
				lst_nth_get( DictConfig->dbl, 1 ),
				DICT_EXACT )) {
	 if (dbg_test(DBG_VERBOSE)) dict_dump_list( list );
	 dict_dump_defs( list, lst_nth_get( DictConfig->dbl, 1 ) );
	 dict_destroy_list( list );
      } else {
	 printf( "No match\n" );
      }
      fprintf( stderr, "%d comparisons\n", _dict_comparisons );
      exit( 0 );
   }

   if (testFile) {
      FILE         *str;
      char         buf[1024], *pt;
      dictDatabase *db = lst_nth_get(DictConfig->dbl, 1);
      int          words = 0;

      if (!(str = fopen(testFile,"r")))
	 err_fatal_errno( "Cannot open \"%s\" for read\n", testFile );
      while (fgets(buf,1024,str)) {
	 lst_List list = lst_create();
	 ++words;
         if ((pt = strchr(buf, '\t'))) *pt = '\0'; /* stop at tab */
	 if (dict_search_database( list, buf, db, DICT_EXACT )) {
	    if (dbg_test(DBG_VERBOSE)) dict_dump_list( list );
	    dict_dump_defs( list, db );
	 } else {
	    fprintf( stderr, "*************** No match for \"%s\"\n", buf );
	 }
	 dict_destroy_list( list );
	 if (words && !(words % 1000))
	    fprintf( stderr,
		     "%d comparisons, %d words\n", _dict_comparisons, words );
      }
      fprintf( stderr,
	       "%d comparisons, %d words\n", _dict_comparisons, words );
      fclose( str);
      exit(0);
      /* Comparisons:
	 P5/133
	 1878064 comparisons, 113955 words
	 39:18.72u 1.480s 55:20.27 71%
	 */

	
   }

   setsig(SIGCHLD, reaper);
   setsig(SIGHUP,  handler);
   if (!dbg_test(DBG_NOFORK)) setsig(SIGINT,  handler);
   setsig(SIGQUIT, handler);
   setsig(SIGILL,  handler);
   setsig(SIGTRAP, handler);
   setsig(SIGTERM, handler);
   setsig(SIGPIPE, handler);
   setsig(SIGALRM, handler);

   fflush(stdout);
   fflush(stderr);

   if (detach) {
      FILE *str;

      if (!forceStartup && (str = fopen(DICTD_PID_FILE, "r"))) {
	 char buf[80];
	 if (fread(buf,1,80,str) > 0) {
	    buf[strlen(buf)-1] = '\0'; /* Kill newline */
	    err_fatal( __FUNCTION__,
		       "dictd already running with pid %s\n"
		       "   remove %s or use -f to force startup\n",
		       buf, DICTD_PID_FILE );
	 }
	 fclose(str);
      }
      
      net_detach();
      
      if ((str = fopen(DICTD_PID_FILE, "w"))) {
	 fprintf( str, "%d\n", (int)getpid() );
	 fclose( str );
      }
   }

   if (flg_test(LOG_TIMESTAMP)) log_option( LOG_OPTION_FULL );
   else                         log_option( LOG_OPTION_NO_FULL );
   
   if (logFile)   log_file( "dictd", logFile );
   if (useSyslog) log_syslog( "dictd", 0 );
   if (!detach)   log_stream( "dictd", stderr );

   if ((logFile || useSyslog || !detach) && !logOptions) set_minimal();

   masterSocket = net_open_tcp( service, depth );

   time(&startTime);
   tim_start( "dictd" );
   alarm(_dict_markTime);

#if !PERSISTENT
   log_info(":I: %d starting %s %24.24s\n",
	    getpid(), dict_get_banner(0), ctime(&startTime));
#else
   if (_dict_persistent_limit) {
      _dict_persistent_pids = malloc(_dict_persistent_limit
				     * sizeof(_dict_persistent_pids[0]));
      _dict_sem = semget( IPC_PRIVATE, 1, IPC_CREAT|IPC_EXCL|S_IRWXU );
      semctl( _dict_sem, 0, SETVAL, _dict_persistent_limit-1 );
      if (flg_test(LOG_SERVER))
         log_info(":I: %d starting, %d/%d persistent daemons\n",
		  getpid(),
                  _dict_persistent_limit,
		  _dict_persistent_prestart);
   }
#endif

   if (dbg_test(DBG_VERBOSE)) dict_config_print( NULL, DictConfig );
   dict_init_databases( DictConfig );

   
   dict_initsetproctitle(argc, argv, envp);

   for (;;) {
      dict_setproctitle( "%s: %d/%d",
			 dict_get_banner(1),
			 _dict_daemon_count,
			 _dict_forks );
#if !PERSISTENT
      if (flg_test(LOG_SERVER))
         log_info( ":I: %d accepting on %s\n", getpid(), service );
      if ((childSocket = accept(masterSocket,
				(struct sockaddr *)&csin, &alen)) < 0) {
	 if (errno == EINTR) continue;
	 err_fatal_errno( __FUNCTION__, ":E: can't accept" );
      }

      if (_dict_daemon || dbg_test(DBG_NOFORK)) {
	 dict_daemon(childSocket,&csin,&argv,delay,0);
	 close(childSocket);
      } else {
	 if (_dict_daemon_count < _dict_daemon_limit) {
	    if (!start_daemon()) { /* child */
	       alarm(0);
	       dict_daemon(childSocket,&csin,&argv,delay,0);
	       close(childSocket);
	       exit(0);
	    } else {		   /* parent */
	       close(childSocket);
	    }
	 } else {
	    dict_daemon(childSocket,&csin,&argv,delay,1);
	 }
      }
#else
      if (_dict_persistent_limit) {
	 if (_dict_persistent_count == _dict_persistent_limit)
	    sleep(1);
	 sembuf.sem_num = 0;
	 sembuf.sem_op  = -1;
	 sembuf.sem_flg = SEM_UNDO;
	 semop( _dict_sem, &sembuf, 1 );
	 if (flg_test(LOG_SERVER))
            log_info( ":I: %d accepting, sem = %d\n",
                      getpid(), semctl( _dict_sem, 0, GETVAL, 0 ) );
      } else
	 if (flg_test(LOG_SERVER)) log_info( ":I: %d accepting\n", getpid() );
      if ((childSocket = accept(masterSocket,
				(struct sockaddr *)&csin, &alen)) < 0) {
	 if (errno == EINTR) continue;
	 err_fatal_errno( __FUNCTION__, ":E: can't accept" );
      }
      if (_dict_persistent_limit) {
	 sembuf.sem_num = 0;
	 sembuf.sem_op  = 1;
	 sembuf.sem_flg = SEM_UNDO;
	 semop( _dict_sem, &sembuf, 1 );
      }
      if (_dict_daemon || dbg_test(DBG_NOFORK)) {
	 dict_daemon(childSocket,&csin,&argv,delay,0);
	 close(childSocket);
      } else {
	 if (_dict_persistent_count < _dict_persistent_limit) {
	    if (!start_persistent()) { /* child */
	       alarm(0);
	       dict_daemon(childSocket,&csin,&argv,delay,0);
	       close(childSocket);
	    } else {		   /* parent */
	       close(childSocket);
	    }
	 } else {
	    if (!start_daemon()) {  /* child */
	       alarm(0);
	       close(masterSocket);
	       exit(dict_daemon(childSocket,&csin,&argv,delay,0));
	    } else {		   /* parent */
	       close(childSocket);
	    }
	 }
      }
#endif
   }
}
