/* fmt.h -- 
 * Created: Sun Sep 22 16:03:15 1996 by faith@cs.unc.edu
 * Revised: Sun Sep 22 16:39:32 1996 by faith@cs.unc.edu
 * Copyright 1996 Rickard E. Faith (faith@cs.unc.edu)
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in
 *        the documentation and/or other materials provided with the
 *        distribution.  (Accompanying source code, or an offer for such
 *        source code as described in the GNU General Public License, is
 *        sufficient to meet this condition.)
 *  
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *  
 * $Id: fmt.h,v 1.1 1996/09/24 01:07:50 faith Exp $
 * 
 */

#ifndef _FMT_H_
#define _FMT_H_

extern void fmt_newline( void );
extern void fmt_new( const char *word );
extern void fmt_string( const char *string );
extern void fmt_def( const char *pos, int entry );
extern void fmt_open( const char *basename );
extern void fmt_close( void );

#endif
