# SoftGRE documentation

SoftGRE is a Linux daemon that discovers IPv4 GRE Ethernet peers and provisions local
gretap interfaces. VLAN interfaces connect each remote to configured Linux bridges. The
kernel forwards the traffic; the daemon observes packets to discover peers and source MAC
addresses.

## Quick start

Use a Linux host with IPv4, bridge, 802.1Q, GRE/gretap, AF_PACKET and bridge firewall
support. Build dependencies are a C11 compiler, Make, Autoconf, Automake, pkg-config and
GLib development headers. Runtime dependencies are GLib and ebtables, with the
corresponding kernel support.

```sh
sudo apt-get update
sudo apt-get install build-essential autoconf automake pkg-config libglib2.0-dev ebtables iptables iproute2
git clone https://github.com/jpereira/softgre.git
cd softgre
./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var
make -j2
make check
sudo make install
sudoedit /etc/softgred/softgred.conf
sudo /usr/sbin/softgred -f
```

Select the interface whose IPv4 address terminates GRE. Configure the VLAN-to-bridge
mapping before starting. Set `bridge-force=true` to create missing bridges, or create them
yourself with `ip link add br-gre type bridge`. Existing bridge interfaces are brought up
and retained on exit. The daemon must run as root. Run in the foreground under a service
manager to collect stderr logs.

## Architecture

The capture socket is Linux `AF_PACKET`, bound to the configured interface, without
promiscuous mode. A classic BPF filter selects IPv4 GRE in the kernel, including up to two
outer VLAN tags. The bounded parser validates Ethernet, IPv4 lengths and options,
fragmentation, GRE flags and protocol, inner VLAN ID and source MAC. Only packets
addressed to the capture interface's selected IPv4 address can trigger provisioning.
Outgoing packets are ignored.

For each new remote, native `NETLINK_ROUTE` requests create an unkeyed gretap interface
and a VLAN interface per configured mapping. Requests include kernel acknowledgments,
bounded attributes and a three-second receive timeout. GLib hash tables index remote IPv4
addresses and MAC addresses. One `poll` loop owns capture, API clients and `signalfd`
signals, so readers never race deletions. The capture loop handles at most 64 packets per
batch and the API accepts at most 16 clients per iteration. Network and firewall setup
happen synchronously for new peers and MACs.

Kernel bridges and gretap forward established traffic. Repeated packets from known MACs
use hash lookups and do not spawn commands or recreate interfaces. ebtables receives
explicit argument arrays, never shell command strings. Each external command has a
three-second deadline.

## Configuration

The default configuration is `/etc/softgred/softgred.conf` when built with
`--sysconfdir=/etc`. A default `/usr/local` installation uses
`/usr/local/etc/softgred/softgred.conf` instead.

```ini
[global]
interface = eth0
tunnel-prefix = sgre
bridge-map = 1000@br-radius, 10@br-hotspot, 11@br-radius
bridge-force = true
pid-file = /var/run/softgred.pid

[service]
bind-in = lo
port = 8888
max-listen = 10

[debug]
enable = false
print-time = true
```

| Key | Meaning / default |
| --- | --- |
| `global.interface` | Required capture interface, with a usable local IPv4 address. |
| `global.tunnel-prefix` | Interface prefix, default `sgre`, maximum six characters. |
| `global.bridge-map` | Required comma-separated `VLAN@bridge` entries, up to ten. |
| `global.bridge-force` | Create missing bridges, default `false`. |
| `global.pid-file` | Locked PID file, default under the configured PID directory. |
| `service.bind-in` | Bind to interface IPv4 address; `lo` by default, `any` binds all. |
| `service.port` | TCP port, `1..65535`, default `8888`. |
| `service.max-listen` | Simultaneous clients, `1..128`, default `10`. |
| `debug.enable` | Enable level-one diagnostic logs, default `false`. |
| `debug.print-time` | Timestamp logs, default `false`; `debug.time` is also accepted. |

