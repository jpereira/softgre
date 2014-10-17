# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Fri 17 Oct 2014 01:53:50 AM BRT
####

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
softgredconf_DATA = webinterface/webinterface_config.php

webinterfacedir = $(pkgdatadir)/webinterface
webinterface_DATA =          \
	webinterface/index.php   \
	webinterface/api.php     \
	webinterface/config.php  \
	webinterface/README.api

webinterfacelibdir = $(pkgdatadir)/webinterface/lib
webinterfacelib_DATA =            \
	webinterface/lib/PHPTail.php  \
	webinterface/lib/Rest.inc.php \
	webinterface/lib/Helper.inc.php

EXTRA_DIST = $(apacheconf_DATA)     \
			 $(softgredconf_DATA)   \
			 $(webinterface_DATA)   \
			 $(webinterfacelib_DATA)

CLEANFILES = etc/apache/softgred.conf \
			 webinterface/config.php \
			 webinterface/webinterface_config.php

