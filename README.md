<img src="docs/assets/softgre-logo.svg" width="144" alt="SoftGRE tunnel logo">

# SoftGRE

SoftGRE discovers IPv4 GRE Ethernet peers, creates `gretap` and VLAN interfaces, and
attaches them to Linux bridges. A Python dashboard shows learned MACs, tunnel interfaces,
bridge membership and packet counters. Packet capture uses AF_PACKET with a kernel BPF
filter; interface management uses native NETLINK_ROUTE.

- [Complete English guide](docs/en_US/index.md)
- [Documentation website](https://jpereira.github.io/softgre/)
- [Debian/Ubuntu releases](https://github.com/jpereira/softgre/releases)

## Run the Ubuntu 24.04 lab

Use Docker on a Linux host. The examples use the `homelab` Docker context; omit that
option for a local Linux Docker engine. Replace `192.168.1.253` with the Docker host's
main IPv4 address. The lab creates its interfaces inside the container's network
namespace and requires only `NET_ADMIN` and `NET_RAW`.

```sh
git clone https://github.com/jpereira/softgre.git
cd softgre
docker --context homelab build -f Dockerfile.lab -t softgre-lab:ubuntu24.04 .
docker --context homelab run -d --name test-softgre --restart unless-stopped --cap-add NET_ADMIN --cap-add NET_RAW -p 192.168.1.253:8080:8080 softgre-lab:ubuntu24.04
```

Choose an unused container name and port. Open `http://192.168.1.253:8080/index.html`.
The daemon and unprivileged Python backend start together. The initial state has capture
interface `sg-test-in` at `192.0.2.1/24`, injection peer `sg-test-peer`, and bridges
`br-radius` and `br-hotspot`. No remote tunnels are learned until packets arrive.

## Watch a complete provisioning flow

Send GRE Ethernet/VLAN frames from three synthetic clients:

```sh
docker --context homelab exec test-softgre python3 /opt/softgre-lab/send.py
```

The script sends five packets for each client, waits for MAC learning and verifies
bridge attachment using the live HTTP API and kernel interface data. On a fresh lab,
the resulting topology is:

```text
Remote 192.0.2.2 -> sgre0
  MACs: 02:00:00:00:00:42, 02:00:00:00:00:43
  sgre0.1000 -> br-radius
  sgre0.10   -> br-hotspot

Remote 192.0.2.3 -> sgre1
  MAC: 02:00:00:00:00:44
  sgre1.1000 -> br-radius
  sgre1.10   -> br-hotspot
```

Each new remote receives every configured VLAN interface, not only the VLAN from its
first packet. Interface slot names depend on which names are free. Confirm the result:

```sh
docker --context homelab exec test-softgre ip -d link show type gretap
docker --context homelab exec test-softgre bridge link show
docker --context homelab exec test-softgre brctl show
docker --context homelab exec test-softgre /opt/softgre-lab/status.sh
```

The dashboard refreshes every five seconds and shows two tunnels, three learned MACs,
VLAN membership and RX/TX counters. Add another remote with a unique source MAC:

```sh
docker --context homelab exec test-softgre python3 /opt/softgre-lab/send.py --remote 192.0.2.4 --mac 02:00:00:00:00:45 --vlan 10 --count 20
```

Follow the event log, which includes IPv4 addresses, created interfaces, bridges and MACs:

```sh
docker --context homelab exec test-softgre tail -f /var/log/softgred.log
```

Example event messages, excluding their timestamp/PID/severity prefix:

```text
Tunnel created remote=192.0.2.2 local=192.0.2.1 interface=sgre0 ingress=sg-test-in
VLAN attached remote=192.0.2.2 interface=sgre0.1000 vlan=1000 bridge=br-radius
Client learned remote=192.0.2.2 mac=02:00:00:00:00:42 tunnel=sgre0 clients=1
```

Clear learned state with `SIGUSR1`, then inject again to watch reprovisioning:

```sh
docker --context homelab exec test-softgre sh -c 'kill -USR1 "$(cat /run/softgre-lab/softgred.pid)"'
docker --context homelab exec test-softgre python3 /opt/softgre-lab/send.py
```

This lab tests real GRE decoding, kernel provisioning and bridge attachment with
synthetic peers. It does not provide a separate remote router or a complete subscriber
LAN. Interface packet counters include all their traffic. Stop the persistent lab with
`docker --context homelab stop test-softgre`.

## Build and run on Linux

```sh
sudo apt-get install build-essential autoconf automake pkg-config libglib2.0-dev ebtables iptables iproute2 python3
./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var --with-piddir=/run --enable-web
make -j2
make check
sudo make install
```

Edit `/etc/softgred/softgred.conf` for an interface with a local IPv4 address:

```ini
[global]
interface = eth0
tunnel-prefix = sgre
bridge-map = 1000@br-radius,10@br-hotspot
bridge-force = true
pid-file = /run/softgred.pid
log-file = /var/log/softgred.log

[service]
bind-in = lo
port = 8888
max-listen = 32
```

Start the daemon and the Python backend in separate terminals:

```sh
sudo /usr/sbin/softgred -f
python3 /usr/share/softgred/webinterface/server.py --bind 0.0.0.0 --port 8080
```

Open `http://SERVER_IP:8080/index.html`. The TCP API remains on loopback. The dashboard
has no authentication; expose it on a trusted management network or through an
access-controlled reverse proxy. See the guide for automatic startup with the optional
`softgred-web.service`, log rotation, package downloads and persistent container storage.

## Protocol scope

Accepted traffic uses outer IPv4, unkeyed GRE v0 with zero flags, Ethernet protocol
`0x6558`, and one inner VLAN mapped to a configured bridge. The parser supports up to
two outer VLAN tags and IPv4 options. Outer IPv6, IP fragments, GRE keys/checksums,
untagged inner Ethernet and inner QinQ are unsupported. A MAC belongs to one remote;
learning does not provide authentication, automatic roaming or idle expiry.
## Relationship to Alcatel-Lucent/Nokia 7750 SR

SoftGRE implements packet-driven GRE discovery and Linux bridging. The corresponding
7750 SR feature is WLAN-GW soft-GRE. Nokia describes learning AP encapsulation without
configuring every AP endpoint. Its WLAN-GW design uses ISA/ESA resources; available
features depend on the platform and SR OS release. See the official
[Wi-Fi aggregation and offload guide](https://documentation.nokia.com/sr/26-3/7750-sr/books/tpsda/wi-fi-aggregation-offload.html).

| Area | 7750 SR WLAN-GW soft-GRE | This project |
| --- | --- | --- |
| Peer discovery | Learns AP encapsulation without a configured IP per AP. | Learns remote IPv4 and MACs from accepted packets. |
| Encapsulation | Ethernet and MPLS-over-GRE access variants. | IPv4, GRE v0, flags zero, Ethernet `0x6558`, one inner VLAN. |
| VLAN mapping | Inner VLAN selects a retail service context. | Static `VLAN@bridge` mappings create Linux VLAN interfaces. |
| Forwarding | WLAN-GW processing on ISA/ESA resources. | Native `gretap`, VLANs and bridges forward in the Linux kernel. |
| Subscriber management | ESM and RADIUS integration. | MAC permissions; no built-in RADIUS, DHCP server or subscriber billing. |
| Mobility | Inter-AP mobility anchoring. | A MAC stays with one remote until learned state is cleared. |
| QoS | Per-tunnel/per-SSID shaping. | No built-in bandwidth shaping. |
| Availability | WLAN-GW standby/redundancy functions. | One daemon; no state replication or automatic failover. |

The subscriber-management example is documented in Nokia's
[Basic Open SSID configuration guide](https://documentation.nokia.com/acg/25-3-2/books/tpsda-classic/c091-wifi-aggoffbasicssid.html).
For residential bridging, Nokia documents soft-GRE access in its
[Virtual Residential Gateway guide](https://documentation.nokia.com/sr/25-10/7750-sr/books/tpsda/virtual-residential-gateway-triple-play.html).
This project's service boundary is the Linux bridge; external routing, DHCP and subscriber
services must be designed separately. No interoperability or throughput comparison with
a physical 7750 SR has been measured. Matching the encapsulation subset alone does not
establish feature parity. Compile-time ceilings are 4096 tunnel slots, 1024 MACs per tunnel
and ten VLAN-to-bridge mappings; free interface names and host resources affect capacity.

## Validation

```sh
scripts/check-linux.sh
python3 tests/test_web.py
DOCKER_CONTEXT=homelab scripts/check-lab.sh
python3 scripts/build-docs.py
python3 tests/test_docs.py
```

The full compiler matrix needs GCC, Clang, sanitizer runtimes and Python. The CI covers
packet parsing, configuration, firewall ownership, signals, rollback, real GRE/VLAN
forwarding, log rotation, both web adapters, packages and the lab's restart lifecycle.

Licensed under LGPL-3.0-or-later; see [LICENSE.md](LICENSE.md) and [COPYING](COPYING).
