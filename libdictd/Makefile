LIB       =	dictd
SRCS      =	data.c index.c strategy.c

CPPFLAGS +=	 -I${SRCTOP}/dictdplugins/include -I${SRCDIR_dictzip} \
   -I${SRCDIR_libcommon} -I${SRCDIR_dictd} -I${.CURDIR}

CFLAGS.check +=	-Wno-error=cast-qual -Wno-error=unused-variable \
   -Wno-empty-body

MKINSTALL =	no

MKC_FEATURES +=	strlcpy efun

.include <mkc.mk>
