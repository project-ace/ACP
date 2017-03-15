#
# Copyright (c) 2012-2017 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_builddir)/src/include -I$(top_srcdir)/src/include
LDADD = \
	$(top_builddir)/src/bl/ib/libacpbl_tofu.la \
	$(top_builddir)/src/ml/libacpml.la

noinst_PROGRAMS = \
	       atomic \
	       inifini \
	       remote2remote

noinst_SCRIPTS = \
	      run_127nodes.sh \
	      run_128nodes.sh \
	      run_2nodes.sh \
	      run_3nodes.sh

atomic_DEPENDENCIES = $(LDADD)
atomic_SOURCES = atomic.c acp.h

inifini_DEPENDENCIES = $(LDADD)
inifini_SOURCES = inifini.c acp.h

remote2remote_DEPENDENCIES = $(LDADD)
remote2remote_SOURCES = remote2remote.c acp.h
