/*
 * Created: Sun Mar  2 17:22:21 2003 by Aleksey Cheusov <vle@gmx.net>
 * Copyright 1996, 1997, 1998, 2000, 2002, 2003
 *    Rickard E. Faith (faith@dict.org)
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
 * $Id: strategy.c,v 1.3 2003/08/06 17:55:54 cheusov Exp $
 * 
 */

#include "strategy.h"
#include "dictdplugin.h"
#include "maa.h"
#include "dictd.h"

#include <stdlib.h>
#include <string.h>

int default_strategy  = DICT_DEFAULT_STRATEGY;

static dictStrategy strategyInfo[] = {
   {"exact",     "Match headwords exactly",                    DICT_EXACT },
   {"prefix",    "Match prefixes",                             DICT_PREFIX },
   {"substring", "Match substring occurring anywhere in a headword", DICT_SUBSTRING},
   {"suffix",    "Match suffixes",                             DICT_SUFFIX},
   {"re",        "POSIX 1003.2 (modern) regular expressions",  DICT_RE },
   {"regexp",    "Old (basic) regular expressions",            DICT_REGEXP },
   {"soundex",   "Match using SOUNDEX algorithm",              DICT_SOUNDEX },
   {"lev", "Match headwords within Levenshtein distance one",  DICT_LEVENSHTEIN },
   {"word", "Match separate words within headwords",           DICT_WORD },
};
#define STRATEGIES (sizeof(strategyInfo)/sizeof(strategyInfo[0]))

static dictStrategy **strategies = NULL;
static int strategy_count = 0;
static int strategy_id    = 0;

#define NEW_STRAT_ID (int)(100)

void dict_init_strategies (void)
{
   int i;
   strategy_id = NEW_STRAT_ID;

   strategies = (dictStrategy **) xmalloc (STRATEGIES * sizeof (dictStrategy *));

   for (i=0; i < STRATEGIES; ++i){
      strategies [i] = (dictStrategy *) xmalloc (sizeof (dictStrategy));
      *strategies [i] = strategyInfo [i];

      assert (strategyInfo [i].number < NEW_STRAT_ID);
   }

   strategy_count = STRATEGIES;
}

static void dict_free_strategy (dictStrategy *strat)
{
   if (strat -> number >= NEW_STRAT_ID){
      /* Free memory allocated for new strategies */
      xfree ((void *) strat -> name);
      xfree ((void *) strat -> description);
   }

   xfree (strat);
}

void dict_destroy_strategies (void)
{
   int i;

   for (i=0; i < strategy_count; ++i){
      dict_free_strategy (strategies [i]);
   }

   xfree (strategies);

   strategies     = NULL;
   strategy_count = 0;
}

int get_strategy_count (void)
{
   return strategy_count;
}

dictStrategy **get_strategies (void)
{
   return strategies;
}

static int lookup_strategy_index ( const char *strategy )
{
   int i;

   for (i = 0; i < strategy_count; i++) {
      if (!strcasecmp (strategy, strategies [i] -> name)){
         return i;
      }
   }

   return -1;
}

int lookup_strategy( const char *strategy )
{
   int idx;
   if (strategy[0] == '.' && strategy[1] == '\0')
      return default_strategy;

   idx = lookup_strategy_index (strategy);
   if (-1 == idx)
      return -1;
   else
      return strategies [idx] -> number;
}

static dictStrategy * lookup_strat( const char *strategy )
{
   int idx = lookup_strategy_index (strategy);
   if (-1 == idx)
      return NULL;
   else
      return strategies [idx];
}

static void dict_disable_strategy (dictStrategy *strat)
{
   int i;

   for (i=0; i < strategy_count; ++i){
      if (strategies [i] == strat){
	 dict_free_strategy (strat);

	 memmove (
	    strategies + i,
	    strategies + i + 1,
	    (strategy_count - i - 1) * sizeof (strategies [0]));

	 --strategy_count;

	 return;
      }
   }

   abort ();
}

void dict_disable_strategies (const char *strats)
{
   char buffer [400];
   int  i;
   int  len = strlen (strats);
   const char *s = NULL;
   dictStrategy *si = NULL;

   strlcpy (buffer, strats, sizeof (buffer));

   for (i = 0; i < len; ++i){
      if (',' == buffer [i])
	 buffer [i] = '\0';
   }

   for (i = 0; i < len; ++i){
      if (!buffer [i] || (i > 0 && buffer [i-1]))
	 continue;

      s = buffer + i;

      si = lookup_strat (s);

      if (si){
	 dict_disable_strategy (si);
      }else{
	 fprintf (stderr, "Unknown strategy '%s'\n", s);
	 exit (1);
      }
   }
}

void dict_add_strategy (const char *strat, const char *description)
{
   strategies = xrealloc (
      strategies, sizeof (*strategies) * (strategy_count + 1));

   strategies [strategy_count] = xmalloc (sizeof (dictStrategy));

   strategies [strategy_count] -> number      = strategy_id;
   strategies [strategy_count] -> name        = xstrdup (strat);
   strategies [strategy_count] -> description = xstrdup (description);

   ++strategy_id;
   ++strategy_count;
}
