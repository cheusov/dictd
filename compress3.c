/* compress.c -- 
 * Created: Sun Aug 20 12:55:22 1995 by r.faith@ieee.org
 * Revised: Tue Aug 22 22:48:32 1995 by r.faith@ieee.org
 * Copyright 1995 Rickard E. Faith (r.faith@ieee.org)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
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
 * $Id: compress3.c,v 1.1 1996/09/23 15:33:22 faith Exp $
 *
 * Typical usage:
 *
 * compress -a < dictionary | sort -r | cut -f2 > wordlist.tmp
 * head -4080 < wordlist.tmp > wordlist
 * tail +4081 < wordlist.tmp | grep -v '^..$' >> wordlist
 * compress wordlist < dictionary > compressed_dictionary
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define MAXWORDS 199155

char Buffer[BUFSIZ];

struct wordlist {
   char *word;
   int  freq;
   int  length;
   int  value;
};

struct wordlist wordlist[MAXWORDS];
int             count;

char *ascii[128];

struct idx {
   char          *word;
   unsigned long offset;
};

struct idx Idx[100000];
int        IdxCount;

typedef struct bucket {
   const char    *key;
   int           datum;
   struct bucket *next;
} *bucketType;

typedef struct table {
   unsigned long prime;
   unsigned long entries;
   bucketType    *buckets;
} *tableType;

typedef tableType hsh_HashTable;

hsh_HashTable h;

unsigned long hsh_string_hash( const char *key )
{
   const char    *pt = (char *)key;
   unsigned long h  = 0;

   while (*pt) {
      h += *pt++;
      h *= 2654435789U;		/* prime near %$\frac{\sqrt{5}-1}{2}2^{32}$% */
   }

   return h;
}

int hsh_string_compare( const char *key1, const char *key2 )
{
   return strcmp( key1, key2 );
}

hsh_HashTable hsh_create( void )
{
   tableType     t;
   unsigned long i;
   unsigned long prime = 200003;
   
   t             = malloc( sizeof( struct table ) );
   t->prime      = prime;
   t->entries    = 0;
   t->buckets    = malloc( prime * sizeof( struct bucket ) );

   for (i = 0; i < prime; i++) t->buckets[i] = NULL;

   return t;
}


static void _hsh_insert( hsh_HashTable table, unsigned int hash,
			 const char *key, int datum )
{
   tableType     t = (tableType)table;
   unsigned long h = hash % t->prime;
   bucketType    b;
   
   b        = malloc( sizeof( struct bucket ) );
   b->key   = key;
   b->datum = datum;
   b->next  = NULL;
   
   if (t->buckets[h]) b->next = t->buckets[h];
   t->buckets[h] = b;
   ++t->entries;
}

int hsh_insert( hsh_HashTable table, const char *key, int datum )
{
   tableType     t         = (tableType)table;
   unsigned long hashValue = hsh_string_hash( key );
   unsigned long h;

   h = hashValue % t->prime;

   if (t->buckets[h]) {		/* Assert uniqueness */
      bucketType pt;
      
      for (pt = t->buckets[h]; pt; pt = pt->next)
	    if (!hsh_string_compare( pt->key, key )) return 1;
   }

   _hsh_insert( t, hashValue, key, datum );
   return 0;
}

int hsh_increment( hsh_HashTable table, const char *key )
{
   tableType     t         = (tableType)table;
   unsigned long hashValue = hsh_string_hash( key );
   unsigned long h;

   h = hashValue % t->prime;

   if (t->buckets[h]) {		/* Assert uniqueness */
      bucketType pt;
      
      for (pt = t->buckets[h]; pt; pt = pt->next)
	 if (!hsh_string_compare( pt->key, key )) {
	    ++pt->datum;
	    return pt->datum;
	 }
   }

   _hsh_insert( t, hashValue, strdup( key ), 1 );
   return 1;
}


int hsh_retrieve( hsh_HashTable table, const char *key )
{
   tableType     t = (tableType)table;
   unsigned long h = hsh_string_hash( key ) % t->prime;

   if (t->buckets[h]) {
      bucketType pt;
      bucketType prev;
      
      for (prev = NULL, pt = t->buckets[h]; pt; prev = pt, pt = pt->next)
	    if (!hsh_string_compare( pt->key, key )) {
	       if (prev) {
				/* Self organize */
		  prev->next    = pt->next;
		  pt->next      = t->buckets[h];
		  t->buckets[h] = pt;
	       }
	       return pt->datum;
	    }
   }

   return 0;
}

