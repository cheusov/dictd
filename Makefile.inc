WARNS     =	4

MKC_CHECK_FUNCS2 +=	initgroups:unistd.h getgroups:unistd.h daemon:stdlib.h
MKC_FEATURES     +=	macro

DICT_VERSION =	2.0.0

CPPFLAGS +=	-D_DICT_VERSION=${DICT_VERSION:Q}
CPPFLAGS +=	-D_DICT_CONFIG_PATH=$(SYSCONFDIR:U.)
CPPFLAGS +=	-D_DICT_PLUGIN_PATH=$(LIBEXECDIR:U.)
CPPFLAGS +=	-D_DICT_DICTIONARY_PATH=$(DATADIR:U.)

#CPPFLAGS +=	-D_POSIX_C_SOURCE=200809L
#CPPFLAGS +=	-D_DEFAULT_SOURCE
#CPPFLAGS +=	-D_BSD_SOURCE
#CPPFLAGS +=	-D_NETBSD_SOURCE

AWK               ?=	awk
MKC_REQUIRE_PROGS +=	${AWK}
INTEXTS_REPLS     +=	AWK ${AWK:U${PROG.${AWK}}}

#CCSTD              =	c99
