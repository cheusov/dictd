LIB         =	dictdplugin_dbi
SRCS        =	dictdplugin_dbi.c

MKDLL       =	yes
MKSTATICLIB =	no

CPPFLAGS   +=	-I${SRCDIR_dictdplugins_include} -I${SRCDIR_libdictd} \
   -I${SRCDIR_libcommon}

MKC_REQUIRE_FUNCLIBS +=		dbi_conn_error:dbi

.include <mkc.mk>
