/* fmt.c -- 
 * Created: Sun Sep 22 15:56:04 1996 by faith@cs.unc.edu
 * Revised: Sun Sep 22 20:05:41 1996 by faith@cs.unc.edu
 * Copyright 1996 Rickard E. Faith (faith@cs.unc.edu)
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in
 *        the documentation and/or other materials provided with the
 *        distribution.  (Accompanying source code, or an offer for such
 *        source code as described in the GNU General Public License, is
 *        sufficient to meet this condition.)
 *  
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *  
 * $Id: fmt.c,v 1.1 1996/09/24 01:07:50 faith Exp $
 * 
 */

#include <stdio.h>
#include <string.h>
#include "base64.h"

#define MAX_WIDTH 73

#define BUFFERSIZE 10240

static int  indentLevel;
static int  currentPosition;
static int  firstString;
static FILE *dct;
static FILE *idx;

void fmt_newline( void )
{
   int i;
   
   fprintf( dct, "\n" );
   for (i = 0; i < indentLevel; i++) fprintf( dct, " " );
   currentPosition = indentLevel;
   firstString = 1;
}

void fmt_new( const char *word )
{
   static int first = 1;
   
   indentLevel = 0;
   currentPosition = 0;
   firstString = 1;
   if (!first) {
      fmt_newline();
      if (idx) fprintf( idx, "\t%s\n", base64_encode( ftell( dct ) ) );
      fmt_newline();
   } else
      first = 0;
   if (word && idx)
      fprintf( idx, "%s\t%s", word, base64_encode( ftell( dct ) ) );
}

void fmt_string( const char *string )
{
   char buf[BUFFERSIZE];
   char *pt;
   int  len;

   for (pt = buf; *string; string++) {
      if (*string == '_') *pt++ = ' ';
      else                *pt++ = *string;
   }
   *pt = '\0';

   pt = strtok( buf, " " );
   if (pt) {
      do {
	 len = strlen( pt );
	 if (currentPosition + len >= MAX_WIDTH) fmt_newline();
	 if (!firstString) fprintf( dct, " " );
	 fprintf( dct, "%s", pt );
	 firstString = 0;
	 currentPosition += len + 1;
      } while ((pt = strtok( NULL, " " )));
   } else {
      fmt_newline();
      fmt_newline();
   }
}

void fmt_def( const char *pos, int entry )
{
   char buf[BUFFERSIZE] = { 0, };

   indentLevel = 5;
   fmt_newline();
   firstString = 1;
   if (entry != -1) {
      if (entry) {
	 if (pos) sprintf( buf, "%s %d:", pos, entry );
	 else     sprintf( buf, "%d:", entry );
      } else {
	 if (pos) sprintf( buf, "%s:", pos );
      }
      fmt_string( buf );
   }
   indentLevel = currentPosition;
}

void fmt_open( const char *basename )
{
   char buf[BUFFERSIZE];

   if (!basename) {
      dct = stdout;
      idx = NULL;
   } else {
      sprintf( buf, "%s.dct", basename );
      if (!(dct = fopen( buf, "w" ))) {
	 fprintf( stderr, "Cannot open \"%s\" for write\n", buf );
	 exit( 1 );
      }
      
      sprintf( buf, "%s.idx", basename );
      if (!(idx = fopen( buf, "w" ))) {
	 fprintf( stderr, "Cannot open \"%s\" for write\n", buf );
	 exit( 1 );
      }
   }
}

void fmt_close()
{
   if (!dct) {
      fprintf( stderr, "File already closed!\n" );
      exit( 1 );
   }
   fmt_new( NULL );
   if (dct && dct != stdout) fclose( dct );
   if (idx && idx != stdout) fclose( idx );
}
