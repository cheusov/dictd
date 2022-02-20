/*
 * Created by Aleksey Cheusov <vle@gmx.net>
 * Public Domain
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <unistd.h>

void setutf8locale(void);

#if !HAVE_FUNC2_DAEMON_STDLIB_H
int daemon(int nochdir, int noclose);
#endif

#if !HAVE_FUNC2_INITGROUPS_UNISTD_H
int initgroups(const char *name, gid_t basegid);
#endif

#if !HAVE_FUNC2_GETGROUPS_UNISTD_H
int getgroups(int gidsetlen, gid_t *gidset);
#endif

#endif // _COMMON_H_
