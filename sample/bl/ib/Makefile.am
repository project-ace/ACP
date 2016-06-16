#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_builddir)/src/include -I$(top_srcdir)/src/include
LDADD = \
	$(top_builddir)/src/bl/ib/libacpbl_ib.la \
	$(top_builddir)/src/ml/libacpml.la

noinst_PROGRAMS = \
	       acpbl \
	       acpbl_atomic \
	       acpbl_atomic8 \
	       acpbl_ohandle \
	       acpbl_rm \
	       acpbl_rr \
	       acpbl_rr2

#noinst_SCRIPTS = \
#	      run-acecls-exec.sh \
#	      run-acecls.sh \
#	      test-acecls.sh \
#	      test01.sh

acpbl_DEPENDENCIES = $(LDADD)
acpbl_SOURCES = acpbl_test.c acp.h

acpbl_atomic_DEPENDENCIES = $(LDADD)
acpbl_atomic_SOURCES = acpbl_test_atomic.c acp.h

acpbl_atomic8_DEPENDENCIES = $(LDADD)
acpbl_atomic8_SOURCES = acpbl_test_atomic8.c acp.h

acpbl_ohandle_DEPENDENCIES = $(LDADD)
acpbl_ohandle_SOURCES = acpbl_test_order_handle.c acp.h

acpbl_rm_DEPENDENCIES = $(LDADD)
acpbl_rm_SOURCES = acpbl_test_rm.c acp.h

acpbl_rr_DEPENDENCIES = $(LDADD)
acpbl_rr_SOURCES = acpbl_test_rr.c acp.h

acpbl_rr2_DEPENDENCIES = $(LDADD)
acpbl_rr2_SOURCES = acpbl_test_rr2.c acp.h
