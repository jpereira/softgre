MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = bin

debian-package:
	rm -f debian/changelog
	dch --create -v $(PACKAGE_VERSION) --package softgre
	debuild -us -uc -B

