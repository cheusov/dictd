/*
 * Created by Aleksey Cheusov <vle@gmx.net>
 * Public Domain
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <unistd.h>
#include <sys/socket.h>

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

int net_read( int s, char *buf, int maxlen );
int net_write( int s, const char *buf, int len );
const char *inet_ntopW (struct sockaddr *sa);

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define DICT_VERSION STRINGIFY(_DICT_VERSION)
#define DICT_CONFIG_PATH STRINGIFY(_DICT_CONFIG_PATH)
#define DICT_PLUGIN_PATH STRINGIFY(_DICT_PLUGIN_PATH)
#define DICT_DICTIONARY_PATH STRINGIFY(_DICT_DICTIONARY_PATH)

#endif // _COMMON_H_
