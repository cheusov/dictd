/* net.c -- 
 * Created: Fri Feb 21 20:58:10 1997 by faith@dict.org
 * Revised: Sat Mar 30 10:31:44 2002 by faith@dict.org
 * Copyright 1997, 1998, 1999, 2000, 2002 Rickard E. Faith (faith@dict.org)
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
 * $Id: net.c,v 1.22 2002/08/02 19:43:14 faith Exp $
 * 
 */


#include "dictd.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/termios.h>

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
   struct hostent     *hostEntry;
   struct servent     *serviceEntry;
   struct protoent    *protocolEntry;
   struct sockaddr_in ssin;
   int                s;
   int                hosts = 0;
   char               **current;

   memset( &ssin, 0, sizeof(ssin) );
   ssin.sin_family = AF_INET;

   if ((serviceEntry = getservbyname(service, "tcp"))) {
      ssin.sin_port = serviceEntry->s_port;
   } else if (!(ssin.sin_port = htons(atoi(service))))
      return NET_NOSERVICE;

   if (!(protocolEntry = getprotobyname("tcp")))
      return NET_NOPROTOCOL;
   
   if ((hostEntry = gethostbyname(host))) {
      ++hosts;
   } else if ((ssin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
      return NET_NOHOST;
   
   if (hosts) {
      for (current = hostEntry->h_addr_list; *current; current++) {
	 memcpy( &ssin.sin_addr.s_addr, *current, hostEntry->h_length );
	 PRINTF(DBG_VERBOSE,
		("Trying %s (%s)\n",host,inet_ntoa(ssin.sin_addr)));
	 if ((s = socket(PF_INET, SOCK_STREAM, protocolEntry->p_proto)) < 0)
	    err_fatal_errno( __FUNCTION__, "Can't open socket on port %d\n",
			     ntohs(ssin.sin_port) );      
	 if (connect(s, (struct sockaddr *)&ssin, sizeof(ssin)) >= 0)
	    return s;
	 close(s);
      }
   } else {
      if ((s = socket(PF_INET, SOCK_STREAM, protocolEntry->p_proto)) < 0)
	 err_fatal_errno( __FUNCTION__, "Can't open socket on port %d\n",
			  ntohs(ssin.sin_port) );
      if (connect(s, (struct sockaddr *)&ssin, sizeof(ssin)) >= 0)
	 return s;
      close(s);
   }

   return NET_NOCONNECT;
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
   
#if !defined(__hpux__) && !defined(__CYGWIN__)
   if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
				/* detach from controlling tty */
      ioctl(fd, TIOCNOTTY, 0);
      close(fd);
   }
#endif
   
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
