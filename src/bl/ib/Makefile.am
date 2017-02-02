#
# Copyright (c) 2012-2016 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_ib.la
libacpbl_ib_la_SOURCES = acpbl_ib.c acpbl.h acpbl_sync.h acpbl_input.c acpbl_input.h
libacpbl_ib_la_LDFLAGS = -version-info $(libacpbl_ib_version)
libacpbl_ib_la_LIBADD = -lpthread -libverbs

if HAVE_MPICC
lib_LTLIBRARIES += libacpbl_ib_mpi.la
libacpbl_ib_mpi_la_SOURCES = acpbl_ib.c acpbl.h acpbl_sync.h acpbl_input.c acpbl_input.h
libacpbl_ib_mpi_la_CPPFLAGS = $(AM_CPPFLAGS) -DMPIACP $(MPI_CPPFLAGS)
	libacpbl_ib_la_LDFLAGS = -version-info $(libacpbl_ib_version)
libacpbl_ib_mpi_la_LIBADD = -lpthread -libverbs
endif
