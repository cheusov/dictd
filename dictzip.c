/* dictzip.c -- 
 * Created: Tue Jul 16 12:45:41 1996 by r.faith@ieee.org
 * Revised: Sun Sep 29 10:06:25 1996 by faith@cs.unc.edu
 * Copyright 1996 Rickard E. Faith (r.faith@ieee.org)
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
 * $Id: dictzip.c,v 1.3 1996/09/29 14:07:04 faith Exp $
 * 
 */

#include "dict.h"

int dict_zip( const char *inFilename, const char *outFilename )
{
   char          inBuffer[IN_BUFFER_SIZE];
   char          outBuffer[OUT_BUFFER_SIZE];
   int           count;
   unsigned long inputCRC = crc32( 0L, Z_NULL, 0 );
   z_stream      zStream;
   FILE          *outStr;
   FILE          *inStr;
   int           len;
   struct stat   st;
   char          *header;
   int           headerLength;
   int           dataLength;
   int           extraLength;
#if HEADER_CRC
   int           headerCRC;
#endif
   unsigned long chunks;
   unsigned long chunk = 0;
   int           i;
   char          tail[8];
    
    
   /* Open files */
   if (!(inStr = fopen( inFilename, "r" )))
      err_fatal_errno( __FUNCTION__,
		       "Cannot open \"%s\" for read", inFilename );
   if (!(outStr = fopen( outFilename, "w" )))
      err_fatal_errno( __FUNCTION__,
		       "Cannot open \"%s\"for write", outFilename );

   /* Initialize compression engine */
   zStream.zalloc    = NULL;
   zStream.zfree     = NULL;
   zStream.opaque    = NULL;
   zStream.next_in   = NULL;
   zStream.avail_in  = 0;
   zStream.next_out  = NULL;
   zStream.avail_out = 0;
   if (deflateInit2( &zStream,
		     Z_BEST_COMPRESSION,
		     Z_DEFLATED,
		     -15,	/* Suppress zlib header */
		     Z_BEST_COMPRESSION,
		     Z_DEFAULT_STRATEGY ) != Z_OK)
      err_fatal( __FUNCTION__, "deflate_init: %s\n", zStream.msg );

   /* Write initial header information */
   fstat( fileno( inStr ), &st );
   chunks = st.st_size / IN_BUFFER_SIZE;
   if (st.st_size % IN_BUFFER_SIZE) ++chunks;
   printf( "%lu chunks * %d per chunk = %lu (filesize = %lu)\n",
	   chunks, IN_BUFFER_SIZE, chunks * IN_BUFFER_SIZE, st.st_size );
   dataLength   = chunks * 4;
   extraLength  = 4 + 4 + dataLength;
   headerLength = GZ_FEXTRA_START
		  + extraLength		/* FEXTRA */
		  + strlen( inFilename ) + 1	/* FNAME  */
		  + (HEADER_CRC ? 2 : 0);	/* FHCRC  */
   printf( "data = %d, extra = %d, header = %d\n",
	   dataLength, extraLength, headerLength );
   header = malloc( headerLength );
   for (i = 0; i < headerLength; i++) header[i] = 0;
   header[GZ_ID1]        = GZ_MAGIC1;
   header[GZ_ID2]        = GZ_MAGIC2;
   header[GZ_CM]         = Z_DEFLATED;
   header[GZ_FLG]        = GZ_FEXTRA | GZ_FNAME;
#if HEADER_CRC
   header[GZ_FLG]        |= GZ_FHCRC;
#endif
   header[GZ_MTIME+3]    = (st.st_mtime & 0xff000000) >> 24;
   header[GZ_MTIME+2]    = (st.st_mtime & 0x00ff0000) >> 16;
   header[GZ_MTIME+1]    = (st.st_mtime & 0x0000ff00) >>  8;
   header[GZ_MTIME+0]    = (st.st_mtime & 0x000000ff) >>  0;
   header[GZ_XFL]        = GZ_MAX;
   header[GZ_OS]         = GZ_OS_UNIX;
   header[GZ_XLEN+1]     = (extraLength & 0xff00) >> 8;
   header[GZ_XLEN+0]     = (extraLength & 0x00ff) >> 0;
   header[GZ_SI1]        = GZ_RND_S1;
   header[GZ_SI2]        = GZ_RND_S2;
   header[GZ_SUBLEN+1]   = ((extraLength - 4) & 0xff00) >> 8;
   header[GZ_SUBLEN+0]   = ((extraLength - 4) & 0x00ff) >> 0;
   header[GZ_CHUNKLEN+1] = (IN_BUFFER_SIZE & 0xff00) >> 8;
   header[GZ_CHUNKLEN+0] = (IN_BUFFER_SIZE & 0x00ff) >> 0;
   strcpy( &header[GZ_FEXTRA_START + extraLength], inFilename );
   fwrite( header, 1, headerLength, outStr );
    
   /* Read, compress, write */
   while (!feof( inStr )) {
      if ((count = fread( inBuffer, 1, IN_BUFFER_SIZE, inStr ))) {
#if 0
	 printf( "got %d bytes\n", count );
#endif
	 inputCRC = crc32( inputCRC, inBuffer, count );
	 zStream.next_in   = inBuffer;
	 zStream.avail_in  = count;
	 zStream.next_out  = outBuffer;
	 zStream.avail_out = OUT_BUFFER_SIZE;
#if 0
	 if (deflate( &zStream, Z_NO_FLUSH ) != Z_OK)
	    err_fatal( __FUNCTION__, "deflate: %s\n", zStream.msg );
#else
	 if (deflate( &zStream, Z_FULL_FLUSH ) != Z_OK)
	    err_fatal( __FUNCTION__, "deflate: %s\n", zStream.msg );
#endif
	 assert( zStream.avail_in == 0 );
	 len = OUT_BUFFER_SIZE - zStream.avail_out;
	 header[GZ_RNDDATA + chunk*4 + 3] = (len & 0xff000000) >> 24;
	 header[GZ_RNDDATA + chunk*4 + 2] = (len & 0x00ff0000) >> 16;
	 header[GZ_RNDDATA + chunk*4 + 1] = (len & 0x0000ff00) >>  8;
	 header[GZ_RNDDATA + chunk*4 + 0] = (len & 0x000000ff) >>  0;
	 fwrite( outBuffer, 1, len, outStr );
#if 0
	 printf( "wrote %d bytes\n", len );
#endif
      }
   }
    
   /* Write last bit */
   dmalloc_verify(0);
   zStream.next_in   = inBuffer;
   zStream.avail_in  = 0;
   zStream.next_out  = outBuffer;
   zStream.avail_out = OUT_BUFFER_SIZE;
   if (deflate( &zStream, Z_FINISH ) != Z_STREAM_END)
      err_fatal( __FUNCTION__, "deflate: %s\n", zStream.msg );
   assert( zStream.avail_in == 0 );
   len = OUT_BUFFER_SIZE - zStream.avail_out;
   fwrite( outBuffer, 1, len, outStr );
   printf( "wrote %d bytes, final, crc = %lx\n", len, inputCRC );

   /* Write CRC and length */
   dmalloc_verify(0);
   tail[0 + 3] = (inputCRC & 0xff000000) >> 24;
   tail[0 + 2] = (inputCRC & 0x00ff0000) >> 16;
   tail[0 + 1] = (inputCRC & 0x0000ff00) >>  8;
   tail[0 + 0] = (inputCRC & 0x000000ff) >>  0;
   tail[4 + 3] = (st.st_size & 0xff000000) >> 24;
   tail[4 + 2] = (st.st_size & 0x00ff0000) >> 16;
   tail[4 + 1] = (st.st_size & 0x0000ff00) >>  8;
   tail[4 + 0] = (st.st_size & 0x000000ff) >>  0;
   fwrite( tail, 1, 8, outStr );

   /* Write final header information */
   dmalloc_verify(0);
   rewind( outStr );
#if HEADER_CRC
   headerCRC = crc32( 0L, Z_NULL, 0 );
   headerCRC = crc32( headerCRC, header, headerLength - 2);
   header[headerLength - 1] = (headerCRC & 0xff00) >> 8;
   header[headerLength - 2] = (headerCRC & 0x00ff) >> 0;
#endif
   fwrite( header, 1, headerLength, outStr );

   /* Close files */
   dmalloc_verify(0);
   fclose( outStr );
   fclose( inStr );
    
   /* Shut down compression */
   if (deflateEnd( &zStream ) != Z_OK)
      err_fatal( __FUNCTION__, "defalteEnd: %s\n", zStream.msg );

   return 0;
}

