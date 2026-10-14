#!/bin/bash
# Serve APs on the container's Docker network: capture GRE on the default-route interface,
# give each service bridge a gateway address and NAT client traffic to the uplink.
set -euo pipefail
if [[ ${SOFTGRE_LAB:-} != isolated ]]; then
    echo 'Use the isolated SoftGRE gateway container' >&2
    exit 2
fi
ip link set lo up
wan=$(ip -4 route show default | awk '{print $5; exit}')
if [[ -z $wan ]]; then
    echo 'No IPv4 default route; attach the container to a Docker network' >&2
    exit 1
fi
bridge() {
    if ! ip link show "$1" >/dev/null 2>&1; then
        ip link add "$1" type bridge
    fi
    ip addr replace "$2" dev "$1"
    ip link set "$1" up
    # Clients reach the uplink; GRE itself leaves from the WAN address, untranslated.
    if ! iptables -t nat -C POSTROUTING -s "$3" -o "$wan" -j MASQUERADE 2>/dev/null; then
        iptables -t nat -A POSTROUTING -s "$3" -o "$wan" -j MASQUERADE
    fi
}
bridge br-radius 10.100.0.1/24 10.100.0.0/24
bridge br-hotspot 172.16.10.1/23 172.16.10.0/23
echo 1 >/proc/sys/net/ipv4/ip_forward
mkdir -p /run/softgre-lab
cat >/run/softgre-lab/softgred.conf <<CONF
[global]
interface=$wan
tunnel-prefix=sgre
bridge-map=1000@br-radius,10@br-hotspot
bridge-force=false
pid-file=/run/softgre-lab/softgred.pid
[service]
bind-in=lo
port=8888
max-listen=32
CONF
