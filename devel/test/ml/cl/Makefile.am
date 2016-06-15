#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

AM_CPPFLAGS = -I$(top_builddir)/src/include -I$(top_srcdir)/src/include

udp_LDADD = \
	    $(top_builddir)/src/bl/udp/libacpbl_udp.la \
	    $(top_builddir)/src/ml/libacpml.la

ib_LDADD = \
	   $(top_builddir)/src/bl/ib/libacpbl_ib.la \
	   $(top_builddir)/src/ml/libacpml.la

noinst_PROGRAMS = \
	       testch01_udp \
	       testch02_udp \
	       testch03_udp \
	       testch04_udp \
	       testch05_udp \
	       testch06_udp

if WITH_INFINIBAND
noinst_PROGRAMS += \
	       testch01_ib \
	       testch02_ib \
	       testch03_ib \
	       testch04_ib \
	       testch05_ib \
	       testch06_ib
endif

noinst_SCRIPTS = testch01.sh
testch01.sh: testch.sh.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/testch01.sh:${subdir}/testch.sh.in ) \
	&& sed -i -e 's/@command@/testch01/g' testch01.sh
	chmod 755 testch01.sh
CLEANFILES = testch01.sh
EXTRA_DIST = testch.sh.in

noinst_SCRIPTS += testch02.sh
testch02.sh: testch.sh.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/testch02.sh:${subdir}/testch.sh.in ) \
	&& sed -i -e 's/@command@/testch02/g' testch02.sh
	chmod 755 testch02.sh
CLEANFILES += testch02.sh

noinst_SCRIPTS += testch03.sh
testch03.sh: testch.sh.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/testch03.sh:${subdir}/testch.sh.in ) \
	&& sed -i -e 's/@command@/testch03/g' testch03.sh
	chmod 755 testch03.sh
CLEANFILES += testch03.sh

noinst_SCRIPTS += testch04.sh
testch04.sh: testch.sh.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/testch04.sh:${subdir}/testch.sh.in ) \
	&& sed -i -e 's/@command@/testch04/g' testch04.sh
	chmod 755 testch04.sh
CLEANFILES += testch04.sh

noinst_SCRIPTS += testch05.sh
testch05.sh: testch.sh.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/testch05.sh:${subdir}/testch.sh.in ) \
	&& sed -i -e 's/@command@/testch05/g' testch05.sh
	chmod 755 testch05.sh
CLEANFILES += testch05.sh

noinst_SCRIPTS += testch06.sh
testch06.sh: testch.sh.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/testch06.sh:${subdir}/testch.sh.in ) \
	&& sed -i -e 's/@command@/testch06/g' testch06.sh
	chmod 755 testch06.sh
CLEANFILES += testch06.sh

testch01_udp_LDADD = $(udp_LDADD)
testch01_udp_DEPENDENCIES = $(testch01_udp_LDADD)
testch01_udp_SOURCES = testch01.c acp.h

testch02_udp_LDADD = $(udp_LDADD)
testch02_udp_DEPENDENCIES = $(testch02_udp_LDADD)
testch02_udp_SOURCES = testch02.c acp.h

testch03_udp_LDADD = $(udp_LDADD)
testch03_udp_DEPENDENCIES = $(testch03_udp_LDADD)
testch03_udp_SOURCES = testch03.c acp.h

testch04_udp_LDADD = $(udp_LDADD)
testch04_udp_DEPENDENCIES = $(testch04_udp_LDADD)
testch04_udp_SOURCES = testch04.c acp.h

testch05_udp_LDADD = $(udp_LDADD)
testch05_udp_DEPENDENCIES = $(testch05_udp_LDADD)
testch05_udp_SOURCES = testch05.c acp.h

testch06_udp_LDADD = $(udp_LDADD)
testch06_udp_DEPENDENCIES = $(testch06_udp_LDADD)
testch06_udp_SOURCES = testch06.c acp.h

if WITH_INFINIBAND
testch01_ib_LDADD = $(ib_LDADD)
testch01_ib_DEPENDENCIES = $(testch01_ib_LDADD)
testch01_ib_SOURCES = testch01.c acp.h

testch02_ib_LDADD = $(ib_LDADD)
testch02_ib_DEPENDENCIES = $(testch02_ib_LDADD)
testch02_ib_SOURCES = testch02.c acp.h

testch03_ib_LDADD = $(ib_LDADD)
testch03_ib_DEPENDENCIES = $(testch03_ib_LDADD)
testch03_ib_SOURCES = testch03.c acp.h

testch04_ib_LDADD = $(ib_LDADD)
testch04_ib_DEPENDENCIES = $(testch04_ib_LDADD)
testch04_ib_SOURCES = testch04.c acp.h

testch05_ib_LDADD = $(ib_LDADD)
testch05_ib_DEPENDENCIES = $(testch05_ib_LDADD)
testch05_ib_SOURCES = testch05.c acp.h

testch06_ib_LDADD = $(ib_LDADD)
testch06_ib_DEPENDENCIES = $(testch06_ib_LDADD)
testch06_ib_SOURCES = testch06.c acp.h
endif
