#
# Copyright (c) 2012-2017 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I./tofu2 -I$(top_srcdir)/src/include

lib_LTLIBRARIES = libacpbl_tofu2.la
libacpbl_tofu2_la_DEPENDENCIES = $(top_builddir)/src/bl/common/libacpbl_common.a
libacpbl_tofu2_la_SOURCES = acpbl_tofu2.c acpbl_tofu2.h acpbl_tofu2_sys.h acpbl_tofu2_gma.c acpbl_tofu2_gmm.c acpbl_tofu2_thread.c acpbl.h acpbl_sync.h
#libacpbl_tofu2_la_CFLAGS = -V -Kfast,noswp,noeval -KPIC -Xg -DACPBL_TOFU2 -DFCC_SPARC64
libacpbl_tofu2_la_LDFLAGS = -version-info $(libacpbl_tofu2_version) -L$(top_builddir)/src/bl/common/
libacpbl_tofu2_la_LIBADD = -lacpbl_tofu2_sys -lacpbl_common -lc -lpthread

if HAVE_MPICC
lib_LTLIBRARIES += libacpbl_tofu2_mpi.la
libacpbl_tofu2_mpi_la_DEPENDENCIES = $(top_builddir)/src/bl/common/libacpbl_common.a
libacpbl_tofu2_mpi_la_SOURCES = acpbl_tofu2.c acpbl_tofu2.h acpbl_tofu2_sys.h acpbl_tofu2_gma.c acpbl_tofu2_gmm.c acpbl_tofu2_thread.c acpbl.h acpbl_sync.h
libacpbl_tofu2_mpi_la_CPPFLAGS = $(AM_CPPFLAGS) -DMPIACP $(MPI_CPPFLAGS)
#libacpbl_tofu2_mpi_la_CFLAGS = -V -Kfast,noswp,noeval -KPIC -Xg -DACPBL_TOFU2 -DFCC_SPARC64
libacpbl_tofu2_mpi_la_LDFLAGS = -version-info $(libacpbl_tofu2_version) -L$(top_builddir)/src/bl/common/
libacpbl_tofu2_mpi_la_LIBADD = -lacpbl_tofu2_sys -lacpbl_common -lc -lpthread
endif
