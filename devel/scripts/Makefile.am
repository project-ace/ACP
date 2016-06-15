#
# Copyright (c) 2012-2016 ACE Project
# $COPYRIGHT$
#

bin_SCRIPTS = acprun
acprun: acprun.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/acprun:${subdir}/acprun.in ) \
	&& chmod 755 acprun
CLEANFILES = acprun
EXTRA_DIST = acprun.in

bin_SCRIPTS += macprun
macprun: macprun.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/macprun:${subdir}/macprun.in ) \
	&& chmod 755 macprun
CLEANFILES = macprun
EXTRA_DIST = macprun.in

bin_SCRIPTS += acpcc
acpcc: acpcc.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/acpcc:${subdir}/acpcc.in ) \
	&& chmod 755 acpcc
CLEANFILES += acpcc
EXTRA_DIST += acpcc.in

install-exec-hook-always:
	test -z "$(bindir)" || $(MKDIR_P) "$(DESTDIR)$(bindir)"
	(cd $(DESTDIR)$(bindir); rm -f acpgcc; $(LN_S) acpcc acpgcc)
	(cd $(DESTDIR)$(bindir); rm -f acpc++; $(LN_S) acpcc acpc++)
	(cd $(DESTDIR)$(bindir); rm -f acpcxx; $(LN_S) acpcc acpcxx)
	(cd $(DESTDIR)$(bindir); rm -f macpcc; $(LN_S) acpcc macpcc)
	(cd $(DESTDIR)$(bindir); rm -f macpgcc; $(LN_S) acpcc macpgcc)
	(cd $(DESTDIR)$(bindir); rm -f macpc++; $(LN_S) acpcc macpc++)
	(cd $(DESTDIR)$(bindir); rm -f macpg++; $(LN_S) acpcc macpg++)
	(cd $(DESTDIR)$(bindir); rm -f macpcxx; $(LN_S) acpcc macpcxx)
	(cd $(DESTDIR)$(bindir); rm -f macpgxx; $(LN_S) acpcc macpgxx)
	(cd $(DESTDIR)$(bindir); rm -f macpCC; $(LN_S) acpcc macpCC)

uninstall-local-always:
	rm -f $(DESTDIR)$(bindir)/acpgcc \
	$(DESTDIR)$(bindir)/acpc++ \
	$(DESTDIR)$(bindir)/acpcxx \
	$(DESTDIR)$(bindir)/macpcc \
	$(DESTDIR)$(bindir)/macpgcc \
	$(DESTDIR)$(bindir)/macpc++ \
	$(DESTDIR)$(bindir)/macpg++ \
	$(DESTDIR)$(bindir)/macpcxx \
	$(DESTDIR)$(bindir)/macpgxx \
	$(DESTDIR)$(bindir)/macpCC

install-exec-hook: install-exec-hook-always
uninstall-local: uninstall-local-always

include $(top_srcdir)/config/versions

man_MANS = acpcc.1 acpgcc.1 acpc++.1 acpcxx.1

acpcc.1: generic_wrapper.1in
	rm -f acpcc.1
	sed -e 's/#COMMAND#/acpcc/g' \
	  -e 's/#PACKAGE_NAME#/@PACKAGE_NAME@/g' \
	  -e 's/#PACKAGE_VERSION#/@PACKAGE_VERSION@/g' \
	  -e "s/#ACP_DATE#/$(ACP_DATE)/g" \
	  -e 's/#PROJECT#/ACP/g' \
	  -e 's/#PROJECT_SHORT#/ACP/g' \
	  -e 's/#LANGUAGE#/C/g' \
	  < generic_wrapper.1in > acpcc.1
CLEANFILES += acpcc.1

acpgcc.1: generic_wrapper.1in
	rm -f acpgcc.1
	sed -e 's/#COMMAND#/acpcc/g' \
	  -e 's/#PACKAGE_NAME#/@PACKAGE_NAME@/g' \
	  -e 's/#PACKAGE_VERSION#/@PACKAGE_VERSION@/g' \
	  -e "s/#ACP_DATE#/$(ACP_DATE)/g" \
	  -e 's/#PROJECT#/ACP/g' \
	  -e 's/#PROJECT_SHORT#/ACP/g' \
	  -e 's/#LANGUAGE#/C/g' \
	  < generic_wrapper.1in > acpgcc.1
CLEANFILES += acpgcc.1

acpc++.1: generic_wrapper.1in
	rm -f acpc++.1
	sed -e 's/#COMMAND#/acpcc/g' \
	  -e 's/#PACKAGE_NAME#/@PACKAGE_NAME@/g' \
	  -e 's/#PACKAGE_VERSION#/@PACKAGE_VERSION@/g' \
	  -e "s/#ACP_DATE#/$(ACP_DATE)/g" \
	  -e 's/#PROJECT#/ACP/g' \
	  -e 's/#PROJECT_SHORT#/ACP/g' \
	  -e 's/#LANGUAGE#/C++/g' \
	  < generic_wrapper.1in > acpc++.1
CLEANFILES += acpc++.1

acpcxx.1: generic_wrapper.1in
	rm -f acpcxx.1
	sed -e 's/#COMMAND#/acpcc/g' \
	  -e 's/#PACKAGE_NAME#/@PACKAGE_NAME@/g' \
	  -e 's/#PACKAGE_VERSION#/@PACKAGE_VERSION@/g' \
	  -e "s/#ACP_DATE#/$(ACP_DATE)/g" \
	  -e 's/#PROJECT#/ACP/g' \
	  -e 's/#PROJECT_SHORT#/ACP/g' \
	  -e 's/#LANGUAGE#/C++/g' \
	  < generic_wrapper.1in > acpcxx.1
CLEANFILES += acpcxx.1

man_MANS += acprun.1
acprun.1: acprun.1in
	rm -f acprun.1
	sed -e 's/#COMMAND#/acpcc/g' \
	  -e 's/#PACKAGE_NAME#/@PACKAGE_NAME@/g' \
	  -e 's/#PACKAGE_VERSION#/@PACKAGE_VERSION@/g' \
	  -e "s/#ACP_DATE#/$(ACP_DATE)/g" \
	  -e 's/#PROJECT#/ACP/g' \
	  -e 's/#PROJECT_SHORT#/ACP/g' \
	  -e 's/#LANGUAGE#/C/g' \
	  < acprun.1in > acprun.1
CLEANFILES += acprun.1

distclean-local:
	rm -f *.1
