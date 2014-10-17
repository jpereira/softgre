# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Thu 16 Oct 2014 11:15:23 PM BRT
####

etc/apache/softgred.conf: etc/apache/softgred.conf.in
	$(SED) -e "s|@SOFTGRED_DIR@|$(pkgdatadir)|" $< > $@

webinterface/config.php: webinterface/config.php.in
	$(SED) -e "s|@SOFTGRED_CONFDIR@|$(sysconfdir)/softgred|" \
		   -e "s|@SOFTGRED_LOGDIR@|$(localstatedir)/log|" $< > $@

webinterface/webinterface_config.php: webinterface/webinterface_config.php.in 
	$(SED) -e "s|@SOFTGRED_CONFDIR@|$(sysconfdir)/softgred|" \
		   -e "s|@SOFTGRED_LOGDIR@|$(localstatedir)/log|" $< > $@

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

