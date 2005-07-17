/* index.c -- 
 * Created: Wed Oct  9 14:52:23 1996 by faith@dict.org
 * Revised: Tue Apr 23 09:14:43 2002 by faith@dict.org
 * Copyright 1996, 1997, 1998, 2000, 2002 Rickard E. Faith (faith@dict.org)
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
 * $Id: index.c,v 1.102 2005/07/17 16:10:57 cheusov Exp $
 * 
 */

#include "dictP.h"
#include "dictzip.h"
#include "index.h"
#include "include_regex.h"
#include "strategy.h"
#include "str.h"

#ifdef USE_PLUGIN
#include "plugin.h"
#endif

#include <sys/stat.h>

#include <fcntl.h>
#include <ctype.h>

#if HAVE_WCTYPE_H
#include <wctype.h>
#endif

#if HAVE_WCHAR_H
#include <wchar.h>
#endif

#include <stdio.h>

extern int mmap_mode;

#define FIND_PREV(begin, pt) while (pt > begin && pt [-1] != '\n') --pt;
#define FIND_NEXT(pt, end) while (pt < end && *pt++ != '\n');

#define MAXWORDLEN    512
#define BMH_THRESHOLD   3	/* When to start using Boyer-Moore-Hoorspool */

#ifndef SYSTEM_UTF8_FUNCS
/* defaults to run in UTF-8 mode */
int utf8_mode=1;        /* dictd uses UTF-8 dictionaries */
#else
/* defaults to run in ASCII mode */
int utf8_mode=0;        /* dictd uses UTF-8 dictionaries */
#endif
int bit8_mode = 0;      /* dictd uses 8-BIT dictionaries */

int optStart_mode = 1;	/* Optimize search range for constant start */

dictConfig *DictConfig;

int _dict_comparisons;
static int isspacealnumtab [UCHAR_MAX + 1];
static int isspacealnumtab_allchars[UCHAR_MAX + 1];
static int isspacepuncttab [UCHAR_MAX + 1];
static int char2indextab   [UCHAR_MAX + 2];
static int index2chartab   [UCHAR_MAX + 2];
static int tolowertab      [UCHAR_MAX + 1];

char global_alphabet_8bit [UCHAR_MAX + 2];
char global_alphabet_ascii [UCHAR_MAX + 2];

static int chartab [UCHAR_MAX + 1];
static int charcount = 0;

/* #define isspacealnum(x) (isspacealnumtab[(unsigned char)(x)]) */
#define c2i(x) (char2indextab[(unsigned char)(x)])
#define i2c(x) (index2chartab[(unsigned char)(x)])
#define c(x)   (((x) < charcount) ? chartab[(unsigned char)(x)] : 0)
#define altcompare(a,b,c) (1)

/*
  compares two 8bit strings (containing one character)
  according to locate
*/
static int dict_table_init_compare_strcoll (const void *a, const void *b)
{
    return strcoll(*(const char **)a, *(const char **)b);
}

/*
  compares two strings (containing one character)
  converting ASCII character to lower case.
*/
static int dict_table_init_compare_utf8 (const void *a, const void *b)
{
    int c1 = ** (const unsigned char **) a;
    int c2 = ** (const unsigned char **) b;

    if (c1 <= CHAR_MAX)
	c1 = tolower (c1);
    if (c2 <= CHAR_MAX)
	c2 = tolower (c2);

    return c1 - c2;
}

static void dict_make_global_alphabet (void)
{
   int i;
   int j = 0;
   int ch;

   for (i=0; i < charcount; ++i){
      global_alphabet_8bit [i] = ch = c(i);

      if (ch < 128)
	 global_alphabet_ascii [j++] = ch;
   }

   global_alphabet_8bit  [charcount] = 0;
   global_alphabet_ascii [j] = 0;

   PRINTF(DBG_SEARCH, (
	  "global_alphabet_8bit  = '%s'\n",
	  global_alphabet_8bit));
   PRINTF(DBG_SEARCH, (
	  "global_alphabet_ascii = '%s'\n",
	  global_alphabet_ascii));
}

static void dict_table_init(void)
{
   int      i;
   unsigned char s[2 * UCHAR_MAX + 2];
   unsigned char *p[UCHAR_MAX + 1];

   for (i = 0; i <= UCHAR_MAX; i++) {
      if (isspace(i) || isalnum(i) || (utf8_mode && i >= 0x80)){
	 isspacealnumtab [i] = 1;
      }else{
	 isspacealnumtab [i] = 0;
      }

      tolowertab [i] = tolower (i);
      if (i >= 0x80){
	 if (utf8_mode || (!utf8_mode && !bit8_mode)){
	    /* utf-8 or ASCII mode */
	    tolowertab [i] = i;
	 }
      }

      if (isspace(i) || ispunct(i)){
	 isspacepuncttab [i] = 1;
      }else{
	 isspacepuncttab [i] = 0;
      }

      isspacealnumtab_allchars [i] = 1;
   }
   isspacepuncttab['\t'] = isspacepuncttab['\n'] = 1; /* special */
   isspacealnumtab['\t'] = isspacealnumtab['\n'] = 0; /* special */
   isspacealnumtab_allchars['\t'] = isspacealnumtab_allchars['\n'] = 0; /* special */

   charcount = 0;
   for (i = 0; i <= UCHAR_MAX; i++){
      if (islower (i) || (utf8_mode && i >= 0xC0))
	 chartab [charcount++] = i;
   }

                                /* Populate an array with length-1 strings */
   for (i = 0; i <= UCHAR_MAX; i++) {
      if (!isupper (i)){
	 s[2 * i] = i;
      }else{
	 s[2 * i] = 0;
      }

      s[2 * i + 1] = '\0';
      p[i]         = &s[2 * i];
   }
                                /* Sort those strings in the locale */
   if (utf8_mode)
      qsort(p, UCHAR_MAX + 1, sizeof(p[0]), dict_table_init_compare_utf8);
   else
      qsort(p, UCHAR_MAX + 1, sizeof(p[0]), dict_table_init_compare_strcoll);

                                /* Extract our unordered arrays */
   for (i = 0; i <= UCHAR_MAX; i++) {
      char2indextab[*p[i]] = i;
      index2chartab[i]     = *p[i];
   }
   char2indextab[UCHAR_MAX + 1] = UCHAR_MAX;
   index2chartab[UCHAR_MAX + 1] = UCHAR_MAX; /* we may index here in  */

   if (dbg_test(DBG_SEARCH)) {
      for (i = 0; i <= UCHAR_MAX; ++i){
	 if (p [i][0] <= CHAR_MAX)
	    printf ("sorted list: %s\n", p [i]);
	 else
	    printf ("sorted list: %i\n", (unsigned char) p [i] [0]);
      }
   }

   if (dbg_test(DBG_SEARCH)) {
      for (i = 0; i < charcount; i++)
	 printf("%03d %d ('%c')\n", i, c(i), c(i));
      for (i = 0; i <= UCHAR_MAX; i++)
	 printf("c2i(%d/'%c') = %d; i2c(%d) = %d/'%c'\n",
		i, (char) isgraph(i) ? i : '.',
		c2i(i), c2i(i),
		i2c(c2i(i)), (char) i2c(c2i(i)) ? i2c(c2i(i)) : '.');
   }


   dict_make_global_alphabet ();
}

