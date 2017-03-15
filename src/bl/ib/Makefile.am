#
# Copyright (c) 2012-2017 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_ib.la
libacpbl_ib_la_DEPENDENCIES = $(top_builddir)/src/bl/common/libacpbl_common.a
libacpbl_ib_la_SOURCES = acpbl_ib.c acpbl.h acpbl_sync.h
libacpbl_ib_la_LDFLAGS = -version-info $(libacpbl_ib_version) -L$(top_builddir)/src/bl/common/
libacpbl_ib_la_LIBADD = -lacpbl_common -lpthread -libverbs

if HAVE_MPICC
lib_LTLIBRARIES += libacpbl_ib_mpi.la
libacpbl_ib_mpi_la_DEPENDENCIES = $(top_builddir)/src/bl/common/libacpbl_common.a
libacpbl_ib_mpi_la_SOURCES = acpbl_ib.c acpbl.h acpbl_sync.h
libacpbl_ib_mpi_la_CPPFLAGS = $(AM_CPPFLAGS) -DMPIACP $(MPI_CPPFLAGS)
libacpbl_ib_mpi_la_LDFLAGS = -version-info $(libacpbl_ib_version) -L$(top_builddir)/src/bl/common/
libacpbl_ib_mpi_la_LIBADD = -lacpbl_common -lpthread -libverbs
endif