VLAN IDs are `1..4094`, unique within the mapping. Interface names contain ASCII letters,
digits, underscores, dots or hyphens and fit within 15 characters. The six-character
prefix limit leaves room for the tunnel slot and the four-digit VLAN suffix. A map entry
uses an existing Linux bridge or a bridge created when `bridge-force=true`. Several VLANs
may target the same bridge, which joins those broadcast domains deliberately. Empty
entries, malformed numbers and duplicate VLANs fail configuration loading. All optional
groups and keys can be omitted independently.

Strings and booleans use their actual C types. Booleans accept `true`, `false`, `1` and
`0`. Logs go to stderr. Debug levels are selected with repeated `-d`, up to four.
Configuration is loaded at startup; apply changes by restarting the daemon. Unrecognized
keys are ignored.

## CLI and lifecycle

```text
softgred [-f] [-c FILE] [-p FILE] [-d...] [-t] [-h] [-v]
```

| Option | Behavior |
| --- | --- |
| `-f`, `--foreground` | Foreground execution, recommended with systemd or containers. |
| `-c`, `--config-file` | Configuration path. |
| `-p`, `--pid-file` | Override the configuration's PID path. |
| `-d`, `--debug` | Increase logging level, up to four. |
| `-t`, `--debug-time` | Timestamp logs. |
| `-h`, `--help` | Help without root or configuration loading. |
| `-v`, `--version` | Version without root or configuration loading. |

A PID file uses an exclusive advisory lock held throughout the daemon's lifetime and
refuses symlinks. Background startup reports success only after capture, API, bridges and
firewall are ready. The foreground process returns failure when startup or capture fails.
`SIGINT` and `SIGTERM` stop the daemon, remove owned tunnels and firewall chains, and
remove the PID file. `SIGUSR1` removes all provisioned tunnels, MAC permissions and
indexes; discovery remains active. `SIGUSR2` prints tunnel, MAC, packet, ignored-packet
and provisioning-failure counters to stderr.

Existing unrelated interfaces and firewall rules remain in place. Dynamically created
target bridges remain after exit. Tunnel slots are reused after successful cleanup; names
already present in the network namespace are skipped. There is no automatic idle expiry or
implicit MAC mobility. A MAC can belong to only one remote, and its permission covers
configured VLANs on that tunnel. Restarting or `SIGUSR1` clears the learned ownership
before a MAC can be learned elsewhere.

## Read-only TCP API

The API listens on loopback by default. It has no authentication or encryption. Keep it on
loopback or a trusted management interface and enforce access with the host firewall.
`any` exposes it on all IPv4 interfaces. Compile with `--disable-service` to omit the TCP
listener.

Each connection accepts one command terminated by LF, CRLF or a TCP write-half-close. The
server sends a response and closes. Send a newline: partial reads are accumulated safely.
Commands are case-sensitive and matched exactly. Each client has a five-second total
deadline, a 255-byte request limit and a 1 MiB response limit. Excess connections are
closed.

```sh
printf 'STUN\n' | nc 127.0.0.1 8888
printf 'LMIP 192.0.2.2\n' | nc 127.0.0.1 8888
printf 'GTMC 02:00:00:00:00:42\n' | nc 127.0.0.1 8888
```

| Command | Response |
| --- | --- |
| `HELP` | Human-readable command list. |
| `VERS` | `RESULT: OK`, `BODY: version`. |
| `QUIT` | Close without a body. |
| `STUN` | `RESULT: OK`, `BODY: remote@interface;...`, or `RESULT: NULL`. |
| `LMIP IPv4` | `RESULT: OK`, `BODY: mac;mac;...`, or `RESULT: NOTFOUND`. |
| `GTMC MAC` | `RESULT: OK`, `BODY: remote@interface`, or `RESULT: NOTFOUND`. |
| `STAT` | `RESULT: OK`, `BODY: OS;version;architecture;uptime;daemon_version;started`. |