static int compare_allchars(
    const char *word,
    const char *start, const char *end )
{
   int c1, c2;
   int result;

   PRINTF(DBG_SEARCH,("   We are inside index.c:compare_allchars\n"));

   /* FIXME.  Optimize this inner loop. */
   while (*word && start < end && *start != '\t') {
//      if (!isspacealnum(*start)) {
//	 ++start;
//	 continue;
//      }
#if 0
      if (isspace( (unsigned char) *start ))
	 c2 = ' ';
      else
	 c2 = * (unsigned char *) start;

      if (isspace( (unsigned char) *word ))
	 c1 = ' ';
      else
	 c1 = * (unsigned char *) word;

#else
      c2 = * (unsigned char *) start;
      c1 = * (unsigned char *) word;
#endif
      if (c1 != c2) {
	    if (c1 < c2){
	       result = -2;
	    }else{
	       result = 1;
	    }
	 if (dbg_test(DBG_SEARCH)){
	    printf("   result = %d (%i != %i) \n", result, c1, c2);
	 }
         return result;
      }
      ++word;
      ++start;
   }

   PRINTF(DBG_SEARCH,("   result = %d\n",
		      *word ? 1 : ((*start != '\t') ? -1 : 0)));
   return  *word ? 1 : ((*start != '\t') ? -1 : 0);
}

static int compare_alnumspace(
    const char *word,
    const dictIndex *dbindex,
    const char *start, const char *end )
{
   int c1, c2;
   int result;

   assert (dbindex);

   PRINTF(DBG_SEARCH,("   We are inside index.c:compare_alnumspace\n"));

   /* FIXME.  Optimize this inner loop. */
   while (*word && start < end && *start != '\t') {
      if (!dbindex -> isspacealnum[* (const unsigned char *) start]) {
	 ++start;
	 continue;
      }
#if 0
      if (isspace( (unsigned char) *start ))
	 c2 = ' ';
      else
	 c2 = tolowertab [* (unsigned char *) start];

      if (isspace( (unsigned char) *word ))
	 c1 = ' ';
      else
	 c1 = tolowertab [* (unsigned char *) word];
#else
      c2 = tolowertab [* (unsigned char *) start];
      c1 = tolowertab [* (unsigned char *) word];
#endif
      if (c1 != c2) {
	 if (utf8_mode){
	    if (
	       (c1 <= CHAR_MAX ? c2i (c1) : c1) <
	       (c2 <= CHAR_MAX ? c2i (c2) : c2))
	    {
	       result = -2;
	    }else{
	       result = 1;
	    }
	 }else{
	    result = (c2i (c1) < c2i (c2) ? -2 : 1);
	 }
	 if (dbg_test(DBG_SEARCH)){
	    if (utf8_mode)
	       printf(
		  "   result = %d (%i != %i) \n", result, c1, c2);
	    else
	       printf(
		  "   result = %d ('%c'(c2i=%i) != '%c'(c2i=%i)) \n",
		  result,
		  c1,
		  c2i (c1),
		  c2,
		  c2i (c2));
	 }
         return result;
      }
      ++word;
      ++start;
   }

   while (
       *start != '\t' &&
       !dbindex -> isspacealnum[* (const unsigned char *) start])
   {
      ++start;
   }

   PRINTF(DBG_SEARCH,("   result = %d\n",
		      *word ? 1 : ((*start != '\t') ? -1 : 0)));
   return  *word ? 1 : ((*start != '\t') ? -1 : 0);
}

/* Compare:
   
   Return -2 if word <  word-pointed-to-by-start
          -1 if word prefix word-pointed-to-by-start
           0 if word == word-pointed-to-by-start
	   1 if word >  word-pointed-to-by-start
	   2 if some kind of error happened

   The comparison must be the same as "sort -df" phone directory order:
   ignore all characters except letters, digits, and blanks; fold upper
   case into the equivalent lowercase.

   word already has all of the illegal characters removed
*/

static int compare(
   const char *word,
   const dictIndex *dbindex,
   const char *start, const char *end )
{
   char       buf[80], *d;
   const char *s;

   assert (dbindex);

   if (dbg_test(DBG_SEARCH)) {
      for (
	 d = buf, s = start;
	 d - buf + 1 < (int) sizeof (buf) && s < end && *s != '\t';)
      {
	 *d++ = *s++;
      }

      *d = '\0';
      printf( "compare \"%s\" with \"%s\" (sizes: %lu and %lu)\n",
         word, buf, (unsigned long) strlen( word ), (unsigned long) strlen( buf ) );
   }

   ++_dict_comparisons;		/* counter for profiling */

   if (dbindex &&
       (dbindex -> flag_allchars || dbindex -> flag_utf8 ||
	dbindex -> flag_8bit))
   {
      return compare_allchars( word, start, end );
   }else{
      return compare_alnumspace( word, dbindex, start, end );
   }
}

