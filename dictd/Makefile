PROG =	dictd
SRCS =	dictd.c daemon.c net.c servscan.l servparse.y
MAN  =	dictd.8

BINDIR =	${SBINDIR}

.ifdef USE_PLUGIN
SRCS     +=	plugin.c
CPPFLAGS +=	-DUSE_PLUGIN=1
MKC_FEATURES +=	libdl
.endif

INFILES =	dictd.8

YHEADER =	1

CPPFLAGS +=	-I${SRCDIR_dictdplugins_include:U.} -I${SRCDIR_libdictd} \
   -I${SRCDIR_dictd} -I${SRCDIR_libcommon}

CFLAGS.check +=	-Wno-error=cast-qual -Wno-error=unused-function

MKC_REQUIRE_FUNCLIBS +=	lst_create:maa deflate:z

dictd.o: servparse.h

.include <mkc.mk>
