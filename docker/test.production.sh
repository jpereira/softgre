#!/bin/sh
# Tests for docker/Dockerfile (production image): metadata, no GLib, and a live start.
set -eu
cd "$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
image=softgre-production-test
name=softgre-production-ci
conf=$(mktemp)
cleanup() {
    docker rm -f "$name" >/dev/null 2>&1 || true
    rm -f "$conf"
}
trap cleanup EXIT HUP INT TERM
docker build -f docker/Dockerfile -t "$image" .
docker run --rm "$image" --version
docker run --rm --entrypoint dpkg-query "$image" -W softgred
docker run --rm --entrypoint sh "$image" -c \
    'deps=$(ldd /usr/sbin/softgred) || exit 1
     if printf "%s\n" "$deps" | grep -qi glib; then
         echo "softgred links GLib" >&2
         exit 1
     fi'
cat >"$conf" <<'CONF'
[global]
interface=lo
bridge-map=10@br-test
bridge-force=true
log-file=/tmp/softgred.log
pid-file=/tmp/softgred.pid
[service]
bind-in=lo
CONF
chmod 644 "$conf"
docker run -d --name "$name" --cap-add NET_ADMIN --cap-add NET_RAW \
    -v "$conf:/etc/softgred/softgred.conf:ro" "$image" >/dev/null
for attempt in $(seq 1 30); do
    if docker exec "$name" python3 -c '
import json, urllib.request
with urllib.request.urlopen("http://127.0.0.1:8080/api?action=status", timeout=3) as reply:
    assert json.load(reply)["status"] == "OK"
' 2>/dev/null; then
        echo 'docker/test.production.sh: image starts and the dashboard answers'
        exit 0
    fi
    sleep 1
done
docker logs "$name" | tail -20
echo 'Production image did not become ready' >&2
exit 1