static const char *binary_search(
    const char *word,
    const dictIndex *dbindex,
    const char *start, const char *end )
{
   const char *pt;

   assert (dbindex);

   PRINTF(DBG_SEARCH,("%s %p %p\n", word, start, end));

   pt = start + (end-start)/2;
   FIND_PREV(start, pt);
   while (start < end) {
      switch (compare( word, dbindex, pt, end )){
	 case -2: case -1: case 0:
	    end = pt;
	    break;
	 case 1:
	    start = pt;
	    FIND_NEXT(start, end)
	    break;
	 case  2:
	    return end;     /* ERROR!!! */
	 default:
	    assert (0);
      }
      PRINTF(DBG_SEARCH,("%s %p %p\n",word,start,end));
      pt = start + (end-start)/2;
      FIND_PREV(start, pt);
   }

   return start;
}

static const char *binary_search_8bit(
    const char *word,
    const dictIndex *dbindex,
    const char *start, const char *end )
{
   char       buf[80], *d;
   const char *s;
   const char *pt;
   int cmp;

   assert (dbindex);

   PRINTF(DBG_SEARCH,("word/start/end %s/%p/%p\n",word,start,end));

   pt = start + (end-start)/2;
   FIND_PREV(start, pt);
   while (start < end) {
      if (dbg_test(DBG_SEARCH)) {
         for (
	    d = buf, s = pt;
	    s < end && *s != '\t' && d - buf + 1 < (int) sizeof (buf);)
	 {
	    *d++ = *s++;
	 }

         *d = '\0';
         printf( "compare \"%s\" with \"%s\" (sizes: %lu and %lu)\n",
            word, buf, (unsigned long) strlen( word ), (unsigned long) strlen( buf ) );
      }

      if (
	 dbindex &&
	 (dbindex -> flag_utf8 || dbindex -> flag_allchars))
      {
	 cmp = compare_allchars ( word, pt, end );
      }else{
	 cmp = compare_alnumspace ( word, dbindex, pt, end );
      }

      switch (cmp){
      case -2: case -1: case 0:
	 end = pt;
	 break;
      case 1:
	 start = pt;
	 FIND_NEXT(start, end)
	 break;
      case  2:
	 return end;     /* ERROR!!! */
      default:
	 assert (0);
      }
      PRINTF(DBG_SEARCH,("%s %p %p\n",word,start,end));
      pt = start + (end-start)/2;
      FIND_PREV(start, pt);
   }

   return start;
}

static const char *linear_search(
    const char *word,
    const dictIndex *dbindex,
    const char *start, const char *end )
{
   const char *pt;

   assert (dbindex);

   for (pt = start; pt < end;) {
      switch (compare( word, dbindex, pt, end )) {
      case -2: return NULL;	/* less than and not prefix */
      case -1:			/* prefix */
      case  0: return pt;	/* exact */
      case  1: break;           /* greater than */
      case  2: return NULL;     /* ERROR!!! */
      }
      FIND_NEXT(pt,end);
   }
   return NULL;
}

static const char *dict_index_search( const char *word, dictIndex *idx )
{
   const char    *start;
   const char    *end;
   int first, last;

   assert (idx);

   /* With optStart:
      17071 comparisons, 1000 words
      33946 comparisons, 2000 words

      Without optStart:
      20889 comparisons, 1000 words
      41668 comparisons, 2000 words

      Linear:
       594527 comparisons, 1000 words
      2097035 comparisons, 2000 words
   */

   if (optStart_mode){
      first   = * (const unsigned char *) word;
      last    = i2c(c2i(first)+1);

      if (dbg_test(DBG_SEARCH)) {
	 if (!utf8_mode || (last <= CHAR_MAX && first <= CHAR_MAX))
	    printf("binary_search from %c to %c\n", first, last);
	 else
	    printf("binary_search from %i to %i\n", first, last);
      }

      end   = idx->optStart [last];
      start = idx->optStart [first];
#if 0
      fprintf (stderr, "start1 = %p\n", start);
      fprintf (stderr, "end1   = %p\n", end);
#endif
   }else{
      start = idx->start;
      end   = idx->end;
   }

   if (end < start) end = idx->end;

   start = binary_search( word, idx, start, end );

   PRINTF(DBG_SEARCH,("binary_search returns %p\n",start));

   start = linear_search( word, idx, start, idx->end );

   PRINTF(DBG_SEARCH,("linear_search returns %p\n",start));

   return start;
}

