/* servparse.y -- Parser for dictd server configuration file
 * Created: Fri Feb 28 08:31:38 1997 by faith@cs.unc.edu
 * Revised: Fri Feb 28 18:26:58 1997 by faith@cs.unc.edu
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
 * $Id: servparse.y,v 1.1 1997/03/01 04:23:29 faith Exp $
 * 
 */

%{
#include "dict.h"
#define YYDEBUG 1
#define YYERROR_VERBOSE

static dictDatabase *db;
static dictConfig   *dc;

#define SET(field,s,t) do {                               \
   if (db->field)                                         \
      src_parse_error( stderr, s.src, #field "already set" ); \
   db->field = t.string;                                  \
   printf( "Got %s\n", t.string ); \
} while(0);
%}

%union {
   dictToken    token;
   dictDatabase *db;
   dictAccess   *access;
   lst_List     list;
}

				/* Terminals */

%token <token> '{' '}' T_ACCESS T_ALLOW T_DENY T_GROUP T_DATABASE T_DATA
%token <token> T_INDEX T_FILTER T_PREFILTER T_POSTFILTER

%token <token>  T_STRING
%type  <access> AccessSpec
%type  <db>     Database
%type  <list>   DatabaseList Access AccessSpecList

%%

Program : Access DatabaseList
          { dc = xmalloc(sizeof(struct dictConfig));
	    memset( dc, 0, sizeof(struct dictConfig) );
	    dc->acl = $1;
	    dc->dbl = $2;
	    dict_set_config( dc );
	  }
        | DatabaseList
          { dc = xmalloc(sizeof(struct dictConfig));
	    memset( dc, 0, sizeof(struct dictConfig) );
	    dc->dbl = $1;
	    dict_set_config( dc );
	  }
        ;

Access : T_ACCESS '{' AccessSpecList '}' { $$ = $3; }
       ;

DatabaseList : Database { $$ = lst_create(); lst_append($$, $1); }
             | DatabaseList Database { lst_append($1, $2); $$ = $1; }
             ;

AccessSpecList : AccessSpec { $$ = lst_create(); lst_append($$, $1); }
               | AccessSpecList AccessSpec { lst_append($1, $2); $$ = $1; }
               ;

AccessSpec : T_ALLOW T_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->allow = DICT_ALLOW;
		a->spec  = $2.string;
		$$ = a;
	     }
           | T_DENY T_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->allow = DICT_DENY;
		a->spec  = $2.string;
		$$ = a;
	     }
           ;

Database : T_DATABASE T_STRING
           {
	      db = xmalloc(sizeof(struct dictDatabase));
	      memset( db, 0, sizeof(struct dictDatabase));
	      db->databaseName = $2.string;
	   }
           '{' SpecList '}' { $$ = db; }
         ;

SpecList : Spec
         | SpecList Spec
         ;

Spec : T_DATA T_STRING       { SET(dataFilename,$1,$2); }
     | T_INDEX T_STRING      { SET(indexFilename,$1,$2); }
     | T_FILTER T_STRING     { SET(filter,$1,$2); }
     | T_PREFILTER T_STRING  { SET(prefilter,$1,$2); }
     | T_POSTFILTER T_STRING { SET(postfilter,$1,$2); }
     | Access                { db->acl = $1; }
     ;