void hsh_iterate( hsh_HashTable table,
		  int (*iterator)( const char *key, int datum ) )
{
   tableType     t = (tableType)table;
   unsigned long i;

   for (i = 0; i < t->prime; i++) {
      if (t->buckets[i]) {
	 bucketType pt;
	 
	 for (pt = t->buckets[i]; pt; pt = pt->next)
	       if (iterator( pt->key, pt->datum ))
		     return;
      }
   }
}

int hsh_entries( hsh_HashTable table )
{
   return table->entries;
}

int print( const char *key, int datum )
{
   if (datum > 1) printf( "%8d\t%s\n", datum, key );
   return 0;
}

/* 0x80..0xfc 0x01..0xff: 5 bits -> 32 * 255
   Actually, we reserve 0xc0 for space at eol, so 4 bits -> 16 * 255 = 4080
   0xfd..0xff 0x01..0xff 0x01..0xff: 2 bits -> 3 * 255 * 255 = 195075

   for a total of 199155
*/

int decode( char *c )
{
   int value;
   int code[3];

   code[0] = (unsigned char)*c++;
   code[1] = (unsigned char)*c++;
   code[2] = (unsigned char)*c;
   
   if (code[0] <= 0xfc) {
      value = ((code[0] & 0x7c) >> 2) * 255 + code[1] - 1;
   } else {
      value = ((code[0] & 0x03) - 1) * 255 * 255
	      + (code[1] - 1) * 255
	      + code[2] - 1;
   }
   return value;
}

char *encode( int value )
{
   static char buffer[5];

   if (value < 255 * 16) {
      int set = value / 255;
      
      buffer[0] = 0x80 + (set << 2);
      buffer[1] = value - set * 255 + 1;
      buffer[2] = 0;
   } else {
      int set = value / 255 / 255;
      int r = value - set * 255 * 255;
      int subset = r / 255;
      
      buffer[0] = 0xfc + set + 1;
      buffer[1] = subset + 1;
      buffer[2] = r - subset * 255 + 1;
      buffer[3] = 0;
   }
   return buffer;
}

int maximize( const char *key, int datum )
{
   int len = strlen( key );
   int i;
   int flag = 0;
   
   if (datum == 1) {
#if 0
      if (len > 4) hsh_increment( h, key + len - 4 );
      else if (len > 3) hsh_increment( h, key + len - 3 );
      else if (len > 2) hsh_increment( h, key + len - 2 );
#else
      for (i = 1; i < len - 1; i++)
	 if (hsh_retrieve( h, key + i )) {
	    hsh_increment( h, key + i );
	    ++flag;
	    break;
	 }
#if 0
      if (!flag) hsh_increment( h, key + 1 );
#endif
#endif
   }
   return 0;
}

void do_analyze( void )
{
   int lines = 0;
   int count = 0;
   int ascii[128] = { 0 };
   int i;

   static int count_ascii( const char *key, int datum )
      {
	 if (datum == 1) {
	    char *pt;

	    for (pt = key; *pt; pt++) ++ascii[ (unsigned char)*pt ];
	 }
	 return 0;
      }
   
   while (fgets( Buffer, BUFSIZ, stdin )) {
      char *pt;
      char *word;

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words (%d unique) on %d lines\n",
		  count, hsh_entries( h ), lines );

      for (pt = Buffer; *pt; pt++) {
	 if ((unsigned char)*pt >= 0x80) {
	    fprintf( stderr, "We're fucked: found character code %d\n", *pt );
	 } else {
/* 	    ++ascii[ (unsigned char)*pt ]; */
	 }
      }
      
      for (word = pt = Buffer; *pt; pt++) {
	 if (*pt == ' ' || *pt == '\n') {
	    *pt = '\0';
	    if (strlen( word ) > 1) {
	       hsh_increment( h, word );
	       ++count;
	    }
	    word = pt + 1;
	 }
      }
   }

   printf( "!" );
   for (i = 1; i < 128; i++)
      if (!ascii[i] && i != 8 && i != '\r' && i != 127) printf( " %d", i );
   printf( "\n" );

#if 0
   hsh_iterate( h, maximize );
#endif
   hsh_iterate( h, print );
   hsh_iterate( h, count_ascii );
   for (i = 1; i < 128; i++) {
      if (ascii[i]) printf( "%8d\tASCII%d\n", ascii[i], i );
   }
}

