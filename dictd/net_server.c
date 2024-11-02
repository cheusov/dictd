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
#include <stdbool.h>

#include "dictd.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (-1)
#endif

#define ESOOPEN -1
#define ESOOPT -2
#define ESOOPTIPV6 -3
#define ESOBIND -4
#define ESOLISTEN -5

static void handle_so_setup_error(int err, const char * routine, const char *address, const char *service) {
	switch (err) {
		case ESOOPEN:
			err_fatal_errno(routine, "Can't open socket\n");
			break;
		case ESOOPT:
			err_fatal_errno(routine, "Can't setsockopt\n");
			break;
		case ESOOPTIPV6:
			err_fatal_errno(routine, "Can't setsockopt for ipv6 only\n");
			break;
		case ESOBIND:
			err_fatal_errno(routine, "Can't bind %s/tcp to %s\n",
							 service, address?address:"ANY" );
			break;
		case ESOLISTEN:
			err_fatal_errno(routine, "Can't listen to %s/tcp on %s\n",
							 service, address );
			break;
		default:
			err_fatal_errno(routine, "Unknown error setting up socket: %d\n", err);
			break;
	}
}

static int setup_socket(const struct addrinfo *r, int queueLength, bool set_ipv6_only, int ipv6_only_val) {
	int s;
	int err;
	const int one = 1;

	s = socket (r->ai_family, r->ai_socktype, r->ai_protocol);
	if (s < 0) {
		return ESOOPEN;
	}

	err = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (err != 0){
		close(s);
		return ESOOPT;
	}
	if (r->ai_family == AF_INET6 && set_ipv6_only) {
		err = setsockopt (s, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only_val, sizeof (ipv6_only_val));
		if (err != 0){
			close(s);
			return ESOOPTIPV6;
		}
	}

	if (bind(s, r->ai_addr, r->ai_addrlen) < 0) {
		close(s);
		return ESOBIND;
	}

	if (listen( s, queueLength ) < 0) {
		close(s);
		return ESOLISTEN;
	}
	return s;
}

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
			if (address_family == AF_INET6) { // Prevent dual stack
				err = setsockopt (s, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof (one));
				if (err != 0){
					// ignore error when not supported, it just a best effort
				}
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

int * net_open_tcp2 (
	const char *address,
	const char *service,
	int queue_len,
	int address_family,
	int * sock_fds_len)
{
	struct addrinfo hints, *r = NULL, *rtmp = NULL;
	int *sock_fds = NULL;
	*sock_fds_len = 0;
	int err = 0;
	int sock = -1;

	if (address_family == AF_UNSPEC) {
		// attempt dual stack
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET6;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		
		if (getaddrinfo(address, service, &hints, &r) != 0)
			err_fatal( __func__, "getaddrinfo: Failed, address = \"%s\", service = \"%s\"\n", address, service);
		
		for (rtmp = r; rtmp != NULL; rtmp = rtmp->ai_next) {
			if (rtmp->ai_family != AF_INET6) {
				continue;
			}
			sock = setup_socket(rtmp, queue_len, true, 0);
			break;
		}
		freeaddrinfo(r); r=NULL;
		if (sock < 0 && sock != ESOOPTIPV6) {
			handle_so_setup_error(sock, __func__, address, service);
		}

		if (sock == ESOOPTIPV6) { //dual stack failed, then open two sockets
			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_UNSPEC;
			hints.ai_protocol = IPPROTO_TCP;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_PASSIVE;
			if (getaddrinfo(address, service, &hints, &r) != 0)
				err_fatal( __func__, "getaddrinfo: Failed, address = \"%s\", service = \"%s\"\n", address, service);
			int sock_ipv4 = -1, sock_ipv6 = -1;
			err = 0;
			for (rtmp = r; rtmp != NULL; rtmp = rtmp->ai_next) {
				if (rtmp->ai_family == AF_INET && sock_ipv4 == -1) {
					sock_ipv4 = setup_socket(rtmp, queue_len, false, 0);
					if (sock_ipv4 < 0) {
						err = sock_ipv4;
						break;
					}
				} else if (rtmp->ai_family == AF_INET6 && sock_ipv6 == -1) {
					sock_ipv6 = setup_socket(rtmp, queue_len, true, 1);
					if (sock_ipv6 < 0) {
						err = sock_ipv6;
						break;
					}
				}
			}
			freeaddrinfo(r); r=NULL;

			if (err < 0) {
				if (sock_ipv6 > 0) close(sock_ipv6);
				if (sock_ipv4 > 0) close(sock_ipv4);
				handle_so_setup_error(err, __func__, address, service);
			}

			sock_fds = malloc(2*sizeof(int));
			sock_fds[0] = sock_ipv4;
			sock_fds[1] = sock_ipv6;
			*sock_fds_len = 2;

		} else { // dual stack worked
			sock_fds = malloc(1*sizeof(int));
			sock_fds[0] = sock;
			*sock_fds_len = 1;
		}

	} else {
		sock = net_open_tcp(address, service, queue_len, address_family);
		sock_fds = malloc(1*sizeof(int));
		sock_fds[0] = sock;
		*sock_fds_len = 1;
	}

	return sock_fds;
}
