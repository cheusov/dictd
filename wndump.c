/* wndump.c -- Dump WordNet datafiles
 * Created: Sun Sep 22 14:03:26 1996 by faith@cs.unc.edu
 * Revised: Mon Sep 23 20:42:14 1996 by faith@cs.unc.edu
 * Copyright 1996 Rickard E. Faith (faith@cs.unc.edu)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.  (Accompanying source code, or an offer for such
 *       source code as described in the GNU General Public License, is
 *       sufficient to meet this condition.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $Id: wndump.c,v 1.1 1996/09/24 01:07:52 faith Exp $
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fmt.h"

#define PROGRAM_NAME    "wndump"
#define PROGRAM_VERSION "1.0"

#define BUFFERSIZE 10240

static void process( const char *w, const char *p, const char *d,
		     const char *lastW, const char *lastP,
		     const char *nextW, const char *nextP )
{
   static int entry = 1;
   
   if (!strcmp( lastW, w )) { /* same as last work */
      if (!strcmp( lastP, p )) { /* same pos */
	 if (p[0] != '!') fmt_def( NULL, ++entry );
	 fmt_string( d );
      } else {		/* new pos */
	 entry = 1;
	 fmt_def( p, !strcmp( nextW, w ) && !strcmp( nextP, p ) ? entry : 0 );
	 fmt_string( d );
      }
   } else {		/* new word */
      fmt_new( w );
      fmt_string( w );
      entry = 1;
      if (p[0] == '!')
	 fmt_def( NULL, -1 );
      else
	 fmt_def( p, !strcmp( nextW, w ) && !strcmp( nextP, p ) ? entry : 0 );
      fmt_string( d );
   }
}

static void usage( void )
{
   fprintf( stderr,
	    "Usage: wndump 1 < data/WordNet/* | sort | wndump 2\n" );
   exit( 1 );
}

int main( int argc, char **argv )
{
   char buffer[BUFFERSIZE];
   char *pos;
   int  i;
   char *word[BUFFERSIZE];
   char *sense[BUFFERSIZE];
   char *pt;
   int  count;
   int  haveLicense = 0;
   int  license     = 0;
   long t;

   if (argc != 2 || (argv[1][0] != '1'
		     && argv[1][0] != '2'
		     && argv[1][0] != 'd')) usage();

   if (argv[1][0] == '1') {
      time( &t );
      printf( "!info! ! %02d Original database from:"
	      " ftp://clarity.princeton.edu/pub/wordnet/wn1.5unix.tar.gz\n",
	      ++license );
      printf( "!info! ! %02d \n", ++license );
      printf( "!info! ! %02d This human-readable version generated from the"
	      " pristine database sources on %24.24s by %s, version %s"
	      " (written by Rik Faith, faith@cs.unc.edu).  The following"
	      " restrictions apply to the original database, and to this"
	      " derivative work.  No additional restrictions are claimed.\n",
	      ++license, ctime(&t), PROGRAM_NAME, PROGRAM_VERSION );
      printf( "!info! ! %02d \n", ++license );

      while (fgets( buffer, BUFFERSIZE-1, stdin )) {
	 if (!haveLicense) {
	    if (buffer[0] == ' ' && buffer[1] == ' ') {
	       pt = strchr( buffer + 2, ' ' );
	       pt[ strlen(pt) - 3] = '\0';
	       printf( "!info! ! %02d %s\n", ++license, pt+1 );
	    } else {
	       ++haveLicense;
	    }
	    continue;
	 }
	 pt = strchr( buffer, '|' );
	 if (pt) pt[ strlen(pt) - 3] = '\0';
	 strtok( buffer, " " );	/* synset_offset */
	 strtok( NULL, " " );	/* lex_file_num */
	 pos = strtok( NULL, " " ); /* pos */
	 if (!pos) continue;
	 count = atoi( strtok( NULL, " " ) ); /* id */
	 for (i = 0; i < count; i++) {
	    word[i] = strdup( strtok( NULL, " " ) );
	    sense[i] = strdup( strtok( NULL, " " ) );
	 }
	 for (i = 0; i < count; i++) {
	    printf( "%s ", word[i] );
	    switch (*pos) {
	    case 'a': printf( "adj" );     break;
	    case 's': printf( "adj" );     break;
	    case 'r': printf( "adv" );     break;
	    default:  printf( "%s", pos ); break;
	    }
	    printf( " %s", sense[i] );
	    if (pt) printf( " %s", pt + 2 );
	    if (count > 1) {
	       int j;
	       printf( " [syn: " );
	       for (j = 0; j < count; j++) {
		  if (j == i) continue;
		  printf( "%s", word[j] );
		  if (!((i != count - 1 && j == count - 1)
			|| (i == count - 1 && j == count - 2))) {
		     printf( "; " );
		  }
	       }
	       printf( "]" );
	    }
	    printf( "\n" );
	 }
	 for (i = 0; i < count; i++) {
	    free( word[i] );
	    free( sense[i] );
	 }
      }
   } else {
      char lastW[BUFFERSIZE] = { '\0', };
      char lastP[BUFFERSIZE] = { '\0', };
      char w[BUFFERSIZE]     = { '\0', };
      char p[BUFFERSIZE]     = { '\0', };
      char d[BUFFERSIZE]     = { '\0', };
      char *nextW, *nextP, *nextD;

      if (fgets( buffer, BUFFERSIZE-1, stdin )) {
	 strcpy( w, strtok( buffer, " " ) );
	 strcpy( p, strtok( NULL, " " ) );
	 strtok( NULL, " " );
	 strcpy( d, strtok( NULL, "\0" ) );

	 fmt_open( argv[1][0] == 'd' ? NULL : "wordnet" );
	 while (fgets( buffer, BUFFERSIZE-1, stdin )) {
	    nextW = strtok( buffer, " " );
	    nextP = strtok( NULL, " " );
	    strtok( NULL, " " );
	    nextD = strtok( NULL, "\0" );
	    if (d[0]) {
	       d[ strlen(d)-1 ] = '\0'; /* remove newline */
	       process( w, p, d, lastW, lastP, nextW, nextP );
	    }
	    strcpy( lastW, w );
	    strcpy( lastP, p );
	    strcpy( w, nextW );
	    strcpy( p, nextP );
	    if (nextD) strcpy( d, nextD );
	    else       strcpy( d, "" );
	 }

	 process( w, p, d, lastW, lastP, "", "" );
	 fmt_close();
      } else {
	 usage();
      }
   }
   
   return 0;
}
