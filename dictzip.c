/* dictzip.c -- 
 * Created: Tue Jul 16 12:45:41 1996 by r.faith@ieee.org
 * Revised: Wed Oct  9 16:15:35 1996 by faith@cs.unc.edu
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
 * $Id: dictzip.c,v 1.5 1996/10/10 01:46:48 faith Exp $
 * 
 */

#include "dict.h"
#include <sys/stat.h>

int dict_data_filter( char *buffer, int *len, int maxLength,
		      const char *filter )
{
   char *outBuffer = xmalloc( maxLength + 2 );
   int  outLen;

   if (!filter) return 0;
   
   outLen = pr_filter( filter, buffer, *len, outBuffer, maxLength + 1 );
   if (outLen > maxLength )
      err_fatal( __FUNCTION__,
		 "Filter grew buffer from %d past limit of %d\n",
		 *len, maxLength );
   memcpy( buffer, outBuffer, outLen );
   xfree( outBuffer );

   PRINTF(DBG_UNZIP|DBG_ZIP,("Length was %d, now is %d\n",*len,outLen));

   *len = outLen;
   
   return 0;
}

static int dict_read_header( FILE *str,
			     const char *filename,
			     dictData *header,
			     int computeCRC )
{
   int           id1, id2, si1, si2;
   char          buffer[BUFFERSIZE];
   int           extraLength, subLength;
   int           i;
   char          *pt;
   int           c;
   struct stat   sb;
   unsigned long crc   = crc32( 0L, Z_NULL, 0 );
   int           count;
   unsigned long offset;

   
   rewind( str );
   header->str          = str;
   header->filename     = strdup( filename );
   
   header->headerLength = GZ_XLEN - 1;
   header->type         = DICT_UNKNOWN;
   
   id1                  = getc( str );
   id2                  = getc( str );

   if (id1 != GZ_MAGIC1 || id2 != GZ_MAGIC2) {
      header->type = DICT_TEXT;
      fstat( fileno( str ), &sb );
      header->compressedLength = header->length = sb.st_size;
      header->origFilename     = strdup( filename );
      header->mtime            = sb.st_mtime;
      if (computeCRC) {
	 rewind( str );
	 while (!feof( str )) {
	    if ((count = fread( buffer, 1, BUFFERSIZE, str ))) {
	       crc = crc32( crc, buffer, count );
	    }
	 }
      }
      header->crc = crc;
      return 0;
   }
   header->type = DICT_GZIP;
   
   header->method       = getc( str );
   header->flags        = getc( str );
   header->mtime        = getc( str ) <<  0;
   header->mtime       |= getc( str ) <<  8;
   header->mtime       |= getc( str ) << 16;
   header->mtime       |= getc( str ) << 24;
   header->extraFlags   = getc( str );
   header->os           = getc( str );
   
   if (header->flags & GZ_FEXTRA) {
      extraLength          = getc( str ) << 0;
      extraLength         |= getc( str ) << 8;
      header->headerLength += extraLength + 2;
      si1                  = getc( str );
      si2                  = getc( str );
      
      if (si1 == GZ_RND_S1 || si2 == GZ_RND_S2) {
	 subLength            = getc( str ) << 0;
	 subLength           |= getc( str ) << 8;
	 header->version      = getc( str ) << 0;
	 header->version     |= getc( str ) << 8;
	 
	 if (header->version != 1)
	    err_internal( __FUNCTION__,
			  "dzip header version %d not supported\n",
			  header->version );
   
	 header->chunkLength  = getc( str ) << 0;
	 header->chunkLength |= getc( str ) << 8;
	 header->chunkCount   = getc( str ) << 0;
	 header->chunkCount  |= getc( str ) << 8;
	 
	 if (header->chunkCount <= 0) return 5;
	 header->chunks = xmalloc( sizeof( header->chunks[0] )
				   * header->chunkCount );
	 for (i = 0; i < header->chunkCount; i++) {
	    header->chunks[i]  = getc( str ) << 0;
	    header->chunks[i] |= getc( str ) << 8;
	 }
	 header->type = DICT_DZIP;
      } else {
	 fseek( str, header->headerLength, SEEK_SET );
      }
   }
   
   if (header->flags & GZ_FNAME) { /* FIXME! Add checking against header len */
      pt = buffer;
      while ((c = getc( str )) && c != EOF)
	 *pt++ = c;
      *pt = '\0';
      header->origFilename = strdup( buffer );
      header->headerLength += strlen( header->origFilename ) + 1;
   } else {
      header->origFilename = NULL;
   }
   
   if (header->flags & GZ_COMMENT) { /* FIXME! Add checking for header len */
      pt = buffer;
      while ((c = getc( str )) && c != EOF)
	 *pt++ = c;
      *pt = '\0';
      header->comment = strdup( buffer );
      header->headerLength += strlen( header->comment ) + 1;
   } else {
      header->comment = NULL;
   }

   if (header->flags & GZ_FHCRC) {
      getc( str );
      getc( str );
      header->headerLength += 2;
   }

   if (ftell( str ) != header->headerLength + 1)
      err_internal( __FUNCTION__,
		    "File position (%lu) != header length + 1 (%d)\n",
		    ftell( str ), header->headerLength + 1 );

   fseek( str, -8, SEEK_END );
   header->crc     = getc( str ) <<  0;
   header->crc    |= getc( str ) <<  8;
   header->crc    |= getc( str ) << 16;
   header->crc    |= getc( str ) << 24;
   header->length  = getc( str ) <<  0;
   header->length |= getc( str ) <<  8;
   header->length |= getc( str ) << 16;
   header->length |= getc( str ) << 24;
   header->compressedLength = ftell( str );

				/* Compute offsets */
   header->offsets = xmalloc( sizeof( header->offsets[0] )
			      * header->chunkCount );
   for (offset = header->headerLength + 1, i = 0;
	i < header->chunkCount;
	i++) {
      header->offsets[i] = offset;
      offset += header->chunks[i];
   }
   
   return 0;
}

