softgredconfdir = $(confdir)
softgredconf_DATA = etc/softgred/softgred.conf
etc/softgred/softgred.conf: $(srcdir)/etc/softgred/softgred.conf.in
	$(MKDIR_P) etc/softgred
	$(SED) -e 's|@SOFTGRED_PIDDIR@|$(piddir)|g' $< > $@
if WEB
webinterfacedir = $(pkgdatadir)/webinterface
webinterface_DATA = webinterface/index.html webinterface/dashboard.css \
    webinterface/dashboard.js webinterface/api.php
endif
EXTRA_DIST = etc/softgred/softgred.conf.in softgred.service \
    webinterface/index.html webinterface/dashboard.css webinterface/dashboard.js webinterface/api.php
CLEANFILES = etc/softgred/softgred.conf
