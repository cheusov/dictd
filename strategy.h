/*
 * Created: Sun Mar  2 17:16:13 2003 by Aleksey Cheusov <vle@gmx.net>
 * Copyright 1994-2003 Rickard E. Faith (faith@dict.org)
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
 */

#ifndef _STRATEGY_H_
#define _STRATEGY_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DICT_DEFAULT_STRATEGY    DICT_LEVENSHTEIN

typedef struct dictStrategy {
   const char *name;
   const char *description;
   int        number;
} dictStrategy;

extern int default_strategy;

/* initialize/destroy the default strategy list */
extern void dict_init_strategies (void);
extern void dict_destroy_strategies (void);

/* disables comma-separated strategies */
extern void dict_disable_strategies (const char *strategies);
/* adds new strategy */
extern void dict_add_strategy (const char *strat, const char *description);

/* */
extern int get_strategy_count (void);
extern dictStrategy **get_strategies (void);
extern int lookup_strategy( const char *strategy );

#endif
