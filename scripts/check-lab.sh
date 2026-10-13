#!/bin/sh
# Use DOCKER_CONTEXT=homelab for remote Linux validation; never reuse the persistent demo.
set -eu
name=softgre-lab-ci
started=false
cleanup() {
    if test "$started" = true; then
        docker rm -f "$name" >/dev/null
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
wait_ready() {
    for attempt in $(seq 1 30); do
        state=$(docker inspect --format '{{.State.Health.Status}}' "$name")
        if test "$state" = healthy; then return; fi
        sleep 1
    done
    docker logs "$name"
    echo 'Lab did not become healthy' >&2
    exit 1
}
docker run -d --name "$name" --cap-add NET_ADMIN --cap-add NET_RAW \
    softgre-lab:ubuntu24.04
started=true
wait_ready
docker exec "$name" sh -c '
for tool in brctl ifconfig netstat htop vim strace lsof systemctl journalctl service logrotate; do
    command -v "$tool"
done
'
docker exec "$name" python3 /opt/softgre-lab/send.py --interval 0.02
docker exec "$name" /opt/softgre-lab/status.sh
docker restart --time 15 "$name"
wait_ready
docker exec "$name" python3 -c '
import json
import urllib.request
with urllib.request.urlopen("http://127.0.0.1:8080/api?action=overview") as reply:
    assert json.load(reply)["tunnel_count"] == 0, "Restart retained stale learning"
'
docker exec "$name" python3 /opt/softgre-lab/send.py --interval 0.02
docker stop --time 15 "$name"
test "$(docker inspect --format '{{.State.ExitCode}}' "$name")" = 0
