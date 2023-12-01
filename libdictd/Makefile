LIB       =	dictd
SRCS      =	data.c index.c strategy.c

CPPFLAGS +=	 -I${SRCTOP}/dictdplugins/include -I${SRCDIR_dictzip} \
   -I${SRCDIR_libcommon} -I${SRCDIR_dictd} -I${.CURDIR}

CFLAGS.check +=	-Wno-error=unused-variable -Wno-empty-body
CFLAGS.check +=	-erroff=SEC_NULL_PTR_DEREF

MKINSTALL =	no

MKC_FEATURES +=	strlcpy efun

.include <mkc.mk>
