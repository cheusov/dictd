LIBDEPS +=	libcommon:libdictd
LIBDEPS +=	libcommon:dict
LIBDEPS +=	libcommon:dictfmt
LIBDEPS +=	libdictd:dictd libcommon:dictd
LIBDEPS +=	libdictd:dictzip

.ifdef USE_PLUGIN
SUBPRJ  +=	dictdplugins/include:libdictd
SUBPRJ  +=	dictdplugins/dictdplugin_judy dictdplugins/dictdplugin_dbi
SUBPRJ_DFLT +=	dictdplugins/dictdplugin_judy dictdplugins/dictdplugin_dbi
.endif
SUBPRJ  +=	colorit dictl dict_lookup

SUBPRJ_DFLT +=	dict dictd dictfmt dictzip colorit dictl dict_lookup

MKC_REQD =	0.38.2

test:
	@set -e; cd ${.CURDIR}/test; \
	PATH=${OBJDIR_dictd}:${OBJDIR_dict}:${OBJDIR_dictzip}:${OBJDIR_dictfmt}:$${PATH}; \
	export PATH; \
	sh ./dictd_test.in; \
	sh ./dictzip_test.in

CLEANFILES +=	test/_* test/db.expect.cyrillic_1.index_suffix \
   test/dictd_cyrillic_4_res.expected.txt test/input.cyrillic_4.txt \
   test/testdb.hello.txt.dict test/testdb.hello.txt.index test/log.txt

.include "use.mk"
.include "help.mk"
.include <mkc.mk>

.export SRCTOP OBJDIR_dictzip OBJDIR_dictfmt OBJDIR_dict OBJDIR_dictd
