LIB         =	dictdplugin_judy
SRCS        =	dictdplugin_judy.c

MKDLL       =	yes
MKSTATICLIB =	no

CPPFLAGS   +=	-I${SRCDIR_dictdplugins_include} -I${SRCDIR_libdictd} \
   -I${SRCDIR_libcommon}

MKC_REQUIRE_FUNCLIBS +=		JudyLIns:Judy

.include <mkc.mk>
