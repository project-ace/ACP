#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Copyright (c) 2014      Kyushu University 
# Copyright (c) 2014      Institute of Systems, Information Technologies 
#                         and Nanotechnologies 2014
# Copyright (c) 2014      FUJITSU LIMITED
#
# This software is released under the BSD License, see LICENSE. 

AC_PREREQ(2.60)
AC_INIT(ACP, 1.0.0, [http://ace.cc.kyushu-u.ac.jp])

AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(config)

AM_INIT_AUTOMAKE([1.14 -Wall])
AM_MAINTAINER_MODE

AC_CONFIG_SRCDIR([src/include/acp.h])
AC_CONFIG_HEADERS([config/config.h])

AC_CANONICAL_HOST

# Checks for programs.
AM_PROG_AR
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S

AC_PATH_PROG([RUBY], ruby)
AC_SUBST([RUBY])

# Check to see if building shared libraries
libtool_build_shared_libs='no'
if test "$enable_shared" = 'yes'; then
	libtool_build_shared_libs='yes'
fi 
# Check to see if building static libraries
libtool_build_static_libs='no'
if test "$enable_static" = 'yes'; then
	libtool_build_static_libs='yes'
fi

AM_CONDITIONAL(WITH_SHARED_LIBS, test "${libtool_build_shared_libs}" = 'yes')

# Checks for libraries.
AC_CHECK_LIB([socket], [socket])
ibverbs_libs='no'
AC_CHECK_LIB([ibverbs], [ibv_get_device_list], [
	      ibverbs_libs='yes'
	      ])
AM_CONDITIONAL(WITH_INFINIBAND, test "${ibverbs_libs}" = 'yes')

AC_ARG_WITH(ndev, AC_HELP_STRING([--with-ndev=NAME],
	    [the priority order of default network device [ib, udp]]))

if test x"$with_ndev" = x'udp'; then
	AC_SUBST(NETDEV,'UDP')
elif test "${ibverbs_libs}" = 'yes'; then
	AC_SUBST(NETDEV,'IB')
else
	AC_SUBST(NETDEV,'UDP')
fi

AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h inttypes.h netdb.h netinet/in.h stdint.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime gethostbyname inet_ntoa memset socket strdup strtol])

# Get shared library version numbers
. config/versions
AC_SUBST(libacpbl_udp_version)
AC_SUBST(libacpbl_ib_version)
AC_SUBST(libacpml_version)

top_acp_builddir="`pwd`"
AC_SUBST(top_acp_builddir)
cd "$srcdir"
top_acp_srcdir="`pwd`"
AC_SUBST(top_acp_srcdir)
cd "$top_acp_builddir"

AC_MSG_NOTICE([builddir: $top_acp_builddir])
AC_MSG_NOTICE([srcdir: $top_acp_srcdir])
if test "$top_acp_builddir" != "$top_acp_srcdir"; then
    AC_MSG_NOTICE([Detected VPATH build])
fi

AC_CONFIG_FILES([Makefile
                 sample/Makefile
                 scripts/Makefile
                 src/Makefile
                 src/bl/Makefile
                 src/bl/ib/Makefile
                 src/bl/udp/Makefile
                 src/include/Makefile
                 src/ml/Makefile
                 src/ml/cl/Makefile
                 src/ml/dl/Makefile
                 test/Makefile
                 test/bl/Makefile
                 test/bl/ib/Makefile
                 test/bl/udp/Makefile
                 test/ml/Makefile
                 test/ml/dl/Makefile
                 test/ml/ch/Makefile])
AC_OUTPUT
