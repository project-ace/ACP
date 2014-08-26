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
	       acpch_udp_test0 \
	       acpch_udp_test1 \
	       acpch_udp_test2 \
	       acpch_udp_test3 \
	       acpch_udp_test4

if WITH_INFINIBAND
noinst_PROGRAMS += \
	       acpch_ib_test0 \
	       acpch_ib_test1 \
	       acpch_ib_test2 \
	       acpch_ib_test3 \
	       acpch_ib_test4
endif

noinst_SCRIPTS = \
	      run2-test0.sh \
	      run2-test1.sh \
	      run2-test2.sh \
	      run2-test3.sh \
	      run2-test4.sh

acpch_udp_test0_LDADD = $(udp_LDADD)
acpch_udp_test0_DEPENDENCIES = $(acpch_udp_test0_LDADD)
acpch_udp_test0_SOURCES = acpch_test0.c acp.h

acpch_udp_test1_LDADD = $(udp_LDADD)
acpch_udp_test1_DEPENDENCIES = $(acpch_udp_test1_LDADD)
acpch_udp_test1_SOURCES = acpch_test1.c acp.h

acpch_udp_test2_LDADD = $(udp_LDADD)
acpch_udp_test2_DEPENDENCIES = $(acpch_udp_test2_LDADD)
acpch_udp_test2_SOURCES = acpch_test2.c acp.h

acpch_udp_test3_LDADD = $(udp_LDADD)
acpch_udp_test3_DEPENDENCIES = $(acpch_udp_test3_LDADD)
acpch_udp_test3_SOURCES = acpch_test3.c acp.h

acpch_udp_test4_LDADD = $(udp_LDADD)
acpch_udp_test4_DEPENDENCIES = $(acpch_udp_test4_LDADD)
acpch_udp_test4_SOURCES = acpch_test4.c acp.h

if WITH_INFINIBAND
acpch_ib_test0_LDADD = $(ib_LDADD)
acpch_ib_test0_DEPENDENCIES = $(acpch_ib_test0_LDADD)
acpch_ib_test0_SOURCES = acpch_test0.c acp.h

acpch_ib_test1_LDADD = $(ib_LDADD)
acpch_ib_test1_DEPENDENCIES = $(acpch_ib_test1_LDADD)
acpch_ib_test1_SOURCES = acpch_test1.c acp.h

acpch_ib_test2_LDADD = $(ib_LDADD)
acpch_ib_test2_DEPENDENCIES = $(acpch_ib_test2_LDADD)
acpch_ib_test2_SOURCES = acpch_test2.c acp.h

acpch_ib_test3_LDADD = $(ib_LDADD)
acpch_ib_test3_DEPENDENCIES = $(acpch_ib_test3_LDADD)
acpch_ib_test3_SOURCES = acpch_test3.c acp.h

acpch_ib_test4_LDADD = $(ib_LDADD)
acpch_ib_test4_DEPENDENCIES = $(acpch_ib_test4_LDADD)
acpch_ib_test4_SOURCES = acpch_test4.c acp.h
endif