dictData *dict_data_open( const char *filename, int computeCRC )
{
   dictData    *h = xmalloc( sizeof( struct dictData ) );
   FILE        *str;
   struct stat sb;

   memset( h, 0, sizeof( struct dictData ) );
   h->initialized = 0;
   
   if (stat( filename, &sb ) || !S_ISREG(sb.st_mode)) {
      err_warning( __FUNCTION__,
		   "%s is not a regular file -- ignoring\n", filename );
      return h;
   }
   
   if (!(str = fopen( filename, "r" ))) {
      err_fatal_errno( __FUNCTION__,
		       "Cannot open \"%s\" for read\n", filename );
   }
   if (dict_read_header( str, filename, h, computeCRC )) {
      err_fatal( __FUNCTION__,
		 "\"%s\" not in text or dzip format\n", filename );
   }
   
   return h;
}

void dict_data_close( dictData *header )
{
   if (header->str)          fclose( header->str );
   if (header->filename)     xfree( header->filename );
   if (header->origFilename) xfree( header->origFilename );
   if (header->comment)      xfree( header->comment );
   if (header->chunks)       xfree( header->chunks );
   if (header->offsets)      xfree( header->offsets );

   if (header->initialized) {
      if (inflateEnd( &header->zStream ))
	 err_internal( __FUNCTION__,
		       "Cannot shut down inflation engine: %s\n",
		       header->zStream.msg );
   }

   memset( header, 0, sizeof( struct dictData ) );
   xfree( header );
}

