<img src="docs/assets/softgre-logo.svg" width="144" alt="SoftGRE tunnel logo">

# SoftGRE

SoftGRE is a self-provisioning GRE Ethernet concentrator for Linux. Access points (or any
CPE) tunnel their subscribers' VLAN-tagged Ethernet frames over IPv4 GRE to one server.
You never configure a tunnel per AP: when traffic from a new AP arrives, `softgred`
creates its `gretap` and VLAN interfaces, attaches them to your Linux bridges and opens the
bridge firewall for each client MAC it sees. This is the "soft-GRE" model used for Wi-Fi
aggregation and hotspots, built from standard kernel parts.

![SoftGRE traffic flow](docs/assets/softgre-flow.svg)

**[Documentation website](https://jpereira.github.io/softgre/)** ·
[Releases](https://github.com/jpereira/softgre/releases) ·
[Container image](https://github.com/jpereira/softgre/pkgs/container/softgre)

## How it works

1. **The AP side is static.** Each AP has a `gretap` toward the concentrator's IPv4
   address and one 802.1Q VLAN per service (for example VLAN 1000 for a RADIUS-backed SSID
   and VLAN 10 for an open hotspot).
2. **Passive discovery.** `softgred` reads a copy of the GRE traffic with AF_PACKET and a
   kernel BPF filter, accepting only GRE v0 with VLAN-tagged Ethernet for a mapped VLAN.
3. **On-demand provisioning.** The first packet from a new remote creates `sgreN` through
   native NETLINK_ROUTE, plus one VLAN interface per `bridge-map` entry, each attached to
   its bridge (`sgreN.1000 -> br-radius`, `sgreN.10 -> br-hotspot`).
4. **MAC learning and firewall.** Each new client MAC is bound to its remote and allowed
   in the daemon's own ebtables chains; other tunnel traffic is dropped.
5. **The kernel forwards.** The kernel handles forwarding; the daemon observes captured
   GRE copies for discovery.
6. **Services sit behind the bridges.** Routing, NAT and DHCP attach to each bridge, so
   every VLAN can have its own subnet, mask, gateway and DNS.

A Python dashboard shows tunnels, learned MACs, bridges and counters; an event log
records every tunnel and client; a dnsmasq lease hook ties each DHCP lease to its AP.

## Try it with Docker

Run a gateway with the Ubuntu 24.04 packages, systemd, the dashboard and dnsmasq, then
start simulated APs that send real GRE from other containers and request DHCP leases:

```sh
git clone https://github.com/jpereira/softgre.git && cd softgre
docker build -f docker/Dockerfile.lab --target gateway -t softgre-gateway:ubuntu24.04 .
docker network create softgre-net
docker run -d --name softgre-gw --network softgre-net --privileged --cgroupns=private \
  --tmpfs /run --tmpfs /run/lock -p 8080:8080 softgre-gateway:ubuntu24.04
# wait for: docker inspect --format '{{.State.Health.Status}}' softgre-gw -> healthy
docker run -d --name ap1 --network softgre-net --privileged --no-healthcheck \
  --entrypoint /opt/softgre-lab/ap.sh softgre-gateway:ubuntu24.04 softgre-gw
docker logs ap1
```

```text
AP 172.21.0.3 -> gateway 172.21.0.2, tunnel softgre (02:a0:00:00:00:03)
c1000 VLAN 1000 MAC 02:10:00:00:03:01: ip=10.100.0.100/24 router=10.100.0.1 dns=1.1.1.1
c10 VLAN 10 MAC 02:10:00:00:03:02: ip=172.16.10.137/23 router=172.16.10.1 dns=8.8.8.8
AP ready
```

Open `http://localhost:8080/index.html` to see the tunnel and its clients. The
[Try it with Docker](https://jpereira.github.io/softgre/en_US/try-it.html) guide covers
more APs, option 82, logs, reprovisioning, cleanup and a single-container lab.

## Install

The C daemon links only against libc. No GLib headers (`glib.h`) or libraries are
required: the hash maps, string buffers, INI parser and process helpers ship in this
repository. ebtables handles bridge firewall rules; Python 3 runs the dashboard and lease
hook.

- **Debian and Ubuntu:** download the package for your distribution from the
  [latest release](https://github.com/jpereira/softgre/releases/tag/v2.0.4) and install it
  with `sudo apt install ./softgred_2.0.4-1+<distribution>_amd64.deb`.
- **Container:** `ghcr.io/jpereira/softgre:2.0.4` runs the daemon and dashboard with
  `--network host --cap-add NET_ADMIN --cap-add NET_RAW` and your `softgred.conf` mounted.
- **Source:** `./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var`, then
  `make`, `make check` and `sudo make install`.

Then set the capture interface and VLAN-to-bridge mappings in
`/etc/softgred/softgred.conf`:

```ini
[global]
interface = eth0
bridge-map = 1000@br-radius,10@br-hotspot
bridge-force = true
# GRE is unauthenticated; allow only your APs to provision (omit to accept any remote).
allow-remotes = 203.0.113.0/24
```

See [Install and deploy](https://jpereira.github.io/softgre/en_US/install.html) for
requirements, systemd units and the dashboard service.

## Documentation

| Page | Contents |
| --- | --- |
| [Overview](https://jpereira.github.io/softgre/en_US/index.html) | What SoftGRE is, how it works and its scope. |
| [Try it with Docker](https://jpereira.github.io/softgre/en_US/try-it.html) | Gateway with simulated APs and the single-container lab. |
| [DHCP and subscriber services](https://jpereira.github.io/softgre/en_US/dhcp.html) | One network per bridge, the lease hook and option 82. |
| [Install and deploy](https://jpereira.github.io/softgre/en_US/install.html) | Packages, container image, source builds and systemd. |
| [Configuration and operation](https://jpereira.github.io/softgre/en_US/configuration.html) | Settings, signals, logs and troubleshooting. |
| [API and dashboard](https://jpereira.github.io/softgre/en_US/api.html) | TCP commands and the Python and PHP adapters. |
| [Architecture and security](https://jpereira.github.io/softgre/en_US/architecture.html) | Capture, provisioning, firewall, limits and the 7750 SR comparison. |
| [Development and testing](https://jpereira.github.io/softgre/en_US/development.html) | Test suites, formatting, releases and the docs site. |

The pages are built from [`docs/en_US/`](docs/en_US/index.md), which also reads well on
GitHub.

## Development

```sh
docker/test.test.sh     # compilers, sanitizers, integration, DHCP, web, package, fuzzing
docker/test.format.sh   # formatting
docker/test.lab.sh      # lab and gateway with external APs
docker/test.openwrt.sh  # OpenWrt SDK build and rootfs smoke test
docker/test.all.sh      # every image, including release packages for each distribution
```

[Development and testing](https://jpereira.github.io/softgre/en_US/development.html) lists
every suite CI runs, the formatter and the release process.

Licensed under LGPL-3.0-or-later; see [LICENSE.md](LICENSE.md).
