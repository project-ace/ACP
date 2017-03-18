#
# Copyright (c) 2012-2017 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_srcdir)/src/include

noinst_LIBRARIES = libacpbl_common.a

if HAVE_TOFU
libacpbl_common_a_CFLAGS = -V -Kfast,noswp,noeval -KPIC -Xg -DACPBL_TOFU -DFCC_SPARC64
endif
if HAVE_TOFU2
libacpbl_common_a_CFLAGS = -V -Kfast,noswp,noeval -KPIC -Xg -DACPBL_TOFU2 -DFCC_SPARC64
endif
libacpbl_common_a_SOURCES = acpbl_input.c acpbl_input.h
