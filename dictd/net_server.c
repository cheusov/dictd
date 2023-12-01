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

#include "dictd.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (-1)
#endif

const char *net_hostname( void )
{
	static char hostname[128] = "";
	int err;

	if (!hostname[0]) {
		if (err = gethostname(hostname, sizeof(hostname)), err != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
			exit(EXIT_FAILURE);
		}
	}

	hostname[sizeof(hostname)-1] = '\0';
	return hostname;
}

int net_open_tcp (
	const char *address,
	const char *service,
	int queueLength,
	int address_family)
{
	struct addrinfo hints, *r, *rtmp;
	int s = -1;
	int err;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = address_family;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(address, service, &hints, &r) != 0)
		err_fatal( __func__, "getaddrinfo: Failed, address = \"%s\", service = \"%s\"\n", address, service);

	for (rtmp = r; r != NULL; r = r->ai_next) {
		s = socket (r->ai_family, r->ai_socktype, r->ai_protocol);

		if (s < 0) {
			if (r->ai_next != NULL)
				continue;

			freeaddrinfo(rtmp);
			err_fatal_errno(__func__, "Can't open socket\n");
		}

		{
			const int one = 1;
			err = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
			if (err != 0){
				err_fatal_errno(__func__, "Can't setsockopt\n");
			}
		}

		if (bind(s, r->ai_addr, r->ai_addrlen) < 0) {
			if (r->ai_next != NULL) {
				close(s);
				continue;
			}
			freeaddrinfo(rtmp);
			err_fatal_errno( __func__, "Can't bind %s/tcp to %s\n",
							 service, address?address:"ANY" );
		}

		if (listen( s, queueLength ) < 0) {
			if (r->ai_next != NULL) {
				close(s);
				continue;
			}
			freeaddrinfo(rtmp);
			err_fatal_errno( __func__, "Can't listen to %s/tcp on %s\n",
							 service, address );
		}

		break;
	}
	freeaddrinfo(rtmp);

	return s;
}
