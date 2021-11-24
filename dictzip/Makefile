PROG =		dictzip
MAN  =		dictzip.1

INFILES =	dictzip.1

CPPFLAGS +=	-I${SRCDIR_dictdplugins_include:U.} -I${SRCDIR_libdictd}

MKC_REQUIRE_FUNCLIBS +=	lst_create:maa deflate:z

.include <mkc.mk>
