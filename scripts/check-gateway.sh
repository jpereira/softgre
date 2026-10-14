#!/bin/sh
# Run the gateway image with AP containers on a private Docker network; never reuse a demo.
set -eu
image=${1:-softgre-gateway:ubuntu24.04}
network=softgre-gateway-ci
gateway=softgre-gateway-ci
aps='softgre-ap1-ci softgre-ap2-ci'
cleanup() {
    docker rm -f $gateway $aps >/dev/null 2>&1 || true
    docker network rm $network >/dev/null 2>&1 || true
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
cleanup
docker network create $network >/dev/null
docker run -d --name $gateway --network $network --privileged --cgroupns=private \
    --tmpfs /run --tmpfs /run/lock "$image" >/dev/null
for attempt in $(seq 1 30); do
    state=$(docker inspect --format '{{.State.Health.Status}}' $gateway)
    if test "$state" = healthy; then break; fi
    if test "$attempt" = 30; then
        docker logs $gateway
        echo 'Gateway did not become healthy' >&2
        exit 1
    fi
    sleep 1
done
docker exec $gateway systemctl is-active softgred softgred-web dnsmasq
option=
for ap in $aps; do
    docker run -d --name "$ap" --network $network --privileged --no-healthcheck \
        --entrypoint /opt/softgre-lab/ap.sh "$image" $gateway $option >/dev/null
    option=--option82
done
for ap in $aps; do
    for attempt in $(seq 1 60); do
        if docker logs "$ap" 2>&1 | grep -q 'AP ready'; then break; fi
        if test "$attempt" = 60; then
            docker logs "$ap"
            echo "$ap did not finish DHCP" >&2
            exit 1
        fi
        sleep 1
    done
    docker logs "$ap"
    if docker logs "$ap" 2>&1 | grep -q 'no DHCP lease'; then exit 1; fi
    address=$(docker inspect --format "{{(index .NetworkSettings.Networks \"$network\").IPAddress}}" "$ap")
    docker exec "$ap" ip netns exec c1000 ping -c 1 -W 2 10.100.0.1 >/dev/null
    docker exec "$ap" ip netns exec c10 ping -c 1 -W 2 172.16.10.1 >/dev/null
    docker exec $gateway grep -q "interface=br-radius remote=$address" /var/log/softgred-dhcp.log
    docker exec $gateway grep -q "interface=br-hotspot remote=$address" /var/log/softgred-dhcp.log
done
docker exec $gateway grep -q 'circuit-id=hotspot remote-id=02:a0:' /var/log/softgred-dhcp.log
docker exec $gateway cat /var/log/softgred-dhcp.log
docker exec $gateway python3 -c '
import json
import urllib.request
with urllib.request.urlopen("http://127.0.0.1:8080/api?action=overview") as reply:
    assert json.load(reply)["tunnel_count"] == 2, "Expected one tunnel per AP"
'
echo 'Gateway: external APs, DHCP per bridge, lease hook and dashboard passed'