Successful responses contain `RESULT: OK` and a `BODY:` line. Invalid commands or
arguments produce `RESULT: INVALID`. MAC queries accept exactly six colon-separated
hexadecimal octets, including uppercase. LMIP returns MACs in lexical order; STUN ordering
is unspecified. Uptime is in seconds; `started` is the Unix timestamp when configuration
was loaded. The optional PHP interface forwards these read-only commands and provides a
small dependency-free dashboard.

## Optional PHP dashboard

Build with `--enable-web` to install the read-only dashboard under
`/usr/share/softgred/webinterface` for a `/usr` prefix. Serve that directory with PHP 8.1
or newer on a trusted management network. Frontend assets are static and require no Node
build. The adapter uses `SOFTGRE_API_HOST` and `SOFTGRE_API_PORT` environment variables,
defaulting to `127.0.0.1:8888`. The host must be an IPv4 literal; the port must be
`1..65535`.

```sh
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-web
make -j2
sudo make install
php -S 127.0.0.1:8080 -t /usr/share/softgred/webinterface
```

The PHP development server is a local preview. In production, configure PHP-FPM with your
web server and restrict access. The adapter accepts GET only and validates IP/MAC
arguments before sending a newline-framed command. Connection timeout is two seconds,
response deadline is five seconds and responses are capped at 1 MiB. It returns JSON with
`status` and `body` fields.

| Endpoint | TCP command |
| --- | --- |
| `api.php?action=status` | `STAT` |
| `api.php?action=tunnels` | `STUN` |
| `api.php?action=ues&arg=192.0.2.2` | `LMIP 192.0.2.2` |
| `api.php?action=cpe&arg=02:00:00:00:00:42` | `GTMC 02:00:00:00:00:42` |
| `api.php?action=version` | `VERS` |
| `api.php?action=help` | `HELP` |

Invalid requests return HTTP 400, unsupported methods 405, backend failures 502 and
response timeouts 504. The dashboard renders responses as text, never HTML. Neither
dashboard nor adapter provides authentication. The default daemon/package/container builds
do not install the dashboard. `--enable-web` requires the TCP service to be enabled. Run
`python3 tests/test_web.py` with PHP installed to test the adapter using a fake backend
and real HTTP connections.

## Firewall and trust model

SoftGRE owns the `SOFTGRE_IN` and `SOFTGRE_OUT` ebtables chains. It adds FORWARD jumps
scoped to each owned tunnel's VLAN interfaces, permits learned source/destination MACs,
and drops other matching traffic. Egress broadcast/multicast is permitted for ARP, DHCP
and neighbor discovery. It never flushes FORWARD or changes its global policy. Startup
refuses preexisting owned chain names, preventing accidental takeover of another instance
or abandoned firewall state. Each learned MAC installs one ingress and one egress rule.
Cleanup revokes permissions and removes jumps before deleting owned chains. Packet-derived
values never enter a shell.

GRE has no built-in authentication. Observing a source MAC is discovery, not identity
verification. Use ingress anti-spoofing and restrict IP protocol 47 to trusted remotes or
a protected transport. Packets with an unknown VLAN, foreign destination,
zero/multicast/loopback source IPv4, invalid source MAC, fragmented IPv4, keyed GRE, GRE
optional fields or nonzero GRE version cannot provision. Accepted payloads are VLAN-tagged
Ethernet in GRE protocol `0x6558`; inner ARP, IPv4 and IPv6 are forwarded by the kernel.
IPv6 outer transport, untagged inner Ethernet, inner QinQ, keyed GRE and GRE checksums are
not supported. The parser does not authenticate or validate inner IP payloads. The parser
governs discovery; established traffic is forwarded by the kernel and controlled by bridge
firewall rules.

