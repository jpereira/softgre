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
log-file = /var/log/softgred.log

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
| `global.log-file` | Event log, default `/var/log/softgred.log`; absolute path required. |
| `global.pid-file` | Locked PID file, default under the configured PID directory. |
| `service.bind-in` | Bind to interface IPv4 address; `lo` by default, `any` binds all. |
| `service.port` | TCP port, `1..65535`, default `8888`. |
| `service.max-listen` | Simultaneous clients, `1..128`, default `10`. |
| `debug.enable` | Enable level-one diagnostic logs, default `false`. |
| `debug.print-time` | Timestamp stderr, default `false`; `debug.time` is also accepted. |

VLAN IDs are `1..4094`, unique within the mapping. Interface names contain ASCII letters,
digits, underscores, dots or hyphens and fit within 15 characters. The six-character
prefix limit leaves room for the tunnel slot and the four-digit VLAN suffix. A map entry
uses an existing Linux bridge or a bridge created when `bridge-force=true`. Several VLANs
may target the same bridge, which joins those broadcast domains deliberately. Empty
entries, malformed numbers and duplicate VLANs fail configuration loading. All optional
groups and keys can be omitted independently.

Strings and booleans use their actual C types. Booleans accept `true`, `false`, `1` and
`0`. Logs append to `/var/log/softgred.log` and are also sent to stderr. Debug levels are selected with repeated `-d`, up to four.
Configuration is loaded at startup; apply changes by restarting the daemon. Unrecognized
keys are ignored.

## CLI and lifecycle

```text
softgred [-f] [-c FILE] [-p FILE] [-l FILE] [-d...] [-t] [-h] [-v]
```

| Option | Behavior |
| --- | --- |
| `-f`, `--foreground` | Foreground execution, recommended with systemd or containers. |
| `-c`, `--config-file` | Configuration path. |
| `-p`, `--pid-file` | Override the configuration's PID path. |
| `-l`, `--log-file` | Override the event log path. |
| `-d`, `--debug` | Increase logging level, up to four. |
| `-t`, `--debug-time` | Timestamp stderr; file records always have timestamps. |
| `-h`, `--help` | Help without root or configuration loading. |
| `-v`, `--version` | Version without root or configuration loading. |

A PID file uses an exclusive advisory lock held throughout the daemon's lifetime and
refuses symlinks. Background startup reports success only after capture, API, bridges and
firewall are ready. The foreground process returns failure when startup or capture fails.
`SIGINT` and `SIGTERM` stop the daemon, remove owned tunnels and firewall chains, and
remove the PID file. `SIGUSR1` removes all provisioned tunnels, MAC permissions and
indexes; discovery remains active. `SIGUSR2` prints tunnel, MAC, packet, ignored-packet
and provisioning-failure counters to the event log and stderr. `SIGHUP` reopens the log
file without reloading configuration or removing learned state.

Existing unrelated interfaces and firewall rules remain in place. Dynamically created
target bridges remain after exit. Tunnel slots are reused after successful cleanup; names
already present in the network namespace are skipped. There is no automatic idle expiry or
implicit MAC mobility. A MAC can belong to only one remote, and its permission covers
configured VLANs on that tunnel. Restarting or `SIGUSR1` clears the learned ownership
before a MAC can be learned elsewhere.

## Event logs and rotation

The daemon appends to `/var/log/softgred.log` by default, in foreground and background
execution. Each record has local date/time with timezone, PID and severity. New files
have mode `0600`; messages are also mirrored to stderr for systemd and Docker.
`global.log-file` or `-l FILE` selects another absolute path. The parent directory must
exist. Startup fails if the log cannot be opened. Runtime write failures are reported on
stderr while network processing continues. Symlinks, hard links, special files, files
owned by another user and world-writable files are rejected.

Events cover startup/shutdown, bridge preparation, remote/local IPv4 addresses, GRE
interfaces, VLAN interfaces and their bridges, newly learned MACs, removals and failed
provisioning. Known-client traffic does not create a record for every packet. MAC
ownership remains with its original remote until learned state is cleared. GRE has no
connection handshake; learning represents successful packet-based discovery.

```sh
sudo tail -f /var/log/softgred.log
docker exec test-softgre tail -f /var/log/softgred.log
```

Event messages, after the timestamp/PID/severity prefix:

```text
Tunnel created remote=192.0.2.2 local=192.0.2.1 interface=sgre0 ingress=sg-test-in
VLAN attached remote=192.0.2.2 interface=sgre0.1000 vlan=1000 bridge=br-radius
VLAN attached remote=192.0.2.2 interface=sgre0.10 vlan=10 bridge=br-hotspot
Client learned remote=192.0.2.2 mac=02:00:00:00:00:42 tunnel=sgre0 clients=1
```

