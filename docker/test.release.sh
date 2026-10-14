#!/bin/sh
# Tests for docker/Dockerfile.release: build and verify the .deb for each supported distribution.
# Usage: docker/test.release.sh [debian12|debian13|ubuntu22.04|ubuntu24.04 ...] (default: all)
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
base_of() {
    case $1 in
        debian12) echo debian:bookworm-slim ;;
        debian13) echo debian:trixie-slim ;;
        ubuntu22.04) echo ubuntu:22.04 ;;
        ubuntu24.04) echo ubuntu:24.04 ;;
        *)
            echo "Unknown distribution: $1" >&2
            exit 2
            ;;
    esac
}
if test $# -eq 0; then set -- debian12 debian13 ubuntu22.04 ubuntu24.04; fi
version=$(./bump.sh --check)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
for distro in "$@"; do
    docker build -f docker/Dockerfile.release --build-arg BASE="$(base_of "$distro")" \
        -t "softgre-release-$distro" .
    mkdir -p "$work/$distro"
    docker run --rm -v "$work/$distro:/out" "softgre-release-$distro" "$distro" /out
    arch=$(docker run --rm --entrypoint dpkg "softgre-release-$distro" --print-architecture)
    package="$work/$distro/softgred_$version-1+${distro}_${arch}.deb"
    if test ! -s "$package"; then
        echo "Missing package for $distro: $package" >&2
        exit 1
    fi
    echo "built $(basename "$package")"
done
echo 'docker/test.release.sh: packages built and verified'
