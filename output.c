/* output.c -- Output routines for dict program
 * Created: Sun Dec  4 09:12:37 1994 by faith@cs.unc.edu
 * Revised: Sun Dec  4 21:20:50 1994 by faith@cs.unc.edu
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

static void open_dict( Entry *entry )
{
   if (!entry->str) {
      if (!(entry->str = fopen( entry->filename, "r" ))) {
	 fprintf( stderr, "Cannot open \"%s\" for read\n", entry->filename );
	 exit( 1 );
      }
   }
}

void print_entry( char *pos, Entry *entry, int style, FILE *str )
{
   size_t offset;
   int    first  = 1;
   int    stop   = '\n';
   int    prev   = 0;
   int    devils = 0;

   open_dict( entry );

   if (style && STYLE_NORMAL) {
      if (entry->description && !(style & STYLE_QUIET))
	    fprintf( str, "\nFrom %s:\n\n", entry->description );
      else
	    fprintf( str, "\n" );
   }
   
   while (pos < entry->back && *pos++ != '\t');
   offset = atol( pos );

   if (Debug) fprintf( stderr, "Offset = %lu\n", (unsigned long)offset );

   fseek( entry->str, offset, SEEK_SET );

   while (fgets( buffer, BUFFER_SIZE, entry->str )) {
      char *pt = buffer;
      
      if (!devils && *pt == stop) {
	 if (!fgets( buffer, BUFFER_SIZE, entry->str ) || buffer[0] != '\t')
	       break;
	 else
	       fprintf( str, "\n" );
      }
      if (devils && prev == '\n' && isupper( *pt )) break;
      
      if (first) {
	 char *p   = buffer;

	 if (isupper( *p )) {
	    for (; *p; p++) {
	       if (*p == ',' || *p == '.') {
		  ++devils;
		  break;
	       }
	       if (isalpha( *p ) && !isupper( *p )) break;
	    }
	 }
	 
	 if (*pt == ':') stop = *pt;
	 if (*pt == '@') stop = *pt++;
	     
	 first = 0;
      }
      
      fprintf( str, "%s", pt );
      prev = *pt;
   }
}

/* The basic algorithm for the soundex routine is from Donald E. Knuth's
   THE ART OF COMPUTER PROGRAMMING, Volume 3: Sorting and Searching
   (Addison-Wesley Publishing Co., 1973, pages 391 and 392).  Knuth notes
   that the method was originally described by Margaret K. Odell and Robert
   C. Russell [US Patents 1261167 (1918) and 1435663 (1922)]. */

static char *soundex( char *string )
{
   static char result[5];
   char        *pt = result;
   int         upper_case;
   /*                   abcdefghijklmnopqrstuvwxyz */
   static char map[] = "01230120022455012623010202";
   char        previous = 0;
   char        transform;
   int         i;

   strcpy( result, "Z000" );

   for (i = 0; *string && i < 4; ++string) {
      if (isalpha( *string )) {
	 upper_case = toupper( *string );
	 transform  = map[ upper_case - 'A' ];
	 if (!i) {
	    *pt++ = upper_case;
	    ++i;
	 } else {
	    if (transform != '0' && transform != previous) {
	       *pt++ = transform;
	       ++i;
	    }
	 }
	 previous = transform;
      }
   }

   return result;
}

static void add_to_list( char *item, char ***list, int *count, int *maximum )
{
   int len = strlen( item );
   
   if (*count >= *maximum) {
      if (*list) *list = realloc( list, sizeof( char * ) * (*maximum += 100) );
      else       *list = malloc( sizeof( char * ) * (*maximum += 100 ) );
   }

   if (*item == '@' || *item == ':') {
      ++item;
      --len;
      if (item[ len - 1 ] == ':') --len;
   }

   (*list)[*count] = strdup( item );
   (*list)[*count][len] = '\0';
   ++(*count);
}

static int cmp( const void *a, const void *b )
{
   return strcmp( *(char **)a, *(char **)b );
}

int finished( char *pos, char *folded, char *back )
{
   while (pos < back && !isalnum( *pos )) ++pos;

   return tolower( *pos ) != tolower( *folded );
}

void search_substring( char *target, char *folded, Entry *entry,
		       char ***list, int *count, int *maximum )
{
}

/* I was unable to locate V. I. Levenshtein, "Binary codes capable of
   correcting deletions, insertions, and reversals,"
   Sov. Phys.-Dokt. 10(8): Feb. 1966, 707-710.

   So, I used Joseph J. Pollock and Antonio Zamora, "Automatic spelling
   correction in scientific and scholarly text," CACM, 27(4): Apr. 1985,
   358-368.  They point out (p. 363) that the precedence of these tests,
   when used for spelling correction, is OMISSION = TRANSPOSITION >
   INSERTION > SUBSTITUTION.  If list is not sorted, then this order should
   be used.

   In this routine, we only consider corrections with a Levenshtein
   distance of 1.

*/

