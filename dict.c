/* dict.c -- 
 * Created: Fri Mar 28 19:16:29 1997 by faith@cs.unc.edu
 * Revised: Tue Jul  8 23:48:14 1997 by faith@acm.org
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: dict.c,v 1.8 1997/07/09 04:00:57 faith Exp $
 * 
 */

#include "dict.h"
#include "md5.h"
#include <stdarg.h>

extern int        yy_flex_debug;

#define BUFFERSIZE  2048
#define PIPESIZE     256
#define DEF_STRAT    "."

#define CMD_PRINT     0
#define CMD_DEFPRINT  1
#define CMD_CONNECT   2
#define CMD_CLIENT    3
#define CMD_AUTH      4
#define CMD_INFO      5
#define CMD_SERVER    6
#define CMD_DBS       7
#define CMD_STRATS    8
#define CMD_HELP      9
#define CMD_MATCH    10
#define CMD_DEFINE   11
#define CMD_SPELL    12
#define CMD_WIND     13
#define CMD_CLOSE    14

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
int      client_defines = 0;

struct def {
   lst_List   data;
   const char *word;
   const char *db;
   const char *dbname;
};

struct reply {
   int        s;
   const char *host;
   const char *service;
   const char *msgid;
   lst_List   data;
   int        retcode;
   int        count;		/* definitions found */
   int        matches;		/* matches found */
   int        listed;		/* Databases or strategies listed */
   struct def *defs;
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
      PRINTF(DBG_RAW,("* Text: %s\n",line));
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
      printf( "  %s\n", e );
   }
}

static void client_print_matches( lst_List l )
{
   lst_Position p;
   const char   *e;
   arg_List     a;
   const char   *prev = NULL;
   const char   *db;
   static int   first = 1;
   int          pos = 0;
   int          len;

   if (!l) return;
   LST_ITERATE(l,p,e) {
      a = arg_argify( e, 0 );
      if (arg_count(a) != 2)
	 err_internal( __FUNCTION__,
		       "MATCH command didn't return 2 args: %s\n", e );
      if ((db = str_find(arg_get(a,0))) != prev) {
	 if (!first) printf( "\n" );
	 first = 0;
	 printf( "From %s:", db );
	 prev = db;
	 pos = 6 + strlen(db);
      }
      len = strlen(arg_get(a,1));
      if (pos + len + 2 > 70) {
	 printf( "\n" );
	 pos = 0;
      }
      if (strchr( arg_get(a,1),' ')) {
	 printf( "  \"%s\"", arg_get(a,1) );
	 pos += len + 4;
      } else {
	 printf( "  %s", arg_get(a,1) );
	 pos += len + 2;
      }
   }
   printf( "\n" );
}

