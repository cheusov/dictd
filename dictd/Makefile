PROG =	dictd
SRCS =	dictd.c daemon.c net_server.c servscan.l servparse.y
MAN  =	dictd.8

BINDIR =	${SBINDIR}

EGDIR ?=	${DATADIR}/examples/dictd # specific to pkgsrc

FILESDIR =	${EGDIR}
FILES    =	dictd1.conf dictd2.conf dictd3.conf dictd4.conf \
   dictd_complex.conf dictd_mime.conf dictd_virtual.conf dictd_site.txt

DPSRCS =	servparse.h servparse.c

USE_DBDIR ?=	${DATADIR}/dictd

.ifdef USE_PLUGIN
SRCS     +=	plugin.c
CPPFLAGS +=	-DUSE_PLUGIN=1
MKC_FEATURES +=	libdl
.endif

INFILES =	dictd.8
INTEXTS_REPLS+=	DICT_DICTIONARY_PATH ${USE_DBDIR:Q}

YHEADER =	1

CPPFLAGS +=	-I${SRCTOP}/dictdplugins/include -I${SRCDIR_libdictd} \
   -I${SRCDIR_dictd} -I${SRCDIR_libcommon}

CFLAGS.check +=	-Wno-error=unused-function \
   -Wno-error=unused-const-variable -Wno-error=format-truncation
CFLAGS.check +=	-erroff=E_STATEMENT_NOT_REACHED
CFLAGS.check +=	-Wno-error=deprecated-declarations # daemon(3) is considered deprecated

MKC_REQUIRE_FUNCLIBS +=	lst_create:maa deflate:z
MKC_CHECK_FUNCS2     += daemon:stdlib.h \
   initgroups:unistd.h getgroups:unistd.h

MKC_REQUIRE_FUNCLIBS +=	lst_create:maa bind:socket inet_ntop:nsl
MKC_CHECK_FUNCLIBS   +=	daemon
MKC_FEATURES         += efun strlcat

.include <mkc.configure.mk>

.if !${HAVE_FUNCLIB.daemon:U0}
SRCS +=			daemon3.c
.endif

dictd.o: servparse.h

.include <mkc.mk>
