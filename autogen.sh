#!/bin/sh
set -eu
cd "$(dirname "$0")"
autoreconf --force --install
./configure "$@"
