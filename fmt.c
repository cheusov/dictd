/* fmt.c -- 
 * Created: Sun Sep 22 15:56:04 1996 by faith@cs.unc.edu
 * Revised: Wed Sep 25 22:00:04 1996 by faith@cs.unc.edu
 * Copyright 1996 Rickard E. Faith (faith@cs.unc.edu)
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
 * $Id: fmt.c,v 1.2 1996/09/26 02:27:40 faith Exp $
 * 
 */

#include "dict.h"

#define MAX_WIDTH 73

#define BUFFERSIZE 10240

static int  indentLevel;
static int  currentPosition;
static int  firstString;
static FILE *dct;
static FILE *idx;
static int  pending;
static int  count;

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
   char       buf[BUFFERSIZE];
   char       *pt;

   if (word) {
      for (pt = buf; *word; word++) {
	 if (*word == '_') *pt++ = ' ';
	 else              *pt++ = *word;
      }
      *pt = '\0';
   }
   
   pending = 0;
   indentLevel = 0;
   currentPosition = 0;
   firstString = 1;
   if (!first) {
      fmt_newline();
      if (idx) fprintf( idx, "\t%s\n", b64_encode( ftell( dct ) ) );
      fmt_newline();
   } else
      first = 0;
   if (word && idx) {
      fprintf( idx, "%s\t%s", buf, b64_encode( ftell( dct ) ) );
      ++count;
      if (count && !(count % 1000)) {
	 printf( "%10d words\r", count );
	 fflush( stdout );
      }
   }
}

void fmt_line( const char *line )
{
   if (strlen( line )) {
      while (pending) {
	 fmt_newline();
	 --pending;
      }
      fprintf( dct, "%s", line );
      fmt_newline();
   } else ++pending;
}

void fmt_flush( void )
{
   indentLevel = 0;
   fmt_newline();
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
      
      sprintf( buf, "sort -df > %s.idx", basename );
      if (!(idx = popen( buf, "w" ))) {
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
   if (idx && idx != stdout) pclose( idx );
   printf( "%12d words total\n", count );
   fflush( stdout );
}
