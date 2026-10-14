# Overview

SoftGRE is a self-provisioning GRE Ethernet concentrator for Linux. Access points (or any
CPE) tunnel their subscribers' VLAN-tagged Ethernet frames over IPv4 GRE to one server.
You never configure a tunnel per AP: when traffic from a new AP arrives, `softgred`
creates its `gretap` and VLAN interfaces, attaches them to your Linux bridges and opens the
bridge firewall for each client MAC it sees. This is the "soft-GRE" model used for Wi-Fi
aggregation and hotspots, built from standard kernel parts.

![SoftGRE traffic flow: APs tunnel VLAN-tagged client traffic over IPv4 GRE to softgred, which creates gretap and VLAN interfaces, attaches them to bridges and allows learned MACs, with an optional DHCP server per bridge](../assets/softgre-flow.svg)

## How it works

1. **The AP side is static.** Each AP has a `gretap` toward the concentrator's IPv4
   address and one 802.1Q VLAN per service, for example VLAN 1000 for a RADIUS-backed SSID
   and VLAN 10 for an open hotspot. Clients are bridged into those VLANs.
2. **Passive discovery.** `softgred` reads a copy of the GRE traffic on the capture
   interface with AF_PACKET and a kernel BPF filter. It accepts only GRE v0 carrying
   VLAN-tagged Ethernet (`0x6558`) addressed to its own IPv4 address, with a mapped VLAN.
3. **On-demand provisioning.** The first packet from a new remote creates `sgreN` for it
   through native NETLINK_ROUTE, plus one VLAN interface for every `bridge-map` entry,
   each attached to its bridge (`sgreN.1000` to `br-radius`, `sgreN.10` to `br-hotspot`).
4. **MAC learning and firewall.** Each new source MAC is bound to the remote it came from
   and gets an ebtables accept rule in the daemon's own `SOFTGRE_IN` and `SOFTGRE_OUT`
   chains. Other traffic on tunnel ports is dropped. Broadcast and multicast toward tunnels
   stay allowed, so ARP, DHCP and neighbor discovery work. Global firewall rules are
   untouched.
5. **The kernel forwards.** After provisioning, frames flow through `gretap`, VLANs and
   bridges entirely in the kernel. The daemon only handles discovery.
6. **Services sit behind the bridges.** Each bridge is a separate service network. Routing,
   NAT and an optional DHCP server live there, so every VLAN can hand out its own subnet,
   mask, gateway and DNS.

## What you get

- `softgred`, a small C daemon with a bounded packet parser and one `poll` loop.
- A read-only TCP API and a Python dashboard with tunnels, learned MACs, bridge membership
  and packet counters.
- An event log of every tunnel, VLAN attachment and learned client, with logrotate support.
- A dnsmasq lease hook that records which AP and tunnel each DHCP lease came through.
- Debian and Ubuntu packages, a production container image and systemd units.
- Docker labs, from a single container with synthetic packets to a gateway with simulated
  APs sending real GRE and requesting DHCP leases.

## Scope

SoftGRE accepts IPv4 outer transport, unkeyed GRE v0 and one inner VLAN per frame.
Learning a MAC is discovery, not authentication: restrict GRE to trusted remotes. There is
no built-in RADIUS, DHCP server, shaping or failover; those services attach to the
bridges. See [Architecture and security](architecture.md) for the full protocol scope and
trust model.

## Where to go next

- [Try it with Docker](try-it.md): run a gateway with simulated APs, or the single-container
  lab, without touching your host network.
- [DHCP and subscriber services](dhcp.md): one network per bridge, the lease hook and DHCP
  option 82.
- [Install and deploy](install.md): packages, container image, source builds and systemd.
- [Configuration and operation](configuration.md): settings, signals, logs and
  troubleshooting.
- [API and dashboard](api.md): TCP commands and the Python and PHP adapters.
- [Architecture and security](architecture.md): capture, provisioning, firewall and limits.
- [Development and testing](development.md): test suites, formatting, releases and this
  site.
