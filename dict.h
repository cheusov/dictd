/* dict.h -- 
 * Created: Wed Apr 16 08:44:21 1997 by faith@cs.unc.edu
 * Revised: Tue Jul  8 20:16:13 1997 by faith@acm.org
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
 * $Id: dict.h,v 1.10 1997/07/09 04:00:58 faith Exp $
 * 
 */

#ifndef _DICT_H_
#define _DICT_H_

#include "dictP.h"
#include "maa.h"
#include "zlib.h"
#include "net.h"
#include "codes.h"

#include <signal.h>
#include <sys/utsname.h>

#define DBG_VERBOSE     (0<<30|1<< 0) /* Verbose                           */
#define DBG_RAW         (0<<30|1<< 1) /* Trace client/server interaction   */
#define DBG_SCAN        (0<<30|1<< 2) /* Debug configuration file scanner  */
#define DBG_PARSE       (0<<30|1<< 3) /* Debug configuration file parser   */
#define DBG_PIPE        (0<<30|1<< 4) /* Debug pipelining                  */
#define DBG_SERIAL      (0<<30|1<< 5) /* Disable pipelining                */
#define DBG_TIME        (0<<30|1<< 6) /* Time transaction                  */



				/* dmalloc must be last */
#ifdef DMALLOC_FUNC_CHECK
# include "dmalloc.h"
#endif

#endif
