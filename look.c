/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
 *
 * This version of look.c is from the 4.4BSD-Lite distribution.
 * Modified extensively on Sun Nov 27 08:14:32 1994 by faith@cs.unc.edu
 * Revised: Sun Dec  4 10:21:05 1994 by faith@cs.unc.edu
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
static char sccsid[] = "@(#)look.c	8.1 (Berkeley) 6/14/93";
static char sccsid2[] = "(look.c has been extensively modified for dict)";
#endif /* not lint */

#if 0
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#if 0
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define DEBUG 0

#if DEBUG
extern int Debug;
#endif

#define	EQUAL		0
#define	GREATER		1
#define	LESS		(-1)
#define NO_COMPARE	(-2)

#define CONV(c) ((c == ' ' || isalnum(c)) ? tolower(c) : NO_COMPARE)

char        *look( char *string, char *front, char *back );
int         compare( char *s1, char *s2, char *back );
static char *binary_search( char *string, char *front, char *back );
static char *linear_search( char *string, char *front, char *back );

char *look(char *string, char *front, char *back)
{
   int  ch;
   char *readp, *writep;

   /* Reformat string string to avoid doing it multiple times later. */
   for (readp = writep = string; (ch = *readp++);) {
      ch = CONV(ch);
      if (ch != NO_COMPARE)
	    *(writep++) = ch;
   }
   *writep = '\0';

   front = binary_search(string, front, back);
   front = linear_search(string, front, back);

   return front;
}


/*
 * Binary search for "string" in memory between "front" and "back".
 * 
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 * 
 * Invariants:
 * 	front points to the beginning of a line at or before the first 
 *	matching string.
 * 
 * 	back points to the beginning of a line at or after the first 
 *	matching line.
 * 
 * Base of the Invariants.
 * 	front = NULL; 
 *	back = EOF;
 * 
 * Advancing the Invariants:
 * 
 * 	p = first newline after halfway point from front to back.
 * 
 * 	If the string at "p" is not greater than the string to match, 
 *	p is the new front.  Otherwise it is the new back.
 * 
 * Termination:
 * 
 * 	The definition of the routine allows it return at any point, 
 *	since front is always at or before the line to print.
 * 
 * 	In fact, it returns when the chosen "p" equals "back".  This 
 *	implies that there exists a string is least half as long as 
 *	(back - front), which in turn implies that a linear search will 
 *	be no more expensive than the cost of simply printing a string or two.
 * 
 * 	Trying to continue with binary search at this point would be 
 *	more trouble than it's worth.
 */

#define	SKIP_PAST_NEWLINE(p, back) \
while (p < back && *p++ != '\n');

static char *binary_search( char *string, char *front, char *back )
{
   char *p;
#if DEBUG
   char *save = front;
#endif

#if DEBUG
   if (Debug) printf( "front = %lu, back = %lu\n",
		      (unsigned long)front,
		      (unsigned long)back );
#endif

   p = front + (back - front) / 2;
   SKIP_PAST_NEWLINE(p, back);

   /*
    * If the file changes underneath us, make sure we don't
    * infinitely loop.
    */
   while (p < back && back > front) {
#if DEBUG
      if (Debug) {
	 char buffer[100];
	 char *s = p;
	 char *d = buffer;

	 printf( "front = %p, p = %p, back = %p\n", front, p, back );

	 while (s < back && *s != '\t') *d++ = *s++;
	 *d = '\0';
	 
	 printf( "%10lu; \"%s\" with \"%s\" = %d",
		 (long unsigned)(p - save),
		 string,
		 buffer,
		 compare( string, p, back ));

	 s = front;
	 d = buffer;
	 while (s < back && *s != '\t') *d++ = *s++;
	 *d = '\0';
	 printf( " (front = \"%s\",", buffer );
	 
	 s = back;
	 s -= 2;
	 while (*s != '\n') --s;
	 ++s;
	 d = buffer;
	 while (s < back && *s != '\t') *d++ = *s++;
	 *d = '\0';
	 printf( " back = \"%s\")\n", buffer );
	 
      }
#endif
      
      if (compare(string, p, back) == GREATER)
	    front = p;
      else
	    back = p;
      p = front + (back - front) / 2;
      SKIP_PAST_NEWLINE(p, back);
#if DEBUG
      if (Debug) printf( "front = %p, p = %p, back = %p\n", front, p, back );
#endif
   }
   return (front);
}

/*
 * Find the first line that starts with string, linearly searching from front
 * to back.
 * 
 * Return NULL for no such line.
 * 
 * This routine assumes:
 * 
 * 	o front points at the first character in a line. 
 *	o front is before or at the first line to be printed.
 */

static char *linear_search( char *string, char *front, char *back )
{
   while (front < back) {
      switch (compare(string, front, back)) {
      case EQUAL:		/* Found it. */
	 return (front);
	 break;
      case LESS:		/* No such string. */
	 return (NULL);
	 break;
      case GREATER:		/* Keep going. */
	 break;
      }
      SKIP_PAST_NEWLINE(front, back);
   }
   return (NULL);
}

/*
 * Return LESS, GREATER, or EQUAL depending on how the string1 compares with
 * string2 (s1 ??? s2).
 * 
 * 	o Matches up to len(s1) are EQUAL. 
 *	o Matches up to len(s2) are GREATER.
 * 
 * Compare understands about the -f and -d flags, and treats comparisons
 * appropriately.
 * 
 * The string "s1" is null terminated.  The string s2 is '\n' terminated (or
 * "back" terminated).
 */

int compare( char *s1, char *s2, char *back )
{
   int ch;

   for (; *s1 && s2 < back && *s2 != '\n';) {
      ch = *s2;
      ch = CONV(ch);
      if (ch == NO_COMPARE) {
	 ++s2;		/* Ignore character in comparison. */
	 continue;
      }
      if (*s1 != ch)
	    return (*s1 < ch ? LESS : GREATER);
      ++s1;
      ++s2;
   }
   return (*s1 ? GREATER : EQUAL);
}
