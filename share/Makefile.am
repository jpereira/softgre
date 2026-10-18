softgredconfdir = $(confdir)
softgredconf_DATA = etc/softgred/softgred.conf
etc/softgred/softgred.conf: $(srcdir)/etc/softgred/softgred.conf.in
	$(MKDIR_P) etc/softgred
	$(SED) -e 's|@SOFTGRED_PIDDIR@|$(piddir)|g' $< > $@
if OPENWRT
initddir = $(sysconfdir)/init.d
dist_initd_SCRIPTS = openwrt/softgred
else
pkgdata_SCRIPTS = ../scripts/dhcp-lease-hook.py
endif
if WEB
pkgdata_DATA = ../scripts/run-services.py
webinterfacedir = $(pkgdatadir)/webinterface
webinterface_DATA = webinterface/index.html webinterface/dashboard.css \
    webinterface/dashboard.js webinterface/api.php webinterface/server.py
endif
EXTRA_DIST = etc/softgred/softgred.conf.in softgred.service softgred-web.service \
    webinterface/index.html webinterface/dashboard.css webinterface/dashboard.js \
    webinterface/api.php webinterface/server.py
CLEANFILES = etc/softgred/softgred.conf
