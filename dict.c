/* dict.c -- 
 * Created: Fri Mar 28 19:16:29 1997 by faith@cs.unc.edu
 * Revised: Tue Jul  8 17:15:34 1997 by faith@acm.org
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: dict.c,v 1.7 1997/07/08 21:21:06 faith Exp $
 * 
 */

#include "dict.h"
#include "md5.h"
#include <stdarg.h>

extern int        yy_flex_debug;

#define BUFFERSIZE  2048
#define PIPESIZE     256

#define CMD_PRINT   0
#define CMD_CONNECT 1
#define CMD_CLIENT  2
#define CMD_AUTH    3
#define CMD_INFO    4
#define CMD_SERVER  5
#define CMD_DBS     6
#define CMD_STRATS  7
#define CMD_HELP    8
#define CMD_MATCH   9
#define CMD_DEFINE 10
#define CMD_SPELL  11
#define CMD_WIND   12
#define CMD_CLOSE  13

struct cmd {
   int        command;
   int        sent;
   const char *host;
   const char *service;
   const char *database;
   const char *strategy;
   const char *word;
   const char *client;
   const char *user;
   const char *key;
};

lst_List cmd_list;

struct reply {
   int        s;
   const char *host;
   const char *service;
   const char *msgid;
   lst_List   data;
   int        retcode;
} cmd_reply;

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

