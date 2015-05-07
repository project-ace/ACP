#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_udp.la
libacpbl_udp_la_SOURCES = acpbl_udp.c acpbl_udp_gmm.c acpbl_udp_gma.c \
	acpbl.h acpbl_sync.h acpbl_udp.h acpbl_udp_gmm.h acpbl_udp_gma.h
libacpbl_udp_la_LDFLAGS = -version-info $(libacpbl_udp_version)
libacpbl_udp_la_LIBADD = -lpthread
