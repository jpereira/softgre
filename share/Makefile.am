# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Seg 27 Out 2014 20:57:37 BRST
####

etc/softgred/softgred.conf: etc/softgred/softgred.conf.in
	$(SED) -e "s|@SOFTGRED_PIDDIR@|$(confdir)|" \
		   -e "s|@SOFTGRED_LOGDIR@|$(logdir)|" $< > $@

etc/apache/softgred.conf: etc/apache/softgred.conf.in
	$(SED) -e "s|@SOFTGRED_DIR@|$(pkgdatadir)|" $< > $@

webinterface/config.php: webinterface/config.php.in
	$(SED) -e "s|@SOFTGRED_CONFDIR@|$(confdir)|" \
		   -e "s|@SOFTGRED_LOGDIR@|$(logdir)|" $< > $@

webinterface/webinterface_config.php: webinterface/webinterface_config.php.in 
	$(SED) -e "s|@SOFTGRED_CONFDIR@|$(confdir)|" \
		   -e "s|@SOFTGRED_LOGDIR@|$(logdir)|" $< > $@

apacheconfdir = $(sysconfdir)/apache2/conf.d/
apacheconf_DATA = etc/apache/softgred.conf

softgredconfdir = $(sysconfdir)/softgred/
softgredconf_DATA = etc/softgred/softgred.conf \
                    webinterface/webinterface_config.php

webinterfacedir = $(pkgdatadir)/webinterface
webinterface_DATA =          \
	webinterface/wrapper.php \
	webinterface/api.php     \
	webinterface/config.php  \
	webinterface/README.api

webinterfacelibdir = $(pkgdatadir)/webinterface/lib
webinterfacelib_DATA =            \
	webinterface/lib/PHPTail.php  \
	webinterface/lib/Rest.inc.php \
	webinterface/lib/Helper.inc.php

include dashboard.mk

dashboarddir = $(webinterfacedir)
dashboard_DATA = webinterface/dashboard/dist/index.html \
				 webinterface/dashboard/dist/favicon.ico

# extra dist files
EXTRA_DIST = $(apacheconf_DATA)       \
			 $(softgredconf_DATA)     \
			 $(webinterface_DATA)     \
			 $(webinterfacelib_DATA)  \
			 $(dashboard_DATA) 		  \
			 $(dashboardfonts_DATA)   \
			 $(dashboardimages_DATA)  \
			 $(dashboardscripts_DATA) \
			 $(dashboardstyles_DATA)  \
			 $(dashboardviews_DATA)

CLEANFILES = etc/softgred/softgred.conf \
             etc/apache/softgred.conf   \
			 webinterface/config.php    \
			 webinterface/webinterface_config.php

# dashboard
#hook-dist: build-dashboard

build-dashboard:
	make -C webinterface/dashboard release
	scripts/gen-dist-dashboard.sh > dashboard.mk

#.PHONY: build-dashboard

