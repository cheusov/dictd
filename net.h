/* net.h -- 
 * Created: Sat Feb 22 00:39:54 1997 by faith@dict.org
 * Revised: Fri Dec 22 06:06:58 2000 by faith@dict.org
 * Copyright 1997, 2000 Rickard E. Faith (faith@dict.org)
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
 * $Id: net.h,v 1.7 2000/12/22 14:15:25 faith Exp $
 * 
 */


extern const char *net_hostname( void );
extern int        net_connect_tcp( const char *host, const char *service );
extern int        net_open_tcp( const char *service, int queueLength );
extern void       net_detach( void );
extern int        net_read( int s, char *buf, int maxlen );
extern int        net_write( int s, const char *buf, int len );

#define NET_NOHOST     (-1)
#define NET_NOSERVICE  (-2)
#define NET_NOPROTOCOL (-3)
#define NET_NOCONNECT  (-4)
