/* index.c -- 
 * Created: Wed Oct  9 14:52:23 1996 by faith@cs.unc.edu
 * Revised: Fri Feb 28 21:56:58 1997 by faith@cs.unc.edu
 * Copyright 1996, 1997 Rickard E. Faith (faith@cs.unc.edu)
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
 * $Id: index.c,v 1.2 1997/03/01 05:21:20 faith Exp $
 * 
 */

#include "dict.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>

#define FIND_NEXT(pt,end) while (pt < end && *pt++ != '\n');
#define DEBUG 0

static int compare( const char *word, const char *start, const char *end,
		    int exact, int strict )
{
   int c;

#if DEBUG
   char       buf[4096], *d;
   const char *s;

   for (d = buf, s = start; s < end && *s != '\t';) *d++ = *s++;
   *d = '\0';
   printf( "compare \"%s\" with \"%s\"\n", word, buf );
#endif
   
   for (; *word && start < end && *start != '\t';) {
      if (!strict && (*start == ' ' || !isalnum( *start ))) {
	 ++start;
	 continue;
      }
      if (strict) c = *start;
      else       c = tolower( *start );
      if (*word != c) {
#if DEBUG
	 printf( "   result = %d\n", (*word < c) ? -1 : 1 );
#endif
	 return (*word < c) ? -1 : 1;
      }
      ++word;
      ++start;
   }
#if DEBUG
   printf( "   result = %d\n",
	   *word ? 1 : ((exact && *start != '\t') ? -1 : 0) );
#endif
   return  *word ? 1 : ((exact && *start != '\t') ? -1 : 0);
}

static const char *binary_search( const char *word,
				  const char *start, const char *end )
{
   const char *pt;

   PRINTF(DBG_SEARCH,("%s %p %p\n",word,start,end));

   pt = start + (end-start)/2;
   FIND_NEXT(pt,end);
   while (pt < end) {
      if (compare( word, pt, end, 0, 0 ) > 0) start = pt;
      else                                    end   = pt;
      PRINTF(DBG_SEARCH,("%s %p %p\n",word,start,end));
      pt = start + (end-start)/2;
      FIND_NEXT(pt,end);
   }

   return start;
}

static const char *linear_search( const char *word,
				  const char *start, const char *end )
{
   const char *pt;

   for (pt = start; pt < end;) {
      switch (compare( word, pt, end, 0, 0 )) {
      case -1: return NULL;
      case  0: return pt;
      case  1: break;
      }
      FIND_NEXT(pt,end);
   }
   return NULL;
}

const char *dict_index_search( const char *word, dictIndex *idx )
{
   const char *start;
   
   if (!idx)
      err_internal( __FUNCTION__, "No information on index file\n" );

   start = binary_search( word, idx->start, idx->end );
   PRINTF(DBG_SEARCH,("binary_search returns %p\n",start));
   start = linear_search( word, start,      idx->end );
   PRINTF(DBG_SEARCH,("linear_search returns %p\n",start));

   return start;
}

static dictWord *dict_word_create( const char *entry, dictDatabase *database )
{
   int        firstTab  = 0;
   int        secondTab = 0;
   int        newline   = 0;
   dictWord   *dw       = xmalloc( sizeof( struct dictWord ) );
   char       *buf;
   int        offset    = 0;
   int        state     = 0;
   const char *pt       = entry;
   
   for (;pt < database->index->end && *pt != '\n'; pt++, offset++) {
      if (*pt == '\t') {
	 switch (++state) {
	 case 1: firstTab = offset;  break;
	 case 2: secondTab = offset; break;
	 default:
	    err_internal( __FUNCTION__,
			  "Too many tabs in index entry \"%*.*s\"\n",
			  secondTab, secondTab, entry );
	 }
      }
   }
   newline = offset;
   
   if (state != 2)
      err_internal( __FUNCTION__,
		    "Too few tabs in index entry \"%20.20s\"\n", entry );

   buf = alloca( newline + 1 );
   strncpy( buf, entry, newline );
   buf[firstTab] = buf[secondTab] = buf[newline] = '\0';

   dw->word     = xstrdup( buf );
   dw->start    = b64_decode( buf + firstTab + 1 );
   dw->end      = b64_decode( buf + secondTab + 1 );
   dw->database = database;

   return dw;
}

static int dict_dump_datum( const void *datum )
{
   dictWord *dw = (dictWord *)datum;
   printf( "\"%s\" %lu %lu\n", dw->word, dw->start, dw->end );
   return 0;
}

void dict_dump_list( lst_List list )
{
   lst_iterate( list, dict_dump_datum );
}

static int dict_destroy_datum( const void *datum )
{
   dictWord *dw = (dictWord *)datum;
		 
   if (dw->word) xfree( (char *)dw->word );
   dw->word     = NULL;
   dw->start    = 0;
   dw->end      = 0;
   dw->database = NULL;
   xfree( dw );
   return 0;
}

