/* jargdump.c -- Dump Jargon File
 * Created: Wed Sep 25 19:48:14 1996 by faith@cs.unc.edu
 * Revised: Wed Sep 25 20:55:03 1996 by faith@cs.unc.edu
 * Public Domain 1996 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: jargdump.c,v 1.1 1996/09/26 02:27:42 faith Exp $
 * 
 */

#include "dict.h"
#include <time.h>

#define PROGRAM_NAME    "jargdump"
#define PROGRAM_VERSION "1.0"

static void usage( void )
{
   fprintf( stderr, "Usage: jargdump [-d] < data/jarg*\n" );
   exit( 1 );
}

int main( int argc, char **argv )
{
   char buffer[BUFFERSIZE];
   char buffer2[BUFFERSIZE];
   long t;
   int  haveLicense = 0;
   char *pt;

   maa_init( argv[0] );
   time( &t );

   if (argc != 1 && argc != 2) usage();

   if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'd')
      fmt_open( NULL );
   else
      fmt_open( "jargon" );
   
   fmt_new( "!info!" );
   fmt_string( "!info!" );
   fmt_def( "", -1 );
   fmt_string( "Original database from:"
	       " ftp://prep.ai.mit.edu/pub/gnu/jarg400.txt.gz\n" );
   fmt_newline();
   fmt_newline();
   sprintf( buffer,
	    "This version generated from the"
	    " pristine database sources on %24.24s by %s, version %s"
	    " (written by Rik Faith, faith@cs.unc.edu).  The original"
	    " database is in the public domain.",
	    ctime(&t), PROGRAM_NAME, PROGRAM_VERSION );
   fmt_string( buffer );
   fmt_flush();

   while (fgets( buffer, BUFFERSIZE-1, stdin )) {
      buffer[ strlen(buffer)-1 ] = '\0';
      if (!strncmp( buffer, "******", 6 )
	  || !strncmp( buffer, "------", 6 )
	  || !strncmp( buffer + 1, "======", 6 )) continue;
      if (!haveLicense) {
	 if (buffer[0] == ':') {
	    haveLicense = 1;
	 } else {
	    fmt_line( buffer );
	    continue;
	 }
      }

      if (buffer[0] == ':' && buffer[1]) {
	 strcpy( buffer2, buffer );
	 pt = strchr( buffer2 + 1, ':' );
	 if (pt) *pt = '\0';
	 fmt_new( buffer2 + 1 );
	 fmt_line( buffer );
      } else {
	 fmt_line( buffer );
      }
   }
   fmt_close();
   
   return 0;
}
