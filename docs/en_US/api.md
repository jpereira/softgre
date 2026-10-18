# API and dashboard

## Read-only TCP API

The API listens on loopback port 8888 by default. It has no authentication or encryption.
Keep it on loopback or a trusted management interface and enforce access with the host
firewall; `bind-in = any` exposes it on all IPv4 interfaces. Build with
`--disable-service` to omit the listener.

Each connection accepts one command terminated by LF, CRLF or a TCP write-half-close; the
server sends a response and closes. Partial reads are accumulated safely. Commands are
case-sensitive. Each client has a five-second total deadline, a 255-byte request limit
and a 1 MiB response limit, and excess connections are closed.

```sh
printf 'STUN\n' | nc 127.0.0.1 8888
printf 'LMIP 192.0.4.2\n' | nc 127.0.0.1 8888
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

Invalid commands or arguments produce `RESULT: INVALID`. MAC queries accept exactly six
colon-separated hexadecimal octets, including uppercase. `LMIP` returns MACs in lexical
order; `STUN` ordering is unspecified. Uptime is in seconds and `started` is the Unix
timestamp when configuration was loaded. The [DHCP lease hook](dhcp.md#which-ap-is-behind-each-lease)
uses `GTMC` to tie leases to tunnels.

## Python dashboard

The dashboard is installed with the packages and started by systemd or the supervisor
(see [Install and deploy](install.md#run-with-systemd)). From a source tree, run it
directly; Python 3.9 or later is enough, with no external dependencies:

```sh
python3 share/webinterface/server.py --bind 0.0.0.0 --port 8080
```

Open `http://SERVER_IP:8080/index.html` through the server rather than `file://`.
`--bind` accepts an IPv4 literal and defaults to `127.0.0.1` when run standalone.
`--api-host` and `--api-port` select the daemon's endpoint, defaulting to
`127.0.0.1:8888`. The environment defaults are `SOFTGRE_WEB_BIND`, `SOFTGRE_WEB_PORT`,
`SOFTGRE_API_HOST` and `SOFTGRE_API_PORT`.

The page refreshes every five seconds. It shows tunnels, learned MACs, interfaces, bridge
membership and kernel RX/TX packet counters. Counters belong to the interfaces and
include all their traffic; they are not GRE validation counters. An overview shows up to
64 tunnels and 128 interfaces, with a notice when tunnels are truncated. Interface data
comes from the server's network namespace, so run it in the daemon's namespace. The
routes `/api` and `/api.php` both return JSON; `api.php` is a compatibility route that
does not execute PHP.

The adapter validates IP and MAC arguments, supports read-only GET queries, connects
within two seconds and caps backend responses at 1 MiB. The full overview has a
five-second backend deadline. HTTP connections live at most ten seconds and the server
admits up to 16 at a time. Invalid requests return 400, unsupported methods 405,
unavailable or malformed backends 502, backend timeouts 504 and excess connections 503.
Only the HTML, JavaScript and CSS are served. Neither adapter provides authentication.

## PHP alternative

Serve the same directory with PHP 8.1 or later. Command queries work; the live interface
overview requires Python:

```sh
php -S 127.0.0.1:8080 -t /usr/share/softgred/webinterface
```

The PHP development server is a local preview; use PHP-FPM with your web server for a
managed deployment. Both adapters map these queries to TCP commands:

| Endpoint | TCP command |
| --- | --- |
| `api.php?action=status` | `STAT` |
| `api.php?action=tunnels` | `STUN` |
| `api.php?action=ues&arg=192.0.4.2` | `LMIP 192.0.4.2` |
| `api.php?action=cpe&arg=02:00:00:00:00:42` | `GTMC 02:00:00:00:00:42` |
| `api.php?action=version` | `VERS` |
| `api.php?action=help` | `HELP` |