void dict_data_print_header( FILE *str, dictData *header )
{
   char        *date, *year;
   long        ratio, num, den;
   static int  first = 1;

   if (first) {
      fprintf( str,
	       "type   crc        date    time chunks  size     compr."
	       "  uncompr. ratio name\n" );
      first = 0;
   }
   
   switch (header->type) {
   case DICT_TEXT:
      date = ctime( &header->mtime ) + 4; /* no day of week */
      date[12] = date[20] = '\0'; /* no year or newline*/
      year = &date[16];
      fprintf( str, "text %08lx %s %11s ", header->crc, year, date );
      fprintf( str, "            " );
      fprintf( str, "          %9ld ", header->length );
      fprintf( str, "  0.0%% %s",
	       header->origFilename ? header->origFilename : "" );
      putc( '\n', str );
      break;
   case DICT_GZIP:
   case DICT_DZIP:
      fprintf( str, "%s", header->type == DICT_DZIP ? "dzip " : "gzip " );
#if 0
      switch (header->method) {
      case 0:  fprintf( str, "store" ); break;
      case 1:  fprintf( str, "compr" ); break;
      case 2:  fprintf( str, "pack " ); break;
      case 3:  fprintf( str, "lzh  " ); break;
      case 8:  fprintf( str, "defla" ); break;
      default: fprintf( str, "?    " ); break;
      }
#endif
      date = ctime( &header->mtime ) + 4; /* no day of week */
      date[12] = date[20] = '\0'; /* no year or newline*/
      year = &date[16];
      fprintf( str, "%08lx %s %11s ", header->crc, year, date );
      if (header->type == DICT_DZIP) {
	 fprintf( str, "%5d %5d ", header->chunkCount, header->chunkLength );
      } else {
	 fprintf( str, "            " );
      }
      fprintf( str, "%9ld %9ld ", header->compressedLength, header->length );
      /* Algorithm for calculating ratio from gzip-1.2.4,
         util.c:display_ratio Copyright (C) 1992-1993 Jean-loup Gailly.
         May be distributed under the terms of the GNU General Public
         License. */
      num = header->length-(header->compressedLength-header->headerLength);
      den = header->length;
      if (!den)
	 ratio = 0;
      else if (den < 2147483L)
	 ratio = 1000L * num / den;
      else
	 ratio = num / (den/1000L);
      if (ratio < 0) {
	 putc( '-', str );
	 ratio = -ratio;
      } else putc( ' ', str );
      fprintf( str, "%2ld.%1ld%%", ratio / 10L, ratio % 10L);
      fprintf( str, " %s",
	       header->origFilename ? header->origFilename : "" );
      putc( '\n', str );
      break;
   case DICT_UNKNOWN:
   default:
      break;
   }
}

char *dict_data_read( dictData *h, unsigned long start, unsigned long end,
		      const char *preFilter, const char *postFilter )
{
   char          *buffer, *pt;
   unsigned long size = end - start;
   int           count;
   char          inBuffer[IN_BUFFER_SIZE];
   char          outBuffer[OUT_BUFFER_SIZE];
   int           firstChunk, lastChunk;
   int           firstOffset, lastOffset;
   int           i;

   if (end < start)
      err_internal( __FUNCTION__,
		    "Section ends (%lu) before starting (%lu)\n", end, start );
   buffer = xmalloc( size + 1 );
   
   switch (h->type) {
   case DICT_GZIP:
      err_fatal( __FUNCTION__,
		 "Cannot seek on pure gzip format files.\n"
		 "Use plain text (for performance)"
		 " or dzip format (for space savings).\n" );
      break;
   case DICT_TEXT:
      fseek( h->str, start, SEEK_SET );
      if ((count = fread( buffer, 1, size, h->str )) != size)
	 err_fatal_errno( __FUNCTION__,
			  "Read %d of %lu bytes from text file\n",
			  count, size );
      buffer[size] = '\0';
      break;
   case DICT_DZIP:
      if (!h->initialized) {
	 ++h->initialized;
	 h->zStream.zalloc    = NULL;
	 h->zStream.zfree     = NULL;
	 h->zStream.opaque    = NULL;
	 h->zStream.next_in   = 0;
	 h->zStream.avail_in  = 0;
	 h->zStream.next_out  = NULL;
	 h->zStream.avail_out = 0;
	 if (inflateInit2( &h->zStream, -15 ) != Z_OK)
	    err_internal( __FUNCTION__,
			  "Cannot initialize inflation engine: %s\n",
			  h->zStream.msg );
      }
      firstChunk  = start / h->chunkLength;
      firstOffset = start - firstChunk * h->chunkLength;
      lastChunk   = end / h->chunkLength;
      lastOffset  = end - lastChunk * h->chunkLength;
      PRINTF(DBG_UNZIP,
	     ("start = %lu, end = %lu\n"
	      "firstChunk = %d, firstOffset = %d,"
	      " lastChunk = %d, lastOffset = %d\n",
	      start, end, firstChunk, firstOffset, lastChunk, lastOffset ));
      for (pt = buffer, i = firstChunk; i <= lastChunk; i++) {
	 fseek( h->str, h->offsets[i], SEEK_SET );
	 PRINTF(DBG_UNZIP,("Seek to 0x%lx\n", h->offsets[i] ));
	 if ((count = fread( outBuffer, 1, h->chunks[i], h->str ))
	     != h->chunks[i] ) {
	    err_fatal_errno( __FUNCTION__,
			     "Read %d of %u bytes from dzip file\n",
			     count, h->chunks[i] );
	 }
	 dict_data_filter( outBuffer, &count, OUT_BUFFER_SIZE, preFilter );
	 
	 h->zStream.next_in   = outBuffer;
	 h->zStream.avail_in  = h->chunks[i];
	 h->zStream.next_out  = inBuffer;
	 h->zStream.avail_out = IN_BUFFER_SIZE;
	 if (inflate( &h->zStream,  Z_PARTIAL_FLUSH ) != Z_OK)
	    err_fatal( __FUNCTION__, "inflate: %s\n", h->zStream.msg );
	 if (h->zStream.avail_in)
	    err_internal( __FUNCTION__,
			  "inflate did not flush (%d pending, %d avail)\n",
			  h->zStream.avail_in, h->zStream.avail_out );
	 
	 count = IN_BUFFER_SIZE - h->zStream.avail_out;
	 dict_data_filter( inBuffer, &count, IN_BUFFER_SIZE, postFilter );
	 
	 if (i == firstChunk) {
	    if (i == lastChunk) {
	       memcpy( pt, inBuffer + firstOffset, lastOffset-firstOffset);
	       pt += lastOffset - firstOffset;
	    } else {
	       if (count != h->chunkLength )
		  err_internal( __FUNCTION__,
				"Length = %d instead of %d\n",
				count, h->chunkLength );
	       memcpy( pt, inBuffer + firstOffset,
		       h->chunkLength - firstOffset );
	       pt += h->chunkLength - firstOffset;
	    }
	 } else if (i == lastChunk) {
	    memcpy( pt, inBuffer, lastOffset );
	    pt += lastOffset;
	 } else {
	    assert( count == h->chunkLength );
	    memcpy( pt, inBuffer, h->chunkLength );
	    pt += h->chunkLength;
	 }
      }
      *pt = '\0';
      break;
   case DICT_UNKNOWN:
      err_fatal( __FUNCTION__, "Cannot read unknown file type\n" );
      break;
   }
   
   return buffer;
}

