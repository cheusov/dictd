/* dict.c -- 
 * Created: Fri Mar 28 19:16:29 1997 by faith@cs.unc.edu
 * Revised: Wed Apr 16 11:14:31 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: dict.c,v 1.4 1997/04/30 12:03:48 faith Exp $
 * 
 */

#include "dict.h"

extern int        yy_flex_debug;

#define BUFFERSIZE  2048
#define COMMANDSIZE  256

static void handler( int sig )
{
   const char *name = NULL;
   
   switch (sig) {
   case SIGHUP:  name = "SIGHUP";  break;
   case SIGINT:  name = "SIGINT";  break;
   case SIGQUIT: name = "SIGQUIT"; break;
   case SIGILL:  name = "SIGILL";  break;
   case SIGTRAP: name = "SIGTRAP"; break;
   case SIGTERM: name = "SIGTERM"; break;
   case SIGPIPE: name = "SIGPIPE"; break;
   }

   if (name)
      err_fatal( __FUNCTION__, "Caught %s, exiting\n", name );
   else
      err_fatal( __FUNCTION__, "Caught signal %d, exiting\n", sig );

   exit(0);
}

static void setsig( int sig, void (*f)(int) )
{
   struct sigaction   sa;
   
   sa.sa_handler = f;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sigaction(sig, &sa, NULL);
}

static const char *id_string( const char *id )
{
   static char buffer[BUFFERSIZE];
   arg_List a = arg_argify( id, 0 );

   if (arg_count(a) >= 2)
      sprintf( buffer, "%s", arg_get( a, 2 ) );
   else
      buffer[0] = '\0';
   arg_destroy( a );
   return buffer;
}

static const char *client_get_banner( void )
{
   static char       *buffer= NULL;
   const char        *id = "$Id: dict.c,v 1.4 1997/04/30 12:03:48 faith Exp $";
   struct utsname    uts;
   
   if (buffer) return buffer;
   uname( &uts );
   buffer = xmalloc(256);
   sprintf( buffer,
	    "%s (version %s on %s %s)", err_program_name(), id_string( id ),
	    uts.sysname,
	    uts.release );
   return buffer;
}

static void banner( void )
{
   fprintf( stderr, "%s\n", client_get_banner() );
   fprintf( stderr,
	    "Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)\n" );
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
   };
   const char        **p = license_msg;
   
   banner();
   while (*p) fprintf( stderr, "   %s\n", *p++ );
}
    
static void help( void )
{
   static const char *help_msg[] = {
      "   --help            give this help",
      "-L --license         display software license",
      "-v --verbose         verbose mode",
      "-V --version         display version number",
      "-d --debug <option>  select debug option",
      "-p --port <port>     port number",
      "-h --host <host>     host",
      0 };
   const char        **p = help_msg;

   banner();
   while (*p) fprintf( stderr, "%s\n", *p++ );
}

static lst_List client_read_text( int s )
{
   lst_List l = lst_create();
   char     line[BUFFERSIZE];
   int      len;

   while ((len = net_read(s, line, BUFFERSIZE)) > 0) {
      PRINTF(DBG_VERBOSE,("Read: %s\n",line));
      if (line[0] == '.' && line[1] == '\0') break;
      if (len >= 2 && line[0] == '.' && line[1] == '.') 
	 lst_append( l, xstrdup(line + 1) );
      else
	 lst_append( l, xstrdup(line) );
   }
   if (len < 0) err_fatal_errno( __FUNCTION__, "Error reading from socket\n" );
   return l;
}

static void client_print_text( lst_List l )
{
   lst_Position p;
   const char   *e;

   if (!l) return;
   LST_ITERATE(l,p,e) {
      printf( "%s\n", e );
   }
}

static void client_free_text( lst_List l )
{
   lst_Position p;
   char         *e;
   
   if (!l) return;
   LST_ITERATE(l,p,e) {
      if (e) xfree(e);
   }
   lst_destroy(l);
}

static void client_copy_text( int s )
{
   lst_List l;

   l = client_read_text( s );
   client_print_text( l );
   client_free_text( l );
}
   

