#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_builddir)/src/include -I$(top_srcdir)/src/include

noinst_PROGRAMS = \
		  acpbl_udp_test \
		  acpbl_udp_test2
noinst_SCRIPTS = \
		 test.sh

acpbl_udp_test_SOURCES = acpbl_udp_test.c acp.h
acpbl_udp_test_LDADD = $(top_builddir)/src/bl/udp/libacpbl_udp.la
acpbl_udp_test_DEPENDENCIES = $(acpbl_udp_test_LDADD)

acpbl_udp_test2_SOURCES = acpbl_udp_test2.c acp.h
acpbl_udp_test2_LDADD = $(top_builddir)/src/bl/udp/libacpbl_udp.la
acpbl_udp_test2_DEPENDENCIES = $(acpbl_udp_test2_LDADD)
