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

#include "dictP.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <fcntl.h>

#include "dictd.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (-1)
#endif

static char netHostname[MAXHOSTNAMELEN];

const char *inet_ntopW (struct sockaddr *sa) {
   static char buf[40];

   switch (sa->sa_family) {
   case AF_INET:
      return inet_ntop (sa->sa_family, &(((struct sockaddr_in *)sa)->sin_addr), buf, sizeof(buf));
   case AF_INET6:
      return inet_ntop (sa->sa_family, &(((struct sockaddr_in6 *)sa)->sin6_addr), buf, sizeof(buf));
   default:
      errno = EAFNOSUPPORT;
      return NULL;
   }
}

const char *net_hostname( void )
{
   struct hostent *hostEntry;
   static char    *hostname = NULL;
   
   if (!netHostname[0]) {
      memset( netHostname, 0, sizeof(netHostname) );
      gethostname( netHostname, sizeof(netHostname)-1 );
      
      if ((hostEntry = gethostbyname(netHostname))) {
	 hostname = xstrdup(hostEntry->h_name);
      } else {
	 hostname = xstrdup(netHostname);
      }
   }
   
   return hostname;
}

int net_connect_tcp( const char *host, const char *service )
{
   struct addrinfo *r = NULL;
   struct addrinfo *rtmp = NULL;
   struct addrinfo hints;
   int s;

   memset (&hints, 0, sizeof (struct addrinfo));
   hints.ai_family = PF_UNSPEC;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_ADDRCONFIG;

   if (getaddrinfo (host, service, &hints, &r) != 0) {
      return NET_NOHOST;
   }

   for (rtmp = r; r != NULL; r = r->ai_next) {
      s = socket (r->ai_family, r->ai_socktype, r->ai_protocol);
      if (s < 0) {
	 if (r->ai_next != NULL)
	    continue;

	 err_fatal_errno( __FUNCTION__, "Can't open socket\n");
      }

      PRINTF(DBG_VERBOSE,("Trying %s (%s)...", host, inet_ntopW(r->ai_addr)));

      if (connect (s, r->ai_addr, r->ai_addrlen) >= 0) {
	 PRINTF(DBG_VERBOSE,("Connected."));
	 freeaddrinfo (rtmp);
	 return s;
      }

      PRINTF(DBG_VERBOSE,("Failed: %s\n", strerror (errno)));

      close (s);
   }
   freeaddrinfo (rtmp);

   return NET_NOCONNECT;
}

int net_open_tcp (
   const char *address,
   const char *service,
   int queueLength)
{
   struct addrinfo hints, *r, *rtmp;
   int s = -1;
   int err;

   memset (&hints, 0, sizeof (struct addrinfo));
   hints.ai_family = PF_UNSPEC;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   if (getaddrinfo (address, service, &hints, &r) != 0)
      err_fatal ( __FUNCTION__, "getaddrinfo: Failed, address = \"%s\", service = \"%s\"\n", address, service);

   for (rtmp = r; r != NULL; r = r->ai_next) {
      s = socket (r->ai_family, r->ai_socktype, r->ai_protocol);

      if (s < 0) {
	 if (r->ai_next != NULL)
	    continue;

	 freeaddrinfo (rtmp);
	 err_fatal_errno (__FUNCTION__, "Can't open socket\n");
      }

      {
	 const int one = 1;
	 err = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
	 if (err != 0){
	    err_fatal_errno (__FUNCTION__, "Can't setsockopt\n");
	 }
      }

      if (bind(s, r->ai_addr, r->ai_addrlen) < 0) {
	 if (r->ai_next != NULL) {
	    close (s);
	    continue;
	 }
	 freeaddrinfo (rtmp);
	 err_fatal_errno( __FUNCTION__, "Can't bind %s/tcp to %s\n",
			  service, address?address:"ANY" );
      }

      if (listen( s, queueLength ) < 0) {
	 if (r->ai_next != NULL) {
	    close (s);
	    continue;
	 }
	 freeaddrinfo (rtmp);
	 err_fatal_errno( __FUNCTION__, "Can't listen to %s/tcp on %s\n",
			  service, address );
      }

      break;
   }
   freeaddrinfo (rtmp);

   return s;
}

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
