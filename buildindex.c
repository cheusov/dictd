/* buildindex.c -- Buildindex
 * Created: Sat Nov 26 23:38:07 1994 by faith@cs.unc.edu
 * Revised: Fri Dec  2 17:42:05 1994 by faith@cs.unc.edu
 * Copyright 1994 Rickard E. Faith (faith@cs.unc.edu)
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
 * NOTE: This program is not distributed with any data.  This program
 * operates on many different data types.  You are responsible for
 * obtaining any data upon which this program operates.
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef __linux__
#include <getopt.h>
#else
extern int optind;
#endif

#define buffer_size 1024

static int Debug;
static int jargon;
static int devils;
static int foldoc;
static int latin;
static int reformat;

static void add( FILE *str, char *word, unsigned long offset )
{
   fprintf( str, "%s\t%lu\n", word, offset );
}

int main( int argc, char **argv )
{
   FILE          *infile;
   FILE          *outfile;
   char          buffer[buffer_size];
   int           c;
   int           len;
   unsigned long lines     = 0;
   unsigned long headwords = 0;
   int           prev      = 0;

   while ((c = getopt( argc, argv, "Djdflr" )) != EOF)
	 switch (c) {
	 case 'D': ++Debug;    break;
	 case 'j': ++jargon;   break;
	 case 'd': ++devils;   break;
	 case 'f': ++foldoc;   break;
	 case 'l': ++latin;    break;
	 case 'r': ++reformat; break;
	 }
   
   if (argc - optind != 1
       || (!jargon && !devils && !foldoc && !latin && !reformat)) {
      fprintf( stderr, "usage: buildindex [-Djdflr] input_file\n" );
      return 1;
   }

   if (!(infile = fopen( argv[2], "r" ))) {
      fprintf( stderr, "Cannot open \"%s\" for input\n", argv[2] );
      return 1;
   }

   if (reformat) {
      int first = 1;
      
      while (fgets( buffer, buffer_size, infile )) {
	 if (first) first = 0;
	 else if (buffer[0] != ' ') printf( "\n" );
	 printf( "%s", buffer );
      }

      fclose( infile );
      return 0;
   }

   sprintf( buffer, "sort -d -f > %s.index", argv[optind] );

   if (!(outfile = popen( buffer, "w" ))) {
      fprintf( stderr, "Cannot open pipe for: \"%s\"\n", buffer );
      return 1;
   }

   while (fgets( buffer, buffer_size, infile )) {
      ++lines;
      len = strlen( buffer );
      
      if (!(lines % 1000)) {
	 fprintf( stderr, "\r%10lu lines, %lu headwords", lines, headwords );
      }
      
      if (jargon && buffer[0] == ':') {
	 char *pt = strchr( buffer + 1, ':' );

	 if (pt) {
	    *++pt = '\0';
	    add( outfile, buffer, ftell( infile ) - len );
	    ++headwords;
	 }
      }

      if (devils && prev == '\n' && isupper( buffer[0] )) {
	 char *pt = strchr( buffer, ',' );

	 if (!pt) pt = strchr( buffer, '.' );

	 if (pt) {
	    *pt = '\0';
	    add( outfile, buffer, ftell( infile ) - len );
	    ++headwords;
	 }
      }

      if (foldoc && prev == '\n' && buffer[0] != ' ' && buffer[0] != '\t') {
	 char *pt = strchr( buffer, '\n' );

	 if (pt) {
	    *pt = '\0';
	    add( outfile, buffer, ftell( infile ) - len );
	    ++headwords;
	 }
      }

      if (latin && buffer[0] != '\n' && buffer[0] != ' ') {
	 char *pt = buffer;

	 while (pt && (*pt == ' ' || isalpha( *pt ))) ++pt;
	 if (pt && pt[-1] == ' ') --pt;
	 while (pt && *pt == ' ') --pt;
	 if (pt && isalpha( *pt )) ++pt;

	 if (pt) {
	    *pt = '\0';
	    if (strlen( buffer )) {
	       add( outfile, buffer, ftell( infile ) - len );
	       ++headwords;
	    }
	 }
      }

      prev = buffer[0];
   }

   pclose( outfile );
   fclose( infile );

   fprintf( stderr, "\r%10lu lines, %lu headwords total\n", lines, headwords );

   return 0;
}
