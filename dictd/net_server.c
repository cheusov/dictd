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
	struct addrinfo hints, *r = NULL, *ai = NULL;
	int *sock_fds = NULL;
	*sock_fds_len = 0;

	int sock = -1;


	fprintf(stderr,
		"[tcp2] address='%s' service='%s' family=%d\n",
		address ? address : "(null)", service, address_family);

	/*
	 * ------------------------------------------------------------
	 *  CASE 1: AF_UNSPEC → attempt dual-stack first
	 * ------------------------------------------------------------
	 */
	if (address_family == AF_UNSPEC) {

		fprintf(stderr, "[tcp2] Attempting dual-stack (AF_UNSPEC)\n");

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family   = AF_UNSPEC;					 // musl-friendly
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags    = AI_ADDRCONFIG | AI_V4MAPPED;

		int rc = getaddrinfo(address, service, &hints, &r);
		if (rc != 0) {
			fprintf(stderr,
				"[tcp2] getaddrinfo(AF_UNSPEC) failed: %s (%d)\n",
				gai_strerror(rc), rc);
			err_fatal(__func__,
				"getaddrinfo: Failed, address=\"%s\", service=\"%s\"\n",
				address, service);
		}

		/*
		 * Try IPv6 first (dual-stack if possible)
		 */
		for (ai = r; ai != NULL; ai = ai->ai_next) {
			fprintf(stderr,
				"[tcp2] ai_family=%d ai_socktype=%d ai_protocol=%d\n",
				ai->ai_family, ai->ai_socktype, ai->ai_protocol);

			if (ai->ai_family == AF_INET6) {
				fprintf(stderr, "[tcp2] Trying IPv6 dual-stack socket\n");
				sock = setup_socket(ai, queue_len, true, 0);
				fprintf(stderr, "[tcp2] setup_socket returned %d\n", sock);
				break;
			}
		}

		/*
		 * If dual-stack failed → fall back to separate IPv4 + IPv6 sockets
		 */
		if (sock == ESOOPTIPV6 || sock < 0) {
			fprintf(stderr,
				"[tcp2] Dual-stack failed (%d), falling back to separate sockets\n",
				sock);

			int sock4 = -1, sock6 = -1;
			int err = 0;

			for (ai = r; ai != NULL; ai = ai->ai_next) {
				if (ai->ai_family == AF_INET && sock4 < 0) {
					fprintf(stderr, "[tcp2] Trying IPv4 socket\n");
					sock4 = setup_socket(ai, queue_len, false, 0);
					fprintf(stderr, "[tcp2] IPv4 setup returned %d\n", sock4);
					if (sock4 < 0) { err = sock4; break; }
				}
				if (ai->ai_family == AF_INET6 && sock6 < 0) {
					fprintf(stderr, "[tcp2] Trying IPv6-only socket\n");
					sock6 = setup_socket(ai, queue_len, true, 1);
					fprintf(stderr, "[tcp2] IPv6 setup returned %d\n", sock6);
					if (sock6 < 0) { err = sock6; break; }
				}
			}

			freeaddrinfo(r); r = NULL;

			if (err < 0) {
				fprintf(stderr, "[tcp2] Fallback socket setup failed: %d\n", err);
				if (sock4 > 0) close(sock4);
				if (sock6 > 0) close(sock6);
				handle_so_setup_error(err, __func__, address, service);
			}

			/*
			 * NEW BEHAVIOR: return only ONE socket
			 * (dictd cannot handle multiple listening sockets)
			 */
			if (sock4 >= 0) {
				fprintf(stderr, "[tcp2] Using IPv4 only\n");
				if (sock6 >= 0) close(sock6);
				sock_fds = malloc(sizeof(int));
				sock_fds[0] = sock4;
				*sock_fds_len = 1;
				return sock_fds;
			}

			if (sock6 >= 0) {
				fprintf(stderr, "[tcp2] Using IPv6 only\n");
				sock_fds = malloc(sizeof(int));
				sock_fds[0] = sock6;
				*sock_fds_len = 1;
				return sock_fds;
			}

			err_fatal(__func__, "No usable sockets\n");
		}

		/*
		 * Dual-stack succeeded
		 */
		freeaddrinfo(r); r = NULL;

		fprintf(stderr, "[tcp2] Dual-stack IPv6 socket succeeded\n");
		sock_fds = malloc(sizeof(int));
		sock_fds[0] = sock;
		*sock_fds_len = 1;

		return sock_fds;
	}

	/*
	 * ------------------------------------------------------------
	 *  CASE 2: Caller requested a specific family (IPv4 or IPv6)
	 * ------------------------------------------------------------
	 */
	fprintf(stderr, "[tcp2] Using single-family mode: %d\n", address_family);

	sock = net_open_tcp(address, service, queue_len, address_family);

	sock_fds = malloc(sizeof(int));
	sock_fds[0] = sock;
	*sock_fds_len = 1;

	fprintf(stderr, "[tcp2] Returning 1 socket\n");
	return sock_fds;
}