The package installs a logrotate rule: daily rotation, seven retained files, compression
with one-cycle delay and a fresh `0600 root:root` log. The post-rotation hook sends HUP to
the PID in `/run/softgred.pid`. Source installs can copy `share/softgred.logrotate` to
`/etc/logrotate.d/softgred`. Adjust the rule for custom log or PID paths. Rotation runs
when the host's logrotate scheduler runs; containers need an external scheduler or
manual rotation. After renaming the log, signal the daemon to open its pathname:

```sh
sudo kill -HUP "$(cat /run/softgred.pid)"
docker exec test-softgre sh -c 'kill -HUP "$(cat /run/softgre-lab/softgred.pid)"'
```

Failed reopening preserves the previous descriptor. Container logs are stored inside
its filesystem; mount persistent storage if they must survive removal. The default
systemd unit permits writes to `/run` and `/var/log`.

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
was loaded. The web interface forwards these read-only commands and provides a
small dependency-free dashboard.

## Python and PHP dashboard

The same `index.html` uses either adapter. Python 3.9+ serves the static assets and the
TCP adapter without external dependencies. Run from the source directory:

```sh
python3 share/webinterface/server.py --bind 0.0.0.0 --port 8080
```

Open `http://SERVER_IP:8080/index.html` in your browser. Open the page through the Python
server rather than `file://`; its requests use the same HTTP origin. `--bind` accepts an
IPv4 literal. The standalone default is `127.0.0.1`; `0.0.0.0` listens on every IPv4
interface. `--api-host` and `--api-port` select the daemon's TCP endpoint, defaulting to
`127.0.0.1:8888`. Environment defaults are `SOFTGRE_WEB_BIND`, `SOFTGRE_WEB_PORT`,
`SOFTGRE_API_HOST` and `SOFTGRE_API_PORT`. The daemon must have its TCP service enabled.

The Python dashboard refreshes every five seconds. It shows tunnels, learned MACs,
interfaces, bridge membership and kernel RX/TX packet counters. Counters belong to the
interfaces and include all their traffic; they are not GRE validation counters. An overview
shows up to 64 tunnels and 128 interfaces, with a notice when tunnels are truncated.
Interface names and counters come from the Python server's network namespace. Keep it in
the daemon's namespace to see its interfaces. The HTTP routes `/api` and `/api.php` both
return JSON; `api.php` is a compatibility route, without executing PHP.

The adapter validates IP/MAC arguments, supports read-only GET queries, connects within
two seconds and caps backend responses at 1 MiB. The full overview has a five-second
backend deadline. HTTP connections have a ten-second lifetime and the server admits up to
16 active connections. Invalid requests return 400, unsupported methods 405, unavailable
or malformed backends 502, backend timeouts 504 and excess connections 503. Static routes
serve only the HTML, JavaScript and CSS; source files and directory listings are excluded.
Neither adapter provides authentication. Use a trusted management network or an
access-controlled reverse proxy for remote access.

### Install and start with softgred

The dashboard is included by default in source and package builds. Python is required
by the package. Use `--disable-web` to omit the frontend in a custom source build; the
web backend requires the TCP API. For a source installation with systemd:

```sh
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var
make -j2
sudo make install
sudo install -m 644 share/softgred.service /etc/systemd/system/softgred.service
sudo install -m 644 share/softgred-web.service /etc/systemd/system/softgred-web.service
sudo systemctl daemon-reload
sudo systemctl enable --now softgred.service
```

Packages enable the daemon and companion and request startup on systemd hosts. Configure
the capture interface for your host; startup errors are reported in the event log.
The daemon wants the web unit, which follows its stops and restarts. To disable the
companion on a systemd host, mask `softgred-web.service`; unmask it to restore startup.
The web service uses an unprivileged dynamic user and listens on `0.0.0.0:8080`.

Without systemd, start both processes through the installed supervisor:

```sh
sudo python3 /usr/share/softgred/run-services.py
```

The production Docker image uses this supervisor and expose port 8080. It drops web privileges,
stops both children when one exits and propagates shutdown. Customize the HTTP address
or backend connection with the documented environment variables. Select a main address
or another API port for systemd with:

```sh
sudo tee /etc/default/softgred-web <<'CONF'
SOFTGRE_WEB_BIND=192.168.1.253
SOFTGRE_WEB_PORT=8080
SOFTGRE_API_HOST=127.0.0.1
SOFTGRE_API_PORT=8888
CONF
sudo systemctl restart softgred-web.service
sudo journalctl -u softgred -u softgred-web -f
```

### PHP alternative

Serve the same directory with PHP 8.1+; command queries are available, while the live
interface overview requires Python:

```sh
php -S 127.0.0.1:8080 -t /usr/share/softgred/webinterface
```