Limits are 4096 tunnels and 1024 MACs per tunnel. Capture buffering requests 4 MiB (the
kernel may cap it). A setup failure backs off new work globally for one second to bound
repeated failures. Kernel deletion failures keep records for another cleanup attempt. An
incomplete full cleanup pauses discovery until a successful SIGUSR1 retry. Full cleanup
flushes owned MAC chains in bulk so it does not launch a process per removed MAC. After a
crash or forced kill, inspect owned interfaces and chains before restarting; never flush
unrelated firewall state.

## Linux deployment

The supplied `share/softgred.service` runs in the foreground with `CAP_NET_ADMIN` and
`CAP_NET_RAW`, collects logs in the journal and starts after the network is online.
Install the unit manually when using `make install`; Debian packaging installs its systemd
unit automatically.

```sh
sudo install -m 644 share/softgred.service /etc/systemd/system/softgred.service
sudo systemctl daemon-reload
sudo systemctl enable --now softgred
sudo journalctl -u softgred -f
```

The unit expects `/usr/sbin/softgred` and `/etc/softgred/softgred.conf`. Configure a
network-address ready dependency for your network manager if `network-online.target` alone
does not guarantee the selected interface is ready. Only one instance should own a given
network namespace.

```sh
docker build -t softgre .
docker run --rm --network host --cap-add NET_ADMIN --cap-add NET_RAW \
  -v /absolute/path/softgred.conf:/etc/softgred/softgred.conf:ro softgre
```

The host-network example configures the Linux host's networking. Use a disposable
namespace for experiments. The host kernel must support GRE, VLANs, bridges and ebtables;
the container cannot supply missing kernel modules. The production image contains the
daemon, GLib and ebtables and runs in the foreground. Build and run Docker on Linux; a
macOS client can target a remote Docker context.

## Releases and Debian/Ubuntu packages

Version 2.0.1 is defined in `VERSION`. Autoconf reads that file; `bump.sh` also updates
the Debian changelog and version references in the bilingual guides. Download
distribution-specific amd64 packages from the GitHub release:

