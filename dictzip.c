/* dictzip.c -- 
 * Created: Tue Jul 16 12:45:41 1996 by r.faith@ieee.org
 * Revised: Tue Jul 23 19:39:54 1996 by faith@cs.unc.edu
 * Copyright 1996 Rickard E. Faith (r.faith@ieee.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *    (If you use this software in a product, acknowledgment in product
 *    documentation would be appreciated, but is not required.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: dictzip.c,v 1.1 1996/09/24 01:07:48 faith Exp $
 * 
 */

#include "maa.h"
#include "zlib.h"
#include <assert.h>
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>

#ifdef DMALLOC_FUNC_CHECK
#include "dmalloc.h"
#endif

#define HEADER_CRC 0		/* Conflicts with gzip 1.2.4 */

#define IN_BUFFER_SIZE (64 * 1024)
				/* OUT_BUFFER_SIZE 10% + 12 bytes larger */
#define OUT_BUFFER_SIZE (IN_BUFFER_SIZE + IN_BUFFER_SIZE / 20 + 12)

				/* Magic for GZIP (rfc1952)                */
#define GZ_MAGIC1     0x1f	/* First magic byte                        */
#define GZ_MAGIC2     0x8b	/* Second magic byte                       */

				/* FLaGs (bitmapped), from rfc1952         */
#define GZ_FTEXT      0x01	/* Set for ASCII text                      */
#define GZ_FHCRC      0x02	/* Header CRC16                            */
#define GZ_FEXTRA     0x04	/* Optional field (random access index)    */
#define GZ_FNAME      0x08	/* Original name                           */
#define GZ_COMMENT    0x10	/* Zero-terminated, human-readable comment */
#define GZ_MAX           2	/* Maximum compression                     */
#define GZ_FAST          4	/* Fasted compression                      */

				/* These are from rfc1952                  */
#define GZ_OS_FAT        0	/* FAT filesystem (MS-DOS, OS/2, NT/Win32) */
#define GZ_OS_AMIGA      1	/* Amiga                                   */
#define GZ_OS_VMS        2	/* VMS (or OpenVMS)                        */
#define GZ_OS_UNIX       3      /* Unix                                    */
#define GZ_OS_VMCMS      4      /* VM/CMS                                  */
#define GZ_OS_ATARI      5      /* Atari TOS                               */
#define GZ_OS_HPFS       6      /* HPFS filesystem (OS/2, NT)              */
#define GZ_OS_MAC        7      /* Macintosh                               */
#define GZ_OS_Z          8      /* Z-System                                */
#define GZ_OS_CPM        9      /* CP/M                                    */
#define GZ_OS_TOPS20    10      /* TOPS-20                                 */
#define GZ_OS_NTFS      11      /* NTFS filesystem (NT)                    */
#define GZ_OS_QDOS      12      /* QDOS                                    */
#define GZ_OS_ACORN     13      /* Acorn RISCOS                            */
#define GZ_OS_UNKNOWN  255      /* unknown                                 */

#define GZ_RND_S1       'R'	/* First magic for random access format    */
#define GZ_RND_S2       'A'	/* Second magic for random access format   */

#define GZ_ID1           0	/* GZ_MAGIC1                               */
#define GZ_ID2           1	/* GZ_MAGIC2                               */
#define GZ_CM            2	/* Compression Method (Z_DEFALTED)         */
#define GZ_FLG	         3	/* FLaGs (see above)                       */
#define GZ_MTIME         4	/* Modification TIME                       */
#define GZ_XFL           8	/* eXtra FLags (GZ_MAX or GZ_FAST)         */
#define GZ_OS            9	/* Operating System                        */
#define GZ_XLEN         10	/* eXtra LENgth (16bit)                    */
#define GZ_FEXTRA_START 12	/* Start of extra fields                   */
#define GZ_SI1          12	/* Subfield ID1                            */
#define GZ_SI2          13      /* Subfield ID2                            */
#define GZ_SUBLEN       14	/* Subfield length (16bit)                 */
#define GZ_CHUNKLEN     16	/* Chunk length (32bit)                    */
#define GZ_RNDDATA      20	/* Random access data (32bit values)       */


int main( int argc, char **argv )
{
    char          inBuffer[IN_BUFFER_SIZE];
    char          outBuffer[OUT_BUFFER_SIZE];
    int           count;
    unsigned long inputCRC = crc32( 0L, Z_NULL, 0 );
    z_stream      zStream;
    FILE          *outStr;
    FILE          *inStr;
    int           len;
    char          *inFilename = argv[1];
    char          *outFilename;
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
    
				/* Initialize Libmaa */
    maa_init( argv[0] );
    
				/* Open files */
    if (!(inStr = fopen( inFilename, "r" )))
	err_fatal_errno( __FUNCTION__,
			 "Cannot open \"%s\" for read", inFilename );
    outFilename = malloc( strlen( inFilename + 4 ) );
    strcpy( outFilename, inFilename );
    strcat( outFilename, ".gz" );
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
	    printf( "got %d bytes\n", count );
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
	    printf( "wrote %d bytes\n", len );
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
