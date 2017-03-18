#
# Copyright (c) 2012-2017 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_tofu.la
libacpbl_tofu_la_DEPENDENCIES = $(top_builddir)/src/bl/common/libacpbl_common.a
libacpbl_tofu_la_SOURCES = acpbl_tofu.c acpbl_tofu.h acpbl_tofu_gma.c acpbl_tofu_gmm.c acpbl_tofu_thread.c acpbl_tofu_sys.c acpbl.h acpbl_sync.h
#libacpbl_tofu_la_CFLAGS = -V -Kfast,noswp,noeval -KPIC -Xg -DACPBL_TOFU -DFCC_SPARC64
libacpbl_tofu_la_LDFLAGS = -version-info $(libacpbl_tofu_version) -L$(top_builddir)/src/bl/common/
libacpbl_tofu_la_LIBADD = -lacpbl_common -lc -lpthread

if HAVE_MPICC
lib_LTLIBRARIES += libacpbl_tofu_mpi.la
libacpbl_tofu_mpi_la_DEPENDENCIES = $(top_builddir)/src/bl/common/libacpbl_common.a
libacpbl_tofu_mpi_la_SOURCES = acpbl_tofu.c acpbl_tofu.h acpbl_tofu_gma.c acpbl_tofu_gmm.c acpbl_tofu_thread.c acpbl_tofu_sys.c acpbl.h acpbl_sync.h
libacpbl_tofu_mpi_la_CPPFLAGS = $(AM_CPPFLAGS) -DMPIACP $(MPI_CPPFLAGS)
#libacpbl_tofu_mpi_la_CFLAGS = -V -Kfast,noswp,noeval -KPIC -Xg -DACPBL_TOFU -DFCC_SPARC64
libacpbl_tofu_mpi_la_LDFLAGS = -version-info $(libacpbl_tofu_version) -L$(top_builddir)/src/bl/common/
libacpbl_tofu_mpi_la_LIBADD = -lacpbl_common -lc -lpthread
endif