static dictWord *dict_word_create(
    const char *entry,
    const dictDatabase *database,
    dictIndex *dbindex)
{
   int        firstTab  = 0;
   int        secondTab = 0;
   int        newline   = 0;
   dictWord   *dw       = xmalloc( sizeof( struct dictWord ) );
   char       *buf;
   int        offset    = 0;
   int        state     = 0;
   const char *pt       = entry;
   char       *s, *d;

   assert (dbindex);
   assert (pt >= dbindex -> start && pt < dbindex -> end);

   memset (dw, 0, sizeof (*dw));

   for (;pt < dbindex->end && *pt != '\n'; pt++, offset++) {
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
   memcpy (buf, entry, newline);
   buf[firstTab] = buf[secondTab] = buf [newline] = '\0';

   dw->start    = b64_decode( buf + firstTab + 1 );
   dw->end      = b64_decode( buf + secondTab + 1 );
   dw->def      = NULL;
   dw->def_size = 0;
   dw->database = database;

				/* Apply quoting to word */
   dw->word     = xmalloc(strlen(buf) * 2 + 1);
   for (s = buf, d = (char *)dw->word; *s;) {
       switch (*s) {
       case '"':
       case '\\':
	   *d++ = '\\';
       default:
	   *d++ = *s++;
       }
   }
   *d = '\0';

   return dw;
}

static int dict_dump_datum( const void *datum )
{
   dictWord *dw = (dictWord *)datum;
   printf(
      "\"%s\" %lu/%lu %p/%i\n",
      dw->word, dw->start, dw->end,
      dw->def, dw->def_size );

   return 0;
}

void dict_dump_list( lst_List list )
{
   lst_iterate( list, dict_dump_datum );
}

int dict_destroy_datum( const void *datum )
{
   dictWord *dw = (dictWord *)datum;

   if (!datum)
      return 0;

   if (dw->word)
      xfree( (char *)dw->word );

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
   lst_destroy( list );
}

static int dict_search_exact( lst_List l,
			      const char *word,
			      const dictDatabase *database,
			      dictIndex *dbindex)
{
   const char *pt   = NULL;
   int        count = 0;
   dictWord   *datum;
   const char *previous = NULL;

   assert (dbindex);

   pt   = dict_index_search( word, dbindex );

   while (pt && pt < dbindex->end) {
      if (!compare( word, dbindex, pt, dbindex->end )) {
	 if (!previous || altcompare(previous, pt, dbindex->end)) {
	    ++count;
	    if (l){
	       datum = dict_word_create( previous = pt, database, dbindex );
	       lst_append( l, datum );
	    }
	 }
      } else break;
      FIND_NEXT( pt, dbindex->end );
   }

   return count;
}

static int dict_search_prefix( lst_List l,
			       const char *word,
			       const dictDatabase *database,
			       dictIndex *dbindex)
{
   const char *pt   = dict_index_search( word, dbindex );
   int        count = 0;
   dictWord   *datum;
   const char *previous = NULL;

   assert (dbindex);

   while (pt && pt < dbindex->end) {
      switch (compare( word, dbindex, pt, dbindex->end )) {
	 case -2:
	    return count;
	 case -1:
	 case 0:
	    if (!previous || altcompare(previous, pt, dbindex->end)) {
	       ++count;
	       datum = dict_word_create( previous = pt, database, dbindex );
	       lst_append( l, datum );
	    }
	    break;
	 case 1:
	    return count;
	 case 2:
	    return count; /* ERROR!!! */
	 default:
	    assert (0);
      }
      FIND_NEXT( pt, dbindex->end );
   }

   return count;
}

enum {
   BMH_SUBSTRING,
   BMH_SUFFIX,
   BMH_WORD,
};

static int dict_search_brute( lst_List l,
			      const unsigned char *word,
			      const dictDatabase *database,
			      dictIndex *dbindex,
			      int flag,
			      int patlen )
{
   const unsigned char *const start = dbindex->start;
   const unsigned char *const end   = dbindex->end;
   const unsigned char *p, *pt;
   int        count = 0;
   dictWord   *datum;
   int        result;
   const char *previous = NULL;

   assert (dbindex);

   p = start;
   while (p < end && !dbindex -> isspacealnum[*p]) ++p;
   for (; p < end; ++p) {
      if (*p == '\t') {
	 while (p < end && *p != '\n') ++p;
	 ++p;
	 while (p < end && !dbindex -> isspacealnum[*p]) ++p;
      }
      if (tolowertab [*p] == *word) {
	 result = compare( word, dbindex, p, end );
	 if (result == -1 || result == 0) {
	    switch (flag){
	    case BMH_SUBSTRING:
	       break;
	    case BMH_SUFFIX:
	       if (result)
		  continue;

	       break;
	    case BMH_WORD:
	       if (p > start && !isspacepuncttab [p [-1]])
		  continue;
	       if (p+patlen < end && !isspacepuncttab [p [patlen]])
		  continue;

	       break;
	    }

	    for (pt = p; pt >= start && *pt != '\n'; --pt)
	       if (*pt == '\t') goto continue2;
	    if (!previous || altcompare(previous, pt + 1, end)) {
	       ++count;
	       datum = dict_word_create( previous = pt + 1, database, dbindex );
#if 0
	       fprintf( stderr, "Adding %d %s\n",
			compare( word, dbindex, p, end ),
			datum->word);
#endif
	       lst_append( l, datum );
	    }
	    FIND_NEXT(p,end);
	    --p;
	 }
      }
 continue2:
      ;
   }
   
   return count;
}

/* dict_search_bmh implements a version of the Boyer-Moore-Horspool text
   searching algorithm, as described in G. H. Gonnet and R. Baeza-Yates,
   HANDBOOK OF ALGORITHMS AND DATA STRUCTURES: IN PASCAL AND C (2nd ed).
   Addison-Wesley Publishing Co., 1991.  Pages 258-9. */

static int dict_search_bmh( lst_List l,
			    const unsigned char *word,
			    const dictDatabase *database,
			    dictIndex *dbindex,
			    int flag )
{
   const unsigned char *const start = dbindex->start;
   const unsigned char *const end   = dbindex->end;
   const int  patlen = strlen( word );
   int        skip[UCHAR_MAX + 1];
   int        i;
   int        j;
#if 0
   int k;
#endif
   const unsigned char *p, *pt, *ptr;
   int        count = 0;
   const unsigned char *f = NULL; /* Boolean flag, but has to be a pointer */
   dictWord   *datum;
   const unsigned char *wpt;
   const unsigned char *previous = NULL;

   assert (dbindex);

   if (patlen < BMH_THRESHOLD)
      return dict_search_brute( l, word, database, dbindex, flag, patlen );

   for (i = 0; i <= UCHAR_MAX; i++) {
      if (dbindex -> isspacealnum[i])
	 skip[i] = patlen;
      else
	 skip[i] = 1;
   }
   for (i = 0; i < patlen-1; i++)
      skip[(unsigned char)word[i]] = patlen-i-1;

   for (p = start+patlen-1; p < end; f ? (f=NULL) : (p += skip [tolowertab [*p]])) {
      while (*p == '\t') {
	 FIND_NEXT(p,end);
	 p += patlen-1;
	 if (p > end)
	    return count;
      }
      ++_dict_comparisons;		/* counter for profiling */
      
				/* FIXME.  Optimize this inner loop. */
      for (j = patlen - 1, pt = p, wpt = word + patlen - 1; j >= 0; j--) {
	 if (pt < start)
	    break;

 	 while (pt >= start && !dbindex -> isspacealnum[*pt]) {
	    if (*pt == '\n' || *pt == '\t')
	       goto continue2;

	    --pt;
	 }

	 if (tolowertab [*pt--] != *wpt--)
	    break;
      }

      if (j == -1) {
	 switch (flag){
	 case BMH_SUBSTRING:
	    break;
	 case BMH_SUFFIX:
	    if (p[1] != '\t')
	       continue;

	    break;
	 case BMH_WORD:
	    ptr = p - patlen + 1;

	    if (ptr > start && !isspacepuncttab [ptr [-1]])
	       continue;
	    if (p < end && !isspacepuncttab [p [1]])
	       continue;

	    break;
	 }

	 for (; pt > start && *pt != '\n'; --pt)
	    if (*pt == '\t')
	       goto continue2;

	 ++pt;

	 assert (pt >= start && pt < end);

	 if (!previous || altcompare(previous, pt, dbindex->end)) {
	    ++count;
	    datum = dict_word_create( previous = pt, database, dbindex );
#if 0
	    fprintf( stderr, "Adding %d %s, word = %s\n",
		     compare( word, dbindex, p, dbindex->end ),
		     datum->word,
		     word );
#endif
	    if (l)
	       lst_append( l, datum );
	 }
	 FIND_NEXT(p,end);
	 f = p += patlen-1;	/* Set boolean flag to non-NULL value */
	 if (p > end) return count;
      }
continue2:
      ;
   }

   return count;
}

static int dict_search_substring( lst_List l,
				  const char *word,
				  const dictDatabase *database,
				  dictIndex *dbindex)
{
   return dict_search_bmh( l, word, database, dbindex, BMH_SUBSTRING );
}

static int dict_search_word(
   lst_List l,
   const char *word,
   const dictDatabase *database)
{
   lst_Position pos;
   dictWord *dw;
   const char *p;
   char *ptr;
   int ret1, ret2;
   int count;
   int len;

   assert (database);
   assert (database -> index);

   if (database->index_word){
      ret2 = dict_search_exact( l, word, database, database->index );
      if (ret2 < 0)
	 return ret2;

      count = lst_length (l);

      ret1 = dict_search_exact( l, word, database, database->index_word );
      if (ret1 < 0)
	 return ret1;

      LST_ITERATE (l, pos, dw){
	 if (count-- <= 0){
	    xfree (dw -> word);

	    p = database -> index -> start + dw -> start;
	    assert (p == database -> index -> start || p [-1] == '\n');
	    len = strchr (p, '\t') - p;

	    dw -> word = xmalloc (len + 1);
	    memcpy (dw -> word, p, len);
	    dw -> word [len] = 0;
	    dw -> start = -2;
	    dw -> end   = 0;
#if 1
	    p += len + 1;
	    len = strchr (p, '\t') - p;
	    ptr = (char *) alloca (len + 1);
	    memcpy (ptr, p, len);
	    ptr [len] = '\0';

	    dw -> start = b64_decode (ptr);

	    p += len + 1;
	    len = strchr (p, '\n') - p;
	    ptr = (char *) alloca (len + 1);
	    memcpy (ptr, p, len);
	    ptr [len] = '\0';

	    dw -> end = b64_decode (ptr);
#endif
	 }
      }

      return ret1 + ret2;
   }else{
      return dict_search_bmh( l, word, database, database->index, BMH_WORD );
   }
}

/* return non-zero if success, 0 otherwise */
static int dict_match (
   const regex_t *re,
   const char *word, size_t word_len,
   int eflags)
{
#if defined(REG_STARTEND)
   regmatch_t    subs[1];
   subs [0].rm_so = 0;
   subs [0].rm_eo = word_len;
   return !regexec(re, word, 1, subs, eflags | REG_STARTEND);
#else
   char *word_copy = (char *) alloca (word_len + 1);
   memcpy (word_copy, word, word_len);
   word_copy [word_len] = 0;
   return !regexec(re, word_copy, 0, NULL, eflags);
#endif
}

static int dict_search_regexpr( lst_List l,
				const char *word,
				const dictDatabase *database,
				dictIndex *dbindex,
				int type )
{
   const char    *start = dbindex->start;
   const char    *end   = dbindex->end;
   const char    *p, *pt;
   int           count = 0;
   dictWord      *datum;
   regex_t       re;
   char          erbuf[100];
   int           err;
   unsigned char first;
   const char    *previous = NULL;

   assert (dbindex);

#if 1
   /* optimization code */
   if (optStart_mode){
      if (
	 *word == '^'
	 && dbindex -> isspacealnum [(unsigned char) word[1]]
	 && strchr (word, '|') == NULL)
      {
	 first = word[1];

	 end   = dbindex->optStart[i2c(c2i(first)+1)];
	 start = dbindex->optStart[first];

#if 0
	 fprintf (stderr, "optStart_regexp [%i] = %p\n", first, start);
	 fprintf (stderr, "optStart_regexp [%i] = %p\n", i2c(c2i(first)+1), end);
#endif

	 if (end < start)
	    end = dbindex->end;

//	 FIND_NEXT(end, dbindex -> end);
      }
   }
#endif

   if ((err = regcomp(&re, word, REG_ICASE|REG_NOSUB|type))) {
      regerror(err, &re, erbuf, sizeof(erbuf));
      log_info( "regcomp(%s): %s\n", word, erbuf );
      return 0;
   }

   pt = start;
   while (pt && pt < end) {
      for (p = pt; *p != '\t' && p < end; ++p);
      ++_dict_comparisons;

      if (dict_match (&re, pt, p - pt, 0)) {
	 if (!previous || altcompare(previous, pt, end)) {
	    ++count;
	    datum = dict_word_create( previous = pt, database, dbindex );
#if 0
	    fprintf( stderr, "Adding %d %s\n",
		     compare( word, dbindex, pt, end ),
		     datum->word);
#endif
	    lst_append( l, datum );
	 }
      }
      pt = p + 1;
      FIND_NEXT( pt, end );
   }

   regfree(&re);
   
   return count;
}

static int dict_search_re( lst_List l,
			   const char *word,
			   const dictDatabase *database,
			   dictIndex    *dbindex)
{
   return dict_search_regexpr( l, word, database, dbindex, REG_EXTENDED );
}

static int dict_search_regexp( lst_List l,
			       const char *word,
			       const dictDatabase *database,
			       dictIndex    *dbindex)
{
   return dict_search_regexpr( l, word, database, dbindex, 0 /*REG_BASIC*/ );
}

static int dict_search_soundex( lst_List l,
				const char *word,
				const dictDatabase *database,
				dictIndex    *dbindex)
{
   const char *pt;
   const char *end;
   int        count = 0;
   dictWord   *datum;
   char       soundex  [10];
   char       soundex2 [5];
   char       buffer[MAXWORDLEN];
   char       *d;
   const unsigned char *s;
   int        i;
   int        c = (unsigned char)*word;
   const char *previous = NULL;

   assert (dbindex);

   if (optStart_mode){
      pt  = dbindex->optStart[ c ];
      end = dbindex->optStart[ i2c(c2i(c)+1) ];
      if (end < pt)
	 end = dbindex->end;
   }else{
      pt  = dbindex->start;
      end = dbindex->end;
   }

   txt_soundex2 (word, soundex);

   while (pt && pt < end) {
      for (i = 0, s = pt, d = buffer; i < MAXWORDLEN - 1; i++, ++s) {
	 if (*s == '\t') break;
	 if (!dbindex -> isspacealnum [*s]) continue;
	 *d++ = *s;
      }
      *d = '\0';

      txt_soundex2 (buffer, soundex2);
      if (!strcmp (soundex, soundex2)) {
	 if (!previous || altcompare(previous, pt, end)) {
	    datum = dict_word_create( previous = pt, database, dbindex );
	    lst_append( l, datum );
	    ++count;
	 }
      }
      FIND_NEXT(pt,end);
   }

   return count;
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

typedef struct lev_args_ {
   const dictDatabase *database;
   dictIndex          *dbindex;
   lst_List            l;
} LEV_ARGS;

#define CHECK(word, args)                                \
   if ((pt = dict_index_search((word), (args) -> dbindex))         \
       && !compare((word), (args) -> dbindex, pt, (args) -> dbindex -> end)) \
   { \
      if (!set_member(s,(word))) {                       \
	 ++count;                                        \
	 set_insert(s,str_find((word)));                 \
	 datum = dict_word_create(pt, (args) -> database, (args) -> dbindex);\
	 lst_append((args) -> l, datum);                        \
         PRINTF(DBG_LEV,("  %s added\n",(word)));     \
      }                                               \
   }

#define LEV_VARS \
      char tmp;

#include "lev.h"

static int dict_search_levenshtein (
   lst_List l,
   const char *word,
   const dictDatabase *database,
   dictIndex *dbindex)
{
   LEV_ARGS lev_args = { database, dbindex, l };

   assert (database);
   assert (dbindex);

   if (database -> alphabet){
      return dict_search_lev (
	 word, database -> alphabet, dbindex -> flag_utf8, &lev_args);
   }else{
      if (dbindex -> flag_utf8){
	 return dict_search_lev (
	    word, global_alphabet_ascii, 1, &lev_args);
      }else{
	 return dict_search_lev (
	    word, global_alphabet_8bit, 0, &lev_args);
      }
   }
}

/*
  makes anagram of the 8-bit string 'str'
  if length == -1 then str is 0-terminated string
*/
static void stranagram_8bit (char *str, int length)
{
   char* i = str;
   char* j;
   char v;

   assert (str);

   if (length == -1)
       length = strlen (str);

   j = str + length - 1;

   while (i < j){
       v = *i;
       *i = *j;
       *j = v;

       ++i;
       --j;
   }
}

#if HAVE_UTF8
/*
  makes anagram of the utf-8 string 'str'
  Returns non-zero if success, 0 otherwise
*/
static int stranagram_utf8 (char *str)
{
   size_t len;
   char   *p;

   mbstate_t ps;

   assert (str);

   memset (&ps,  0, sizeof (ps));

   for (p = str; *p; ){
      len = mbrlen__ (p, MB_CUR_MAX__, &ps);
      if ((int) len < 0)
	 return 0; /* not a UTF-8 string */

      if (len > 1)
	  stranagram_8bit (p, len);

      p += len;
   }

   stranagram_8bit (str, -1);
   return 1;
}
#endif

/* makes anagram of utf-8 string 'str' */
static int stranagram (char *str, int utf8_string)
{
   assert (str);

#if HAVE_UTF8
   if (utf8_string){
      return stranagram_utf8 (str);
   }else{
      stranagram_8bit (str, -1);
      return 1;
   }
#else
   stranagram_8bit (str, -1);
   return 1;
#endif
}

static int dict_search_suffix(
   lst_List l,
   const char *word,
   const dictDatabase *database)
{
   int ret;
   lst_Position p;
   dictWord *dw;
   char *buf = NULL;
   int count;

   assert (database);

   if (database->index_suffix){
      buf = (char *) alloca (strlen (word));
      strcpy (buf, word);

      PRINTF(DBG_SEARCH, ("anagram: '%s' ==> ", buf));
      if (!stranagram (buf, utf8_mode)){
	 PRINTF(DBG_SEARCH, ("failed building anagram\n"));
	 return 0; /* invalid utf8 string */
      }

      count = lst_length (l);

      PRINTF(DBG_SEARCH, ("'%s'\n", buf));
      ret = dict_search_prefix ( l, buf, database, database->index_suffix);

      LST_ITERATE (l, p, dw) {
	 if (count-- <= 0){
	    stranagram (dw -> word, utf8_mode);
	 }
      }
      return ret;
   }else{
      return dict_search_bmh( l, word, database, database -> index, BMH_SUFFIX );
   }
}

#if HAVE_UTF8
static const char *utf8_err_msg = "\
error: The request is not a valid UTF-8 string";
#endif

/*
  returns a number of matches ( >= 0 ) or
  negative value for invalid UTF-8 string
*/
int dict_search_database_ (
   lst_List l,
   const char *const word,
   const dictDatabase *database,
   int strategy )
{
   char       *buf      = NULL;
#if HAVE_UTF8
   dictWord   *dw       = NULL;
#endif

   assert (database);
   assert (database -> index);

   if (strategy == DICT_STRAT_DOT){
      strategy = database -> default_strategy;
   }

   buf = alloca( strlen( word ) + 1 );

#if HAVE_UTF8
   if (
      !strcmp(utf8_err_msg, word) ||
      tolower_alnumspace (
	 word, buf, database -> index -> flag_allchars, utf8_mode))
   {
      PRINTF(DBG_SEARCH, ("tolower_... ERROR!!!\n"));
      
      dw = xmalloc (sizeof (dictWord));
      memset (dw, 0, sizeof (dictWord));
      
      dw -> database = database;
      dw -> def      = utf8_err_msg;
      dw -> def_size = -1;
      dw -> word     = strdup (word);
      
      lst_append (l, dw);
      
      return -1;
   }
#else
   tolower_alnumspace (word, buf, database -> index -> flag_allchars, utf8_mode);
#endif

   if (!buf [0] && word [0]){
      /*
        This may happen because of invalid --locale specified.
	Without following line entire dictionary will be returned
          for non-ASCII words.
      */
      return 0;
   }

/*
   if (!database->index)
      database->index =
	  dict_index_open( database->indexFilename, 1, 0, 0 );
   if (!database->index_suffix && database->indexsuffixFilename)
      database->index_suffix =
	  dict_index_open(
	      database->indexsuffixFilename,
	      0, database->index->flag_utf8, database->index->flag_allchars );
*/

   switch (strategy) {
   case DICT_STRAT_EXACT:
      return dict_search_exact( l, buf, database, database->index );

   case DICT_STRAT_PREFIX:
      return dict_search_prefix( l, buf, database, database->index );

   case DICT_STRAT_SUBSTRING:
      return dict_search_substring( l, buf, database, database->index );

   case DICT_STRAT_SUFFIX:
      return dict_search_suffix( l, buf, database );

   case DICT_STRAT_RE:
      return dict_search_re( l, word, database, database->index );

   case DICT_STRAT_REGEXP:
      return dict_search_regexp( l, word, database, database->index );

   case DICT_STRAT_SOUNDEX:
      return dict_search_soundex( l, buf, database, database->index );

   case DICT_STRAT_LEVENSHTEIN:
      return dict_search_levenshtein( l, buf, database, database->index);

   case DICT_STRAT_WORD:
      return dict_search_word( l, buf, database);

   default:
      return 0;
/*
      err_internal( __FUNCTION__, "Search strategy %d unknown\n", strategy );
*/
   }
}

/*
  Replaces invisible databases with db argument.
 */
static void replace_invisible_databases (
   lst_Position pos,
   const dictDatabase *db)
{
   dictWord *dw;

   while (pos){
      dw = (dictWord *) lst_get_position (pos);

      if (
	 dw -> database &&
	 dw -> database -> invisible &&
	 !dw -> database_visible)
      {
	 dw -> database_visible = db;
      }

      pos = lst_next_position (pos);
   }
}

/*
  returns a number of matches ( >= 0 ) or
  negative value for invalid UTF-8 string
*/
int dict_search (
   lst_List l,
   const char *const word,
   const dictDatabase *database,
   int strategy,
   int option_mime,
   int *extra_result,
   const dictPluginData **extra_data,
   int *extra_data_size)
{
   int count = 0;
   dictWord *dw;

   int norm_strategy = strategy & ~DICT_MATCH_MASK;

   if (extra_result)
      *extra_result = DICT_PLUGIN_RESULT_NOTFOUND;

   assert (word);
   assert (database);

   if (
      database -> strategy_disabled &&
      database -> strategy_disabled [norm_strategy])
   {
      /* disable_strategy keyword from configuration file */
#if 0
      PRINTF (DBG_SEARCH, (
	 ":S: strategy '%s' is disabled for database '%s'\n",
	 get_strategies () [norm_strategy] -> name,
	 database -> databaseName ? database -> databaseName : "(unknown)"));
#endif
      return 0;
   }

   PRINTF (DBG_SEARCH, (":S: Searching in '%s'\n", database -> databaseName));

#if 0
   fprintf (stderr, "STRATEGY: %x\n", strategy);
#endif

   if (database -> index){
      PRINTF (DBG_SEARCH, (":S:   database search\n"));
      count = dict_search_database_ (l, word, database, norm_strategy);
   }

#ifdef USE_PLUGIN
   if (!count && database -> plugin){
      PRINTF (DBG_SEARCH, (":S:   plugin search\n"));
      count = dict_search_plugin (
	 l, word, database, strategy,
	 extra_result, extra_data, extra_data_size);

      if (count)
	 return count;
   }
#endif

   if (!count && database -> virtual_db_list){
      lst_Position db_list_pos;
      dictDatabase *db = NULL;
      int old_count = lst_length (l);

      assert (lst_init_position (database -> virtual_db_list));

      LST_ITERATE (database -> virtual_db_list, db_list_pos, db){
	 count += dict_search (
	    l, word, db, strategy, option_mime,
	    extra_result, extra_data, extra_data_size);
      }

      if (count > old_count){
	 replace_invisible_databases (
	    lst_nth_position (l, old_count + 1),
	    database);
      }
   }

   if (!count && database -> mime_db){
      int old_count = lst_length (l);

      count += dict_search (
	 l, word,
	 (option_mime ? database -> mime_mimeDB :
	  database -> mime_nomimeDB),
	 strategy, 0,
	 extra_result, extra_data, extra_data_size);

      if (count > old_count){
	 replace_invisible_databases (
	    lst_nth_position (l, old_count + 1),
	    database);
      }
   }

   if (count > 0 && extra_result)
      *extra_result = DICT_PLUGIN_RESULT_FOUND;

   return count;
}

dictIndex *dict_index_open(
   const char *filename,
   int init_flags, int flag_utf8, int flag_allchars)
{
   struct stat sb;
   static int  tabInit = 0;
   dictIndex   *i;
   dictDatabase db;
   int         j;
   char        buf[2];

   int         old_8bit_format = 0;

   int first_char;
   int first_char_uc;

   if (!filename)
      return NULL;

   i = xmalloc( sizeof( struct dictIndex ) );

   if (!tabInit) dict_table_init();
   tabInit = 1;

   memset( i, 0, sizeof( struct dictIndex ) );

   if ((i->fd = open( filename, O_RDONLY )) < 0)
      err_fatal_errno( __FUNCTION__,
		       "Cannot open index file \"%s\"\n", filename );
   if (fstat( i->fd, &sb ))
      err_fatal_errno( __FUNCTION__,
		       "Cannot stat index file \"%s\"\n", filename );
   i->size = sb.st_size;

   if (mmap_mode){
#ifdef HAVE_MMAP
      if (i->size) {
         i->start = mmap( NULL, i->size, PROT_READ, MAP_SHARED, i->fd, 0 );
         if ((void *)i->start == (void *)(-1))
            err_fatal_errno (
               __FUNCTION__,
               "Cannot mmap index file \"%s\"\b", filename );
      } else i->start = NULL;  /* allow for /dev/null dummy index */
#else
      err_fatal (__FUNCTION__, "This should not happen");
#endif
   }else{
      i->start = xmalloc (i->size);
      if (-1 == read (i->fd, (char *) i->start, i->size))
	 err_fatal_errno (
	    __FUNCTION__,
	    "Cannot read index file \"%s\"\b", filename );

      close (i -> fd);
      i -> fd = 0;
   }

   i->end = i->start + i->size;

   i->flag_8bit     = 0;
   i->flag_utf8     = flag_utf8;
   i->flag_allchars = flag_allchars;
   i->isspacealnum  = isspacealnumtab;

   if (optStart_mode){
      for (j = 0; j <= UCHAR_MAX; j++)
	 i->optStart[j] = i->start;
   }

   if (init_flags){
      memset (&db, 0, sizeof (db));
      db.index = i;

      i->flag_allchars = 1;
      i->isspacealnum = isspacealnumtab_allchars;

      i->flag_allchars =
	 0 != dict_search_database_ (NULL, DICT_FLAG_ALLCHARS, &db, DICT_STRAT_EXACT);
      PRINTF(DBG_INIT, (":I:     \"%s\": flag_allchars=%i\n", filename, i->flag_allchars));

      /* utf8 flag */
      if (!i -> flag_allchars)
	 i -> isspacealnum = isspacealnumtab;

      i->flag_utf8 =
	 0 != dict_search_database_ (NULL, DICT_FLAG_UTF8, &db, DICT_STRAT_EXACT);
      PRINTF(DBG_INIT, (":I:     \"%s\": flag_utf8=%i\n", filename, i->flag_utf8));
      if (i->flag_utf8 && !utf8_mode){
	 log_info( ":E: locale '%s' can not be used for utf-8 dictionaries. Exiting\n", locale );
	 exit (1);
      }

      /* 8bit flag */
      i->flag_8bit =
	 0 != dict_search_database_ (NULL, DICT_FLAG_8BIT_NEW, &db, DICT_STRAT_EXACT);
      old_8bit_format =
	 0 != dict_search_database_ (NULL, DICT_FLAG_8BIT_OLD, &db, DICT_STRAT_EXACT);

      if (old_8bit_format){
	 log_info( ":E: index file '%s' was created using dictfmt <1.9.15\n"
	           ":E:   and can not be used with dictd-1.9.15 or later\n"
	           ":E:   Rebuild it like this:\n"
	           ":E:   dictunformat db.index < db.dict | dictfmt -t --locale <8bit-locale> db-new\n", filename );
	 exit (1);
      }

      PRINTF(DBG_INIT, (":I:     \"%s\": flag_8bit=%i\n", filename, i->flag_8bit));
      if (i->flag_8bit && !bit8_mode){
	 log_info( ":E: locale '%s' can not be used for 8-bit dictionaries. Exiting\n", locale );
	 exit (1);
      }
   }

   if (optStart_mode){
      buf[0] = ' ';
      buf[1] = '\0';
      i->optStart[ ' ' ] = binary_search_8bit( buf, i, i->start, i->end );

      for (j = 0; j < charcount; j++) {
	 first_char = c(j);

	 buf[0] = first_char;
	 buf[1] = '\0';

	 i->optStart [first_char]
	    = binary_search_8bit( buf, i, i->start, i->end );

	 if (!utf8_mode || first_char < 128){
	    first_char_uc = toupper (first_char);

	    assert (first_char_uc >= 0 && first_char_uc <= UCHAR_MAX);

	    i->optStart [first_char_uc] = i->optStart [first_char];
	 }
      }

      for (j = '0'; j <= '9'; j++) {
	 buf[0] = j;
	 buf[1] = '\0';
	 i->optStart[j] = binary_search_8bit( buf, i, i->start, i->end );
      }

      i->optStart[UCHAR_MAX]   = i->end;
      i->optStart[UCHAR_MAX+1] = i->end;

      if (dbg_test (DBG_SEARCH)){
	 for (j=0; j <= UCHAR_MAX; ++j){
	    if (!utf8_mode || j <= CHAR_MAX)
	       printf (
		  "optStart [%c] = (%p) %10s\n",
		  j,
		  i->optStart [j],
		  i->optStart [j]);
	    else
	       printf (
		  "optStart [%i] = (%p) %10s\n",
		  j,
		  i->optStart [j],
		  i->optStart [j]);
	 }
      }
   }

   return i;
}

void dict_index_close( dictIndex *i )
{
   if (!i)
      return;

   if (mmap_mode){
#ifdef HAVE_MMAP
      if (i->fd >= 0) {
         if(i->start)
            munmap( (void *)i->start, i->size );
	 close( i->fd );
	 i->fd = 0;
      }
#else
      err_fatal (__FUNCTION__, "This should not happen");
#endif
   }else{
      if (i -> start)
	 xfree ((char *) i -> start);
   }

   i->start = i->end = NULL;
   i->flag_utf8      = 0;
   i->flag_allchars  = 0;
   i->isspacealnum   = NULL;

   xfree (i);
}
