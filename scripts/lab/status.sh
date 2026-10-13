#!/bin/sh
set -eu
if test "${SOFTGRE_LAB:-}" != isolated; then
    echo 'Run inside the test-softgre lab container' >&2
    exit 2
fi
ip -d link show type gretap
bridge link show
python3 - <<'PY'
import json
import urllib.request
with urllib.request.urlopen('http://127.0.0.1:8080/api?action=overview', timeout=6) as response:
    print(json.dumps(json.load(response), indent=2))
PY