int dict_data_zip( const char *inFilename, const char *outFilename,
		   const char *preFilter, const char *postFilter )
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
   int           chunkLength;
#if HEADER_CRC
   int           headerCRC;
#endif
   unsigned long chunks;
   unsigned long chunk = 0;
   unsigned long total = 0;
   int           i;
   char          tail[8];
   char          *pt, *origFilename;

   
   /* Open files */
   if (!(inStr = fopen( inFilename, "r" )))
      err_fatal_errno( __FUNCTION__,
		       "Cannot open \"%s\" for read", inFilename );
   if (!(outStr = fopen( outFilename, "w" )))
      err_fatal_errno( __FUNCTION__,
		       "Cannot open \"%s\"for write", outFilename );

   origFilename = xmalloc( strlen( inFilename ) + 1 );
   if ((pt = strrchr( inFilename, '/' )))
      strcpy( origFilename, pt + 1 );
   else
      strcpy( origFilename, inFilename );

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
      err_internal( __FUNCTION__,
		    "Cannot initialize deflation engine: %s\n", zStream.msg );

   /* Write initial header information */
   chunkLength = (preFilter ? PREFILTER_IN_BUFFER_SIZE : IN_BUFFER_SIZE );
   fstat( fileno( inStr ), &st );
   chunks = st.st_size / chunkLength;
   if (st.st_size % chunkLength) ++chunks;
   PRINTF(DBG_VERBOSE,("%lu chunks * %u per chunk = %lu (filesize = %lu)\n",
			chunks, chunkLength, chunks * chunkLength,
			st.st_size ));
   dataLength   = chunks * 2;
   extraLength  = 10 + dataLength;
   headerLength = GZ_FEXTRA_START
		  + extraLength		/* FEXTRA */
		  + strlen( origFilename ) + 1	/* FNAME  */
		  + (HEADER_CRC ? 2 : 0);	/* FHCRC  */
   PRINTF(DBG_VERBOSE,("(data = %d, extra = %d, header = %d)\n",
		       dataLength, extraLength, headerLength ));
   header = xmalloc( headerLength );
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
   header[GZ_VERSION+1]  = 0;
   header[GZ_VERSION+0]  = 1;
   header[GZ_CHUNKLEN+1] = (chunkLength & 0xff00) >> 8;
   header[GZ_CHUNKLEN+0] = (chunkLength & 0x00ff) >> 0;
   header[GZ_CHUNKCNT+1] = (chunks & 0xff00) >> 8;
   header[GZ_CHUNKCNT+0] = (chunks & 0x00ff) >> 0;
   strcpy( &header[GZ_FEXTRA_START + extraLength], origFilename );
   fwrite( header, 1, headerLength, outStr );
    
   /* Read, compress, write */
   while (!feof( inStr )) {
      if ((count = fread( inBuffer, 1, chunkLength, inStr ))) {
	 dict_data_filter( inBuffer, &count, IN_BUFFER_SIZE, preFilter );
	 
	 inputCRC = crc32( inputCRC, inBuffer, count );
	 zStream.next_in   = inBuffer;
	 zStream.avail_in  = count;
	 zStream.next_out  = outBuffer;
	 zStream.avail_out = OUT_BUFFER_SIZE;
	 if (deflate( &zStream, Z_FULL_FLUSH ) != Z_OK)
	    err_fatal( __FUNCTION__, "deflate: %s\n", zStream.msg );
	 assert( zStream.avail_in == 0 );
	 len = OUT_BUFFER_SIZE - zStream.avail_out;
	 assert( len <= 0xffff );

	 dict_data_filter( outBuffer, &len, OUT_BUFFER_SIZE, postFilter );
	 
	 assert( len <= 0xffff );
	 header[GZ_RNDDATA + chunk*2 + 1] = (len & 0xff00) >>  8;
	 header[GZ_RNDDATA + chunk*2 + 0] = (len & 0x00ff) >>  0;
	 fwrite( outBuffer, 1, len, outStr );

	 ++chunk;
	 total += count;
	 if (dbg_test( DBG_VERBOSE )) {
	    printf( "chunk %5lu: %lu of %lu total\r",
		    chunk, total, st.st_size );
	    fflush( stdout );
	 }
      }
   }
   PRINTF(DBG_VERBOSE,("total: %lu chunks, %lu bytes\n", chunks, st.st_size));
    
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
   PRINTF(DBG_VERBOSE,("(wrote %d bytes, final, crc = %lx)\n",
		       len, inputCRC ));

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

   xfree( origFilename );
   xfree( header );

   return 0;
}

