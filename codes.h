/* codes.h -- 
 * Created: Wed Apr 16 08:44:03 1997 by faith@cs.unc.edu
 * Revised: Wed Apr 16 10:23:03 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: codes.h,v 1.1 1997/04/30 12:03:46 faith Exp $
 * 
 */

#ifndef _CODES_H_
#define _CODES_H_

#define CODE_DATABASE_LIST           210
#define CODE_STRATEGY_LIST           211
#define CODE_DATABASE_INFO           212
#define CODE_STATUS                  213
#define CODE_HELP                    214

#define CODE_HELLO                   220
#define CODE_GOODBYE                 221

#define CODE_AUTH_OK                 230

#define CODE_DEFINITIONS_FOUND       250
#define CODE_DEFINITION_FOLLOWS      251
#define CODE_MATCHES_FOUND           253
#define CODE_DEFINITIONS_FINISHED    259
#define CODE_MATCHES_FINISHED        259
#define CODE_OK                      259

#define CODE_SYNTAX_ERROR            500
#define CODE_ILLEGAL_PARAM           501
#define CODE_COMMAND_NOT_IMPLEMENTED 502
#define CODE_PARAM_NOT_IMPLEMENTED   503

#define CODE_ACCESS_DENIED           530
#define CODE_AUTH_DENIED             531

#define CODE_INVALID_DB              550
#define CODE_INVALID_STRATEGY        551
#define CODE_NO_MATCH                552
#define CODE_NO_DATABASES            554
#define CODE_NO_STRATEGIES           555

#endif
