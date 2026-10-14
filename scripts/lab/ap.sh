#!/bin/bash
# Simulate a soft-GRE access point: gretap to the concentrator, VLANs 1000 and 10, and one
# DHCP client namespace per VLAN (c1000, c10). Run in its own privileged container.
set -euo pipefail
usage() {
    echo 'Usage: ap.sh GATEWAY [--option82]' >&2
    exit 2
}
test $# -ge 1 || usage
gateway=$(getent ahostsv4 "$1" | awk '{print $1; exit}')
option82=false
shift
for argument in "$@"; do
    case $argument in
        --option82) option82=true ;;
        *) usage ;;
    esac
done
if [[ -z $gateway ]]; then
    echo "Cannot resolve gateway $1" >&2
    exit 1
fi
wan=$(ip -4 route show default | awk '{print $5; exit}')
local=$(ip -4 -o addr show dev "$wan" | awk '{split($4, a, "/"); print a[1]; exit}')
# Derive unique, recognizable MACs from the AP's address on the Docker network.
IFS=. read -r _ _ third fourth <<<"$local"
suffix=$(printf '%02x:%02x' "$third" "$fourth")
ap_mac="02:a0:00:00:$suffix"
ip link add softgre address "$ap_mac" type gretap local "$local" remote "$gateway"
ip link set softgre up
# A restarted container keeps /run: drop namespace files and leases from the last run.
for vlan in 1000 10; do
    ip netns delete "c$vlan" 2>/dev/null || rm -f "/run/netns/c$vlan"
done
rm -rf /run/softgre-ap
mkdir -p /run/softgre-ap
for vlan in 1000 10; do
    if test "$vlan" = 1000; then ssid=radius client=01; else ssid=hotspot client=02; fi
    ip link add link softgre name "softgre.$vlan" type vlan id "$vlan"
    ip link add "lan$vlan" address "$ap_mac" type bridge
    ip link set "softgre.$vlan" master "lan$vlan" up
    ip link set "lan$vlan" up
    ip netns add "c$vlan"
    # No IPv6 autoconfiguration noise from the simulated client.
    for scope in all default; do
        ip netns exec "c$vlan" sh -c "echo 1 >/proc/sys/net/ipv6/conf/$scope/disable_ipv6"
    done
    ip link add "port$vlan" type veth peer name eth0 netns "c$vlan"
    ip link set "port$vlan" master "lan$vlan" up
    ip -n "c$vlan" link set lo up
    ip -n "c$vlan" link set eth0 address "02:10:00:$suffix:$client" up
    extra=()
    if $option82; then
        # Relay agent information as the AP would insert it: circuit-id SSID, remote-id MAC.
        hex() { printf '%s' "$1" | od -An -tx1 | tr -d ' \n'; }
        value=$(printf '01%02x%s02%02x%s' "${#ssid}" "$(hex "$ssid")" "${#ap_mac}" "$(hex "$ap_mac")")
        extra=(-x "0x52:$value")
    fi
    # dnsmasq pings each new address for up to ~3s before offering it; wait like a real client.
    SOFTGRE_LEASE="/run/softgre-ap/c$vlan.lease" ip netns exec "c$vlan" busybox udhcpc -i eth0 \
        -q -n -t 10 -T 3 -A 1 -s /opt/softgre-lab/udhcpc.sh "${extra[@]}" >"/run/softgre-ap/c$vlan.log" 2>&1 \
        || echo "c$vlan: no DHCP lease" >&2
done
echo "AP $local -> gateway $gateway, tunnel softgre ($ap_mac)"
for vlan in 1000 10; do
    if test -s "/run/softgre-ap/c$vlan.lease"; then
        echo "c$vlan VLAN $vlan MAC $(ip -n "c$vlan" -br link show eth0 | awk '{print $3}'): $(cat "/run/softgre-ap/c$vlan.lease")"
    fi
done
echo 'AP ready'
exec sleep infinity
