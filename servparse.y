/* servparse.y -- Parser for dictd server configuration file
 * Created: Fri Feb 28 08:31:38 1997 by faith@cs.unc.edu
 * Revised: Fri Jul 11 11:42:51 1997 by faith@acm.org
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
 * $Id: servparse.y,v 1.8 1997/07/12 01:50:25 faith Exp $
 * 
 */

%{
#include "dictd.h"
#define YYDEBUG 1
#define YYERROR_VERBOSE

static dictDatabase *db;

#define SET(field,s,t) do {                                   \
   if (db->field)                                             \
      src_parse_error( stderr, s.src, #field "already set" ); \
   db->field = t.string;                                      \
} while(0);
%}

%union {
   dictToken     token;
   dictDatabase  *db;
   dictAccess    *access;
   lst_List      list;
   hsh_HashTable hash;
}

				/* Terminals */

%token <token> '{' '}' T_ACCESS T_ALLOW T_DENY T_GROUP T_DATABASE T_DATA
%token <token> T_INDEX T_FILTER T_PREFILTER T_POSTFILTER T_NAME
%token <token> T_USER T_AUTHONLY T_SITE

%token <token>  T_STRING
%type  <token>  Site
%type  <access> AccessSpec
%type  <db>     Database
%type  <list>   DatabaseList Access AccessSpecList
%type  <hash>   UserList

%%

Program : DatabaseList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->dbl = $1;
	  }
        | Access DatabaseList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->acl = $1;
	    DictConfig->dbl = $2;
	  }
        | DatabaseList UserList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->dbl = $1;
	    DictConfig->usl = $2;
	  }
        | Access DatabaseList UserList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->acl = $1;
	    DictConfig->dbl = $2;
	    DictConfig->usl = $3;
	  }
        | Site DatabaseList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->site = $1.string;
	    DictConfig->dbl  = $2;
	  }
        | Site Access DatabaseList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->site = $1.string;
	    DictConfig->acl  = $2;
	    DictConfig->dbl  = $3;
	  }
        | Site DatabaseList UserList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->site = $1.string;
	    DictConfig->dbl  = $2;
	    DictConfig->usl  = $3;
	  }
        | Site Access DatabaseList UserList
          { DictConfig = xmalloc(sizeof(struct dictConfig));
	    memset( DictConfig, 0, sizeof(struct dictConfig) );
	    DictConfig->site = $1.string;
	    DictConfig->acl  = $2;
	    DictConfig->dbl  = $3;
	    DictConfig->usl  = $4;
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

Site : T_SITE T_STRING { $$ = $2; }

UserList : T_USER T_STRING T_STRING
           { $$ = hsh_create(NULL,NULL);
	     hsh_insert( $$, $2.string, $3.string );
	   }
         | UserList T_USER T_STRING T_STRING
           { hsh_insert( $1, $3.string, $4.string ); $$ = $1; }
         ;

AccessSpec : T_ALLOW T_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_ALLOW;
		a->spec = $2.string;
		$$ = a;
	     }
           | T_DENY T_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_DENY;
		a->spec = $2.string;
		$$ = a;
	     }
           | T_AUTHONLY T_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_AUTHONLY;
		a->spec = $2.string;
		$$ = a;
	     }
           | T_USER T_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_USER;
		a->spec = $2.string;
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
     | T_NAME T_STRING       { SET(databaseShort,$1,$2); }
     | Access                { db->acl = $1; }
     ;