static void client_print_listed( lst_List l )
{
   lst_Position p;
   const char   *e;
   arg_List     a;

   if (!l) return;
   LST_ITERATE(l,p,e) {
      a = arg_argify( e, 0 );
      if (arg_count(a) != 2)
	 err_internal( __FUNCTION__,
		       "SHOW command didn't return 2 args: %s\n", e );
      printf( "  %-10.10s %s\n", arg_get(a,0), arg_get(a,1) );
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

static int client_read_status( int s,
			       const char **message,
			       int *count,
			       const char **word,
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
   PRINTF(DBG_RAW,("* Read: %s\n",buf));

   if ((status = atoi(buf)) < 100) status = 600;
   PRINTF(DBG_RAW,("* Status = %d\n",status));

   if (message && (p = strchr(buf, ' '))) *message = p + 1;

   if (count)  *count = 0;
   if (word)   *word = NULL;
   if (db)     *db = NULL;
   if (dbname) *dbname = NULL;
   if (msgid)  *msgid = NULL;

   switch (status) {
   case CODE_HELLO:
      if ((p = strrchr(buf, '>')) && (p = strrchr(p,'<'))) {
	 *msgid = str_copy( p+1 );
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
      if (argc > 1 && word)   *word   = str_find(argv[1]);
      if (argc > 2 && db)     *db     = str_find(argv[2]);
      if (argc > 3 && dbname) *dbname = str_find(argv[3]);
      arg_destroy(cmdline);
      break;
   default:
      break;
   }

   return status;
}

static struct cmd *make_command( int command, ... )
{
   va_list    ap;
   struct cmd *c = xmalloc( sizeof( struct cmd ) );

   memset( c, 0, sizeof( struct cmd ) );
   c->command = command;

   va_start( ap, command );
   switch (command) {
   case CMD_PRINT:
      break;
   case CMD_DEFPRINT:
      c->database = va_arg( ap, const char *);
      c->word     = va_arg( ap, const char *);
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
      c->database = va_arg( ap, const char *);
      c->word     = va_arg( ap, const char *);
      break;
   case CMD_WIND:
      c->database = va_arg( ap, const char *);
      c->strategy = va_arg( ap, const char *);
      c->word     = va_arg( ap, const char *);
      break;
   case CMD_CLOSE:
      break;
   default:
      err_internal( __FUNCTION__, "Illegal command %d\n", command );
   }
   va_end( ap );

   return c;
}

static void append_command( struct cmd *c )
{
   if (!cmd_list) cmd_list = lst_create();
   lst_append( cmd_list, c );
}


static void prepend_command( struct cmd *c )
{
   if (!cmd_list) cmd_list = lst_create();
   lst_push( cmd_list, c );
}


static void request( void )
{
   char              b[BUFFERSIZE];
   char              buffer[PIPESIZE];
   char              *p = buffer;
   lst_Position      pos;
   struct cmd        *c = NULL;
   unsigned char     digest[16];
   char              hex[33];
   struct MD5Context ctx;
   int               i;
   int               len;
   int               total = 0;

   *p = '\0';
   LST_ITERATE(cmd_list,pos,c) {
      b[0] = '\0';
      PRINTF(DBG_PIPE,("* Looking at request %d\n",c->command));
      if (c->sent) return;	/* FIXME!  Keep sending deeper things? */
      switch( c->command) {
      case CMD_PRINT:                                                 break;
      case CMD_DEFPRINT:                                              break;
      case CMD_CONNECT:                                               break;
      case CMD_AUTH:
	 if (!c->key || !c->user) break;
	 if (cmd_reply.msgid) {
	    MD5Init(&ctx);
	    MD5Update(&ctx, c->key, strlen(c->key));
	    MD5Final(digest, &ctx );
	    for (i = 0; i < 16; i++) sprintf( hex+2*i, "%02x", digest[i] );
	    hex[32] = '\0';
	    sprintf( b, "auth %s %s\n", c->user, hex );
	 }
	 break;
      case CMD_CLIENT: sprintf( b, "client \"%s\"\n", c->client );    break;
      case CMD_INFO:   sprintf( b, "show info %s\n", c->database );   break;
      case CMD_SERVER: sprintf( b, "show server\n" );                 break;
      case CMD_DBS:    sprintf( b, "show db\n" );                     break;
      case CMD_STRATS: sprintf( b, "show strat\n" );                  break;
      case CMD_HELP:   sprintf( b, "help\n" );                        break;
      case CMD_MATCH:
	 sprintf( b,
		  "match %s %s \"%s\"\n",
		  c->database, c->strategy, c->word );                break;
      case CMD_DEFINE:
	 sprintf( b, "define %s \"%s\"\n", c->database, c->word );    break;
      case CMD_SPELL:                                                 goto end;
      case CMD_WIND:                                                  goto end;
      case CMD_CLOSE:  sprintf( b, "quit\n" );                        break;
      default:
	 err_internal( __FUNCTION__, "Unknown command %d\n", c->command );
      }
      len = strlen(b);
      if (total + len + 3 > PIPESIZE) break;
      strcpy( p, b );
      p += len;
      total += len;
      ++c->sent;
      if (dbg_test(DBG_SERIAL)) break; /* Don't pipeline. */
   }

end:				/* Ready to send buffer, but are we
				   connected? */
   if (!cmd_reply.s) {
      c = lst_top(cmd_list);
      if (c->command != CMD_CONNECT)
	 err_internal( __FUNCTION__, "Not connected, but no CMD_CONNECT\n" );
      if ((cmd_reply.s = net_connect_tcp( c->host, c->service )) < 0) {
	 err_fatal( __FUNCTION__,
		    "Can't connect to %s.%s\n", c->host, c->service );
      }
      cmd_reply.host    = c->host;
      cmd_reply.service = c->service;
   }
   if ((len = strlen(buffer))) {
      char *pt;
      
      PRINTF(DBG_RAW,("* Sent/%d: %s",c->command,buffer));
      pt = alloca(2*len);
      client_crlf(pt,buffer);
      net_write( cmd_reply.s, pt, strlen(pt) );
   } else {
      PRINTF(DBG_RAW,("* Send/%d\n",c->command)); 
   }
}

static void process( void )
{
   struct cmd *c;
   int        expected;
   const char *message = NULL;
   int        i;
   static int first = 1;
   int        *listed;
   
   while ((c = lst_top( cmd_list ))) {
      request();		/* Send requests */
      lst_pop( cmd_list );
      expected = CODE_OK;
      switch (c->command) {
      case CMD_PRINT:
	 if (!cmd_reply.data) {
	    printf( "Error, status %d\n", cmd_reply.retcode );
	 } else {
	    if (cmd_reply.matches)     client_print_matches( cmd_reply.data );
	    else if (cmd_reply.listed) client_print_listed( cmd_reply.data );
	    else                       client_print_text( cmd_reply.data );
	    client_free_text( cmd_reply.data );
	    cmd_reply.data = NULL;
	    cmd_reply.matches = 0;
	 }
	 expected = cmd_reply.retcode;
	 break;
      case CMD_DEFPRINT:
	 if (cmd_reply.count) {
	    for (i = 0; i < cmd_reply.count; i++) {
	       if (!first) printf( "\n\n" );
	       first = 0;
	       if (cmd_reply.defs[i].dbname) {
		  if (cmd_reply.defs[i].db)
		     printf( "From %s (%s):\n\n",
			     cmd_reply.defs[i].dbname, cmd_reply.defs[i].db );
		  else 
		     printf( "From %s:\n\n", cmd_reply.defs[i].dbname );
	       } else if (cmd_reply.defs[i].db) {
		  printf( "From %s:\n\n", cmd_reply.defs[i].db );
	       } else
		  printf( "From an unknown database:\n\n" );
	       client_print_text( cmd_reply.defs[i].data );
	       client_free_text( cmd_reply.defs[i].data );
	       cmd_reply.defs[i].data = NULL;
	    }
	    xfree( cmd_reply.defs );
	    cmd_reply.count = 0;
	 } else if (cmd_reply.matches) {
	    printf( "No definitions found in %s for \"%s\","
		    " perhaps you mean:\n",
		    c->database, c->word );
	    client_print_matches( cmd_reply.data );
	    client_free_text( cmd_reply.data );
	    cmd_reply.data = NULL;
	    cmd_reply.matches = 0;
	 } else {
	    printf( "No definitions found in %s for \"%s\"\n",
		    c->database, c->word );
	 }
	 expected = cmd_reply.retcode;
	 break;
      case CMD_CONNECT:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL,
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
						 NULL, NULL, NULL, NULL, NULL);
	 break;
      case CMD_AUTH:
	 if (!c->key || !c->user) break;
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL, NULL);
	 if (cmd_reply.retcode == CODE_AUTH_DENIED)
	    err_warning( __FUNCTION__,
			 "Authentication to %s.%s denied\n",
			 cmd_reply.host,
			 cmd_reply.service );
	 expected = CODE_AUTH_OK;
      case CMD_INFO:
	 expected = CODE_DATABASE_INFO;
	 listed = NULL;
	 goto gettext;
      case CMD_SERVER:
	 expected = CODE_SERVER_INFO;
	 listed = NULL;
	 goto gettext;
      case CMD_HELP:
	 expected = CODE_HELP;
	 listed = NULL;
	 goto gettext;
      case CMD_DBS:
	 expected = CODE_DATABASE_LIST;
	 listed = &cmd_reply.listed;
	 goto gettext;
      case CMD_STRATS:
	 expected = CODE_STRATEGY_LIST;
	 listed = &cmd_reply.listed;
	 goto gettext;
   gettext:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 listed,
						 NULL, NULL, NULL, NULL);
	 if (cmd_reply.retcode == expected) {
	    cmd_reply.data = client_read_text( cmd_reply.s );
	    cmd_reply.retcode = client_read_status( cmd_reply.s,
						    &message,
						    NULL,NULL,NULL,NULL,NULL);
	    expected = CODE_OK;
	 }
	 break;
      case CMD_DEFINE:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 &cmd_reply.count,
						 NULL, NULL, NULL, NULL );
	 if (!client_defines) tim_start( "define" );
	 switch (expected = cmd_reply.retcode) {
	 case CODE_DEFINITIONS_FOUND:
	    cmd_reply.defs = xmalloc(cmd_reply.count*sizeof(struct def));
	    expected = CODE_DEFINITION_FOLLOWS;
	    for (i = 0; i < cmd_reply.count; i++) {
	       ++client_defines;
	       cmd_reply.retcode
		  = client_read_status( cmd_reply.s,
					&message,
					NULL,
					&cmd_reply.defs[i].word,
					&cmd_reply.defs[i].db,
					&cmd_reply.defs[i].dbname,
					NULL );
	       if (cmd_reply.retcode != expected) goto error;
	       cmd_reply.defs[i].data = client_read_text( cmd_reply.s );
	    }
	    expected = CODE_OK;
	    cmd_reply.retcode = client_read_status( cmd_reply.s,
						    &message,
						    NULL,NULL,NULL,NULL,NULL );
	    break;
	 case CODE_NO_MATCH:
	    PRINTF(DBG_VERBOSE,
		   ("No match found for \"%s\" in %s\n",c->word,c->database));
	    break;
	 case CODE_INVALID_DB:
	    printf( "%s is not a valid database, use -D for a list\n",
		    c->database );
	    break;
	 case CODE_NO_DATABASES:
	    printf( "There are no databases currently available\n" );
	    break;
	 default:
	    expected = CODE_OK;
	 }
   error:
	 break;
      case CMD_MATCH:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 &cmd_reply.matches,
						 NULL, NULL, NULL, NULL );
	 switch (expected = cmd_reply.retcode) {
	 case CODE_MATCHES_FOUND:
	    cmd_reply.data = client_read_text( cmd_reply.s );
	    expected = CODE_OK;
	    cmd_reply.retcode = client_read_status( cmd_reply.s,
						    &message,
						    NULL,NULL,NULL,NULL,NULL );
	    break;
	 case CODE_NO_MATCH:
	    PRINTF(DBG_VERBOSE,
		   ("No match found in %s for \"%s\" using %s\n",
		    c->database,c->word,c->strategy));
	    break;
	 case CODE_INVALID_DB:
	    printf( "%s is not a valid database, use -D for a list\n",
		    c->database );
	    break;
	 case CODE_INVALID_STRATEGY:
	    printf( "%s is not a valid search strategy, use -S for a list\n",
		    c->strategy );
	    break;
	 case CODE_NO_DATABASES:
	    printf( "There are no databases currently available\n" );
	    break;
	 case CODE_NO_STRATEGIES:
	    printf( "There are no search strategies currently available\n" );
	    break;
	 default:
	    expected = CODE_OK;
	 }
	 break;
      case CMD_SPELL:
	 if (cmd_reply.retcode == CODE_NO_MATCH) {
	    prepend_command( make_command( CMD_MATCH,
					   c->database, DEF_STRAT, c->word ) );
	 }
	 expected = cmd_reply.retcode;
	 break;
      case CMD_WIND:
	 if (cmd_reply.matches) {
	    if (!cmd_reply.data)
	       err_internal( __FUNCTION__,
			     "%d matches, but no list\n", cmd_reply.matches );
	    for (i = cmd_reply.matches; i > 0; --i) {
	       const char *line = lst_nth_get( cmd_reply.data, i );
	       arg_List   a = arg_argify( line, 0 );
	       if (arg_count(a) != 2)
		  err_internal( __FUNCTION__,
				"MATCH command didn't return 2 args: %s\n",
				line );
	       prepend_command( make_command( CMD_DEFPRINT,
					      str_find(arg_get(a,0)),
					      str_copy(arg_get(a,1)) ) );
	       prepend_command( make_command( CMD_DEFINE,
					      str_find(arg_get(a,0)),
					      str_copy(arg_get(a,1)) ) );
	       arg_destroy(a);
	    }
	    client_free_text( cmd_reply.data );
	    cmd_reply.matches = 0;
	 } else {
	    printf( "No matches found in %s for \"%s\" using %s\n",
		    c->database,
		    c->word,
		    c->strategy );
	 }
	 expected = cmd_reply.retcode;
	 break;
      case CMD_CLOSE:
	 cmd_reply.retcode = client_read_status( cmd_reply.s,
						 &message,
						 NULL, NULL, NULL, NULL, NULL);
	 expected = CODE_GOODBYE;
	 break;
      default:
	 err_internal( __FUNCTION__, "Illegal command %d\n", c->command );
      }
      if (cmd_reply.retcode != expected) {
	 err_fatal( __FUNCTION__,
		    "Unexpected status code %d (%s), wanted %d\n",
		    cmd_reply.retcode,
		    message ? message : "no message",
		    expected );
      }
      PRINTF(DBG_RAW,("* Processed %d\n",c->command));
      xfree(c);
   }
}

