/* dict.c -- Dictionary search program
 * Created: Sun Nov 27 08:11:35 1994 by faith@cs.unc.edu
 * Revised: Sun Dec  4 15:29:45 1994 by faith@cs.unc.edu
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

#include "dict.h"

char buffer[BUFFER_SIZE];
int  Debug;

#define LOCAL_BUFSIZ 1024

#ifdef __ultrix__
char strdup( char *src )
{
   char *dest = malloc( sizeof( char ) * (strlen( src ) + 1) );

   strcpy( dest, src );
   return dest;
}
#endif

static int read_database( char *filename )
{
   FILE *str;
   char *name;
   char *path;
   char *desc;
   int  count = 0;

   if (!(str = fopen( filename, "r" ))) return 0;

   while (fgets( buffer, BUFFER_SIZE, str )) {
      if (buffer[0] == '#') continue;
      name = strtok( buffer, " \t" );
      path = strtok( NULL, " \t" );
      desc = strtok( NULL, "\n" );

      add_entry( name, path, NULL, desc );
      ++count;
   }

   fclose( str );
   return count;
}

static void load_database( char *special )
{
   char *home = getenv( "HOME" );
   char buf[LOCAL_BUFSIZ];
   int  read = 0;

   if (special) {
      if (!read_database( special )) {
	 fprintf( stderr, "Cannot load \"%s\" as database\n", special );
	 exit( 1 );
      }
      return;
   }
   
   if (home) {
      if (strlen( home ) >= LOCAL_BUFSIZ - sizeof( DEFAULT_USER )) {
	 fprintf( stderr, "HOME environment variable too long\n" );
	 exit( 1 );
      }
      
      sprintf( buf, "%s/%s", home, DEFAULT_USER );
      read = read_database( buf );
   }

   if (!read) read = read_database( DEFAULT_SYST );
   if (!read) add_entry( DEFAULT_NAME, DEFAULT_PATH, NULL, DEFAULT_DESC );
}

int main( int argc, char **argv )
{
   char        *index      = NULL;
   char        *filename   = NULL;
   char        *dictionary = NULL;
   int         c;
   int         match       = MATCH_EXACT;
   int         search      = SEARCH_LEVENSHTEIN;
   int         style       = STYLE_NORMAL;
   int         action      = ACTION_FIRST;
   int         list        = 0;
   char        buf[LOCAL_BUFSIZ];
   char        *use        = NULL;
   
   while ((c = getopt( argc, argv, "DsSf:i:d:Iaelu:hq" )) != EOF)
	 switch (c) {
	 case 'D': ++Debug;                                           break;
	 case 's': match = MATCH_SUBSTRING;                           break;
	 case 'S': search |= SEARCH_SOUNDEX;                          break;
	 case 'f': filename = optarg;                                 break;
	 case 'i': index = optarg;                                    break;
	 case 'd': dictionary = optarg;      action = ACTION_NAMED;   break;
	 case 'a':                           action = ACTION_ALL;     break;
	 case 'e':                           action = ACTION_EXHAUST; break;
	 case 'I': style |= STYLE_SERVER;                             break;
	 case 'l': ++list;                                            break;
	 case 'u': use = optarg;                                      break;
	 case 'q': style |= STYLE_QUIET;                              break;
	 case 'h':
	 case '?':
	    fprintf( stderr, "Usage: dict options word\n" );
	    exit( 2 );
	 }

   if (filename) add_entry( NULL, filename, index, NULL );
   load_database( use );

   if (list) list_entries( stdout );

   if (argc - optind) {
      find( argv[optind], action, match, search, style, dictionary, stdout );
   } else {
      if (style & STYLE_SERVER) {
	 printf( "Server mode has not been implemented\n" );
      } else {
	 for (;;) {
	    printf( "Word: " );
	    fflush( stdout );
	    if (!fgets( buf, LOCAL_BUFSIZ, stdin ) || buf[0] == '\n') break;
	    find( buf, action, match, search, style, dictionary, stdout );
	 }
      }
   }

   return 0;
}
