#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_ib.la
libacpbl_ib_la_SOURCES = acpbl_ib.c acpbl.h acpbl_sync.h
libacpbl_ib_la_LDFLAGS = -version-info $(libacpbl_ib_version)
libacpbl_ib_la_LIBADD = -lpthread -libverbs

