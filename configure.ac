dnl Process this file with autoconf to produce a configure script for dict.
dnl
dnl Created: Wed Apr 24 14:11:05 1996 by faith@dict.org
dnl Copyright 1996-1999, 2001-2002 Rickard E. Faith (faith@dict.org)
dnl Copyright 2002-2008 Aleksey Cheusov (vle@gmx.net)
dnl
dnl This program is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by the
dnl Free Software Foundation; either version 1, or (at your option) any
dnl later version.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program; if not, write to the Free Software Foundation, Inc.,
dnl 675 Mass Ave, Cambridge, MA 02139, USA.
dnl
dnl Flag conventions:
dnl     CFLAGS and LDFLAGS should be settable on the make commandline
dnl             for optimization and stripping.
dnl     LIBOBJS is an automatically-generated list of extra objects we need


define(PKG_VERSION, 1.13.3)


AC_PREREQ([2.72])
AC_REVISION($Revision: 1.144 $)

AC_INIT([dict],[PKG_VERSION],[dict-beta@dict.org])

AC_CONFIG_SRCDIR([dictd.c])
AC_CONFIG_HEADERS([config.h])

echo Configuring for dict
echo .

DICT_VERSION=PKG_VERSION

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX

AC_SEARCH_LIBS([strerror],[cposix])

REALCC="$CC"
if test "$CC" = gcc; then
    CFLAGS="$CFLAGS -Wall"
    CFLAGS="$CFLAGS -Wwrite-strings -Wcast-align -Wmissing-prototypes"
    CFLAGS="$CFLAGS -Wshadow -Wnested-externs -Waggregate-return"
fi

AC_ARG_ENABLE([plugin],[  --disable-plugin        without plugin support])
if test "x${enableval}" != xno; then
    AC_CHECK_LIB(c, dlopen,
        [USE_PLUGIN=2],
        AC_CHECK_LIB(dl, dlopen,
            [LIBS="$LIBS -ldl"; USE_PLUGIN=2],
            AC_CHECK_LIB(ltdl,lt_dlopen,
                [LIBS="$LIBS -lltdl"; USE_PLUGIN=1])))
fi

dnl Checks for programs.
echo Checking for programs

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_MAKE_SET
AC_PROG_YACC
AC_PROG_LEX(yywrap)

AC_CHECK_PROG(LIBTOOL,libtool,libtool)

AC_CHECK_PROGS(NROFF,gnroff nroff)
AC_CHECK_PROGS(TROFF,groff troff)
AC_CHECK_PROGS(COL,col cat)
AC_CHECK_PROGS(EXPAND,expand cat)

echo .
echo Checking for libraries

AC_CHECK_LIB(maa,maa_shutdown, [LIBMAA=-lmaa],
	[AC_MSG_ERROR([Libmaa not found])])

AC_ARG_WITH(plugin-dbi,
[  --with-plugin-dbi       build libdbi plugin (plugin support should be enabled)],
[
if test "x${withval}" != xno; then
       AC_CHECK_LIB(dbi,  dbi_shutdown,
           [PLUGINS="$PLUGINS dictdplugin_dbi.la"],
           [AC_MSG_ERROR([libdbi not found, cannot continue])])
       AC_CHECK_HEADER([dbi/dbi.h],,
           [AC_MSG_ERROR([dbi/dbi.h not found, cannot continue])])
fi
])

AC_ARG_WITH(plugin-judy,
[  --with-plugin-judy      build judy plugin (plugin support should be enabled)],
[
if test "x${withval}" != xno; then
       AC_CHECK_LIB(Judy, JudySLNext,
           [PLUGINS="$PLUGINS dictdplugin_judy.la"],
           [AC_MSG_ERROR([libJudy not found, cannot continue])])
       AC_CHECK_HEADER([Judy.h],,
           [AC_MSG_ERROR([Judy.h not found, cannot continue])])
fi
])

AC_CHECK_LIB(c,getnameinfo,
	[true],
	[AC_CHECK_LIB(socket,getnameinfo)])

AC_CHECK_LIB(c,inet_ntop,
	[true],
	[AC_CHECK_LIB(nsl,inet_ntop)])

echo .
echo Checking for header files

AC_PROG_EGREP

