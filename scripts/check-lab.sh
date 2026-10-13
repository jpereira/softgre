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
docker run -d --name "$name" --privileged --cgroupns=private --tmpfs /run --tmpfs /run/lock \
    softgre-lab:ubuntu24.04
started=true
wait_ready
docker exec "$name" sh -c 'test "$(cat /proc/1/comm)" = systemd'
daemon_pid=$(docker exec "$name" systemctl show softgred --property MainPID --value)
web_pid=$(docker exec "$name" systemctl show softgred-web --property MainPID --value)
docker exec "$name" systemctl is-active softgred softgred-web
docker exec "$name" service softgred restart
wait_ready
test "$daemon_pid" != "$(docker exec "$name" systemctl show softgred --property MainPID --value)"
test "$web_pid" != "$(docker exec "$name" systemctl show softgred-web --property MainPID --value)"
docker exec "$name" systemctl is-active softgred softgred-web
docker exec "$name" sh -c '
for tool in brctl ifconfig netstat htop vim strace lsof systemctl journalctl service logrotate; do
    command -v "$tool"
done
'
docker exec "$name" python3 /opt/softgre-lab/send.py --interval 0.02
docker exec "$name" /opt/softgre-lab/status.sh
docker exec "$name" journalctl -u softgred --no-pager
docker exec "$name" sh -c '
journalctl -u softgred --no-pager | grep -q "Client learned remote=192.0.2.2"
grep -q "VLAN attached remote=192.0.2.3" /var/log/softgred.log
'
docker exec "$name" systemctl restart softgred-web
docker exec "$name" systemctl is-active softgred-web

docker restart --timeout 15 "$name"
wait_ready
docker exec "$name" python3 -c '
import json
import urllib.request
with urllib.request.urlopen("http://127.0.0.1:8080/api?action=overview") as reply:
    assert json.load(reply)["tunnel_count"] == 0, "Restart retained stale learning"
'
docker exec "$name" python3 /opt/softgre-lab/send.py --interval 0.02
docker stop --timeout 15 "$name"
# A container PID namespace reports systemd halt as SIGINT (128 + 2).
case $(docker inspect --format '{{.State.ExitCode}}' "$name") in
    0 | 130) ;;
    *)
        echo 'Lab did not shut down cleanly' >&2
        exit 1
        ;;
esac
test "$(docker inspect --format '{{.State.OOMKilled}}' "$name")" = false
