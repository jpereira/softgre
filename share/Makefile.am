# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Qui 16 Out 2014 18:39:51 BRT
####

etc/apache/softgred.conf: etc/apache/softgred.conf.in
	$(SED) -e "s|\@SOFTGRED_DIR\@|$(pkgdatadir)|" $< > $@

webinterface/api.php: webinterface/api.php.in
	$(SED) -e "s|\@SOFTGREDCONF_DIR\@|$(sysconfdir)/softgred|" $< > $@

apacheconfdir = $(sysconfdir)/apache2/conf.d/
apacheconf_DATA = etc/apache/softgred.conf

softgredconfdir = $(sysconfdir)/softgred/
softgredconf_DATA = webinterface/config.php

webinterfacedir = $(pkgdatadir)/webinterface
webinterface_DATA =        \
	webinterface/index.php \
	webinterface/api.php   \
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

