LIB       =	dictd
SRCS      =	data.c index.c strategy.c

CPPFLAGS +=	 -I${SRCTOP}/dictdplugins/include -I${SRCDIR_dictzip} \
   -I${SRCDIR_libcommon} -I${SRCDIR_dictd} -I${.CURDIR}

CFLAGS.check +=	-Wno-error=cast-qual -Wno-error=unused-variable
CPPFLAGS     +=	-D_POSIX_C_SOURCE=200809L

MKINSTALL =	no

MKC_FEATURES +=	strlcpy

.include <mkc.mk>
