/* codes.h -- 
 * Created: Wed Apr 16 08:44:03 1997 by faith@cs.unc.edu
 * Revised: Thu May  1 00:01:14 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: codes.h,v 1.2 1997/05/02 14:49:25 faith Exp $
 * 
 */

#ifndef _CODES_H_
#define _CODES_H_

#define CODE_DATABASE_LIST           110
#define CODE_STRATEGY_LIST           111
#define CODE_DATABASE_INFO           112
#define CODE_HELP                    113
#define CODE_SERVER_INFO             114
#define CODE_STATUS                  210

#define CODE_HELLO                   220
#define CODE_GOODBYE                 221

#define CODE_AUTH_OK                 230

#define CODE_DEFINITIONS_FOUND       150
#define CODE_DEFINITION_FOLLOWS      151
#define CODE_MATCHES_FOUND           152
#define CODE_DEFINITIONS_FINISHED    250
#define CODE_MATCHES_FINISHED        250
#define CODE_OK                      250

#define CODE_TEMPORARILY_UNAVAILABLE 420
#define CODE_SHUTTING_DOWN           421

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
