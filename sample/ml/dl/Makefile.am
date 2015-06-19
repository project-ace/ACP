#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_builddir)/src/include -I$(top_srcdir)/src/include

udp_LDADD = \
	    $(top_builddir)/src/bl/udp/libacpbl_udp.la \
	    $(top_builddir)/src/ml/libacpml.la

ib_LDADD = \
	   $(top_builddir)/src/bl/ib/libacpbl_ib.la \
	   $(top_builddir)/src/ml/libacpml.la

noinst_PROGRAMS = \
	       acpdl_udp_test

if WITH_INFINIBAND
noinst_PROGRAMS += \
	       acpdl_ib_test
endif

acpdl_udp_test_LDADD = $(udp_LDADD)
acpdl_udp_test_DEPENDENCIES = $(acpdl_udp_test_LDADD)
acpdl_udp_test_SOURCES = acpdl_test.c acp.h

if WITH_INFINIBAND
acpdl_ib_test_LDADD = $(ib_LDADD)
acpdl_ib_test_DEPENDENCIES = $(acpdl_ib_test_LDADD)
acpdl_ib_test_SOURCES = acpdl_test.c acp.h
endif
