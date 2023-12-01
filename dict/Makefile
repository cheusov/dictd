PROG =	dict
SRCS =	dict.c net_client.c clientscan.l clientparse.y
MAN  =	dict.1

CONFIG_FILES =		dict.conf dict2.conf

EGDIR ?=		${DATADIR}/examples/dict # specific to pkgsrc

INFILES +=		dict.1

FILES +=		${CONFIG_FILES}
FILESDIR_dict.conf =	${EGDIR}
FILESDIR_dict2.conf =	${EGDIR}

YHEADER =		1

DPSRCS =		clientparse.h clientparse.c
clientscan.o: 		clientparse.h

CPPFLAGS +=	-I${SRCDIR_dictdplugins_include:U.} -I${SRCDIR_libdictd} \
   -I${SRCDIR_dictd} -I${SRCDIR_libcommon} -I${.CURDIR}

CFLAGS.check +=	-Wno-error=unused-function \
   -Wno-error=format-truncation -Wno-error=unused-const-variable

MKC_REQUIRE_FUNCLIBS +=	lst_create:maa bind:socket inet_ntop:nsl

.include <mkc.mk>
