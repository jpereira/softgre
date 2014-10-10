# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Thu 09 Oct 2014 11:58:22 PM EDT
####

MAINTAINERCLEANFILES = Makefile.in
AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

SUBDIRS = bin

GIT_BRANCH := $(shell git branch --no-color | grep "^*" | sed -e 's/^\* //')
GIT_COMMIT := $(shell git log --no-color | sed '1!d; s/.* //g')
GIT_TAG    := $(shell git name-rev --tags --name-only $(shell git rev-parse HEAD) | cut -f1 -d^)

#DEBIAN_PKG_VERSION := $(PACKAGE_VERSION)-$(GIT_TAG)
DEBIAN_PKG_VERSION := $(PACKAGE_VERSION)-$(GIT_COMMIT)

if MAINTAINER_MODE
dist-hook: dist-changelog
else
dist-hook:
endif

# always update the ChangeLog
all: version_header dist-changelog
	
version_header:
	echo "/*Generated automatic ($(shell date))*/" > version.h
	echo "#define CURRENT_COMMIT \"$(GIT_COMMIT)\"" >> version.h
	echo "#define CURRENT_BRANCH \"$(GIT_BRANCH)\"" >> version.h
	echo "#define CURRENT_TAG    \"$(GIT_TAG)\"" >> version.h

build-package: dist-changelog debian-package

dist-changelog:
	if git --git-dir=$(top_srcdir)/.git --work-tree=$(top_srcdir) \
	log --no-merges --date=short --pretty='tformat:%cd  %an  <%ae>%n%n%s%n%n%b' $(GIT_BRANCH) | \
	$(SED)	-e '/^[12]...-[01].-[0123].  [^<>]*  <[^<>]*>$$/,/^$$/ b' \
	  		-e '/[^	 ]/,/^[	 ]*$$/ !d' \
		    -e 's/^[	 ]*/	/' \
		    -e 's/^[	 ]*$$//' >.ChangeLog.tmp; \
    then mv -f .ChangeLog.tmp "ChangeLog"; \
    else rm -f .ChangeLog.tmp; exit 1; fi

debian-package:
	rm -f debian/changelog
	dch -v "$(DEBIAN_PKG_VERSION)" --package softgred --create debian/changelog
	debuild -us -uc -B

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update 

.PHONY: dist-changelog

