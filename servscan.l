/* servscan.l -- Scanner for dictd server configuration file
 * Created: Fri Feb 28 08:23:03 1997 by faith@cs.unc.edu
 * Revised: Fri Feb 28 13:33:58 1997 by faith@cs.unc.edu
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
 * $Id: servscan.l,v 1.1 1997/03/01 04:23:30 faith Exp $
 * 
 */

%option stack debug
%{
#include "dict.h"
#include "servparse.h"
extern int  yylex( void );
extern int  yydebug;
extern void yyerror( const char *message );
#define RETURN(val) do {                  \
   yylval.token.src = src_get( yyleng );  \
   return val;                            \
} while (0)
%}
%s OTHER
letter                      [a-zA-Z]
digit                       [0-9]
INTEGER                     {digit}({digit})*
comment                     #.*
word                        ([[:alpha:]_/\-\.\*][[:alnum:]_/\-\.\*]*)
string                      \"(([^\"\n])|\"\")*\"
badstring                   \"(([^\"\n])|\"\")*
NL                          \n
ID                          ([[:alpha:]_][[:alnum:]_]*)
WS                          [[:blank:]]+
%%
<INITIAL>{
   .*{NL}                   src_line(yytext,yyleng); yyless(0); BEGIN(OTHER);
   .*                       src_line(yytext,yyleng); yyless(0); BEGIN(OTHER);
}

access                      RETURN(T_ACCESS);
allow                       RETURN(T_ALLOW);
deny                        RETURN(T_DENY);
group                       RETURN(T_GROUP);
database                    RETURN(T_DATABASE);
data                        RETURN(T_DATA);
index                       RETURN(T_INDEX);
filter                      RETURN(T_FILTER);
prefilter                   RETURN(T_PREFILTER);
postfilter                  RETURN(T_POSTFILTER);
{string}                    { yylval.token.string
				 = str_findn(yytext+1, yyleng-2);
                              RETURN(T_STRING);
                            }
{word}                      { yylval.token.string = str_findn(yytext, yyleng);
                              RETURN(T_STRING);
                            }
{NL}                        BEGIN(INITIAL);
<<EOF>>                     RETURN(0);
{WS}                        src_advance( yyleng );
{comment}                   src_advance( yyleng );
.                           RETURN(yytext[0]);
%%
int yywrap( void )
{
   return 1;
}

void yyerror( const char *message )
{
   src_parse_error( stderr, src_get( yyleng ), message );
   err_fatal( __FUNCTION__, "parse error\n" );
   exit( 1 );
}
