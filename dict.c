/* dict.c -- 
 * Created: Fri Mar 28 19:16:29 1997 by faith@cs.unc.edu
 * Revised: Fri Mar 28 20:10:01 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: dict.c,v 1.3 1997/03/31 01:53:28 faith Exp $
 * 
 */

#include "dictP.h"
#include "maa.h"
#include "net.h"
#include <signal.h>

extern int        yy_flex_debug;

#define BUFFERSIZE 1024

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

static const char *dict_get_banner( void )
{
   static char       *buffer= NULL;
   const char        *id = "$Id: dict.c,v 1.3 1997/03/31 01:53:28 faith Exp $";
   
   if (buffer) return buffer;
   buffer = xmalloc(256);
   sprintf( buffer, "%s %s", err_program_name(), id_string( id ) );
   return buffer;
}

static void banner( void )
{
   fprintf( stderr, "%s\n", dict_get_banner() );
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

int main( int argc, char **argv )
{
   int                s;
   int                c;
   char               buf[BUFFERSIZE];
   const char         *service = "2628";
   const char         *host    = "localhost";
   int                n;
   struct option      longopts[]  = {
      { "verbose", 0, 0, 'v' },
      { "version", 0, 0, 'V' },
      { "debug",   1, 0, 'd' },
      { "port",    1, 0, 'p' },
      { "host",    1, 0, 'h' },
      { "help",    0, 0, 501 },
      { "license", 0, 0, 'L' },
      { 0,         0, 0,  0  }
   };

   maa_init(argv[0]);

#if 0
   dbg_register( DBG_VERBOSE, "verbose" );
   dbg_register( DBG_PARSE,   "parse" );
   dbg_register( DBG_SCAN,    "scan" );
#endif

   while ((c = getopt_long( argc, argv,
			    "vVd:p:h:L", longopts, NULL )) != EOF)
      switch (c) {
      case 'v': dbg_set( "verbose" ); break;
      case 'V': banner(); exit(1);    break;
      case 'd': dbg_set( optarg );    break;
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


   sprintf( buf, "define %s\n", argv[optind] );
   write(s,buf,strlen(buf));
   while ((n = read(s, buf, BUFFERSIZE)) > 0) {
      buf[n] = '\0';
      fputs(buf,stdout);
   }
   close(s);
   return 0;
}
