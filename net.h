/* net.h -- 
 * Created: Sat Feb 22 00:39:54 1997 by faith@cs.unc.edu
 * Revised: Mon Mar 10 21:18:44 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
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
 * $Id: net.h,v 1.3 1997/03/11 04:31:41 faith Exp $
 * 
 */


extern const char *net_hostname( void );
extern int        net_open_tcp( const char *service, int queueLength );
extern void       net_detach( void );
