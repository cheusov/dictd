/* servscan.l -- Scanner for dictd server configuration file
 * Created: Fri Feb 28 08:23:03 1997 by faith@cs.unc.edu
 * Revised: Fri Mar 29 12:42:57 2002 by faith@acm.org
 * Copyright 1997, 1999, 2000, 2002 Rickard E. Faith (faith@acm.org)
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
 * $Id: servscan.l,v 1.21 2003/10/01 17:09:04 cheusov Exp $
 * 
 */

%option stack debug
%{
#include "dictd.h"
#include "servparse.h"
extern int  yylex( void );
extern int  yydebug;
extern void yyerror( const char *message );
static void include_file( void );
static void include_complete( void );
YY_BUFFER_STATE orig_buffer = NULL;
src_Type orig_source = NULL;
#define RETURN(val) do {                  \
   yylval.token.src = src_get( yyleng );  \
   return val;                            \
} while (0)

/*
  \n    --> new line character
  \\    --> backslash
  \<NL> --> no symbol
  \"    --> double quote symbol
*/
static int escape_string (char *s, int len)
{
   int ret = 0;
   int i   = 0;

   for (i=0; i < len; ++i){
      if (s [i] != '\\'){
	 s [ret++] = s [i];
      }else if (s [i+1] == '\\'){
	 s [ret++] = '\\';

	 ++i;
      }else if (s [i+1] == 'n'){
	 s [ret++] = '\n';

	 ++i;
      }else if (s [i+1] == '"'){
	 s [ret++] = '"';

	 ++i;
      }else if (s [i+1] != '\n'){
	 s [ret++] = s [++i];
      }else{
	 ++i;
      }
   }

   return ret;
}

%}
%s OTHER
comment                     #.*
word                        ([[:alnum:]_/\-\.\*][[:alnum:]_/\-\.\*]*)
string                      \"(([^\"\n])|\"\"|\\\n|\\\")*\"
NL                          \n
WS                          [[:blank:]]+
%%
<INITIAL>{
   .*{NL}                   src_line(yytext,yyleng); yyless(0); BEGIN(OTHER);
   .*                       src_line(yytext,yyleng); yyless(0); BEGIN(OTHER);
}
include{WS}({word}|{string})   include_file();
access                      RETURN(T_ACCESS);
allow                       RETURN(T_ALLOW);
deny                        RETURN(T_DENY);
group                       RETURN(T_GROUP);
database                    RETURN(T_DATABASE);
database_exit               RETURN(T_DATABASE_EXIT);
data                        RETURN(T_DATA);
index                       RETURN(T_INDEX);
index_suffix                RETURN(T_INDEX_SUFFIX);
index_word                  RETURN(T_INDEX_WORD);
filter                      RETURN(T_FILTER);
prefilter                   RETURN(T_PREFILTER);
postfilter                  RETURN(T_POSTFILTER);
name                        RETURN(T_NAME);
user                        RETURN(T_USER);
authonly                    RETURN(T_AUTHONLY);
site                        RETURN(T_SITE);
invisible                   RETURN(T_INVISIBLE);
database_virtual            RETURN(T_DATABASE_VIRTUAL);
info                        RETURN(T_INFO);
database_list               RETURN(T_DATABASE_LIST);
database_plugin             RETURN(T_DATABASE_PLUGIN);
plugin                      RETURN(T_PLUGIN);
disable_strategy            RETURN(T_DISABLE_STRAT);

{word}                      { yylval.token.string = str_findn(yytext, yyleng);
                              RETURN(T_STRING);
                            }
{string}                    {
				yylval.token.string =
					str_findn (
						yytext+1,
						escape_string (yytext+1, yyleng-2));
				RETURN(T_STRING);
                            }
{NL}                        BEGIN(INITIAL);
{WS}                        src_advance( yyleng );
{comment}                   src_advance( yyleng );
.                           RETURN(yytext[0]);
<<EOF>>                     { if (orig_buffer) include_complete();
                              else RETURN(0);
                            }
%%
int yywrap( void )
{
   return 1;
}

void yyerror( const char *message )
{
   src_parse_error( stderr, yylval.token.src, message );
   err_fatal( __FUNCTION__, "parse error\n" );
   exit( 1 );
}

static void include_file( void ) {
   const char *file;
   int i;
   for ( i = 8; yytext[i] == ' ' || yytext[i] == '\t'; ++i )
      ;  /* advance past include{WS} to filename */
   if ( yytext[i] == '"' )  /* process {string} */
      file = str_findn(yytext+i+1, yyleng-i-2);
   else                     /* process {word} */
      file = str_findn(yytext+i, yyleng-i);
   orig_source = src_get( yyleng );
   if ( orig_buffer ) {
      src_print_error( stderr, orig_source, NULL );
      err_fatal( __FUNCTION__, "Nested includes are not permitted\n" );
      exit( 1 );
   }
   orig_buffer = YY_CURRENT_BUFFER;
   yyin = fopen( file, "r" );
   if ( ! yyin ) {
      src_print_error( stderr, orig_source, NULL );
      err_fatal( __FUNCTION__, "Cannot open \"%s\" for read\n", file );
      exit( 1 );
   }
   src_new_file( file );
   src_new_line ( 0 );
   yy_switch_to_buffer(
      yy_create_buffer( yyin, YY_BUF_SIZE ) );
   BEGIN(INITIAL);
}

static void include_complete( void ) {
   const char *orig_line;
   yy_delete_buffer( YY_CURRENT_BUFFER );
   yy_switch_to_buffer( orig_buffer );
   orig_buffer = NULL;
   BEGIN(OTHER);
   src_new_file( src_filename( orig_source ) );
   orig_line = src_source_line( orig_source );
   src_line( orig_line, strlen( orig_line ) );
   src_new_line( src_linenumber( orig_source ) );
   src_advance( src_offset( orig_source ) + src_length( orig_source ) );
   orig_source = NULL;
}
