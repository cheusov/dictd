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
 * $Id: servparse.y,v 1.19 2003/10/11 16:51:36 cheusov Exp $
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

%token <token> '{' '}' TOKEN_ACCESS TOKEN_ALLOW TOKEN_DENY TOKEN_GROUP TOKEN_DATABASE TOKEN_DATA
%token <token> TOKEN_INDEX TOKEN_INDEX_SUFFIX TOKEN_INDEX_WORD
%token <token> TOKEN_FILTER TOKEN_PREFILTER TOKEN_POSTFILTER TOKEN_NAME TOKEN_INFO
%token <token> TOKEN_USER TOKEN_AUTHONLY TOKEN_SITE TOKEN_DATABASE_EXIT
%token <token> TOKEN_STRING
%token <token> TOKEN_INVISIBLE TOKEN_DISABLE_STRAT
%token <token> TOKEN_DATABASE_VIRTUAL TOKEN_DATABASE_LIST
%token <token> TOKEN_DATABASE_PLUGIN TOKEN_PLUGIN

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


Access : TOKEN_ACCESS '{' AccessSpecList '}' { $$ = $3; }
       ;

DatabaseList : Database { $$ = lst_create(); lst_append($$, $1); }
             | DatabaseList Database { lst_append($1, $2); $$ = $1; }
             ;

AccessSpecList : AccessSpec { $$ = lst_create(); lst_append($$, $1); }
               | AccessSpecList AccessSpec { lst_append($1, $2); $$ = $1; }
               ;

Site : TOKEN_SITE TOKEN_STRING { $$ = $2; }
     ;

UserList : TOKEN_USER TOKEN_STRING TOKEN_STRING
           { $$ = hsh_create(NULL,NULL);
	     hsh_insert( $$, $2.string, $3.string );
	   }
         | UserList TOKEN_USER TOKEN_STRING TOKEN_STRING
           { hsh_insert( $1, $3.string, $4.string ); $$ = $1; }
         ;

AccessSpec : TOKEN_ALLOW TOKEN_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_ALLOW;
		a->spec = $2.string;
		$$ = a;
	     }
           | TOKEN_DENY TOKEN_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_DENY;
		a->spec = $2.string;
		$$ = a;
	     }
           | TOKEN_AUTHONLY TOKEN_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_AUTHONLY;
		a->spec = $2.string;
		$$ = a;
	     }
           | TOKEN_USER TOKEN_STRING
             {
		dictAccess *a = xmalloc(sizeof(struct dictAccess));
		a->type = DICT_USER;
		a->spec = $2.string;
		$$ = a;
	     }
           ;

Database : TOKEN_DATABASE TOKEN_STRING
           {
	      db = xmalloc(sizeof(struct dictDatabase));
	      memset( db, 0, sizeof(struct dictDatabase));
	      db->databaseName = $2.string;
	      db->normal_db    = 1;
	   }
           '{' SpecList '}' { $$ = db; }
           |
           TOKEN_DATABASE_VIRTUAL TOKEN_STRING
           {
	      db = xmalloc(sizeof(struct dictDatabase));
	      memset( db, 0, sizeof(struct dictDatabase));
	      db->databaseName = $2.string;
	      db->virtual_db   = 1;
	   }
           '{' SpecList_virtual '}' { $$ = db; }
           |
           TOKEN_DATABASE_PLUGIN TOKEN_STRING
           {
	      db = xmalloc(sizeof(struct dictDatabase));
	      memset( db, 0, sizeof(struct dictDatabase));
	      db->databaseName = $2.string;
	      db->plugin_db   = 1;
	   }
           '{' SpecList_plugin '}' { $$ = db; }
           |
	   TOKEN_DATABASE_EXIT
	   {
	      db = xmalloc(sizeof(struct dictDatabase));
	      memset( db, 0, sizeof(struct dictDatabase));
	      db -> databaseName  = strdup("--exit--");
	      db -> databaseShort = strdup("Stop default search here.");
	      db -> exit_db       = 1;
	      $$ = db;
	   }
         ;

SpecList_virtual : Spec_virtual
         | SpecList_virtual Spec_virtual
         ;

Spec_virtual : TOKEN_NAME TOKEN_STRING       { SET(databaseShort,$1,$2); }
     | TOKEN_INFO TOKEN_STRING               { SET(databaseInfo,$1,$2); }
     | TOKEN_DATABASE_LIST TOKEN_STRING      { SET(database_list,$1,$2);}
     | TOKEN_INVISIBLE               { db->invisible = 1; }
     | TOKEN_DISABLE_STRAT TOKEN_STRING { dict_disable_strat (db, $2.string); }
     | Access                    { db->acl = $1; }
     ;

SpecList_plugin : Spec_plugin
         | SpecList_plugin Spec_plugin
         ;

Spec_plugin : TOKEN_NAME TOKEN_STRING       { SET(databaseShort,$1,$2); }
     | TOKEN_INFO TOKEN_STRING               { SET(databaseInfo,$1,$2); }
     | TOKEN_PLUGIN TOKEN_STRING      { SET(pluginFilename,$1,$2);}
     | TOKEN_DATA TOKEN_STRING        { SET(plugin_data,$1,$2);}
     | TOKEN_INVISIBLE               { db->invisible = 1; }
     | TOKEN_DISABLE_STRAT TOKEN_STRING { dict_disable_strat (db, $2.string); }
     | Access                    { db->acl = $1; }
     ;

SpecList : Spec
         | SpecList Spec
         ;

Spec : TOKEN_DATA TOKEN_STRING              { SET(dataFilename,$1,$2); }
     | TOKEN_INDEX TOKEN_STRING             { SET(indexFilename,$1,$2); }
     | TOKEN_INDEX_SUFFIX TOKEN_STRING      { SET(indexsuffixFilename,$1,$2); }
     | TOKEN_INDEX_WORD TOKEN_STRING        { SET(indexwordFilename,$1,$2); }
     | TOKEN_FILTER TOKEN_STRING     { SET(filter,$1,$2); }
     | TOKEN_PREFILTER TOKEN_STRING  { SET(prefilter,$1,$2); }
     | TOKEN_POSTFILTER TOKEN_STRING { SET(postfilter,$1,$2); }
     | TOKEN_NAME TOKEN_STRING       { SET(databaseShort,$1,$2); }
     | TOKEN_INFO TOKEN_STRING               { SET(databaseInfo,$1,$2); }
     | TOKEN_INVISIBLE           { db->invisible = 1; }
     | TOKEN_DISABLE_STRAT TOKEN_STRING { dict_disable_strat (db, $2.string); }
     | Access                { db->acl = $1; }
     ;
