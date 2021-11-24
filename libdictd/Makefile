LIB       =	dictd
SRCS      =	data.c index.c strategy.c

CPPFLAGS +=	 -I${SRCDIR_dictdplugins_include:U.} -I${SRCDIR_dictzip} \
   -I${SRCDIR_libcommon}

CFLAGS.check +=	-Wno-error=cast-qual -Wno-error=unused-variable

MKINSTALL =	no

.include <mkc.mk>