void do_encode( void )
{
   int           lines   = 0;
   int           count   = 0;
   FILE          *str    = NULL;
   int           current = 0;
   unsigned long next = 0, in = 0, out = 0;

   if (IdxCount) {
      if (!(str = fopen( "idx.tmp", "w" ))) {
	 fprintf( stderr, "Cannot open \"%s\" for write\n", "idx.tmp" );
	 exit( 37 );
      }
   }

   if (str) {
      next = Idx[ current ].offset;
      if (in == next) {
	 fprintf( str, "%s\t%lu\n", Idx[ current ].word, out );
	 next = Idx[ ++current ].offset;
      }
   }
   
   while (fgets( Buffer, BUFSIZ, stdin )) {
      char *pt;
      char *word;

      in += strlen( Buffer );

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words encoded on %d lines\n",
		  count, lines );
      
      for (word = pt = Buffer; *pt; pt++) {
	 if (*pt == ' ' || *pt == '\n') {
	    char orig  = *pt;
	    int  value = 0;
	    int  len;
	    int  i;

	    *pt = '\0';
	    len = strlen( word );

	    for (i = 1; i < len; i++) {
	       if ((value = hsh_retrieve( h, word + i - 1 ))) {
		  int j;

		  for (j = 1; j < i; j++) {
		     putchar( *word++ );
		     ++out;
		  }
		  break;
	       }
	    }
	    
	    if (value) {
	       if (value < 0) {
		  putchar( -value );
		  ++out;
	       } else {
		  char *code = encode( value );
		  printf( "%s", code );
		  out += strlen( code );
	       }
	       ++count;

	       if (pt[1] == '\n') {
		  putchar( 0xc0 );
		  ++out;
	       }
	    } else {
	       if (*word && *word != '\n') {
		  printf( "%s", word );
		  out += strlen( word );
	       }
	       if (orig == ' ') {
		  putchar( ' ' );
		  ++out;
	       }
	    }
	    word = pt + 1;
	 }
      }
      putchar( '\n' );
      ++out;

      if (str) {
	 if (in == next) {
	    fprintf( str, "%s\t%lu\n", Idx[ current ].word, out );
	    next = Idx[ ++current ].offset;
	 }
	 if (next && in > next) {
	    fprintf( stderr, "in = %lu, next = %lu (out = %lu)\n",
		     in, next, out );
	    exit( 7 );
	 }
      }
   }
   fclose( str );
}

void do_decode( void )
{
#if 0
   int lines      = 0;
   int count      = 0;
   int need_space = 0;
   
   while (fgets( Buffer, BUFSIZ, stdin )) {
      char *pt;

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words encoded on %d lines\n",
		  count, lines );

      if (need_space && Buffer[0] != '\n') putchar( ' ' );
      need_space = 0;
      
      for (pt = Buffer; *pt; pt++) {
	 if ((unsigned char)*pt >= 0x80) {
	    if ((unsigned char)*pt != 0xc0) {
	       int value = decode( pt );
	       
	       printf( "%s", wordlist[ value ] );
	       
	       if ((unsigned char)*pt > 0xfc) pt += 2;
	       else                           pt += 1;
	       
	       if (pt[1] != '\n'
		   && !(pt[0] == '\n' && !pt[1])) putchar( ' ' );
	       if (pt[0] == '\n' && !pt[1]) ++need_space;
	       ++count;
	    }
	 } else if (ascii[ (unsigned char)*pt ]) {
	    printf( "%s", ascii[ (unsigned char)*pt ] );
	    
	    if (pt[1] != '\n'
		&& !(pt[0] == '\n' && !pt[1])) putchar( ' ' );
	    if (pt[0] == '\n' && !pt[1]) ++need_space;
	    ++count;
	 } else
	    putchar( *pt );
      }
   }
#endif
}

int compar( const void *a, const void *b )
{
   const struct idx *aa = (struct idx *)a;
   const struct idx *bb = (struct idx *)b;

   if (!aa->word) {
      if (!bb->word) return 0;
      else           return 1;
   }
   if (!bb->word) {
      if (!aa->word) return 0;
      else           return -1;
   }
   if (aa->offset < bb->offset) return -1;
   if (aa->offset > bb->offset) return 1;
   return 0;
}

void read_index( char *filename )
{
   FILE *str;
#if 0
   int  i;
#endif
   
   if (!(str = fopen( filename, "r" ))) {
      fprintf( stderr, "Cannot open \"%s\" for read\n", filename );
      exit( 4 );
   }

   while (fgets( Buffer, BUFSIZ, str )) {
      char *word = Buffer;
      char *pt;

      for (pt = Buffer; *pt && *pt != '\t'; ++pt);

      *pt = '\0';
      ++pt;
      
      Idx[ IdxCount ].word   = strdup( word );
      Idx[ IdxCount ].offset = atol( pt );
      ++IdxCount;
   }

   fclose( str );

   qsort( Idx, 100000, sizeof( struct idx ), compar );

#if 0
   for (i = 0; i < 100000; i++) {
      if (!Idx[i].word) break;
      printf( "%s\t%lu\n", Idx[i].word, Idx[i].offset );
   }
#endif
}