static lst_List client_read_text( int s )
{
   lst_List l = lst_create();
   char     line[BUFFERSIZE];
   int      len;

   while ((len = net_read(s, line, BUFFERSIZE)) >= 0) {
      PRINTF(DBG_RAW,("Text: %s\n",line));
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
			       const char **message,
			       int *count,
			       const char **db,
			       const char **dbname,
			       const char **msgid )
{
   static char buf[BUFFERSIZE];
   arg_List    cmdline;
   int         argc;
   char        **argv;
   int         status;
   char        *p;

   if (net_read( s, buf, BUFFERSIZE ) < 0)
      err_fatal_errno( __FUNCTION__, "Error reading from socket\n" );
   PRINTF(DBG_RAW,("Read: %s\n",buf));

   if ((status = atoi(buf)) < 100) status = 600;
   PRINTF(DBG_RAW,("Status = %d\n",status));

   if (message) *message = strchr( buf, ' ' ) + 1;

   if (count)  *count = 0;
   if (db)     *db = NULL;
   if (dbname) *dbname = NULL;
   if (msgid)  *msgid = NULL;

   switch (status) {
   case CODE_HELLO:
      if ((p = strrchr(buf, '>')) && (p = strrchr(p,'<'))) {
	 *msgid = xstrdup( p+1 );
      }
      break;
   case CODE_DATABASE_LIST:
   case CODE_STRATEGY_LIST:
   case CODE_DEFINITIONS_FOUND:
   case CODE_MATCHES_FOUND:
      cmdline = arg_argify(buf,0);
      arg_get_vector( cmdline, &argc, &argv );
      if (argc > 1 && count) *count = atoi(argv[1]);
      arg_destroy(cmdline);
      break;
   case CODE_DEFINITION_FOLLOWS:
      cmdline = arg_argify(buf,0);
      arg_get_vector( cmdline, &argc, &argv );
      if (argc > 1 && db)     *db     = str_find(argv[1]);
      if (argc > 2 && dbname) *dbname = str_find(argv[2]);
      arg_destroy(cmdline);
      break;
   default:
      break;
   }

   return status;
}

static void push( int command, ... )
{
   va_list    ap;
   struct cmd *c = xmalloc( sizeof( struct cmd ) );

   memset( c, 0, sizeof( struct cmd ) );
   c->command = command;

   va_start( ap, command );
   switch (command) {
   case CMD_PRINT:
      break;
   case CMD_CONNECT:
      c->host     = va_arg( ap, const char *);
      c->service  = va_arg( ap, const char *);
      break;
   case CMD_CLIENT:
      c->client   = va_arg( ap, const char *);
      break;
   case CMD_AUTH:
      c->user     = va_arg( ap, const char *);
      c->key      = va_arg( ap, const char *);
      break;
   case CMD_INFO:
      c->database = va_arg( ap, const char *);
      break;
   case CMD_SERVER:
      break;
   case CMD_DBS:
      break;
   case CMD_STRATS:
      break;
   case CMD_HELP:
      break;
   case CMD_MATCH:
      c->database = va_arg( ap, const char *);
      c->strategy = va_arg( ap, const char *);
      c->word     = va_arg( ap, const char *);
      break;
   case CMD_DEFINE:
      c->database = va_arg( ap, const char *);
      c->word     = va_arg( ap, const char *);
      break;
   case CMD_SPELL:
      break;
   case CMD_WIND:
      break;
   case CMD_CLOSE:
      break;
   default:
      err_internal( __FUNCTION__, "Illegal command %d\n", command );
   }
   va_end( ap );

   if (!cmd_list) cmd_list = lst_create();
   lst_append( cmd_list, c );
}

static void request( void )
{
   char              buffer[PIPESIZE];
   char              *p = buffer;
   lst_Position      pos;
   struct cmd        *c = NULL;
   unsigned char     digest[16];
   char              hex[33];
   struct MD5Context ctx;
   int               i;
   int               len;

   *p = '\0';
   LST_ITERATE(cmd_list,pos,c) {
      if (c->sent) return;	/* FIXME!  Keep sending deeper things? */
      switch( c->command) {
      case CMD_PRINT:                                                 goto end;
      case CMD_CONNECT:                                               break;
      case CMD_AUTH:
	 if (!c->key || !c->user) break;
	 if (cmd_reply.msgid) {
	    MD5Init(&ctx);
	    MD5Update(&ctx, c->key, strlen(c->key));
	    MD5Final(digest, &ctx );
	    for (i = 0; i < 16; i++) sprintf( hex+2*i, "%02x", digest[i] );
	    hex[32] = '\0';
	    sprintf( p, "auth %s %s\n", c->user, hex );
	    break;
	 }
	 return;
      case CMD_CLIENT: sprintf( p, "client \"%s\"\n", c->client );    break;
      case CMD_INFO:   sprintf( p, "show info %s\n", c->database );   break;
      case CMD_SERVER: sprintf( p, "show server\n" );                 break;
      case CMD_DBS:    sprintf( p, "show db\n" );                     break;
      case CMD_STRATS: sprintf( p, "show strat\n" );                  break;
      case CMD_HELP:   sprintf( p, "help\n" );                        break;
      case CMD_MATCH:
	 sprintf( p,
		  "match %s %s \"%s\"\n",
		  c->database, c->strategy, c->word );                break;
      case CMD_DEFINE:
	 sprintf( p, "define %s \"%s\"\n", c->database, c->word );    break;
      case CMD_SPELL:                                                 goto end;
      case CMD_WIND:                                                  goto end;
      case CMD_CLOSE:  sprintf( p, "quit\n" );                        break;
      default:
	 err_internal( __FUNCTION__, "Unknown command %d\n", c->command );
      }
      ++c->sent;
      if (dbg_test(DBG_SERIAL)) break; /* Don't pipeline. */
      p += strlen(p);
   }

end:				/* Ready to send buffer, but are we
				   connected? */
   if (!cmd_reply.s) {
      if ((cmd_reply.s = net_connect_tcp( c->host, c->service )) < 0) {
	 err_fatal( __FUNCTION__,
		    "Can't connect to %s.%s\n", c->host, c->service );
      }
      cmd_reply.host    = c->host;
      cmd_reply.service = c->service;
   }
   if ((len = strlen(buffer))) {
      char *pt;
      
      PRINTF(DBG_RAW,("Sent/%d: %s",c->command,buffer));
      pt = alloca(2*len);
      client_crlf(pt,buffer);
      net_write( cmd_reply.s, pt, strlen(pt) );
   } else {
      PRINTF(DBG_RAW,("Send/%d\n",c->command)); 
   }
}

static void process( void )
{
   struct cmd *c;
   int        expected;
   const char *message;
   
   while ((c = lst_top( cmd_list ))) {
      request();		/* Send requests */
      expected = CODE_OK;
      switch (c->command) {
      case CMD_PRINT:
	 if (!cmd_reply.data) {
	    printf( "Error %d\n", cmd_reply.retcode );
	 } else {
	    client_print_text( cmd_reply.data );
	    client_free_text( cmd_reply.data );
	    cmd_reply.data = NULL;
	 }
	 break;
      case CMD_CONNECT:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL,
						 &cmd_reply.msgid );
	 if (cmd_reply.retcode == CODE_ACCESS_DENIED) {
	    err_fatal( __FUNCTION__,
		       "Access to server %s.%s denied when connecting",
		    cmd_reply.host,
		    cmd_reply.service );
	    exit(1);
	 }
	 expected = CODE_HELLO;
	 break;
      case CMD_CLIENT:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL );
	 break;
      case CMD_AUTH:
	 if (!c->key || !c->user) break;
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL );
	 if (cmd_reply.retcode == CODE_AUTH_DENIED)
	    err_warning( __FUNCTION__,
			 "Authentication to %s.%s denied\n",
			 cmd_reply.host,
			 cmd_reply.service );
	 expected = CODE_AUTH_OK;
      case CMD_INFO:
	 expected = CODE_DATABASE_INFO;
	 goto gettext;
      case CMD_SERVER:
	 expected = CODE_SERVER_INFO;
	 goto gettext;
      case CMD_HELP:
	 expected = CODE_HELP;
	 goto gettext;
      case CMD_DBS:
	 expected = CODE_DATABASE_LIST;
	 goto gettext;
      case CMD_STRATS:
	 expected = CODE_STRATEGY_LIST;
	 goto gettext;
   gettext:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL );
	 if (cmd_reply.retcode == expected) {
	    cmd_reply.data = client_read_text( cmd_reply.s );
	    cmd_reply.retcode = client_read_status( cmd_reply.s,
						    &message,
						    NULL, NULL, NULL, NULL );
	    expected = CODE_OK;
	 }
	 break;
      case CMD_MATCH:
      case CMD_DEFINE:
      case CMD_SPELL:
      case CMD_WIND:
      case CMD_CLOSE:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL );
	 expected = CODE_GOODBYE;
	 break;
      default:
	 err_internal( __FUNCTION__, "Illegal command %d\n", c->command );
      }
      if (cmd_reply.retcode != expected) {
	 err_fatal( __FUNCTION__,
		    "Unexpected status code %d (%s), wanted %d\n",
		    cmd_reply.retcode,
		    message,
		    expected );
      }
      lst_pop(cmd_list);
      PRINTF(DBG_RAW,("Processed %d\n",c->command));
      xfree(c);
   }
}

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
   const char        *id = "$Id: dict.c,v 1.7 1997/07/08 21:21:06 faith Exp $";
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

