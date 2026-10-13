#!/bin/bash
# Supervise the daemon and its unprivileged dashboard in a disposable network namespace.
set -euo pipefail
if [[ ${SOFTGRE_LAB:-} != isolated ]]; then
    echo 'Use the isolated test-softgre lab container' >&2
    exit 2
fi
ip link set lo up
if ! ip link show sg-test-in >/dev/null 2>&1; then
    ip link add sg-test-in type veth peer name sg-test-peer
    ip addr add 192.0.2.1/24 dev sg-test-in
fi
ip link set sg-test-in up
ip link set sg-test-peer up
mkdir -p /run/softgre-lab
cat >/run/softgre-lab/softgred.conf <<'CONF'
[global]
interface=sg-test-in
tunnel-prefix=sgre
bridge-map=1000@br-radius,10@br-hotspot
bridge-force=true
pid-file=/run/softgre-lab/softgred.pid
[service]
bind-in=lo
port=8888
max-listen=32
CONF
/usr/sbin/softgred -f -c /run/softgre-lab/softgred.conf &
daemon=$!
web=''
cleanup() {
    trap - EXIT TERM INT
    if [[ -n $web ]]; then kill -TERM "$web" 2>/dev/null || true; fi
    kill -TERM "$daemon" 2>/dev/null || true
    wait "$daemon" 2>/dev/null || true
    if [[ -n $web ]]; then wait "$web" 2>/dev/null || true; fi
}
trap cleanup EXIT
trap 'exit 0' TERM INT
python3 - <<'PY'
import socket
import time
for _ in range(100):
    try:
        with socket.create_connection(('127.0.0.1', 8888), timeout=0.1):
            break
    except OSError:
        time.sleep(0.05)
else:
    raise SystemExit('softgred API did not start')
PY
setpriv --reuid 65534 --regid 65534 --clear-groups \
    python3 /usr/share/softgred/webinterface/server.py --bind 0.0.0.0 &
web=$!
wait -n "$daemon" "$web"
