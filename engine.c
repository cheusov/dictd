/* engine.c -- Engine for dict program
 * Created: Fri Dec  2 20:03:33 1994 by faith@cs.unc.edu
 * Revised: Thu Aug 24 01:13:47 1995 by r.faith@ieee.org
 * Copyright 1994, 1995 Rickard E. Faith (faith@cs.unc.edu)
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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#define MAX_DESCRIPTORS 10	/* really twice this */

static int   entries;
static int   maxEntries;
static int   descriptors;
static Entry *Entries;

int exact( char *target, char *pos, char *back )
{
   while (*target) {
      while (pos < back && *pos != '\t' && !(*pos == ' ' || isalnum( *pos )))
	    if (pos < back) ++pos;
      while (*target && !(*target == ' ' || isalnum( *target )))
	    if (target) ++target;
      if (*target) {
	 if (tolower( *target ) < tolower( *pos )) return -1;
	 if (tolower( *target ) > tolower( *pos )) return  1;
	 ++pos;
	 ++target;
      }
   }

   while (pos < back && *pos != '\t' && !(*pos == ' ' || isalnum( *pos )))
	 if (pos < back) ++pos;

   if (pos <= back && *pos == '\t') return  0;
   else                             return -1;
}

static void check_descriptors( void )
{
   int           i;
   unsigned long lru    = ~0L;
   Entry         *entry = NULL;

   if (descriptors > MAX_DESCRIPTORS) {
      for (i = 0; i < entries; i++) {
	 if (Entries[i].used && Entries[i].used < lru) {
	    lru   = Entries[i].used;
	    entry = &Entries[i];
	 }
      }

      if (entry) {
	 if (Debug) fprintf( stderr, "Spilling \"%s\"\n", entry->filename );

	 if (entry->fd >= 0) {
	    munmap( entry->front, entry->size );
	    close( entry->fd );
	    entry->fd = -1;
	 }

	 if (entry->str) {
	    fclose( entry->str );
	    entry->str = NULL;
	 }
      
	 --descriptors;
      }
   }
   ++descriptors;
}

static void create_map( Entry *entry )
{
   struct stat buf;

   check_descriptors();
      
   if (entry->fd < 0) {
      if ((entry->fd = open( entry->index, O_RDONLY, 0 )) < 0) {
	 if (!entry->indexalt) {
	    fprintf( stderr,
		     "Cannot open index \"%s\" for read\n",
		     entry->index );
	    exit( 1 );
	 }
	 if ((entry->fd = open( entry->indexalt, O_RDONLY, 0 )) < 0 ) {
	    fprintf( stderr,
		     "Cannot open index \"%s\" or \"%s\" for read\n",
		     entry->index, entry->indexalt );
	    exit( 1 );

	 }
      }
      
      if (fstat( entry->fd, &buf )) {
	 fprintf( stderr, "Cannot stat index \"%s\"\n", entry->index );
	 exit( 1 );
      }

      entry->size = buf.st_size;
      
      if ((void *)(entry->front = mmap( NULL,
					buf.st_size,
					PROT_READ,
					MAP_FILE|MAP_SHARED,
					entry->fd,
					0 )) <= (void *)0) {
	 fprintf( stderr, "Cannot mmap index \"%s\"\n", entry->index );
	 exit( 1 );
      }
      
      entry->back = entry->front + buf.st_size;
   }
}

static int lookup( char *word, Entry *entry, int match, int search,
		   int style, FILE *str )
{
   char *pos;
   char *folded;
   int  printed = 0;
   
   create_map( entry );

   folded = strdup( word );
   
   pos = look( folded, entry->front, entry->back );

   if (match != MATCH_NONE) {
      while (pos < entry->back) {
	 int cmp = compare( folded, pos, entry->back );

	 if (cmp > 0 || (printed && cmp < 0)) break;
      
	 if (!exact( folded, pos, entry->back )
	     || (match == MATCH_SUBSTRING && !cmp)) {
	    
	    print_entry( pos, entry, style, str );
	    ++printed;
	 }
	 
	 while (pos < entry->back && *pos++ != '\n');
      }
   }

   if (!printed && search)
	 print_possibilities( word, folded, entry, search, style, str );

   free( folded );

   return printed;
}

void list_entries( FILE *str )
{
   int i;
   
   for (i = 0; i < entries; i++) {
      if (Entries[i].name) fprintf( str, "%s", Entries[i].name );
      fprintf( str, "\t" );
      if (Entries[i].filename) fprintf( str, "%s", Entries[i].filename );
      fprintf( str, "\t" );
      if (Entries[i].description) fprintf( str, "%s", Entries[i].description );
      fprintf( str, "\n" );
   }
}

void add_entry( char *name, char *filename, char *index, char *description )
{
   int  i;
   
   if (entries >= maxEntries) {
      if (Entries)
	    Entries = realloc( Entries, sizeof( Entry ) * (maxEntries += 5) );
      else
	    Entries = malloc( sizeof( Entry ) * (maxEntries += 5) );
   }

   if (name)
	 Entries[entries].name        = strdup( name );
   else
	 Entries[entries].name        = NULL;
	       
   if (filename)
	 Entries[entries].filename    = strdup( filename );
   else
	 Entries[entries].filename    = NULL;


   if (index)
	 Entries[entries].index       = strdup( index );
   else
	 Entries[entries].index       = NULL;

   if (description)
	 Entries[entries].description = strdup( description );
   else
	 Entries[entries].description = NULL;
   
   Entries[entries].fd          = -1;
   Entries[entries].front       = NULL;
   Entries[entries].back        = NULL;
   Entries[entries].str         = NULL;
   Entries[entries].used        = 0;
   Entries[entries].wordlist    = NULL;

   for (i = 0; i < 128; i++)
      Entries[entries].ascii[i] = NULL;

   if (!Entries[entries].filename) {
      fprintf( stderr, "Internal error: filename NULL\n" );
      exit( 1 );
   }

   if (!Entries[entries].index) {
      Entries[entries].index = malloc( strlen( filename ) + 7 );
      strcpy( Entries[entries].index, filename );
      strcat( Entries[entries].index, ".index" );
   }
   
   Entries[entries].indexalt = malloc( strlen( filename ) + 5 );
   strcpy( Entries[entries].indexalt, filename );
   strcat( Entries[entries].indexalt, ".idx" );

   Entries[entries].listname = malloc( strlen( filename ) + 4 );
   strcpy( Entries[entries].listname, filename );
   strcat( Entries[entries].listname, ".cd" );

   ++entries;
}

void find( char *word, int action, int match, int search, int style,
	   char *name, FILE *str )
{
   int i;

   if (Debug) fprintf( stderr, "find %s in %s\n", word, name );
   
   switch (action) {
   case ACTION_FIRST:
      lookup( word, &Entries[0], match, search, style, str );
      break;
   case ACTION_NAMED:
      for (i = 0; i < entries; i++) {
	 if (!strcmp( Entries[i].name, name )) {
	    lookup( word, &Entries[i], match, search, style, str );
	    break;
	 }
      }
      break;
   case ACTION_ALL:
      for (i = 0; i < entries; i++) {
	 if (lookup( word, &Entries[i], match, 0, style, str )) break;
      }
      break;
   case ACTION_EXHAUST:
      for (i = 0; i < entries; i++) {
	 lookup( word, &Entries[i], match, 0, style, str );
      }
      break;
   }
}