The PHP development server is a local preview. Use PHP-FPM with your web server for a
managed deployment. Both adapters use the following command queries:

| Endpoint | TCP command |
| --- | --- |
| `api.php?action=status` | `STAT` |
| `api.php?action=tunnels` | `STUN` |
| `api.php?action=ues&arg=192.0.2.2` | `LMIP 192.0.2.2` |
| `api.php?action=cpe&arg=02:00:00:00:00:42` | `GTMC 02:00:00:00:00:42` |
| `api.php?action=version` | `VERS` |
| `api.php?action=help` | `HELP` |

Run `python3 tests/test_web.py` to test real HTTP connections, backend framing, errors,
limits and the Python overview. PHP compatibility tests run when PHP is installed.

## How-to: Ubuntu 24.04 lab and live dashboard

The lab runs inside a Docker network namespace. It creates test veth interfaces and
bridges without changing the Docker host's network. Systemd runs as PID 1 with journald.
The disposable lab uses privileged mode and a private cgroup namespace for systemd
service isolation. It does not use host networking or mount the host cgroup tree. Use Docker on a Linux host. Run from the source directory:

```sh
docker build -f docker/Dockerfile.lab -t softgre-lab:ubuntu24.04 .
docker run -d --name test-softgre --restart unless-stopped --privileged --cgroupns=private --tmpfs /run --tmpfs /run/lock -p 192.168.1.253:8080:8080 softgre-lab:ubuntu24.04
```

Replace `192.168.1.253` with your Docker host's main IPv4 address. Port 8080 must be free.
Do not reuse a container name belonging to another application. A remote Docker context
transfers the build files; no workspace bind mount is needed.  The Ubuntu 24.04 image generates and installs the `.deb` package. Systemd starts
the test network setup, daemon and Python dashboard; the dashboard uses a dynamic user.
Restarting the container recreates an empty learning state.

The capture veth is `sg-test-in` with local IPv4 `192.0.2.1/24`. Frames enter through
`sg-test-peer`. VLAN 1000 maps to `br-radius`; VLAN 10 maps to `br-hotspot`. The TCP API
listens on container loopback port 8888. Python listens on container port 8080 and Docker
publishes it on the host's main address. Initially there are no learned tunnels.

### 1. Open the dashboard

Open `http://192.168.1.253:8080/index.html` and leave it open.

### 2. Inspect the initial state and follow logs

```sh
docker exec test-softgre /opt/softgre-lab/status.sh
docker exec test-softgre journalctl -u softgred -u softgred-web -f
```

### 3. Inject GRE packets

In another terminal, send real GRE Ethernet frames into the capture interface:

```sh
docker exec test-softgre python3 /opt/softgre-lab/send.py
```

The script sends five packets for each of three clients: two MACs on remote
`192.0.2.2`, and one on `192.0.2.3`. It waits for learning, checks the dashboard's JSON
and verifies each VLAN interface's bridge with `ip -j -d link`. Repeating it preserves
learned clients and increases packet counters.

### 4. View tunnels, MACs and bridges

The dashboard shows two tunnels, three MACs, `sgre0.1000` attached to `br-radius` and
`sgre0.10` attached to `br-hotspot`. The second tunnel also has both configured VLAN
interfaces. Names depend on free tunnel slots. Check the kernel state directly:

```sh
docker exec test-softgre ip -d link show type gretap
docker exec test-softgre bridge link show
docker exec test-softgre /opt/softgre-lab/status.sh
```

### 5. Add another remote

```sh
docker exec test-softgre python3 /opt/softgre-lab/send.py --remote 192.0.2.4 --mac 02:00:00:00:00:45 --vlan 10 --count 20
```

### 6. Clear learned state and test again

Use the daemon's supported signal, then send again:

```sh
docker exec test-softgre sh -c 'kill -USR1 "$(cat /run/softgre-lab/softgred.pid)"'
docker exec test-softgre python3 /opt/softgre-lab/send.py
```

All project containers include `brctl`, `ifconfig`, `netstat`, `htop`, `vim`, `strace`,
`lsof`, `systemctl`, `journalctl`, `service` and `logrotate`. Inspect the live lab with:

```sh
docker exec test-softgre brctl show
docker exec test-softgre ifconfig -a
docker exec test-softgre netstat -lntp
docker exec test-softgre lsof -i
docker exec -it test-softgre htop
```

For automated provisioning, dashboard and restart checks, use a separate temporary lab:

```sh
scripts/check-lab.sh
```

The examples test real provisioning and bridge attachment. The injected peers are
synthetic; the demo does not create a separate remote router or an end-to-end LAN service.
Use the integration suite for the additional bridge-to-GRE broadcast path checks. The lab
is intentionally persistent so the dashboard remains available. Stop it when finished:

