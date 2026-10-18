# Configuration and operation

## Configuration file

The default configuration is `/etc/softgred/softgred.conf` when built with
`--sysconfdir=/etc`. Select the interface whose IPv4 address terminates GRE and map each
VLAN to a bridge before starting:

```ini
[global]
interface = eth0
tunnel-prefix = sgre
bridge-map = 1000@br-radius, 10@br-hotspot, 11@br-radius
bridge-force = true
pid-file = /run/softgred.pid
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
| `global.allow-remotes` | Optional allowlist of remote IPv4 addresses or CIDRs that may provision; omit to accept any remote. |
| `global.log-file` | Event log, default `/var/log/softgred.log`; absolute path required. |
| `global.pid-file` | Locked PID file, default under the configured PID directory. |
| `service.bind-in` | Bind to interface IPv4 address; `lo` by default, `any` binds all. |
| `service.port` | TCP port, `1..65535`, default `8888`. |
| `service.max-listen` | Simultaneous clients, `1..128`, default `10`. |
| `debug.enable` | Enable level-one diagnostic logs, default `false`. |
| `debug.print-time` | Timestamp stderr, default `false`; `debug.time` is also accepted. |

VLAN IDs are `1..4094`, unique within the mapping. Interface names contain ASCII letters,
digits, underscores, dots or hyphens and fit within 15 characters; the six-character
prefix limit leaves room for the tunnel slot and the four-digit VLAN suffix. A map entry
uses an existing Linux bridge, or a bridge created when `bridge-force=true`. Existing
bridges are brought up and retained on exit. Several VLANs may target the same bridge,
which joins those broadcast domains deliberately. Empty entries, malformed numbers and
duplicate VLANs fail configuration loading.

An explicit `allow-remotes` value requires one or more IPv4 addresses or CIDRs. Blank
values, empty comma-separated entries and invalid CIDRs reject startup. Omit the key to
accept any remote; `0.0.0.0/0` explicitly selects all IPv4 addresses. The allowlist matches
claimed source addresses and needs ingress anti-spoofing or a protected transport.

Booleans accept `true`, `false`, `1` and `0`. Optional groups and keys can be omitted
independently, and unrecognized keys are ignored. Blank lines and lines starting with `#`
or `;` are comments; if a key appears twice, the last one wins. Any other line that is not
a `[group]` header or a `key = value` pair makes the daemon refuse to start, so a typo such
as `allow-remotes 192.0.4.0/24` (missing `=`) can never silently fall back to accepting
every remote. Embedded NUL bytes and file read errors also reject startup. Unrecognized
keys are still ignored: check the spelling of security settings. Configuration is loaded at startup; restart the daemon to apply changes.

## Command line

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

The daemon must run as root with `CAP_NET_ADMIN` and `CAP_NET_RAW`. A PID file uses an
exclusive advisory lock held for the daemon's lifetime. It must be a regular file owned
by the daemon's user, with one hard link and no group or world write permission; symlinks
are refused. Keep its parent directory writable only by the administrator. Background
startup reports success only after capture, API, bridges and firewall are ready; the
foreground process returns failure when startup or capture fails.

## Signals and learned state

| Signal | Effect |
| --- | --- |
| `SIGINT`, `SIGTERM` | Stop; remove owned tunnels, firewall chains and the PID file. |
| `SIGUSR1` | Remove all provisioned tunnels, MAC permissions and indexes; discovery continues. |
| `SIGUSR2` | Print tunnel, MAC, packet, ignored-packet and failure counters. |
| `SIGHUP` | Reopen the log file, without reloading configuration or clearing state. |

```sh
sudo kill -USR1 "$(cat /run/softgred.pid)"
```

Existing unrelated interfaces and firewall rules remain in place, and created bridges
remain after exit. Tunnel slots are reused after successful cleanup; names already
present in the network namespace are skipped. There is no automatic idle expiry or
implicit MAC mobility: a MAC belongs to one remote, and its permission covers the
configured VLANs on that tunnel. Restarting or `SIGUSR1` clears the ownership before the
MAC can be learned elsewhere.

## Event logs and rotation

The daemon appends to `/var/log/softgred.log` by default, in foreground and background
execution, and mirrors messages to stderr for systemd and Docker. Each record has local
date and time with timezone, PID and severity. New files have mode `0600`. The parent
directory must exist and startup fails if the log cannot be opened; runtime write
failures are reported on stderr while network processing continues. Symlinks, hard
links, special files, files owned by another user and world-writable files are rejected.

Events cover startup and shutdown, bridge preparation, remote and local IPv4 addresses,
GRE and VLAN interfaces with their bridges, newly learned MACs, removals and failed
provisioning. Known-client traffic does not create a record per packet:

```text
Tunnel created remote=192.0.4.2 local=192.0.4.1 interface=sgre0 ingress=sg-test-in
VLAN attached remote=192.0.4.2 interface=sgre0.1000 vlan=1000 bridge=br-radius
VLAN attached remote=192.0.4.2 interface=sgre0.10 vlan=10 bridge=br-hotspot
Client learned remote=192.0.4.2 mac=02:00:00:00:00:42 tunnel=sgre0 clients=1
```

The package installs a logrotate rule: daily rotation, seven retained files, delayed
compression and a fresh `0600 root:root` log. Its post-rotation hook sends `SIGHUP` to the
PID in `/run/softgred.pid`. Source installs can copy `share/softgred.logrotate` to
`/etc/logrotate.d/softgred`; adjust it for custom log or PID paths. Containers need an
external scheduler or manual rotation. A failed reopen keeps the previous descriptor.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Build fails | Install the C compiler and Autotools; build on Linux. |
| Capture fails | Verify root, interface existence, IPv4 address and NET_RAW capability. |
| Bridge startup fails | Ensure the target is a Linux bridge or enable `bridge-force`. |
| No tunnels appear | Check GRE destination, protocol 47, unkeyed TEB payload and mapped inner VLAN. |
| MAC does not move | Clear learned ownership with SIGUSR1 or restart before learning another remote. |
| Firewall chain exists | Inspect abandoned SOFTGRE chains and tunnel jumps; stop other instances first. |
| Traffic stalls | Check underlay MTU, GRE overhead, bridge firewall and route to the remote. |
| No DHCP lease | Check the bridge address, dnsmasq `interface=` lines and the client's retry timeout. |
| API times out | Terminate commands with newline; verify bind interface, port and firewall. |
| PID lock fails | Check for a running instance and PID directory permissions; avoid symlink paths. |

GRE encapsulation consumes underlay MTU. The kernel selects the tunnel MTU; inspect it
with `ip -d link show` and configure the underlay and clients accordingly. SoftGRE does
not change the host's routes, global IP forwarding policy or kernel module configuration.
After a crash or forced kill, inspect owned interfaces and `SOFTGRE_*` chains before
restarting; never flush unrelated firewall state.
