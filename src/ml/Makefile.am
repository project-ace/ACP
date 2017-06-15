#
# Copyright (c) 2012-2017 ACE Project
# $COPYRIGHT$
#

SUBDIRS = cl dl

lib_LTLIBRARIES = libacpml.la libacpmallochook.la
libacpml_la_SOURCES =
libacpml_la_LDFLAGS = -version-info $(libacpml_version)
libacpml_la_LIBADD = \
		     cl/libacpml_cl.la \
		     dl/libacpml_dl.la

libacpmallochook_la_SOURCES = acp_malloc_hook.c
libacpmallochook_la_LDFLAGS = -version-info $(libacpml_version)
libacpmallochook_la_LIBADD = -ldl