void dict_destroy_list( lst_List list )
{
   lst_iterate( list, dict_destroy_datum );
}

static lst_List dict_search_exact( const char *word,
				   const char *orig,
				   dictDatabase *database )
{
   const char *pt   = dict_index_search( word, database->index );
   lst_List   l     = lst_create();
   int        count = 0;
   dictWord   *datum;

   if (orig[0] == '!') {
      while (pt && pt < database->index->end) {
	 if (!compare( orig, pt, database->index->end, 1, 1 )) {
	    ++count;
	    datum = dict_word_create( pt, database );
	    lst_append( l, datum );
	 } else break;
	 FIND_NEXT( pt, database->index->end );
      }
   } else {
      while (pt && pt < database->index->end) {
	 if (!compare( word, pt, database->index->end, 1, 0 )) {
	    ++count;
	    datum = dict_word_create( pt, database );
	    lst_append( l, datum );
	 } else break;
	 FIND_NEXT( pt, database->index->end );
      }
   }

   if (!count) {
      lst_destroy( l );
      l = NULL;
   }
   return l;
}

static lst_List dict_search_prefix( const char *word,
				    const char *orig,
				    dictDatabase *database )
{
   const char *pt   = dict_index_search( word, database->index );
   lst_List   l     = lst_create();
   int        count = 0;
   dictWord   *datum;

   while (pt && pt < database->index->end) {
      if (compare( word, pt, database->index->end, 0, 0 ) >= 0) {
	 ++count;
	 datum = dict_word_create( pt, database );
	 lst_append( l, datum );
      } else break;
      FIND_NEXT( pt, database->index->end );
   }

   if (!count) {
      lst_destroy( l );
      l = NULL;
   }
   return l;
}

static lst_List dict_search_substring( const char *word,
				       const char *orig,
				       dictDatabase *database )
{
   const char *pt   = dict_index_search( word, database->index );
   lst_List   l     = lst_create();
   int        count = 0;
   dictWord   *datum;

   while (pt && pt < database->index->end) {
      if (compare( word, pt, database->index->end, 0, 0) >= 0) {
	 ++count;
	 datum = dict_word_create( pt, database );
	 lst_append( l, datum );
      } else break;
      FIND_NEXT( pt, database->index->end );
   }

   if (!count) {
      lst_destroy( l );
      l = NULL;
   }
   return l;
}

static lst_List dict_search_regexpr( const char *word,
				     const char *orig,
				     dictDatabase *database )
{
   return NULL;
}

static lst_List dict_search_soundex( const char *word,
				     const char *orig,
				     dictDatabase *database )
{
   return NULL;
}

static lst_List dict_search_levenshtein( const char *word,
					 const char *orig,
					 dictDatabase *database )
{
   return NULL;
}

lst_List dict_search_database( const char *word,
			       dictDatabase *database,
			       int strategy )
{
   char       *buf = alloca( strlen( word ) + 1 );
   char       *pt;
   const char *w   = word;
		  
   for (pt = buf; *w; w++) {
      if (*word == ' ' || !isalnum( *w )) continue;
      *pt++ = tolower(*w);
   }
   *pt = '\0';

   if (!database->index)
      database->index = dict_index_open( database->indexFilename );

   switch (strategy) {
   case DICT_EXACT:       return dict_search_exact( buf, word, database );
   case DICT_PREFIX:      return dict_search_prefix( buf, word, database );
   case DICT_SUBSTRING:   return dict_search_substring( buf, word, database );
   case DICT_REGEXP:      return dict_search_regexpr( buf, word, database );
   case DICT_SOUNDEX:     return dict_search_soundex( buf, word, database );
   case DICT_LEVENSHTEIN: return dict_search_levenshtein( buf, word, database);
   default:
      err_internal( __FUNCTION__, "Search strategy %d unknown\n", strategy );
   }
}

dictIndex *dict_index_open( const char *filename )
{
   dictIndex   *i = xmalloc( sizeof( struct dictIndex ) );
   struct stat sb;

   memset( i, 0, sizeof( struct dictIndex ) );

   if ((i->fd = open( filename, O_RDONLY )) < 0)
      err_fatal_errno( __FUNCTION__,
		       "Cannot open index file \"%s\"\n", filename );
   if (fstat( i->fd, &sb ))
      err_fatal_errno( __FUNCTION__,
		       "Cannot stat index file \"%s\"\n", filename );
   i->size = sb.st_size;

   i->start = mmap( NULL, i->size, PROT_READ, MAP_SHARED, i->fd, 0 );
   if ((void *)i->start == (void *)(-1))
      err_fatal_errno( __FUNCTION__,
		       "Cannot mmap index file \"%s\"\b", filename );

   i->end = i->start + i->size;
   return i;
}

void dict_index_close( dictIndex *i )
{
   if (i->fd >= 0) {
      munmap( (void *)i->start, i->size );
      close( i->fd );
      i->fd = 0;
      i->start = i->end = NULL;
   }
}
