/* compressdict.c -- 
 * Created: Wed Aug 23 22:19:46 1995 by r.faith@ieee.org
 * Revised: Sun Aug 27 23:22:36 1995 by r.faith@ieee.org
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
 * $Id: compressdict.c,v 1.1 1996/09/23 15:33:23 faith Exp $
 *
 * Typical usage:
 *
 * compress -a dictionary
 * compress -e dictionary > compressed_dictionary
 * (To check) compress -d compressed_dictionary > dictionary
 * (to test encode/decode functions) compress -t
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <getopt.h>
#endif
#include <fcntl.h>
#include "dict.h"

extern char *encode( int value );
extern int decode( const char *c );

static char Buffer[BUFSIZ];
static char **wordlist;
static char *ascii[128];

int Debug = 1;

struct idx {
   char          *word;
   unsigned long offset;
};

static struct idx Idx[100000];
static int        IdxCount;

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

static hsh_HashTable h;

static unsigned long hsh_string_hash( const char *key )
{
   const char    *pt = (char *)key;
   unsigned long h  = 0;

   while (*pt) {
      h += *pt++;
      h *= 2654435789U;		/* prime near %$\frac{\sqrt{5}-1}{2}2^{32}$% */
   }

   return h;
}

static int hsh_string_compare( const char *key1, const char *key2 )
{
   return strcmp( key1, key2 );
}

static hsh_HashTable hsh_create( void )
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

static int hsh_insert( hsh_HashTable table, const char *key, int datum )
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

static int hsh_increment( hsh_HashTable table, const char *key )
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


static int hsh_retrieve( hsh_HashTable table, const char *key )
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

static void hsh_iterate( hsh_HashTable table,
			 int (*iterator)( const char *key, int datum, void * ),
			 void *stuff)
{
   tableType     t = (tableType)table;
   unsigned long i;

   for (i = 0; i < t->prime; i++) {
      if (t->buckets[i]) {
	 bucketType pt;
	 
	 for (pt = t->buckets[i]; pt; pt = pt->next)
	       if (iterator( pt->key, pt->datum, stuff ))
		     return;
      }
   }
}

static int hsh_entries( hsh_HashTable table )
{
   return table->entries;
}

static int special_space;

static void register_space( int new )
{
   special_space = new;
}

static int get_space( void )
{
   return special_space;
}

static int print( const char *key, int datum, void *stuff )
{
   FILE *str = (FILE *)stuff;
   
   if (datum > 1) fprintf( str, "%8d\t%s\n", datum, key );
   return 0;
}

static int print_index( const char *key, int datum, void *stuff )
{
   FILE *str = (FILE *)stuff;
   
   fprintf( str, "%8d\t%s\n", datum, key );
   return 0;
}

static int sum( const char *key, int datum, void *stuff )
{
   int *total = (int *)stuff;
   
   if (datum > 1) ++*total;
   return 0;
}

static int sum_index( const char *key, int datum, void *stuff )
{
   int *total = (int *)stuff;
   
   ++*total;
   return 0;
}

static int maximize( const char *key, int datum, void *stuff )
{
   int len = strlen( key );
   int i;
   int flag = 0;

   struct stems {
      int  len;
      char *stem;
   } stems[] = {
      { 7, "ousness" },
      { 7, "ization" },
      { 7, "iveness" },
      { 7, "fulness" },
      { 7, "ational" },
      { 6, "tional"  },
      { 5, "tions"   },
      { 5, "osity"   },
      { 5, "ology"   },
      { 5, "ingly"   },
      { 5, "ility"   },
      { 5, "icate"   },
      { 5, "ement"   },
      { 5, "ative"   },
      { 5, "ation"   },
      { 5, "alize"   },
      { 5, "alism"   },
      { 4, "tion"    },
      { 4, "sses"    },
      { 4, "ness"    },
      { 4, "izer"    },
      { 4, "icle"    },
      { 4, "ical"    },
      { 4, "ence"    },
      { 4, "ator"    },
      { 4, "ance"    },
      { 4, "able"    },
      { 3, "ous"     },
      { 3, "ory"     },
      { 3, "ize"     },
      { 3, "ive"     },
      { 3, "ism"     },
      { 3, "ion"     },
      { 3, "ing"     },
      { 3, "ies"     },
      { 3, "ful"     },
      { 3, "ent"     },
      { 3, "eed"     },
      { 3, "ed,"     },
      { 3, "ble"     },
      { 3, "ate"     },
      { 2, "ss"      },
      { 2, "s,"      },
      { 2, "ed"      },
      { 0, 0 }
   };
   
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

      if (!flag) {
	 struct stems *s;

	 for (s = stems; s->len; ++s) {
	    if (len > s->len && !strcmp( key + len - s->len, s->stem )) {
	       hsh_increment( h, s->stem );
	       break;
	    }
	 }
      }
#if 0
      if (!flag) hsh_increment( h, key + 1 );
#endif
#endif
   }
   return 0;
}