#if 0
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
#endif

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
   const char        *id = "$Id: dict.c,v 1.8 1997/07/09 04:00:57 faith Exp $";
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
   0 };
   const char        **p = license_msg;
   
   banner();
   while (*p) fprintf( stderr, "   %s\n", *p++ );
}
    
static void help( void )
{
   static const char *help_msg[] = {
      "-h --host <server>      specify server",
      "-p --port <service>     specify port",
      "-d --database <dbname>  select a database to search",
      "-m --match              match instead of define",
      "-s --strategy           strategy for matching or defining",
      "-D --dbs                show available databases",
      "-S --strats             show available search strategies",
      "-H --serverhelp         show server help",
      "-i --info <dbname>      show information about a database",
      "-I --serverinfo         show information about the server",
      "-a --noauth             disable authentication",
      "-u --user <username>    username for authentication",
      "-k --key <key>          shared secret for authentication",
      "-V --version            display version information",
      "-L --license            display copyright and license information",
      "   --help               display this help",
      "-v --verbose            be verbose",
      "-r --raw                trace raw transaction",
      "   --debug <flag>       set debugging flag",
      0 };
   const char        **p = help_msg;

   banner();
   while (*p) fprintf( stderr, "%s\n", *p++ );
}

int main( int argc, char **argv )
{
   int                c;
   const char         *service   = "2628";
   const char         *host      = "localhost";
   const char         *database  = "*";
   const char         *strategy  = DEF_STRAT;
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

   setsig(SIGHUP,  handler);
   setsig(SIGINT,  handler);
   setsig(SIGQUIT, handler);
   setsig(SIGILL,  handler);
   setsig(SIGTRAP, handler);
   setsig(SIGTERM, handler);
   setsig(SIGPIPE, handler);
#endif

   fflush(stdout);
   fflush(stderr);

   tim_start("total");

   append_command( make_command( CMD_CONNECT, host, service ) );
   append_command( make_command( CMD_CLIENT, client_get_banner() ) );
   if (doauth) append_command( make_command( CMD_AUTH, user, key ) );
   switch (function) {
   case INFO:
      append_command( make_command( CMD_INFO, database ) );
      append_command( make_command( CMD_PRINT ) );
      break;
   case SERVER:
      append_command( make_command( CMD_SERVER ) );
      append_command( make_command( CMD_PRINT ) );
      break;
   case DBS:
      append_command( make_command( CMD_DBS ) );
      append_command( make_command( CMD_PRINT ) );
      break;
   case STRATS:
      append_command( make_command( CMD_STRATS ) );
      append_command( make_command( CMD_PRINT ) );
      break;
   case HELP:
      append_command( make_command( CMD_HELP ) );
      append_command( make_command( CMD_PRINT ) );
      break;
   case MATCH:
      for (i = optind; i < argc; i++) {
	 append_command( make_command( CMD_MATCH,
				       database, strategy, argv[i] ) );
	 append_command( make_command( CMD_PRINT ) );
      }
      break;
   case DEFINE:
      for (i = optind; i < argc; i++) {
	 if (!strcmp(strategy, DEF_STRAT)) {
	    append_command( make_command( CMD_DEFINE, database, argv[i] ) );
	    append_command( make_command( CMD_SPELL, database, argv[i] ) );
	    append_command( make_command( CMD_DEFPRINT, database, argv[i] ) );
	 } else {
	    append_command( make_command( CMD_MATCH,
					  database, strategy, argv[i] ) );
	    append_command( make_command( CMD_WIND,
					  database, strategy, argv[i] ) );
	 }
      }
   }
   append_command( make_command( CMD_CLOSE ) );
   process();
   
   if (dbg_test(DBG_TIME)) {
      tim_stop("total");
      if (client_defines) {
	 tim_stop("define");
	 fprintf( stderr,
		  "* %d definitions in %.2fr %.2fu %.2fs => %.1f d/sec\n",
		  client_defines,
		  tim_get_real( "define" ),
		  tim_get_user( "define" ),
		  tim_get_system( "define" ),
		  client_defines / tim_get_real( "define" ) );
      }
      fprintf( stderr,
	       "* total %.2fr %.2fu %.2fs\n",
	       tim_get_real( "total" ),
	       tim_get_user( "total" ),
	       tim_get_system( "total" ) );
   }
   
   return 0;
}
