#!/bin/sh
# Tests for docker/Dockerfile.lab: the single-container lab and the gateway with external APs.
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
docker build -f docker/Dockerfile.lab -t softgre-lab:ubuntu24.04 .
scripts/check-lab.sh
docker build -f docker/Dockerfile.lab --target gateway -t softgre-gateway:ubuntu24.04 .
scripts/check-gateway.sh softgre-gateway:ubuntu24.04
echo 'docker/test.lab.sh: lab and gateway passed'
