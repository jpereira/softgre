softgredconfdir = $(confdir)
softgredconf_DATA = etc/softgred/softgred.conf
etc/softgred/softgred.conf: $(srcdir)/etc/softgred/softgred.conf.in
	$(MKDIR_P) etc/softgred
	$(SED) -e 's|@SOFTGRED_PIDDIR@|$(piddir)|g' $< > $@
EXTRA_DIST = etc/softgred/softgred.conf.in
CLEANFILES = etc/softgred/softgred.conf