static void banner( void )
{
   const char *id = "$Id: dictzip.c,v 1.3 1996/09/29 14:07:04 faith Exp $";
   
   fprintf( stderr, "%s\n%s\n", err_program_name() );
   fprintf( stderr,
   "This program is free software; you can redistribute it and/or modify it\n"
   "under the terms of the GNU General Public License as published by the\n"
   "Free Software Foundation; either version 1, or (at your option) any\n"
   "later version.\n"
   "\n"
   "This program is distributed in the hope that it will be useful, but\n"
   "WITHOUT ANY WARRANTY; without even the implied warranty of\n"
   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
   "General Public License for more details.\n"
   "\n"
   "You should have received a copy of the GNU General Public License along\n"
   "with this program; if not, write to the Free Software Foundation, Inc.,\n"
   "675 Mass Ave, Cambridge, MA 02139, USA.\n" );
    
}

static void help( void ) /* __attribute((noreturn))__ */
{
   static const char  *help_msg[] = {
      "-c --stdout      write on standard output, keep original file",
      "-d --decompress  decompress",
      "-f --force       force overwrite of output file",
      "-h --help        give this help",
      "-l --list        list compressed file contents",
      "-L --license     display software license",
      "-t --test        test compressed file integrity",
      "-v --verbose     verbose mode",
      "-V --version     display version number",
      "-d --debug       select debug option",
      0 };
   const char **p = help_msg;

   banner();
   while (*p) fprintf(stderr, "%s\n", *p++ );
   exit( PMERR_HELP );
}

int main( int argc, char **argv )
{
   int c;
   struct option longopts[] = {
      { "verbose",      0, 0, 'v' },
      { "debug",        1, 0, 'd' },
      { "help",         0, 0, 'h' },
      { "version",      0, 0, 'V' },
      { 0,              0, 0,  0  }
   };

   if (signal( SIGINT, SIG_IGN ) != SIG_IGN)  signal( SIGINT, sig_handler );
   if (signal( SIGQUIT, SIG_IGN ) != SIG_IGN) signal( SIGQUIT, sig_handler );

   PgmPath = argv[0];
   
   while ((c = getopt_long( argc, argv,
			    "B:pPVhDd:vi", longopts, NULL )) != EOF)
      switch (c) {
	 case 506: config = optarg;                            break;
	 case 507: config = "/dev/null";                       break;
      }

   
   /* Initialize Libmaa */
   maa_init( argv[0] );
    
   return 0;
}
