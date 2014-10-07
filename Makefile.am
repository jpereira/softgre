MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = bin

GIT_REVISION := master

build-package: build-changelog debian-package

GIT_REVISION = "$(shell git log | head -1 | awk '{print $2}')"
PACKAGE_VERSION = $(PACKAGE_VERSION)-$(GIT_REVISION)

build-changelog:
	if git --git-dir=$(top_srcdir)/.git --work-tree=$(top_srcdir) \
	log --no-merges --date=short --pretty='tformat:%cd  %an  <%ae>%n%n%s%n%n%b' $(GIT_REVISION) | \
	$(SED)	-e '/^[12]...-[01].-[0123].  [^<>]*  <[^<>]*>$$/,/^$$/ b' \
	  		-e '/[^	 ]/,/^[	 ]*$$/ !d' \
		    -e 's/^[	 ]*/	/' \
		    -e 's/^[	 ]*$$//' >.ChangeLog.tmp; \
    then mv -f .ChangeLog.tmp "ChangeLog"; \
    else rm -f .ChangeLog.tmp; exit 1; fi

debian-package:
	rm -f debian/changelog
	dch -v $(PACKAGE_VERSION) --package softgred --create
	debuild -us -uc -B


