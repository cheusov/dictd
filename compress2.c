/* compress.c -- 
 * Created: Sun Aug 20 12:55:22 1995 by r.faith@ieee.org
 * Revised: Mon Aug 21 22:30:50 1995 by r.faith@ieee.org
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
 * $Id: compress2.c,v 1.1 1996/09/23 15:33:21 faith Exp $
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
char *wordlist[MAXWORDS];
char *ascii[128];

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

static void _hsh_destroy_buckets( hsh_HashTable table )
{
   unsigned long i;
   tableType     t    = (tableType)table;

   for (i = 0; i < t->prime; i++) {
      bucketType b = t->buckets[i];

      while (b) {
         bucketType next = b->next;

	 if (b->key) free( b->key );
         free( b );            /* terminal */
         b = next;
      }
   }

   free( t->buckets );         /* terminal */
   t->buckets = NULL;
}

static void _hsh_destroy_table( hsh_HashTable table )
{
   free( table );              /* terminal */
}
 
void hsh_destroy( hsh_HashTable table )
{
   _hsh_destroy_buckets( table );
   _hsh_destroy_table( table );
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

   _hsh_insert( t, hashValue, strdup( key ), datum );
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
   if (1 || datum > 1) printf( "%8d\t%s\n", datum, key );
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

void do_analyze( void )
{
   int lines = 0;
   int count = 0;
   int ascii[128] = { 0 };
   int i;
   
   while (fgets( Buffer, BUFSIZ, stdin )) {
      char *pt;
      char *word;
      int  len = strlen( Buffer );

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words (%d unique) on %d lines\n",
		  count, hsh_entries( h ), lines );

      for (pt = Buffer; *pt; pt++) {
	 if ((unsigned char)*pt >= 0x80) {
	    fprintf( stderr, "We're fucked: found character code %d\n", *pt );
	 } else {
	    ++ascii[ (unsigned char)*pt ];
	 }
      }

      Buffer[len-1] =
      Buffer[len] = 
      Buffer[len+1] = Buffer[len+2] = Buffer[len+3] = Buffer[len+4] = '\0';
      for (word = pt = Buffer; *pt; pt += 3) {
	 char orig = pt[3];
	 pt[3] = '\0';
	 hsh_increment( h, word );
	 pt[3] = orig;
	 word = pt + 3;
	 ++count;
      }
   }

   printf( "!" );
   for (i = 1; i < 128; i++)
      if (!ascii[i] && i != 8 && i != '\r') printf( " %d", i );
   printf( "\n" );

   hsh_iterate( h, print );
}

void do_encode( void )
{
   int lines = 0;
   int count = 0;
   
   while (fgets( Buffer, BUFSIZ, stdin )) {
      char *pt;
      char *word;
      int len = strlen( Buffer );

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words encoded on %d lines\n",
		  count, lines );

      Buffer[len-1] =
      Buffer[len] = 
      Buffer[len+1] = Buffer[len+2] = Buffer[len+3] = Buffer[len+4] = '\0';
      for (word = pt = Buffer; *pt; pt += 3) {
	 int  value;
	 char orig = pt[3];
	 int  msb;
	 int  lsb;
	 
	 pt[3] = '\0';
	 value = hsh_retrieve( h, word );
	 pt[3] = orig;
	 word = pt + 3;
	 ++count;

	 msb = value / 253;
	 lsb = value - msb * 253 + 1;
	 ++msb;
	 if (msb == '\n') msb = 255;
	 if (lsb == '\n') lsb = 255;
	 
	 printf( "%c%c", msb, lsb );
      }
      putchar( '\n' );
   }
}

