# DHCP and subscriber services

SoftGRE has no DHCP server of its own. It delivers each VLAN to a bridge, and any DHCP
server on that bridge serves the clients behind every AP. This page shows one network per
bridge with dnsmasq, how to see which AP is behind each lease, and how to use DHCP option
82 inserted by the AP. The [gateway lab](try-it.md#gateway-with-simulated-aps) runs this
configuration.

## DHCP per bridge

One dnsmasq instance can give each bridge a different network, mask, gateway and DNS. On
the concentrator, give each bridge a gateway address and add one range per bridge:

```sh
sudo ip addr add 10.100.0.1/24 dev br-radius
sudo ip addr add 172.16.10.1/23 dev br-hotspot
```

```ini
# /etc/dnsmasq.d/softgre.conf
port=0
bind-interfaces
interface=br-radius
interface=br-hotspot
dhcp-authoritative

dhcp-range=set:radius,10.100.0.100,10.100.0.199,255.255.255.0,1h
dhcp-option=tag:radius,option:router,10.100.0.1
dhcp-option=tag:radius,option:dns-server,1.1.1.1

dhcp-range=set:hotspot,172.16.10.100,172.16.11.199,255.255.254.0,1h
dhcp-option=tag:hotspot,option:router,172.16.10.1
dhcp-option=tag:hotspot,option:dns-server,8.8.8.8

# Record which AP (remote) and tunnel each lease came through.
dhcp-script=/usr/share/softgred/dhcp-lease-hook.py
```

`port=0` turns off dnsmasq's DNS service so it only does DHCP. Remove it to also run a
local resolver. A client on VLAN 1000 of any AP gets `10.100.0.x/24` with DNS `1.1.1.1`,
and a client on VLAN 10 gets `172.16.10.x/23` with DNS `8.8.8.8`. Add routing and NAT from
the bridges to your uplink as usual.

Before offering a new address, dnsmasq pings it for up to about three seconds to detect
conflicts. Clients with very short DHCP timeouts may retry several times when many of
them start at once; real clients wait long enough.

## Which AP is behind each lease

The DHCP server only sees the client MAC. `softgred` already knows which remote and
tunnel learned that MAC, so the lease hook asks the API (`GTMC <mac>`) on every lease
event and prints one line, which dnsmasq writes to its log:

```text
DHCP lease action=add ip=10.100.0.101 mac=02:10:00:00:02:01 interface=br-radius remote=192.0.2.2 tunnel=sgre0
```

The binding comes from the tunnel the client's traffic actually arrived on, so clients
cannot forge it. Because the API listens on loopback, run dnsmasq on the concentrator.
dnsmasq passes its environment to the hook: set `SOFTGRE_API_PORT` (and
`SOFTGRE_API_HOST`) when the API does not use `127.0.0.1:8888`, and `SOFTGRE_DHCP_LOG` to
also append the lines to a file, for example with a systemd drop-in for dnsmasq:

```ini
[Service]
Environment=SOFTGRE_DHCP_LOG=/var/log/softgred-dhcp.log
```

The hook never fails a lease. If the API is unreachable or the MAC is unknown, it logs
`remote=- tunnel=-`. Hostnames and relay IDs are reduced to safe single tokens, so
clients cannot inject extra log lines.

## Option 82 from the AP

If the AP acts as a DHCP relay agent and inserts option 82, SoftGRE forwards it untouched,
because it is plain bridged Ethernet. This is the way to learn the CPE's own MAC or SSID:
the concentrator only sees the AP's IPv4 address, because the outer Ethernet header of
routed GRE belongs to the last router. dnsmasq matches option 82 without a relay
`giaddr`, so it can apply per-AP or per-SSID policy at lease time:

```ini
# AP B sends remote-id "02:a0:00:00:00:03" (its MAC as text) and circuit-id = SSID.
dhcp-remoteid=set:ap-b,"02:a0:00:00:00:03"
dhcp-option=tag:hotspot,tag:!ap-b,option:dns-server,8.8.8.8
dhcp-option=tag:hotspot,tag:ap-b,option:dns-server,9.9.9.9
```

Quote a text remote-id. Without quotes, dnsmasq reads `02:a0:...` as hex bytes, which
matches APs that send the MAC in binary. When two `dhcp-option` lines match, dnsmasq does
not prefer the more specific one, so exclude the special case with `tag:!`. The lease hook
adds `circuit-id=` and `remote-id=` to its line, as text when printable or as hex
otherwise; dnsmasq cuts binary IDs at the first zero byte.

**Only trust option 82 when the AP removes it from client requests.** A client can send
its own option 82, and dnsmasq cannot tell it apart from the AP's. Configure the AP to
strip or replace client-supplied option 82 before relying on it for policy. The `remote=`
and `tunnel=` binding from the lease hook does not depend on the AP and cannot be forged
by clients.

## End-to-end DHCP test

[`tests/dhcp_integration.py`](https://github.com/jpereira/softgre/blob/master/tests/dhcp_integration.py)
builds this picture with network namespaces: two APs (`192.0.2.2`, `192.0.2.3`) with
`gretap` and VLANs 1000 and 10, one client per VLAN behind each AP, and dnsmasq on
`br-radius` and `br-hotspot` with the configuration above. All four clients run `udhcpc`
at the same time, and their DHCP broadcasts teach `softgred` the tunnels and client MACs.
The test checks that:

- every client got the address range, mask, gateway and DNS of its own bridge;
- every client pings its gateway through the tunnel;
- the clients of AP A ping the clients of AP B on the same VLAN, through both tunnels and
  the learned-MAC firewall rules;
- the API reports the learned MACs and the bridge attachments;
- the lease hook logged the right AP and tunnel for each lease;
- AP B's option 82 reaches dnsmasq: its hotspot client gets the per-AP DNS `9.9.9.9`, and
  the hook logs its circuit-id and remote-id. `udhcpc -x 0x52:...` stands in for the AP's
  relay agent.

```sh
docker build -f docker/Dockerfile.test -t softgre-test .
docker run --rm --privileged -e SOFTGRE_TEST_NAMESPACE=isolated softgre-test \
  python3 tests/dhcp_integration.py /usr/sbin/softgred
```

```text
4 DHCP leases through 2 learned tunnels in 1.24s
sg-ap2-c1000: 02:10:00:00:02:01 via 192.0.2.2/sgre1.1000 -> br-radius ip=10.100.0.101 mask=255.255.255.0 dns=1.1.1.1
sg-ap2-c10: 02:10:00:00:02:02 via 192.0.2.2/sgre1.10 -> br-hotspot ip=172.16.11.142 mask=255.255.254.0 dns=8.8.8.8
sg-ap3-c1000: 02:10:00:00:03:01 via 192.0.2.3/sgre0.1000 -> br-radius ip=10.100.0.100 mask=255.255.255.0 dns=1.1.1.1
sg-ap3-c10: 02:10:00:00:03:02 via 192.0.2.3/sgre0.10 -> br-hotspot ip=172.16.10.137 mask=255.255.254.0 dns=9.9.9.9
DHCP integration: per-bridge networks, masks, gateways and DNS passed
```

`--privileged` is needed to create network namespaces. Every interface and namespace
lives inside the disposable container.
