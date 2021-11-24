PROG =		dictfmt
MAN  =		dictfmt.1 dictunformat.1 dictfmt_index2word.1 \
		dictfmt_index2suffix.1
SCRIPTS =	${INSCRIPTS}

INFILES =	dictfmt.1 dictfmt_index2suffix.1 \
		dictfmt_index2word.1 dictunformat.1
INSCRIPTS =	dictfmt_index2suffix dictfmt_index2word dictunformat

CPPFLAGS +=	-I${SRCDIR_libcommon}

MKC_FEATURES +=	strlcat

MKC_REQUIRE_FUNCLIBS +=	lst_create:maa

.include <mkc.mk>