static int client_read_status( int s,
			       int *count,
			       const char **db,
			       const char **dbname,
			       const char **dburl )
{
   char     buf[BUFFERSIZE];
   arg_List cmdline;
   int      argc;
   char     **argv;
   int      status;

   if (net_read( s, buf, BUFFERSIZE ) < 0)
      err_fatal_errno( __FUNCTION__, "Error reading from socket\n" );
   PRINTF(DBG_VERBOSE,("Read: %s\n",buf));

   if ((status = atoi(buf)) < 100) status = 600;
   PRINTF(DBG_VERBOSE,("Status = %d\n",status));

   *count = 0;
   *db = *dbname = *dburl = NULL;

   switch (status) {
   case CODE_DATABASE_LIST:
   case CODE_STRATEGY_LIST:
   case CODE_DEFINITIONS_FOUND:
   case CODE_MATCHES_FOUND:
      cmdline = arg_argify(buf,0);
      arg_get_vector( cmdline, &argc, &argv );
      if (argc > 0) *count = atoi(argv[1]);
      arg_destroy(cmdline);
      break;
   case CODE_DEFINITION_FOLLOWS:
      cmdline = arg_argify(buf,0);
      arg_get_vector( cmdline, &argc, &argv );
      if (argc > 0) *db     = str_find(argv[1]);
      if (argc > 1) *dbname = str_find(argv[2]);
      if (argc > 2) *dburl  = str_find(argv[3]);
      arg_destroy(cmdline);
      break;
   default:
      break;
   }

   return status;
}

static void client_crlf( char *d, const char *s )
{
   int flag = 0;
   
   while (*s) {
      if (*s == '\n') {
	 *d++ = '\r';
	 *d++ = '\n';
	 ++s;
	 ++flag;
      } else {
	 *d++ = *s++;
	 flag = 0;
      }
   }
   if (!flag) {
      *d++ = '\r';
      *d++ = '\n';
   }
   *d = '\0';
}

static void client_printf( int s, const char *format, ... )
{
   va_list ap;
   char    buf[BUFFERSIZE];
   char    *pt;
   int     len;

   va_start( ap, format );
   vsprintf( buf, format, ap );
   va_end( ap );
   if ((len = strlen( buf )) >= BUFFERSIZE) {
      err_fatal( __FUNCTION__, "Buffer overflow: %d\n", len );
   }

   pt = alloca(2*len);
   client_crlf(pt, buf);
   PRINTF(DBG_VERBOSE,("Sent: %s",pt));
   net_write(s, pt, strlen(pt));
}


static void define( int s, const char *word, const char *database )
{
   client_printf( s, "define %s \"%s\"", database, word );
}

static void client( int s )
{
   client_printf( s, "client \"%s\"", client_get_banner() );
}

static int authenticate( int s )
{
   return 0;
}

static void quit( int s )
{
   client_printf( s, "quit" );
}

