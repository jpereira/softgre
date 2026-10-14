#!/bin/sh
# Tests for docker/Dockerfile.test: compilers, sanitizers, kernel integration, DHCP, web, package.
# Set FUZZ_SECONDS=0 to skip fuzzing (default 10 seconds per target).
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
image=softgre-test
docker build -f docker/Dockerfile.test -t "$image" .
docker run --rm "$image" scripts/check-linux.sh
docker run --rm --cap-add NET_ADMIN --cap-add NET_RAW -e SOFTGRE_TEST_NAMESPACE=isolated "$image" \
    python3 tests/integration.py /usr/sbin/softgred
docker run --rm --privileged -e SOFTGRE_TEST_NAMESPACE=isolated "$image" \
    python3 tests/dhcp_integration.py /usr/sbin/softgred
docker run --rm "$image" python3 tests/test_web.py
docker run --rm "$image" scripts/check-installed-package.sh
docker run --rm "$image" sh -c \
    'python3 scripts/build-docs.py >/dev/null && python3 tests/test_docs.py &&
    python3 tests/test_version.py && ./bump.sh --check'
# The daemon must not depend on third-party libraries beyond libc.
docker run --rm "$image" sh -c \
    'deps=$(ldd /usr/sbin/softgred) || exit 1
     if printf "%s\n" "$deps" | grep -qi glib; then
         echo "softgred links GLib" >&2
         exit 1
     fi'
if test "${FUZZ_SECONDS:-10}" != 0; then
    docker run --rm -e FUZZ_SECONDS="${FUZZ_SECONDS:-10}" "$image" tests/fuzz/run.sh
fi
echo 'docker/test.test.sh: all checks passed'