static const char *id_string( const char *id )
{
   static char buffer[BUFFERSIZE];
   arg_List a = arg_argify( id );

   sprintf( buffer, "%s (%s)", arg_get( a, 2 ), arg_get( a, 3 ) );
   arg_destroy( a );
   return buffer;
}

static void banner( void )
{
   const char *id = "$Id: dictzip.c,v 1.5 1996/10/10 01:46:48 faith Exp $";
   
   fprintf( stderr, "%s %s\n", err_program_name(), id_string( id ) );
   fprintf( stderr, "Copyright 1996 Rickard E. Faith (faith@cs.unc.edu)\n" );
}

static void license( void )
{
   static const char *license_msg[] = {
     "",
     "This program is free software; you can redistribute it and/or modify it",
     "under the terms of the GNU General Public License as published by the",
     "Free Software Foundation; either version 1, or (at your option) any",
     "later version.",
     "",
     "This program is distributed in the hope that it will be useful, but",
     "WITHOUT ANY WARRANTY; without even the implied warranty of",
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU",
     "General Public License for more details.",
     "",
     "You should have received a copy of the GNU General Public License along",
     "with this program; if not, write to the Free Software Foundation, Inc.,",
     "675 Mass Ave, Cambridge, MA 02139, USA.",
   };
   const char        **p = license_msg;
   
   banner();
   while (*p) fprintf( stderr, "   %s\n", *p++ );
}
    
static void help( void )
{
   static const char *help_msg[] = {
      "-d --decompress      decompress",
      "-f --force           force overwrite of output file",
      "-h --help            give this help",
      "-k --keep            do not delete original file",
      "-l --list            list compressed file contents",
      "-L --license         display software license",
      "   --stdout          write to stdout (decompression only)",
      "-t --test            test compressed file integrity",
      "-v --verbose         verbose mode",
      "-V --version         display version number",
      "-D --debug           select debug option",
      "-s --start <offset>  starting offset for decompression (decimal)",
      "-e --end <offset>    ending offset for decompression (decimal)",
      "-S --Start <offset>  starting offset for decompression (base64)",
      "-E --End <offset>    ending offset for decompression (base64)",
      "-p --pre <filter>    pre-compression filter",
      "-P --post <filter>   post-compression filter",
      0 };
   const char        **p = help_msg;

   banner();
   while (*p) fprintf( stderr, "%s\n", *p++ );
}

