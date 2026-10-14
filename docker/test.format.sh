#!/bin/sh
# Tests for docker/Dockerfile.format: source formatting must already be clean.
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
docker build -f docker/Dockerfile.format -t softgre-format .
# Check the working tree, not the copy baked into the image, so unstaged edits are covered.
docker run --rm -v "$PWD:/src" softgre-format scripts/format-code.sh --check
echo 'docker/test.format.sh: formatting is clean'
