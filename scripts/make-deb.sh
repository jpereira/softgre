#!/bin/sh
# Build a binary package in the source tree using its declared Debian dependencies.
set -eu
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
if test "$(uname -s)" != Linux; then
    echo 'make deb requires Linux; use docker/Dockerfile.release' >&2
    exit 2
fi
./bump.sh --check >/dev/null
command -v dpkg-buildpackage >/dev/null
# Debian builds configure and test the source themselves; avoid inherited make settings.
unset MAKEFLAGS MFLAGS MAKELEVEL
exec dpkg-buildpackage -us -uc -b