```sh
docker stop test-softgre
```

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
docker build -f docker/Dockerfile -t softgre .
docker run --rm --network host --cap-add NET_ADMIN --cap-add NET_RAW \
  -v /absolute/path/softgred.conf:/etc/softgred/softgred.conf:ro softgre
```

The host-network example configures the Linux host's networking. Use a disposable
namespace for experiments. The host kernel must support GRE, VLANs, bridges and ebtables;
the container cannot supply missing kernel modules. The production image contains the
daemon, GLib and ebtables and runs in the foreground. Build and run Docker on Linux; a
macOS client can target a remote Docker context.

### Restarting the daemon

Install the main unit from `share/softgred.service` after a source build. Package builds
install it automatically. The unit requires `/usr/sbin/softgred` and the configured
`/etc/softgred/softgred.conf`; configure the capture interface before starting it:

```sh
sudo install -m 644 share/softgred.service /etc/systemd/system/softgred.service
sudo systemctl daemon-reload
sudo systemctl enable --now softgred.service
sudo systemctl restart softgred.service
sudo systemctl status softgred.service
sudo journalctl -u softgred -f
```

Stop a manually started instance before enabling its unit, because the daemon's PID
lock prevents duplicates. `service softgred restart` can delegate to systemd on supported
hosts. `journalctl` is a log reader, without a restart operation. The enabled web
companion follows daemon starts, stops and restarts.

The Ubuntu lab runs systemd and journald. Manage each service inside the container:

```sh
docker exec test-softgre systemctl status softgred softgred-web
docker exec test-softgre systemctl restart softgred
docker exec test-softgre service softgred-web restart
docker exec test-softgre journalctl -u softgred -u softgred-web -f
docker exec test-softgre tail -f /var/log/softgred.log
docker exec test-softgre python3 /opt/softgre-lab/send.py
```

Restarting rebuilds the lab interfaces and clears learned tunnels and MACs; inject again
to populate the dashboard. The log file appends across restarts of the same container.

## Releases and Debian/Ubuntu packages

Version 2.0.1 is defined in `VERSION`. Autoconf reads that file; `bump.sh` also updates
the Debian changelog and version references in the English guide. Download
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
for your system). The package installs the daemon, dashboard, configuration, guides and
systemd units. Both units are enabled and startup is requested on systemd hosts. Set the
capture interface for your host and inspect `/var/log/softgred.log` if startup fails.

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
# Binary build on a Linux host with build dependencies:
./autogen.sh
make deb
# Distribution-specific build and installed-package check inside a matching container:
scripts/build-deb.sh debian12 /out
```

The `make deb` target runs `dpkg-buildpackage -us -uc -b`, including the C tests. Output
is in the parent directory. It does not install the package on the host. Debian Docker
images generate, apt-install and validate their deb; installed-binary integration and
packaged web tests exercise the deployed artifact. Minimal images retain SoftGRE docs.

Package filenames include the target distribution to prevent collisions. Package
architecture is the build host architecture; the supplied release workflow builds amd64.
Build another architecture natively if needed. The workflow does not upload debug-symbol
packages. Release downloads and checksums are public GitHub assets and do not require a
package repository.

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
ASan/UBSan, checks `--disable-service --disable-web`, CLI exit codes and staged installation, then runs
`make distcheck`. Integration tests require a disposable Linux network namespace and root
networking privileges:

```sh
docker build -f docker/Dockerfile.test -t softgre-test .
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

All Dockerfiles are in `docker/`. Run build commands from the project root and keep
`.` as the build context; the root `.dockerignore` excludes Git metadata and generated
artifacts. The `-f docker/Dockerfile...` option selects the image definition.

### Source formatting

Use four spaces for C, Python and shell, and two spaces for frontend assets and YAML.
C includes are grouped with a blank line after the block; function definitions have a
blank line between them. Code wraps at 100 columns where possible. Keep long URLs,
commands, output examples and user-facing strings intact when wrapping changes meaning.
The formatter image provides clang-format 14, Black, shfmt, PHP_CodeSniffer and Prettier
3.6.2 from the declared Debian bookworm tooling environment:

```sh
docker build -f docker/Dockerfile.format -t softgre-format .
docker run --rm softgre-format scripts/format-code.sh --check
```

On a local Linux Docker engine, apply formatting to the mounted workspace:

```sh
docker run --rm -v "$PWD:/src" softgre-format scripts/format-code.sh
```

For remote contexts, run the formatter in its copied source tree and copy the results
back; a remote daemon cannot mount your local workspace. Run `scripts/format-code.sh`
directly when its tools are installed. `--check` reports differences without writing.
CI checks formatting using the same container. PHP entrypoint warnings about mixed
function declarations and request handling are excluded from the formatting check.

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

The documentation site is built from the en_US Markdown guide with Python's
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
