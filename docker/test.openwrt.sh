#!/bin/sh
# Builds the OpenWrt package with the SDK and smoke tests it in an OpenWrt rootfs.
# Usage: docker/test.openwrt.sh [openwrt-release]   (default 24.10.8)
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
release=${1:-24.10.8}
image=softgre-openwrt
# The SDK and rootfs images are x86-64 only; Docker emulates them on other hosts.
docker build --platform linux/amd64 -f docker/Dockerfile.openwrt \
    --build-arg OPENWRT_RELEASE="$release" -t "$image" .
docker run --rm --platform linux/amd64 --cap-add NET_ADMIN --cap-add NET_RAW "$image"
echo 'docker/test.openwrt.sh: all checks passed'