if test "x${USE_PLUGIN}" = "x2"; then
   # We use native dlopen
   AC_CHECK_HEADERS(dlfcn.h, [CFLAGS="$CFLAGS -DUSE_PLUGIN"], [USE_PLUGIN=])
fi
if test "x${USE_PLUGIN}" = "x1"; then
   # We use libltdl
   AC_CHECK_HEADERS(ltdl.h, [CFLAGS="$CFLAGS -DUSE_PLUGIN"], [USE_PLUGIN=])
fi

AC_CHECK_HEADER([zlib.h], ,
    [AC_MSG_ERROR([zlib.h is required for dictd])])

echo .
echo Checking for system constants

AC_C_CONST
AC_C_INLINE

echo .
echo Checking for library functions

AC_CHECK_FUNCS(dlopen)

AC_CHECK_FUNC(daemon,, [AC_LIBOBJ([daemon3])])

AC_FUNC_ALLOCA

AC_REPLACE_FUNCS(setenv)
AC_CHECK_FUNCS(putenv)

AC_LIBOBJ([wcrtomb])
AC_LIBOBJ([wctomb])
AC_LIBOBJ([mbrlen])
AC_LIBOBJ([mbrtowc])
AC_LIBOBJ([mbstowcs])
AC_LIBOBJ([mbtowc])
AC_LIBOBJ([iswspace])
AC_LIBOBJ([iswalnum])
AC_LIBOBJ([towlower])
#	AC_LIBOBJ([wcwidth])

AC_REPLACE_FUNCS(strlcpy strlcat)

echo .
echo Making output files

CC="$REALCC"

test "_$exec_prefix" = _NONE && exec_prefix='${prefix}'

eval3 () { eval echo $(eval echo $(eval echo $@)); }

SYSCONFDIR=`eval3 $sysconfdir`
BINDIR=`eval3 $bindir`
SBINDIR=`eval3 $sbindir`
LIBEXECDIR=`eval3 $libexecdir`
DATADIR=`eval3 $datadir`

AC_SUBST(DICT_VERSION)
AC_SUBST(USE_PLUGIN)
AC_SUBST(EXEEXT)
AC_SUBST(allsubdirs)
AC_SUBST(LIBTOOL)

AC_SUBST(PLUGINS)

AC_SUBST(SYSCONFDIR)
AC_SUBST(BINDIR)
AC_SUBST(SBINDIR)
AC_SUBST(LIBEXECDIR)
AC_SUBST(DATADIR)

AC_SUBST(LIBMAA)
AC_SUBST(srcdir)

AC_CONFIG_FILES([Makefile doc/Makefile])
AC_CONFIG_FILES([dictdplugin-config], [chmod +x dictdplugin-config])
AC_CONFIG_FILES([dictl], [chmod +x dictl])
AC_CONFIG_FILES([colorit], [chmod +x colorit])
AC_CONFIG_FILES([dictunformat], [chmod +x dictunformat])
AC_CONFIG_FILES([dictfmt_index2suffix], [chmod +x dictfmt_index2suffix])
AC_CONFIG_FILES([dictfmt_index2word], [chmod +x dictfmt_index2word])
AC_CONFIG_FILES([examples/dictd_popen.conf])

AC_CONFIG_FILES(colorit.1)
AC_CONFIG_FILES(dict.1)
AC_CONFIG_FILES(dictfmt.1)
AC_CONFIG_FILES(dictfmt_index2suffix.1)
AC_CONFIG_FILES(dictfmt_index2word.1)
AC_CONFIG_FILES(dictl.1)
AC_CONFIG_FILES(dict_lookup.1)
AC_CONFIG_FILES(dictunformat.1)
AC_CONFIG_FILES(dictzip.1)
AC_CONFIG_FILES(dictd.8)

AC_CONFIG_FILES(test/dictd_test)
AC_CONFIG_FILES(test/dictzip_test)

AC_OUTPUT

printf '\n\n'

# PLUGIN SUPPORT
printf "plugin support:  "
if test "_$USE_PLUGIN" = "_"; then
  echo disabled
else
  case "$USE_PLUGIN" in
    1) echo libltdl;;
    2) echo native
  esac

  printf " enabled plugins: %s\n" "$PLUGINS"
fi

printf '\n\n'

touch stamp-h.in
touch stamp-h
#echo .
#echo Done