#if 0
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
#endif
   
int main( int argc, char **argv )
{
   int                c;
   const char         *service   = "2628";
   const char         *host      = "localhost";
   const char         *database  = "*";
   const char         *strategy  = "!";
   int                doauth     = 1;
   const char         *user      = NULL;
   const char         *key       = NULL;
   int                i;
   enum { DEFINE,
	  INFO,
	  SERVER,
	  MATCH,
	  DBS,
	  STRATS,
	  HELP }      function    = DEFINE;
   struct option      longopts[]  = {
      { "host",       1, 0, 'h' },
      { "port",       1, 0, 'p' },
      { "database",   1, 0, 'd' },
      { "info",       1, 0, 'i' },
      { "server",     0, 0, 'I' },
      { "match",      0, 0, 'm' },
      { "strategy",   1, 0, 's' },
      { "dbs",        0, 0, 'D' },
      { "strats",     0, 0, 'S' },
      { "serverhelp", 0, 0, 'H' },
      { "noauth",     0, 0, 'a' },
      { "user",       1, 0, 'u' },
      { "key",        1, 0, 'k' },
      { "version",    0, 0, 'V' },
      { "license",    0, 0, 'L' },
      { "help",       0, 0, 501 },
      { "verbose",    0, 0, 'v' },
      { "raw",        0, 0, 'r' },
      { "debug",      1, 0, 502 },
      { 0,            0, 0,  0  }
   };

   maa_init(argv[0]);

   dbg_register( DBG_VERBOSE, "verbose" );
   dbg_register( DBG_RAW,     "raw" );
   dbg_register( DBG_SCAN,    "scan" );
   dbg_register( DBG_PARSE,   "parse" );
   dbg_register( DBG_PIPE,    "pipe" );
   dbg_register( DBG_SERIAL,  "serial" );
   dbg_register( DBG_TIME,    "time" );

   while ((c = getopt_long( argc, argv,
			    "h:p:d:i:Ims:DSHak:VLvr", longopts, NULL )) != EOF)
      switch (c) {
      case 'h': host = optarg;                        break;
      case 'p': service = optarg;                     break;
      case 'd': database = optarg;                    break;
      case 'i': database = optarg; function = INFO;   break;
      case 'I':                    function = SERVER; break;
      case 'm':                    function = MATCH;  break;
      case 's': strategy = optarg;                    break;
      case 'D':                    function = DBS;    break;
      case 'S':                    function = STRATS; break;
      case 'H':                    function = HELP;   break;
      case 'a': doauth = 0;                           break;
      case 'u': user = optarg;                        break;
      case 'k': key = optarg;                         break;
      case 'V': banner(); exit(1);                    break;
      case 'L': license(); exit(1);                   break;
      case 'v': dbg_set( "verbose" );                 break;
      case 'r': dbg_set( "raw" );                     break;
      case 502: dbg_set( optarg );                    break;
      case 501:					      
      default:  help(); exit(1);                      break;
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

   push( CMD_CONNECT, host, service );
   push( CMD_CLIENT, client_get_banner() );
   if (doauth) push( CMD_AUTH, user, key );
   switch (function) {
   case INFO:   push( CMD_INFO, database ); push( CMD_PRINT ); break;
   case SERVER: push( CMD_SERVER );         push( CMD_PRINT ); break;
   case DBS:    push( CMD_DBS );            push( CMD_PRINT ); break;
   case STRATS: push( CMD_STRATS );         push( CMD_PRINT ); break;
   case HELP:   push( CMD_HELP );           push( CMD_PRINT ); break;
   case MATCH:
      for (i = optind; i < argc; i++) {
	 push( CMD_MATCH, database, strategy, argv[i] );
	 push( CMD_PRINT );
      }
      break;
   case DEFINE:
      for (i = optind; i < argc; i++) {
	 if (!strcmp(strategy, "!")) {
	    push( CMD_DEFINE, database, argv[i] );
	    push( CMD_SPELL );
	    push( CMD_PRINT );
	 } else {
	    push( CMD_MATCH, database, strategy, argv[i] );
	    push( CMD_WIND );
	 }
      }
   }
   push( CMD_CLOSE );
   process();

#if 0
   s = net_connect_tcp( host, service );
   word = argv[optind];
   machine( s, word, database, "" );
   close(s);
#endif
   return 0;
}
