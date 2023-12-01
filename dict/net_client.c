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
#include <errno.h>
#include <maa.h>

#include "defs.h"
#include "net.h"

int net_connect_tcp( const char *host, const char *service, int address_family )
{
	struct addrinfo *r = NULL;
	struct addrinfo *rtmp = NULL;
	struct addrinfo hints;
	int s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = address_family;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

	if (getaddrinfo(host, service, &hints, &r) != 0) {
		return NET_NOHOST;
	}

	for (rtmp = r; r != NULL; r = r->ai_next) {
		s = socket (r->ai_family, r->ai_socktype, r->ai_protocol);
		if (s < 0) {
			if (r->ai_next != NULL)
				continue;

			err_fatal_errno( __func__, "Can't open socket\n");
		}

		PRINTF(DBG_VERBOSE,("Trying %s (%s)...", host, inet_ntopW(r->ai_addr)));

		if (connect (s, r->ai_addr, r->ai_addrlen) >= 0) {
			PRINTF(DBG_VERBOSE,("Connected."));
			freeaddrinfo(rtmp);
			return s;
		}

		PRINTF(DBG_VERBOSE,("Failed: %s\n", strerror(errno)));

		close(s);
	}
	freeaddrinfo(rtmp);

	return NET_NOCONNECT;
}