void read_list( char *filename )
{
   FILE *str;
   int  asciis    = 0;
   int  ascii_val = 0;
   int  first     = 1;
   char *pt;
   
   if (!(str = fopen( filename, "r" ))) {
      fprintf( stderr, "Cannot open \"%s\" for read\n", filename );
      exit( 4 );
   }
   
   while (fgets( Buffer, BUFSIZ, str )) {
      int len = strlen( Buffer );

      if (0 && Buffer[0] == '!' && first) {
	 char *number;

	 for (number = pt = Buffer + 2; *pt; pt++) {
	    if (*pt == ' ' || *pt == '\n') {
	       *pt = '\0';
	       ascii[ atoi( number ) ] = (void *)1; /* kludge */
	       number = pt + 1;
	       ++asciis;
	    }
	 }
	 fprintf( stderr, "Using %d single character codes\n", asciis );
	 first = 0;
      } else {
	 if (Buffer[len-1] == '\n') Buffer[len-1] = '\0';
	 if (0 && asciis) {
	    while (ascii[ ascii_val ] != (void *)1) ++ascii_val;
	    if (ascii_val > 128) {
	       fprintf( stderr, "ascii_val = %d\n", ascii_val );
	       exit( 42 );
	    }
	    fprintf( stderr, "%d gets %s\n", ascii_val, Buffer );
	    ascii[ ascii_val ] = strdup( Buffer );
	    hsh_insert( h, ascii[ ascii_val ], -ascii_val );
	    --asciis;
	 } else {
	    if (Buffer[0] == '!') continue;
	    pt = strchr( Buffer, '\t' );
	    *pt = '\0';
	    ++pt;
	    wordlist[count].word = strdup( pt );
	    wordlist[count].freq = atoi( Buffer );
	    hsh_insert( h, wordlist[count].word, count );
	 }
	 ++count;
      }
   }
   fclose( str );
   fprintf( stderr, "Using %d words for dictionary\n", count );
}

void usage( void )
{
   fprintf( stderr,
	    "usage: compress [-a] [-e wordlist] [-d wordlist] [-i index]\n" );
   exit( 1 );
}

void split( int start, int end, int half )
{
   int total = 0;
   int i;

   fprintf( stderr, "split( %d, %d, %d )\n", start, end, half );
   for (i = start; i <= end; i++) {
      total += wordlist[i].freq;
      ++wordlist[i].length;
      if (total >= half) {
	 if (half / 2 && i != start) {
	    fprintf( stderr, "  Subdividing at %d and %d\n", start, i+1 );
	    split( start, i, half / 2 );
	    split( i+1, end, half / 2 );
	 }
	 break;
      }
   }

   for (++i; i <= end; i++) {
      ++wordlist[i].length;
   }
}

void shannon_fano( void )
{
   int total = 0;
   int maxlength = 0;
   int numl[3000] = { 0 };
   int i;
   int bits = 0;
   
   for (i = 0; i < count; i++) total += wordlist[i].freq;

   split( 0, count-1, total / 2 );

   for (i = 0; i < count; i++) {
      if (wordlist[i].length > maxlength) maxlength = wordlist[i].length;
      ++numl[ wordlist[i].length ];
   }
   fprintf( stderr, "maxlength = %d\n", maxlength );
   for (i = 0; i <= maxlength; i++)
      fprintf( stderr, "numl[%d] = %d\n", i, numl[i] );
   for (i = 0; i < count; i++) {
      bits += wordlist[i].freq * wordlist[i].length;
#if 0
      fprintf( stderr, "%d = %s: %d\n",
	       i, wordlist[i].word, wordlist[i].length );
#endif
   }
   fprintf( stderr, "%d bits = %d bytes\n", bits, bits/8 );
   exit( 0 );
}

#define ANALYZE 1
#define ENCODE  2
#define DECODE  3

int main( int argc, char **argv )
{
   int           Function = 0;
   int           c;
   char          *wordlist = "";
   char          *indexfile = "";

   while ((c = getopt( argc, argv, "ae:d:i:" )) != -1)
      switch (c) {
      case 'a': Function = ANALYZE;                   break;
      case 'e': Function = ENCODE; wordlist = optarg; break;
      case 'd': Function = DECODE; wordlist = optarg; break;
      case 'i': indexfile = optarg;                   break;
      default: usage();
      }

   h = hsh_create();
   
   switch (Function) {
   case ANALYZE:
      do_analyze();
      break;
   case ENCODE:
      read_list( wordlist );
      shannon_fano();
/*       read_index( indexfile ); */
      do_encode();
      break;
   case DECODE:
      read_list( wordlist );
      do_decode();
      break;
   default:
      usage();
   }

   return 0;
}
