/* wndump.c -- Dump WordNet datafiles
 * Created: Sun Sep 22 14:03:26 1996 by faith@cs.unc.edu
 * Revised: Wed Sep 25 22:06:44 1996 by faith@cs.unc.edu
 * Public Domain 1996 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 *
 * $Id: wndump.c,v 1.2 1996/09/26 02:27:43 faith Exp $
 * 
 */

#include "dict.h"

#define PROGRAM_NAME    "wndump"
#define PROGRAM_VERSION "1.0"

static void process( const char *w, const char *p, const char *d,
		     const char *lastW, const char *lastP,
		     const char *nextW, const char *nextP )
{
   static int entry = 1;
   
   if (!strcmp( lastW, w )) { /* same as last work */
      if (!strcmp( lastP, p )) { /* same pos */
	 if (p[0] != '!') fmt_def( NULL, ++entry );
	 fmt_string( d );
      } else {		/* new pos */
	 entry = 1;
	 fmt_def( p, !strcmp( nextW, w ) && !strcmp( nextP, p ) ? entry : 0 );
	 fmt_string( d );
      }
   } else {		/* new word */
      fmt_new( w );
      fmt_string( w );
      entry = 1;
      if (p[0] == '!')
	 fmt_def( NULL, -1 );
      else
	 fmt_def( p, !strcmp( nextW, w ) && !strcmp( nextP, p ) ? entry : 0 );
      fmt_string( d );
   }
}

static void usage( void )
{
   fprintf( stderr,
	    "Usage: wndump 1 < data/WordNet/* | sort | wndump 2\n" );
   exit( 1 );
}

int main( int argc, char **argv )
{
   char buffer[BUFFERSIZE];
   char *pos;
   int  i;
   char *word[BUFFERSIZE];
   char *sense[BUFFERSIZE];
   char *pt;
   int  count;
   int  haveLicense = 0;
   int  license     = 0;
   long t;
   FILE *one, *two;
   int  pid;

   maa_init( argv[0] );
   time( &t );

   if (argc == 1) {
#if 0
      dbg_set( ".pr" );
#endif
      sprintf( buffer, "%s 1", argv[0] );
      pr_open( buffer, PR_CREATE_STDOUT, NULL, &one, NULL );
      pr_open( "sort", PR_USE_STDIN | PR_CREATE_STDOUT, &one, &two, NULL );
      sprintf( buffer, "%s 2", argv[0] );
      pid = pr_open( buffer, PR_USE_STDIN, &two, NULL, NULL );

      printf( "\nstatus = 0x%02x\n", pr_wait( pid ) );
 
      exit( 0 );
   }

   if (argc != 2 || (argv[1][0] != '1'
		     && argv[1][0] != '2'
		     && argv[1][0] != 'd')) usage();

   if (argv[1][0] == '1') {
      printf( "!info! ! %02d Original database from:"
	      " ftp://clarity.princeton.edu/pub/wordnet/wn1.5unix.tar.gz\n",
	      ++license );
      printf( "!info! ! %02d \n", ++license );
      printf( "!info! ! %02d This human-readable version generated from the"
	      " pristine database sources on %24.24s by %s, version %s"
	      " (written by Rik Faith, faith@cs.unc.edu).  The following"
	      " restrictions apply to the original database, and to this"
	      " derivative work.  No additional restrictions are claimed.\n",
	      ++license, ctime(&t), PROGRAM_NAME, PROGRAM_VERSION );
      printf( "!info! ! %02d \n", ++license );

      while (fgets( buffer, BUFFERSIZE-1, stdin )) {
	 if (!haveLicense) {
	    if (buffer[0] == ' ' && buffer[1] == ' ') {
	       pt = strchr( buffer + 2, ' ' );
	       pt[ strlen(pt) - 3] = '\0';
	       printf( "!info! ! %02d %s\n", ++license, pt+1 );
	    } else {
	       ++haveLicense;
	    }
	    continue;
	 }
	 pt = strchr( buffer, '|' );
	 if (pt) pt[ strlen(pt) - 3] = '\0';
	 strtok( buffer, " " );	/* synset_offset */
	 strtok( NULL, " " );	/* lex_file_num */
	 pos = strtok( NULL, " " ); /* pos */
	 if (!pos) continue;
	 count = atoi( strtok( NULL, " " ) ); /* id */
	 for (i = 0; i < count; i++) {
	    word[i] = strdup( strtok( NULL, " " ) );
	    sense[i] = strdup( strtok( NULL, " " ) );
	 }
	 for (i = 0; i < count; i++) {
	    printf( "%s ", word[i] );
	    switch (*pos) {
	    case 'a': printf( "adj" );     break;
	    case 's': printf( "adj" );     break;
	    case 'r': printf( "adv" );     break;
	    default:  printf( "%s", pos ); break;
	    }
	    printf( " %s", sense[i] );
	    if (pt) printf( " %s", pt + 2 );
	    if (count > 1) {
	       int j;
	       printf( " [syn: " );
	       for (j = 0; j < count; j++) {
		  if (j == i) continue;
		  printf( "%s", word[j] );
		  if (!((i != count - 1 && j == count - 1)
			|| (i == count - 1 && j == count - 2))) {
		     printf( "; " );
		  }
	       }
	       printf( "]" );
	    }
	    printf( "\n" );
	 }
	 for (i = 0; i < count; i++) {
	    free( word[i] );
	    free( sense[i] );
	 }
      }
   } else {
      char lastW[BUFFERSIZE] = { '\0', };
      char lastP[BUFFERSIZE] = { '\0', };
      char w[BUFFERSIZE]     = { '\0', };
      char p[BUFFERSIZE]     = { '\0', };
      char d[BUFFERSIZE]     = { '\0', };
      char *nextW, *nextP, *nextD;

      if (fgets( buffer, BUFFERSIZE-1, stdin )) {
	 strcpy( w, strtok( buffer, " " ) );
	 strcpy( p, strtok( NULL, " " ) );
	 strtok( NULL, " " );
	 strcpy( d, strtok( NULL, "\0" ) );

	 fmt_open( argv[1][0] == 'd' ? NULL : "wordnet" );
	 while (fgets( buffer, BUFFERSIZE-1, stdin )) {
	    nextW = strtok( buffer, " " );
	    nextP = strtok( NULL, " " );
	    strtok( NULL, " " );
	    nextD = strtok( NULL, "\0" );
	    if (d[0]) {
	       d[ strlen(d)-1 ] = '\0'; /* remove newline */
	       process( w, p, d, lastW, lastP, nextW, nextP );
	    }
	    strcpy( lastW, w );
	    strcpy( lastP, p );
	    strcpy( w, nextW );
	    strcpy( p, nextP );
	    if (nextD) strcpy( d, nextD );
	    else       strcpy( d, "" );
	 }

	 process( w, p, d, lastW, lastP, "", "" );
	 fmt_close();
      } else {
	 usage();
      }
   }
   
   return 0;
}
