#
# Copyright (c) 2012-2016 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_udp.la
libacpbl_udp_la_SOURCES = acpbl_udp.c acpbl_udp_gmm.c acpbl_udp_gma.c ../common/acpbl_input.c \
	acpbl.h acpbl_sync.h acpbl_udp.h acpbl_udp_gmm.h acpbl_udp_gma.h ../common/acpbl_input.h ../common/acpbl_default_input.h
libacpbl_udp_la_LDFLAGS = -version-info $(libacpbl_udp_version)
libacpbl_udp_la_LIBADD = -lpthread

if HAVE_MPICC
lib_LTLIBRARIES += libacpbl_udp_mpi.la
libacpbl_udp_mpi_la_SOURCES = acpbl_udp.c acpbl_udp_gmm.c acpbl_udp_gma.c ../common/acpbl_input.c \
	acpbl.h acpbl_sync.h acpbl_udp.h acpbl_udp_gmm.h acpbl_udp_gma.h ../common/acpbl_input.h ../common/acpbl_default_input.h
libacpbl_udp_mpi_la_CPPFLAGS = $(AM_CPPFLAGS) -DMPIACP $(MPI_CPPFLAGS)
libacpbl_udp_mpi_la_LDFLAGS = -version-info $(libacpbl_udp_version)
libacpbl_udp_mpi_la_LIBADD = -lpthread
endif