static void machine( int s,
		     const char *word,
		     const char *database,
		     const char *strategy )
{
   int        code;
   int        n;
   char       buf[BUFFERSIZE];
   int        count;
   const char *db;
   const char *dbname;
   const char *dburl;
   enum { INIT, DEF, POST, TERM, FIN } state;
   
   for (state = INIT; state != FIN;) {
      
      code = client_read_status(s, &count, &db, &dbname, &dburl);
      
      if (state == TERM && code != CODE_GOODBYE) {
	 printf( "Server refuses clean termination, exiting\n" );
	 state = FIN;
	 continue;
      }
      
      switch (code) {
      case CODE_HELLO:   client(s);   break;
      case CODE_GOODBYE: state = FIN; break;
#if CODE_DEFINITIONS_FINISHED != CODE_OK
      case CODE_DEFINITIONS_FINISHED:
#endif
#if CODE_MATCHES_FINISHED != CODE_OK
      case CODE_MATCHES_FINISHED:
#endif
      case CODE_OK:
	 switch (state) {
	 case INIT: state = DEF; if (authenticate(s))  break; /* else fall */
	 case DEF:  define(s, word, database);         break;
	 case POST: state = TERM; quit(s);             break;
	 default:                                      break;
	 }
	 break;
      case CODE_INVALID_DB:
	 printf( "%s is an invalid database\n", database );
	 state = TERM;
	 quit(s);
	 break;
      case CODE_NO_MATCH:
	 printf( "No matches for %s\n", word );
	 state = TERM;
	 quit(s);
	 break;
      case CODE_DATABASE_LIST:
	 break;
      case CODE_STRATEGY_LIST:
	 break;
      case CODE_DATABASE_INFO:
	 break;
      case CODE_STATUS:
	 break;
      case CODE_HELP:
	 break;
      case CODE_AUTH_OK:
	 break;
      case CODE_DEFINITIONS_FOUND:
	 break;
      case CODE_DEFINITION_FOLLOWS:
	 printf( "From %s:\n",
		 dbname ? dbname : (db ? db : "unknown database") );
	 client_copy_text( s );
	 state = POST;
	 break;
      case CODE_MATCHES_FOUND:
	 break;
      case CODE_SYNTAX_ERROR:
	 printf( "Syntax error, terminating connection\n" );
	 state = TERM;
	 quit(s);
	 break;
      case CODE_ILLEGAL_PARAM:
	 printf( "Illegal parameter, terminating connection\n" );
	 state = TERM;
	 quit(s);
	 break;
      case CODE_COMMAND_NOT_IMPLEMENTED:
	 printf( "Command not implemented, terminating connection\n" );
	 state = TERM;
	 quit(s);
	 break;
      case CODE_PARAM_NOT_IMPLEMENTED:
	 printf( "Parameter not implemented, terminating connection\n" );
	 state = TERM;
	 quit(s);
	 break;
      case CODE_ACCESS_DENIED:
	 break;
      case CODE_AUTH_DENIED:
	 break;
      case CODE_INVALID_STRATEGY:
	 break;
      case CODE_NO_DATABASES:
	 printf( "No databases available (authentication required?)\n" );
	 break;
      case CODE_NO_STRATEGIES:
	 printf( "No search strategies available\n" );
	 break;
      default:
	 while ((n = read(s, buf, BUFFERSIZE)) > 0) {
	    buf[n] = '\0';
	    fputs(buf,stdout);
	 }
	 printf( "Unknown response code %d\n", state );
	 break;
      }
   }
}
   
int main( int argc, char **argv )
{
   int                s;
   int                c;
   const char         *service  = "2628";
   const char         *host     = "localhost";
   const char         *database = "*";
   const char         *word;
   struct option      longopts[]  = {
      { "verbose",  0, 0, 'v' },
      { "version",  0, 0, 'V' },
      { "debug",    1, 0, 'D' },
      { "port",     1, 0, 'p' },
      { "host",     1, 0, 'h' },
      { "help",     0, 0, 501 },
      { "license",  0, 0, 'L' },
      { "database", 1, 0, 'd' },
      { 0,          0, 0,  0  }
   };

   maa_init(argv[0]);

   dbg_register( DBG_VERBOSE, "verbose" );
   dbg_register( DBG_TRACE,   "trace" );
#if 0
   dbg_register( DBG_PARSE,   "parse" );
   dbg_register( DBG_SCAN,    "scan" );
#endif

   while ((c = getopt_long( argc, argv,
			    "vVD:d:p:h:L", longopts, NULL )) != EOF)
      switch (c) {
      case 'v': dbg_set( "verbose" ); break;
      case 'V': banner(); exit(1);    break;
      case 'D': dbg_set( optarg );    break;
      case 'd': database = optarg;    break;
      case 'p': service = optarg;     break;
      case 'h': host = optarg;        break;
      case 'L': license(); exit(1);   break;
      case 501:
      default:  help(); exit(1);      break;
      }

#if 0
   if (dbg_test(DBG_PARSE))     prs_set_debug(1);
   if (dbg_test(DBG_SCAN))      yy_flex_debug = 1;
   else                         yy_flex_debug = 0;

   DictConfig = xmalloc(sizeof(struct dictConfig));
   prs_file_nocpp( configFile );
   dict_config_print( NULL, DictConfig );
   dict_init_databases( DictConfig );
#endif

   setsig(SIGHUP,  handler);
   setsig(SIGINT,  handler);
   setsig(SIGQUIT, handler);
   setsig(SIGILL,  handler);
   setsig(SIGTRAP, handler);
   setsig(SIGTERM, handler);
   setsig(SIGPIPE, handler);

   fflush(stdout);
   fflush(stderr);

   s = net_connect_tcp( host, service );
   word = argv[optind];
   machine( s, word, database, "" );
   close(s);
   return 0;
}
