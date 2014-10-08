MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = bin

GIT_BRANCH := $(shell git branch --no-color | grep "^*" | sed -e 's/^\* //')
GIT_COMMIT := $(shell git log --no-color | sed '1!d; s/.* //g')
DEBIAN_PKG_VERSION := $(PACKAGE_VERSION)-$(GIT_COMMIT)

build-package: build-changelog debian-package

build-changelog:
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
	dch -v "$(DEBIAN_PKG_VERSION)" --package softgred --create
	debuild -us -uc -B