void do_decode( void )
{
   int lines      = 0;
   int count      = 0;
   int need_space = 0;
   
   while (fgets( Buffer, BUFSIZ, stdin )) {
      char *pt;
      int len = strlen( Buffer );

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words encoded on %d lines\n",
		  count, lines );
      
      Buffer[len-1] =
      Buffer[len] = 
      Buffer[len+1] = Buffer[len+2] = Buffer[len+3] = Buffer[len+4] = '\0';
      for (pt = Buffer; *pt; pt += 2) {
	 int msb = (unsigned char)pt[0];
	 int lsb = (unsigned char)pt[1];

	 if (msb == 255) msb = '\n';
	 if (lsb == 255) lsb = '\n';
	       
	 printf( "%s", wordlist[ (msb - 1) * 253 + lsb - 1 ] );
	 ++count;
      }
      putchar( '\n' );
   }
}

void read_list( char *filename )
{
   FILE *str;
   int  count     = 0;
   int  asciis    = 0;
   int  ascii_val = 0;
   int  first     = 1;
   
   if (!(str = fopen( filename, "r" ))) {
      fprintf( stderr, "Cannot open \"%s\" for read\n", filename );
      exit( 4 );
   }
   
   while (fgets( Buffer, BUFSIZ, str )) {
      int len = strlen( Buffer );

      if (Buffer[0] == '!' && first) {
	 char *pt;
	 char *number;

	 for (number = pt = Buffer + 2; *pt; pt++) {
	    if (*pt == ' ' || *pt == '\n') {
	       *pt = '\0';
#if 0
	       ascii[ atoi( number ) ] = (void *)1; /* kludge */
#endif
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
	    wordlist[count] = strdup( Buffer );
	    hsh_insert( h, wordlist[count], count );
	 }
	 ++count;
      }
   }
   fclose( str );
   fprintf( stderr, "Using %d words for dictionary\n", count );
}

void usage( void )
{
   fprintf( stderr, "usage: compress [-a] [-e wordlist] [-d wordlist]\n" );
   exit( 1 );
}

static void output( char *string )
{
   int value;
   
   if (!string[1]) putchar( string[0] );
   else {
      value = hsh_retrieve( h, string );
      if ( value < 127) {
	 fprintf( stderr, "Fucked, value = %d\n", value );
	 exit( 1 );
      }
      if (value > 256) {
	 int count = value / 256;

	 if (count > 8) {
	    fprintf( stderr, "Fucked, count = %d\n", count );
	    exit( 1 );
	 }
	 putchar( count );
	 putchar( value - count * 256 );
      } else 
	 putchar( value );
   }
}

void lzw( void )
{
   char *in, *out;
   char string[1024];
   int  value;
   int  next = 127;

   h = hsh_create();
   while (fgets( Buffer, BUFSIZ, stdin )) {
      if (Buffer[0] == '\n') {
	 hsh_destroy( h );
	 h = hsh_create();
	 next = 127;
      }
      in = Buffer;
      out = string;
      *out++ = *in++;
      while (*in && *in != '\n') {
	 *out++ = *in;
	 *out = '\0';
	 if (!(value = hsh_retrieve( h, string ))) {
	    out[-1] = '\0';
	    output( string );
	    out[-1] = *in;
	    hsh_insert( h, string, next++ );
	    out = string;
	    *out++ = *in;
	 }
	 ++in;
      }
      *out = '\0';
      output( string );
      putchar( '\n' );
   }
}

#define ANALYZE 1
#define ENCODE  2
#define DECODE  3

int main( int argc, char **argv )
{
   int           Function = 0;
   int           c;
   char          *wordlist = "";

   while ((c = getopt( argc, argv, "ae:d:" )) != -1)
      switch (c) {
      case 'a': Function = ANALYZE;                   break;
      case 'e': Function = ENCODE; wordlist = optarg; break;
      case 'd': Function = DECODE; wordlist = optarg; break;
      default: usage();
      }

   lzw();
   return 0;
   h = hsh_create();
   
   switch (Function) {
   case ANALYZE:                        do_analyze(); break;
   case ENCODE:  read_list( wordlist ); do_encode();  break;
   case DECODE:  read_list( wordlist ); do_decode();  break;
   default:
      usage();
   }

   return 0;
}
