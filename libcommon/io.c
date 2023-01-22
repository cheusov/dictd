/* net.c -- 
 * Created: Fri Feb 21 20:58:10 1997 by faith@dict.org
 * Copyright 1997, 1998, 1999, 2000, 2002 Rickard E. Faith (faith@dict.org)
 * Copyright 2002-2008 Aleksey Cheusov (vle@gmx.net)
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
 */

/*
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "dictd.h"
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"

int net_read( int s, char *buf, int maxlen )
{
	int  len;
	int  n = 0;
	char c;
	char *pt = buf;

	*pt = '\0';

	for (len = 0; len < maxlen && (n = read( s, &c, 1 )) > 0; /*void*/) {
		switch (c) {
			case '\n': *pt = '\0';       return len;
			case '\r':                   break;
			default:   *pt++ = c; ++len; break;
		}
	}
	*pt = '\0';
	if (!n) return len ? len : EOF;
	return n;			/* error code */
}

int net_write( int s, const char *buf, int len )
{
	int left = len;
	int count;

	while (left) {
		if ((count = write(s, buf, left)) != left) {
			if (count <= 0) return count; /* error code */
		}
		left -= count;
	}
	return len;
}

const char *inet_ntopW(struct sockaddr *sa) {
	static char buf[40];

	switch (sa->sa_family) {
		case AF_INET:
			return inet_ntop(sa->sa_family, &(((struct sockaddr_in *)sa)->sin_addr), buf, sizeof(buf));
		case AF_INET6:
			return inet_ntop(sa->sa_family, &(((struct sockaddr_in6 *)sa)->sin6_addr), buf, sizeof(buf));
		default:
			errno = EAFNOSUPPORT;
			return NULL;
	}
}
