# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Qua 15 Out 2014 19:49:09 BRT
####

apacheconfdir = $(sysconfdir)/apache2/conf.d/
dist_apacheconf_DATA = apache/softgred.conf

wwwdir = $(pkgdatadir)/www
www_DATA = \
	www/index.php

wwwlibdir = $(pkgdatadir)/www/lib
wwwlib_DATA = \
	www/lib/PHPTail.php \
	www/lib/Rest.inc.php

wwwapidir = $(pkgdatadir)/www/api
wwwapi_DATA = \
	www/api/.htaccess \
	www/api/index.php \
	www/api/README

EXTRA_DIST = $(www_DATA) $(wwwlib_DATA) $(wwwapi_DATA)

