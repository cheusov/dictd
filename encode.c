/* encode.c -- 
 * Created: Wed Aug 23 23:38:20 1995 by r.faith@ieee.org
 * Revised: Thu Aug 24 01:04:36 1995 by r.faith@ieee.org
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
 * $Id: encode.c,v 1.1 1996/09/23 15:33:26 faith Exp $
 * 
 */

char *encode( int value )
{
   static char buffer[5];

   if (value < 253 * 64) {
      int set = value / 253;

      if (set >= 32)
	 buffer[0] = 0x80 | ((set & 0x1f) << 2) | 1;
      else
	 buffer[0] = 0x80 | (set << 2);
      buffer[1] = value - set * 253;
      buffer[2] = 0;
   } else {
      int diff = value - 253 * 64;
      int set = diff / 253 / 253;
      int r = diff - set * 253 * 253;
      int subset = r / 253;
      
      buffer[0] = 0xfe + set;
      buffer[1] = subset;
      buffer[2] = r - subset * 253;
      buffer[3] = 0;
      
      if (buffer[2] == 0)    buffer[2] = 255;
      if (buffer[2] == '\n') buffer[2] = 254;
   }

   if (buffer[1] == 0)    buffer[1] = 255;
   if (buffer[1] == '\n') buffer[1] = 254;
   
   return buffer;
}
