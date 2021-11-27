LIB       =	dictd
SRCS      =	data.c index.c strategy.c

CPPFLAGS +=	 -I${SRCTOP}/dictdplugins/include -I${SRCDIR_dictzip} \
   -I${SRCDIR_libcommon} -I${SRCDIR_dictd} -I.

CFLAGS.check +=	-Wno-error=cast-qual -Wno-error=unused-variable

MKINSTALL =	no

MKC_FEATURES +=	strlcpy

.include <mkc.mk>