[Download SoftGRE 2.0.1](https://github.com/jpereira/softgre/releases/tag/v2.0.1)

| Distribution | Release asset |
| --- | --- |
| Debian 12 | `softgred_2.0.1-1+debian12_amd64.deb` |
| Debian 13 | `softgred_2.0.1-1+debian13_amd64.deb` |
| Ubuntu 22.04 | `softgred_2.0.1-1+ubuntu22.04_amd64.deb` |
| Ubuntu 24.04 | `softgred_2.0.1-1+ubuntu24.04_amd64.deb` |

Download the package matching your distribution and `SHA256SUMS`, verify its hash and
install with `sudo apt install ./softgred_2.0.1-1+debian12_amd64.deb` (adjust the filename
for your system). The package installs the daemon, configuration, guides and systemd unit,
with no automatic service start. Configure the daemon, then enable it with `sudo systemctl
enable --now softgred`.

```sh
./bump.sh --check
./bump.sh patch
```

Use `major`, `minor`, `patch` (default) or an explicit version as the argument. Each bump
must increase the version and use `MAJOR.MINOR.PATCH` without leading zeroes. Bumping
updates source metadata; it does not commit, tag or push. Commit the bump, then create and
push a matching `vX.Y.Z` tag. The release workflow validates the tagged source with the
full Linux CI, builds and tests each .deb inside its matching distribution image, and
uploads packages plus `SHA256SUMS` to the GitHub release only after all jobs succeed.
Manual reruns accept an existing tag, verify its version and replace matching assets. The
website deploys from the default branch.

```sh
release_version=$(cat VERSION)
git tag "v$release_version"
git push origin master "v$release_version"
# Local package build inside a matching disposable Linux container:
scripts/build-deb.sh debian12 /out
```

Package filenames include the target distribution to prevent collisions. Package
architecture is the build host architecture; the supplied release workflow builds amd64.
Build another architecture natively if needed. The workflow does not upload debug-symbol
packages. Release downloads and checksums are public GitHub assets and do not require a
package repository.

## Testing and performance

`make check` exercises the bounded parser, every truncation of valid packet fixtures, IPv4
options, outer VLAN tags, invalid GRE formats, seeded malformed inputs, typed
configuration, the first bridge mapping, hash indexes, client capacity, rollback, PID
locks, API commands, TCP fragmentation and peer disconnects. Tests use mock kernel
operations for state-machine faults and real loopback sockets for the API. `test-netlink`
checks encoded attributes, nesting and message bounds.

```sh
sudo apt-get install clang python3
scripts/check-linux.sh
```

The validation script builds out of tree with GCC and Clang and `-Werror`, runs
ASan/UBSan, checks `--disable-service`, CLI exit codes and staged installation, then runs
`make distcheck`. Integration tests require a disposable Linux network namespace and root
networking privileges:

```sh
docker build -f Dockerfile.test -t softgre-test .
docker run --rm --cap-add NET_ADMIN --cap-add NET_RAW \
  -e SOFTGRE_TEST_NAMESPACE=isolated softgre-test \
  python3 tests/integration.py bin/softgred
```

The integration suite verifies real gretap and VLAN interfaces, bridge attachment, BPF
capture, 10000 repeated packets without duplicate rules, outer VLANs, API framing, signal
cleanup, unrelated firewall preservation, PID locking and background startup failures. It
prints the packet injection duration; this is a repeatable smoke measurement, not a
forwarding throughput benchmark. CI runs compiler/sanitizer/distribution checks and the
isolated kernel integration suite.

Steady-state discovery uses average O(1) IPv4/MAC lookups. Initial setup needs netlink
round trips and two firewall processes per MAC. Those operations temporarily block the
owner loop, each for at most its timeout. For high churn or millions of learned MACs,
measure setup latency and ebtables rule traversal as well as packet drops before choosing
a deployment size. AF_PACKET still copies captured packets into user space; the daemon
does not use PACKET_MMAP or claim zero-copy capture.

## Troubleshooting and documentation site

| Symptom | Check |
| --- | --- |
| Build fails | Install GLib development headers, compiler and Autotools; build on Linux. |
| Capture fails | Verify root, interface existence, IPv4 address and NET_RAW capability. |
| Bridge startup fails | Ensure the target is a Linux bridge or enable `bridge-force`. |
| No tunnels appear | Check GRE destination, protocol 47, unkeyed TEB payload and mapped inner VLAN. |
| MAC does not move | Clear learned ownership with SIGUSR1 or restart before learning another remote. |
| Firewall chain exists | Inspect abandoned SOFTGRE chains and tunnel jumps; stop other instances first. |
| Traffic stalls | Check underlay MTU, GRE overhead, bridge firewall and route to the remote. |
| API times out | Terminate commands with newline; verify bind interface, port and firewall. |
| PID lock fails | Check for a running instance and PID directory permissions; avoid symlink paths. |

GRE encapsulation consumes underlay MTU. The kernel selects the tunnel MTU; inspect it
with `ip -d link show` and configure the underlay and clients accordingly. SoftGRE does
not change the host's routes, global IP forwarding policy or kernel module configuration.

The documentation site is built from the pt_BR and en_US Markdown guides with Python's
standard library. No Node, third-party assets, tracking, remote fonts or external
JavaScript are required.

```sh
python3 scripts/build-docs.py
python3 tests/test_docs.py
python3 -m http.server 8000 --directory _site
```

The Pages workflow builds `_site` and publishes it through GitHub Actions on the default
branch. The CI workflow validates documentation for every push and pull request. Edit both
language guides when behavior changes. The site builder supports headings, paragraphs,
lists, tables, fenced code, inline code and Markdown links; it escapes raw HTML. The
source license is LGPL-3.0-or-later.
