/* decode.c -- 
 * Created: Wed Aug 23 23:38:33 1995 by r.faith@ieee.org
 * Revised: Thu Aug 24 03:38:06 1995 by r.faith@ieee.org
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
 * $Id: decode.c,v 1.1 1996/09/23 15:33:24 faith Exp $
 * 
 */

#include "dict.h"
#include <sys/stat.h>

/* 0x80..0xfd 0x01..0xff: 6 bits -> 64 * 253 -> 16192
   0xfe..0xff 0x01..0xff 0x01..0xff: 1 bits -> 2 * 253 * 253 = 128018

   for a total of 144210
*/

int decode( const char *c )
{
   int value;
   int code[3];

   code[0] = (unsigned char)*c++;
   code[1] = (unsigned char)*c++;
   code[2] = (unsigned char)*c;

   if (code[1] == 255) code[1] = 0;
   if (code[1] == 254) code[1] = '\n';
   if (code[2] == 255) code[2] = 0;
   if (code[2] == 254) code[2] = '\n';
   
   if (code[0] <= 0xfd) {
      value = (((code[0] & 0x7c) >> 2) + 32 * (code[0] & 1)) * 253 + code[1];
   } else {
      value = 253 * 64
	      + (code[0] & 0x01) * 253 * 253
	      + code[1] * 253
	      + code[2];
   }
   return value;
}

int read_list( int fd, char ***wlPt, char *ascii[128], char *space )
{
   int         count      = 0;
   int         asciis     = 0;
   int         ascii_val  = 0;
   int         first      = 1;
   char        **wordlist = NULL;
   int         expected;
   int         adjust     = 0;
   struct stat sb;
   char        *bigBuf;
   char        *pt;
   int         i;
   char        *number;

   fstat( fd, &sb );
   bigBuf = malloc( sb.st_size );

   read( fd, bigBuf, sb.st_size );
   
   expected = atoi( bigBuf + 1 );
   if (!expected || expected > MAXWORDS) {
      fprintf( stderr,
	       "List has %d words -- %d is maximum number\n",
	       expected, MAXWORDS );
      exit( 1 );
   }

   for (pt = bigBuf; *pt != '\n'; pt++); /* skip first line */
   
   if (*++pt == '!') {		/* handle second line */
      pt += 2;
      for (number = pt; *pt != '\n'; pt++) {
	 if (*pt == ' ' || *pt == '\n') {
	    *pt = '\0';
	    if (first) {
	       *space = atoi( number );
	       first = 0;
	    } else {
	       ascii[ atoi( number ) ] = (void *)1; /* kludge */
	       ++asciis;
	    }
	    number = pt + 1;
	 }
      }
      *pt = '\0';
      if (first) {
	 *space = atoi( number );
	 first = 0;
      } else {
	 ascii[ atoi( number ) ] = (void *)1; /* kludge */
	 ++asciis;
      }
      if (Debug)
	 fprintf( stderr, "Using %d single character codes\n", asciis );
      *wlPt = malloc( sizeof( char **) * (expected - asciis) );
      wordlist = *wlPt;
      first = 0;
      adjust = asciis;
   }

   for (*pt = '\0', i = 0; i < asciis; i++) {
      ++pt;
      while (ascii[ ascii_val ] != (void *)1) ++ascii_val;
      if (ascii_val > 128) {
	 fprintf( stderr, "ascii_val = %d\n", ascii_val );
	 exit( 1 );
      }
      ascii[ ascii_val ] = pt;
      while (*pt != '\n') ++pt;
      *pt = '\0';
      if (Debug)
	 fprintf( stderr, "%d gets %s\n", ascii_val, ascii[ ascii_val ] );
   }

   for (*pt = '\0', i = 0; i < expected - asciis; i++) {
      ++pt;
      wordlist[ count++ ] = pt;
      while (*pt != '\n') ++pt;
      *pt = '\0';
   }

   if (Debug)
      fprintf( stderr, "Using %d+%d words for dictionary\n", count, asciis );

   return expected - asciis;
}

char *decode_line( const char *line, char **wordlist,
		   char *ascii[128], char space )
{
   static char outBuf[BUFSIZ];
   char        *out = outBuf;
   const char  *pt;
   char        *p;
   
   for (pt = line; *pt; pt++) {
      if ((unsigned char)*pt >= 0x80) {
	 if (*pt != space) {
	    int value = decode( pt );

	    for (p = wordlist[ value ]; *p;) *out++ = *p++;
	       
	    if ((unsigned char)*pt >= 0xfe) pt += 2;
	    else                            pt += 1;
	       
	    if (pt[1] != '\n') *out++ = ' ';
	 }
      } else if (ascii[ (unsigned char)*pt ]) {
	 for (p = ascii[ (unsigned char)*pt ]; *p;) *out++ = *p++;
	 
	 if (pt[1] != '\n') *out++ = ' ';
      } else if (*pt != space)
	 *out++ = *pt;
   }
   
   *out = '\0';
   
   return outBuf;
}