int main( int argc, char **argv )
{
   int           c;
   int           i;
   int           decompressFlag = 0;
   int           forceFlag      = 0;
   int           keepFlag       = 0;
   int           listFlag       = 0;
   int           stdoutFlag     = 0;
   int           testFlag       = 0;
   char          buffer[BUFFERSIZE];
   char          *buf;
   char          *pre           = NULL;
   char          *post          = NULL;
   unsigned long start          = 0;
   unsigned long end            = 0;
   dictData      *header;
   struct option longopts[] = {
      { "decompress",   0, 0, 'd' },
      { "force",        0, 0, 'f' },
      { "help",         0, 0, 'h' },
      { "keep",         0, 0, 'k' },
      { "list",         0, 0, 'l' },
      { "license",      0, 0, 'L' },
      { "stdout",       0, 0, 513 },
      { "test",         0, 0, 't' },
      { "verbose",      0, 0, 'v' },
      { "version",      0, 0, 'V' },
      { "debug",        1, 0, 'D' },
      { "start",        1, 0, 's' },
      { "end",          1, 0, 'e' },
      { "Start",        1, 0, 'S' },
      { "End",          1, 0, 'E' },
      { "pre",          1, 0, 'p' },
      { "post",         1, 0, 'P' },
      { 0,              0, 0,  0  }
   };

   /* Initialize Libmaa */
   maa_init( argv[0] );
   dbg_register( DBG_VERBOSE, "verbose" );
   dbg_register( DBG_ZIP,     "zip" );
   dbg_register( DBG_UNZIP,   "unzip" );
   
#if 0
   if (signal( SIGINT, SIG_IGN ) != SIG_IGN)  signal( SIGINT, sig_handler );
   if (signal( SIGQUIT, SIG_IGN ) != SIG_IGN) signal( SIGQUIT, sig_handler );
#endif

   while ((c = getopt_long( argc, argv,
			    "cdfhklLe:E:s:S:tvVD:p:P:",
			    longopts, NULL )) != EOF)
      switch (c) {
      case 'd': ++decompressFlag;                                      break;
      case 'f': ++forceFlag;                                           break;
      case 'k': ++keepFlag;                                            break;
      case 'l': ++listFlag;                                            break;
      case 'L': license(); exit( 1 );                                  break;
      case 513: ++stdoutFlag;                                          break;
      case 't': ++testFlag;                                            break;
      case 'v': dbg_set( "verbose" );                                  break;
      case 'V': banner(); exit( 1 );                                   break;
      case 'D': dbg_set( optarg );                                     break;
      case 's': ++decompressFlag; start = strtoul( optarg, NULL, 10 ); break;
      case 'e': ++decompressFlag; end   = strtoul( optarg, NULL, 10 ); break;
      case 'S': ++decompressFlag; start = b64_decode( optarg );        break;
      case 'E': ++decompressFlag; end   = b64_decode( optarg );        break;
      case 'p': pre = optarg;                                          break;
      case 'P': post = optarg;                                         break;
      default:  
      case 'h': help(); exit( 1 );                                     break;
      }

   for (i = optind; i < argc; i++) {
      if (listFlag) {
	 header = dict_data_open( argv[i], 1 );
	 dict_data_print_header( stdout, header );
	 dict_data_close( header );
      } else if (decompressFlag) {
	 header = dict_data_open( argv[i], 0 );
	 if (!end) end = header->length;
	 buf = dict_data_read( header, start, end, pre, post );
	 fwrite( buf, end-start, 1, stdout );
	 fflush( stdout );
	 xfree( buf );
	 dict_data_close( header );
      } else {
	 sprintf( buffer, "%s.dz", argv[i] );
	 if (!dict_data_zip( argv[i], buffer, pre, post )) {
	    if (!keepFlag && unlink( argv[i] ))
		err_fatal_errno( __FUNCTION__, "Cannot unlink %s\n", argv[i] );
	 } else {
	    err_fatal( __FUNCTION__, "Compression failed\n" );
	 }
      }
   }

   return 0;
}
