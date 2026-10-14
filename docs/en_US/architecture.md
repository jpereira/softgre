# Architecture and security

## Discovery and provisioning

The capture socket is Linux `AF_PACKET`, bound to the configured interface without
promiscuous mode. A classic BPF filter selects IPv4 GRE in the kernel, including up to two
outer VLAN tags. The bounded parser validates Ethernet, IPv4 lengths and options,
fragmentation, GRE flags and protocol, inner VLAN ID and source MAC. Only packets
addressed to the capture interface's IPv4 address can trigger provisioning; outgoing
packets are ignored.

For each new remote, native `NETLINK_ROUTE` requests create an unkeyed gretap interface
and a VLAN interface per configured mapping. Requests include kernel acknowledgments,
bounded attributes and a three-second receive timeout. GLib hash tables index remote IPv4
addresses and MAC addresses. One `poll` loop owns capture, API clients and `signalfd`
signals, so readers never race deletions. The capture loop handles at most 64 packets per
batch and the API accepts at most 16 clients per iteration. Network and firewall setup
happen synchronously for new peers and MACs.

Kernel bridges and gretap forward established traffic. Repeated packets from known MACs
use hash lookups and do not spawn commands or recreate interfaces. ebtables receives
explicit argument arrays, never shell command strings, and each external command has a
three-second deadline.

## Protocol scope

Accepted traffic uses outer IPv4, unkeyed GRE v0 with zero flags, Ethernet protocol
`0x6558` ([RFC 2784](https://www.rfc-editor.org/rfc/rfc2784)) and one inner VLAN mapped to
a configured bridge. Inner ARP, IPv4 and IPv6 are forwarded by the kernel. The parser
supports up to two outer VLAN tags and IPv4 options.

Packets with an unknown VLAN, a foreign destination, a zero, multicast or loopback source
IPv4, an invalid source MAC, IPv4 fragmentation, GRE keys, checksums or other optional
fields, or a nonzero GRE version cannot provision. Outer IPv6, untagged inner Ethernet
and inner QinQ are unsupported. The parser does not authenticate or validate inner IP
payloads.

## Firewall and trust model

SoftGRE owns the `SOFTGRE_IN` and `SOFTGRE_OUT` ebtables chains. It adds FORWARD jumps
scoped to each owned tunnel's VLAN interfaces, permits learned source and destination
MACs, and drops other matching traffic. Egress broadcast and multicast are permitted for
ARP, DHCP and neighbor discovery. It never flushes FORWARD or changes its global policy.
Startup refuses preexisting owned chain names, preventing accidental takeover of another
instance or abandoned firewall state. Each learned MAC installs one ingress and one egress
rule; cleanup revokes permissions and removes jumps before deleting owned chains.
Packet-derived values never enter a shell.

GRE has no built-in authentication, and observing a source MAC is discovery, not identity
verification. Use ingress anti-spoofing and restrict IP protocol 47 to trusted remotes or
a protected transport. The AP's own frames can also be learned, using one client slot per
tunnel. DHCP option 82 is only trustworthy when the AP strips client-supplied copies (see
[DHCP and subscriber services](dhcp.md#option-82-from-the-ap)).

## Limits and performance

Compile-time ceilings are 4096 tunnel slots, 1024 MACs per tunnel and ten VLAN-to-bridge
mappings; free interface names and host resources also affect capacity. Capture buffering
requests 4 MiB, which the kernel may cap. A setup failure backs off new work globally for
one second. Kernel deletion failures keep records for another cleanup attempt, and an
incomplete full cleanup pauses discovery until a successful `SIGUSR1` retry. Full cleanup
flushes owned MAC chains in bulk instead of launching a process per MAC.

Steady-state discovery uses average O(1) IPv4 and MAC lookups. Initial setup needs
netlink round trips and two firewall processes per MAC, which temporarily block the owner
loop, each for at most its timeout. For high churn or very large MAC populations, measure
setup latency, ebtables rule traversal and packet drops before choosing a deployment
size. AF_PACKET still copies captured packets into user space; the daemon does not use
PACKET_MMAP or claim zero-copy capture.

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
| Subscriber management | ESM and RADIUS integration. | MAC permissions and a DHCP lease hook; no built-in RADIUS, DHCP server or billing. |
| Mobility | Inter-AP mobility anchoring. | A MAC stays with one remote until learned state is cleared. |
| QoS | Per-tunnel/per-SSID shaping. | No built-in bandwidth shaping. |
| Availability | WLAN-GW standby/redundancy functions. | One daemon; no state replication or automatic failover. |

The subscriber-management example is documented in Nokia's
[Basic Open SSID configuration guide](https://documentation.nokia.com/acg/25-3-2/books/tpsda-classic/c091-wifi-aggoffbasicssid.html).
For residential bridging, Nokia documents soft-GRE access in its
[Virtual Residential Gateway guide](https://documentation.nokia.com/sr/25-10/7750-sr/books/tpsda/virtual-residential-gateway-triple-play.html).
This project's service boundary is the Linux bridge; routing, DHCP and subscriber
services are designed separately. No interoperability or throughput comparison with a
physical 7750 SR has been measured, and matching the encapsulation subset alone does not
establish feature parity.
