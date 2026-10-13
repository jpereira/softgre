#!/bin/sh
# Install the built package and retain its linked documentation on minimal images.
set -eu
if test "$#" = 0; then
    echo 'Usage: scripts/install-deb.sh PACKAGE.deb [...]' >&2
    exit 2
fi
printf '%s\n' 'path-include=/usr/share/doc/softgred/*' \
    >/etc/dpkg/dpkg.cfg.d/zz-softgred-docs
apt-get update
apt-get install -y --no-install-recommends "$@"
