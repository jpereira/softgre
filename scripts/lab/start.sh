#!/bin/bash
set -euo pipefail
/opt/softgre-lab/setup.sh
exec python3 /usr/share/softgred/run-services.py -c /run/softgre-lab/softgred.conf