void search_levenshtein( char *target, char *folded, Entry *entry,
			 char ***list, int *count, int *maximum )
{
   int  len = strlen( folded );
   int  i;
   int  j;
   int  k;
   char *pos;
   int  tmp;
   char *pt;
   char *front = entry->front;
   char *back  = entry->back;
   
   for (i = 1; i < len; i++) {

				/* Substitutions */
      strcpy( buffer, folded );
      for (j = 'a'; j <= 'z'; j++) {
	 buffer[i] = j;
	 if ((pos = look( buffer, front, back )) && !exact( buffer, pos, back))
	       add_to_list( buffer, list, count, maximum );
      }
	 
				/* Transpositions */
      strcpy( buffer, folded );
      tmp = buffer[i-1];
      buffer[i-1] = buffer[i];
      buffer[i] = tmp;
      if ((pos = look( buffer, front, back )) && !exact( buffer, pos, back ))
	    add_to_list( buffer, list, count, maximum );

				/* Deletions */
      for (pt = buffer, j = 0; j < len; j++)
	    if (i != j) *pt++ = folded[j];
      *pt = '\0';
      if ((pos = look( buffer, front, back )) && !exact( buffer, pos, back ))
	    add_to_list( buffer, list, count, maximum );
      
				/* Insertions */
      for (k = 'a'; k <= 'z'; k++) {
	 for (pt = buffer, j = 0; j < len; j++) {
	    *pt++ = folded[j];
	    if (i == j) *pt++ = k;
	 }
	 *pt = '\0';
	 if ((pos = look( buffer, front, back )) && !exact( buffer, pos, back))
	       add_to_list( buffer, list, count, maximum );
      }
   }
				/* Insertions at the end */

   strcpy( buffer, folded );
   buffer[ len + 1 ] = '\0';
   for (k = 'a'; k <= 'z'; k++) {
      buffer[ len ] = k;
      if ((pos = look( buffer, front, back )) && !exact( buffer, pos, back ))
	    add_to_list( buffer, list, count, maximum );
   }
}

void search_soundex( char *target, char *folded, Entry *entry,
		     char ***list, int *count, int *maximum )
{
   char save[10];
   char *pos;
   char *front = entry->front;
   char *back  = entry->back;
      
   strcpy( save, soundex( folded ) );
   
   if (Debug) fprintf( stderr, "%s => %s\n", folded, save );
   buffer[0] = *folded;
   buffer[1] = '\0';
   pos = look( buffer, front, back );
   while (pos && pos < back && !finished( pos, folded, back )) {
      char *pt = buffer;
	 
      while (pos < back && *pos != '\t') *pt++ = *pos++;
      *pt = '\0';
	 
      if (!strcmp( save, soundex( buffer ) ))
	    add_to_list( buffer, list, count, maximum );
	 
      while (pos < back && *pos++ != '\n');
   }
}


void print_possibilities( char *target, char *folded, Entry *entry,
			  int search, int style, FILE *str )
{
   int  i;
   char **list  = NULL;
   int  count   = 0;
   int  maximum = 0;

   if (search & SEARCH_SUBSTRINGS)
	 search_substring( target, folded, entry, &list, &count, &maximum );
   if (search & SEARCH_LEVENSHTEIN)
	 search_levenshtein( target, folded, entry, &list, &count, &maximum );
   if (search & SEARCH_SOUNDEX)
	 search_soundex( target, folded, entry, &list, &count, &maximum );

   if (!count) {
      fprintf( str, "\"%s\" not found.\n", target );
   } else {
      if (entry->description)
	    fprintf( str, "\"%s\" not found in %s, perhaps you mean:\n",
		     target,
		     entry->description);
      else
	    fprintf( str, "\"%s\" not found, perhaps you mean:\n", target );
   }

   if (count) {
      int len   = 0;
      int width = 0;
      
      qsort( list, count, sizeof( char * ), cmp );

      if (style & STYLE_SERVER) {
	 for (i = 0; i < count; i++) {
	    if (!i || strcmp( list[i], list[i-1] ))
		  fprintf( str, "%s\n", list[i] );
	 }
      } else {
	 for (i = 0; i < count; i++) {
	    int l = strlen( list[i] );
	    
	    if (l > width) width = l;
	 }
	 width += 4;
	 
	 for (i = 0; i < count; i++) {
	    if (!i || strcmp( list[i], list[i-1] )) {
	       if ((len += width) > 78) {
		  fprintf( str, "\n" );
		  len = width;
	       }
	       fprintf( str, "%*s", -width, list[i] );
	    }
	 }
	 fprintf( str, "\n" );
      }
   }
   
   for (i = 0; i < count; i++) free( list[i] );
   free( list );
}
