#!/bin/sh
# Run every docker/test.*.sh in turn. Individual scripts can also be run on their own.
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
for script in test format lab release production openwrt; do
    echo "=== docker/test.$script.sh ==="
    "docker/test.$script.sh"
done
echo 'docker/test.all.sh: everything passed'
