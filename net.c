/* net.c -- 
 * Created: Fri Feb 21 20:58:10 1997 by faith@cs.unc.edu
 * Revised: Fri Jul 11 15:24:46 1997 by faith@acm.org
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
 * $Id: net.c,v 1.15 1997/07/12 01:50:23 faith Exp $
 * 
 */


#include "dictd.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (-1)
#endif

static char netHostname[MAXHOSTNAMELEN];

const char *net_hostname( void )
{
   if (!netHostname[0]) {
      memset( netHostname, 0, sizeof(netHostname) );
      gethostname( netHostname, sizeof(netHostname)-1 );
   }
   return netHostname;
}

int net_connect_tcp( const char *host, const char *service )
{
   struct hostent     *hostEntry;
   struct servent     *serviceEntry;
   struct protoent    *protocolEntry;
   struct sockaddr_in ssin;
   int                s;

   memset( &ssin, 0, sizeof(ssin) );
   ssin.sin_family = AF_INET;

   if ((hostEntry = gethostbyname(host))) {
      memcpy( &ssin.sin_addr.s_addr, hostEntry->h_addr, hostEntry->h_length );
   } else if ((ssin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
      return NET_NOHOST;
   
   if ((serviceEntry = getservbyname(service, "tcp"))) {
      ssin.sin_port = serviceEntry->s_port;
   } else if (!(ssin.sin_port = htons(atoi(service))))
      return NET_NOSERVICE;

   if (!(protocolEntry = getprotobyname("tcp")))
      return NET_NOPROTOCOL;
   
   if ((s = socket(PF_INET, SOCK_STREAM, protocolEntry->p_proto)) < 0)
      err_fatal_errno( __FUNCTION__, "Can't open socket on port %d\n",
		       ntohs(ssin.sin_port) );

   if (connect(s, (struct sockaddr *)&ssin, sizeof(ssin)) < 0)
      return NET_NOCONNECT;

   return s;
}

int net_open_tcp( const char *service, int queueLength )
{
   struct servent     *serviceEntry;
   struct protoent    *protocolEntry;
   struct sockaddr_in ssin;
   int                s;
   const int          one = 1;

   memset( &ssin, 0, sizeof(ssin) );
   ssin.sin_family      = AF_INET;
   ssin.sin_addr.s_addr = INADDR_ANY;

   if ((serviceEntry = getservbyname(service, "tcp"))) {
      ssin.sin_port = serviceEntry->s_port;
   } else if (!(ssin.sin_port = htons(atoi(service))))
      err_fatal( __FUNCTION__, "Can't get \"%s\" service entry\n", service );

   if (!(protocolEntry = getprotobyname("tcp")))
      err_fatal( __FUNCTION__, "Can't get \"tcp\" protocol entry\n" );
   
   if ((s = socket(PF_INET, SOCK_STREAM, protocolEntry->p_proto)) < 0)
      err_fatal_errno( __FUNCTION__, "Can't open socket on port %d\n",
		       ntohs(ssin.sin_port) );

   setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) );

   if (bind(s, (struct sockaddr *)&ssin, sizeof(ssin)) < 0)
      err_fatal_errno( __FUNCTION__, "Can't bind %s/tcp to port %d\n",
		       service, ntohs(ssin.sin_port) );

   if (listen( s, queueLength ) < 0)
      err_fatal_errno( __FUNCTION__, "Can't listen to %s/tcp on port %d\n",
		       service, ntohs(ssin.sin_port) );
   
   return s;
}

void net_detach( void )
{
   int i;
   int fd;

   switch (fork()) {
   case -1: err_fatal_errno( __FUNCTION__, "Cannot fork\n" ); break;
   case 0:  break;		/* child */
   default: exit(0);		/* parent */
   }
   
   /* The detach algorithm is a modification of that presented by Comer,
      Douglas E. and Stevens, David L. INTERNETWORKING WITH TCP/IP, VOLUME
      III: CLIENT-SERVER PROGRAMMING AND APPLICATIONS (BSD SOCKET VERSION).
      Englewood Cliffs, New Jersey: Prentice Hall, 1993 (Chapter 27). */
   
   for (i=getdtablesize()-1; i >= 0; --i) close(i); /* close everything */
   
   if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
				/* detach from controlling tty */
      ioctl(fd, TIOCNOTTY, 0);
      close(fd);
   }
   
   chdir("/");		/* cd to safe directory */
   
   umask(0);		/* set safe umask */
   
   setpgid(0,getpid());	/* Get process group */
   
   fd = open("/dev/null", O_RDWR);    /* stdin */
   dup(fd);			      /* stdout */
   dup(fd);			      /* stderr */
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
