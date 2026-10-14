SUBDIRS = bin share
EXTRA_DIST = README.md LICENSE.md autogen.sh docker/Dockerfile .dockerignore \
    tests/test_core.c tests/test_netlink.c tests/integration.py \
    scripts/check-linux.sh
EXTRA_DIST += docs/en_US/index.md docs/assets/site.css docs/assets/site.js \
    scripts/build-docs.py tests/test_docs.py share/softgred.service docker/Dockerfile.test \
    .github/workflows/ci.yml .github/workflows/pages.yml
EXTRA_DIST += tests/test_web.py debian/control debian/rules debian/changelog \
    debian/softgred.service debian/source/format
EXTRA_DIST += tests/test_firewall.c
EXTRA_DIST += debian/copyright debian/docs debian/README.Debian debian/README.source
EXTRA_DIST += VERSION bump.sh scripts/version.py scripts/build-deb.sh tests/test_version.py \
    docker/Dockerfile.release .github/workflows/release.yml
EXTRA_DIST += tests/test_helper.c
EXTRA_DIST += share/softgred-web.service debian/softgred.softgred-web.service
EXTRA_DIST += docker/Dockerfile.lab scripts/lab/start.sh scripts/lab/status.sh \
    scripts/lab/send.py scripts/lab/packets.py scripts/lab/health.py
EXTRA_DIST += scripts/check-lab.sh
EXTRA_DIST += docs/assets/softgre-logo.svg docs/assets/theme.js
EXTRA_DIST += tests/test_log.c share/softgred.logrotate debian/softgred.logrotate
EXTRA_DIST += .clang-format .prettierrc.json docker/Dockerfile.format scripts/format-code.sh \
    scripts/format-includes.py

.PHONY: deb
deb:
	+$(SHELL) $(abs_top_srcdir)/scripts/make-deb.sh
EXTRA_DIST += scripts/make-deb.sh scripts/check-installed-package.sh scripts/run-services.py
EXTRA_DIST += scripts/install-deb.sh
EXTRA_DIST += scripts/lab/setup.sh scripts/lab/setup.service scripts/lab/daemon.conf
EXTRA_DIST += .github/workflows/containers.yml
EXTRA_DIST += tests/dhcp_integration.py
EXTRA_DIST += docs/assets/softgre-flow.svg
EXTRA_DIST += scripts/dhcp-lease-hook.py
EXTRA_DIST += scripts/lab/gateway-setup.sh scripts/lab/gateway-setup.conf \
    scripts/lab/dnsmasq-softgre.conf scripts/lab/dnsmasq.conf