static void do_analyze( FILE *str, char *wordlistFile )
{
   int  lines = 0;
   int  count = 0;
   int  ascii[128] = { 0 };
   int  i;
   FILE *wl;
   int  total = 0;

   sprintf( Buffer, "sort -r | cut -f2 > %s", wordlistFile );
   if (!(wl = popen( Buffer, "w" ))) {
      fprintf( stderr, "Cannot open pipe \"%s\" for write\n", Buffer );
      exit( 1 );
   }
   
   while (fgets( Buffer, BUFSIZ, str )) {
      char *pt;
      char *word;

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words (%d unique) on %d lines\n",
		  count, hsh_entries( h ), lines );

      for (pt = Buffer; *pt; pt++) {
	 if ((unsigned char)*pt >= 0x80) {
	    fprintf( stderr,
		     "We're fucked: found character code %d on line %d\n",
		     *pt, lines );
	 } else {
	    ++ascii[ (unsigned char)*pt ];
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

   hsh_iterate( h, maximize, NULL );

   hsh_iterate( h, sum, &total );
   if (Debug) fprintf( stderr, "%d words in compression dictionary\n", total );
   fprintf( wl, "#%d\n", total );
   fprintf( wl, "!" );
   for (i = 1; i < 128; i++)
      if (!ascii[i] && i != '\n') fprintf( wl, " %d", i );
   fprintf( wl, "\n" );

   hsh_iterate( h, print, wl );
   pclose( wl );
}

static void do_index( FILE *str )
{
   int  lines = 0;
   int  count = 0;
   FILE *wl;
   int  total = 0;

#if 0
   sprintf( Buffer, "sort -r > %s", wordlistFile );
   if (!(wl = popen( Buffer, "w" ))) {
      fprintf( stderr, "Cannot open pipe \"%s\" for write\n", Buffer );
      exit( 1 );
   }
#else
   wl = stdout;
#endif
   
   while (fgets( Buffer, BUFSIZ, str )) {
      char *pt;
      char *word;

      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d words (%d unique) on %d lines\n",
		  count, hsh_entries( h ), lines );

      for (word = pt = Buffer; *pt; pt++) {
	 if (!isalnum( *pt ) && *pt != '-') {
	    *pt = '\0';
	    if (strlen( word ) > 1) {
	       hsh_increment( h, word );
	       ++count;
	    }
	    word = pt + 1;
	 }
      }
   }

   hsh_iterate( h, sum_index, &total );
   if (Debug) fprintf( stderr, "%d words in index\n", total );

   hsh_iterate( h, print_index, wl );
   pclose( wl );
}

static void do_encode( FILE *strIn, char *indexFile )
{
   int           lines   = 0;
   int           count   = 0;
   FILE          *str    = NULL;
   int           current = 0;
   unsigned long next = 0, in = 0, out = 0;

   if (IdxCount) {
      if (!(str = fopen( indexFile, "w")))  {
	 fprintf( stderr, "Cannot open \"%s\" for write\n", indexFile );
	 exit( 1 );
      }
   }

   if (str) {
      next = Idx[ current ].offset;
      if (in == next) {
	 fprintf( str, "%s\t%lu\n", Idx[ current ].word, out );
	 next = Idx[ ++current ].offset;
      }
   }
   
   while (fgets( Buffer, BUFSIZ, strIn )) {
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
		  putchar( get_space() );
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

static void do_decode( FILE *str )
{
   int lines      = 0;
   
   while (fgets( Buffer, BUFSIZ, str )) {
      ++lines;
      if (lines && !(lines % 10000))
	 fprintf( stderr, "%d lines\n", lines );
      
      printf( "%s", decode_line( Buffer, wordlist, ascii, get_space() ) );
   }
}

static int compar( const void *a, const void *b )
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

static void read_index( char *filename )
{
   FILE *str;
#if 0
   int  i;
#endif
   
   if (!(str = fopen( filename, "r" ))) return;

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

static void test( void )
{
   int i;

   for (i = 0; i < MAXWORDS; i++)
      if (decode(encode( i )) != i) {
	 unsigned char *tmp = encode( i );
	 
	 printf( "%d: 0x%02x %d %d => %d\n",
		 i, tmp[0], tmp[1], tmp[2], decode( tmp ) );
      }
   exit( 0 );
}

static void read_list_by_name( const char *name, int flag )
{
   char space;
   int  words;
   int  i;
   int  fd;

   if ((fd = open( name, O_RDONLY )) < 0) {
      fprintf( stderr, "Cannot open \"%s\" for read\n", name );
      exit( 1 );
   }
   words = read_list( fd, &wordlist, ascii, &space );
   close( fd );

   if (flag) {
      for (i = 0; i < words; i++) hsh_insert( h, wordlist[i], i );
      for (i = 0; i < 128; i++) if (ascii[i]) hsh_insert( h, ascii[i], -i );
   }

   register_space( space );
}

static void usage( void )
{
   fprintf( stderr,
	    "usage: compress [-a] [-e [-i <index>]] <dict> [-d <cdict>]\n" );
   exit( 1 );
}

#define ANALYZE 1
#define ENCODE  2
#define DECODE  3
#define INDEX   4

int main( int argc, char **argv )
{
   int  Function   = 0;
   int  c;
   char *indexfile = NULL;
   char *indexfileIn;
   char *wordlist  = NULL;
   char *infile;
   FILE *str;

   while ((c = getopt( argc, argv, "aedIi:w:t" )) != -1)
      switch (c) {
      case 'a': Function = ANALYZE; break;
      case 'e': Function = ENCODE;  break;
      case 'd': Function = DECODE;  break;
      case 'I': Function = INDEX;   break;
      case 'i': indexfile = optarg; break;
      case 'w': wordlist = optarg;  break;
      case 't': test();
      default: usage();
      }

   if (argc - optind != 1) usage();

   if (!(str = fopen( infile = argv[optind], "r" ))) {
      fprintf( stderr, "Cannot open \"%s\" for read\n", argv[optind] );
      return 1;
   }

   h = hsh_create();

   if (!wordlist) {
      wordlist = malloc( strlen( infile ) + 20 );
      strcpy( wordlist, infile );
      strcat( wordlist, ".cd" );
   }
   
   switch (Function) {
   case ANALYZE:
      do_analyze( str, wordlist );
      break;
   case INDEX:
      do_index( str );
      break;
   case ENCODE:
      if (!indexfile) {
	 indexfile = malloc( strlen( infile ) + 20 );
	 strcpy( indexfile, infile );
	 strcat( indexfile, ".idx" );
      }
      indexfileIn = malloc( strlen( infile ) + 20 );
      strcpy( indexfileIn, infile );
      strcat( indexfileIn, ".index" );
      read_index( indexfileIn );
      read_list_by_name( wordlist, 1 );
      do_encode( str, indexfile );
      break;
   case DECODE:
      read_list_by_name( wordlist, 0 );
      do_decode( str );
      break;
   default:
      usage();
   }

   fclose( str );

   return 0;
}
