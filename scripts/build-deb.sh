#!/bin/sh
# Run in a disposable Linux build container matching the requested distribution.
set -eu
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
distro=${1:?Usage: build-deb.sh debian12|debian13|ubuntu22.04|ubuntu24.04 [OUTPUT]}
output=${2:-/out}
case "$distro" in debian12 | debian13 | ubuntu22.04 | ubuntu24.04) ;; *) exit 2 ;; esac
. /etc/os-release
if test "$distro" != "$ID$VERSION_ID"; then
    echo "Build image mismatch: requested $distro, running $ID$VERSION_ID" >&2
    exit 2
fi
version=$(./bump.sh --check)
backup=$(mktemp)
cp debian/changelog "$backup"
trap 'cp "$backup" debian/changelog; rm -f "$backup"' EXIT HUP INT TERM
python3 - "$version" "$distro" <<'PY'
import sys
from pathlib import Path
path = Path('debian/changelog')
lines = path.read_text().splitlines(keepends=True)
lines[0] = f'softgred ({sys.argv[1]}-1+{sys.argv[2]}) unstable; urgency=medium\n'
path.write_text(''.join(lines))
PY
dpkg-buildpackage -us -uc -b
mkdir -p "$output"
for package in ../softgred_"$version"-1+"$distro"_*.deb; do
    test -f "$package"
    cp "$package" "$output/"
done
