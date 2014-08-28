#
# Copyright (c) 2012-2014 ACE Project
# $COPYRIGHT$
#

bin_SCRIPTS = acprun
acprun: acprun.in
	( cd $(top_builddir) && ./config.status --file=${subdir}/acprun:${subdir}/acprun.in ) \
	&& chmod 755 acprun
CLEANFILES = acprun
EXTRA_DIST = acprun.in

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

uninstall-local-always:
	rm -f $(DESTDIR)$(bindir)/acpgcc \
	$(DESTDIR)$(bindir)/acpc++ \
	$(DESTDIR)$(bindir)/acpcxx

install-exec-hook: install-exec-hook-always
uninstall-local: uninstall-local-always

