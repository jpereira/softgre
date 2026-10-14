# Install and deploy

Run SoftGRE on a Linux host with IPv4, bridge, 802.1Q, GRE/gretap, AF_PACKET and bridge
firewall (ebtables) support. The daemon needs root or `CAP_NET_ADMIN` and `CAP_NET_RAW`.
Only one instance should own a given network namespace. To experiment first, use the
[Docker labs](try-it.md); they do not touch the host network.

## Debian and Ubuntu packages

Download the amd64 package for your distribution from the GitHub release:

[Download SoftGRE 2.0.2](https://github.com/jpereira/softgre/releases/tag/v2.0.2)

| Distribution | Release asset |
| --- | --- |
| Debian 12 | `softgred_2.0.2-1+debian12_amd64.deb` |
| Debian 13 | `softgred_2.0.2-1+debian13_amd64.deb` |
| Ubuntu 22.04 | `softgred_2.0.2-1+ubuntu22.04_amd64.deb` |
| Ubuntu 24.04 | `softgred_2.0.2-1+ubuntu24.04_amd64.deb` |

Download `SHA256SUMS` too, verify the hash and install with
`sudo apt install ./softgred_2.0.2-1+debian12_amd64.deb` (adjust the filename for your
system). The package installs the daemon, the dashboard, the DHCP lease hook, the
configuration, these guides, logrotate rules and both systemd units. The units are enabled
and startup is requested on systemd hosts. Set the capture interface in
`/etc/softgred/softgred.conf` (see [Configuration](configuration.md)) and inspect
`/var/log/softgred.log` if startup fails.

## Container image

[GitHub Packages](https://github.com/jpereira/softgre/pkgs/container/softgre) hosts the
amd64 production image as `ghcr.io/jpereira/softgre:2.0.2` and `latest`. It runs the daemon
and the dashboard together. Configure the capture interface and VLAN mappings in a local
`softgred.conf`, then run it with host networking:

```sh
docker pull ghcr.io/jpereira/softgre:2.0.2
docker run --rm --network host --cap-add NET_ADMIN --cap-add NET_RAW \
  -v /absolute/path/softgred.conf:/etc/softgred/softgred.conf:ro ghcr.io/jpereira/softgre:2.0.2
```

Host networking lets the daemon create interfaces and bridges on the Linux host. The host
kernel must support GRE, VLANs, bridges and ebtables; the container cannot supply missing
kernel modules. The dashboard listens on port 8080. Authenticate with
`docker login ghcr.io` when package visibility requires it; public images support
anonymous pulls. Build the same image locally with
`docker build -f docker/Dockerfile -t softgre .`.

## Build from source

Build dependencies are a C11 compiler, Make, Autoconf, Automake, pkg-config and GLib
development headers. Runtime dependencies are GLib, ebtables and Python 3 for the
dashboard and lease hook.

```sh
sudo apt-get install build-essential autoconf automake pkg-config libglib2.0-dev ebtables iptables iproute2 python3
git clone https://github.com/jpereira/softgre.git
cd softgre
./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var --with-piddir=/run
make -j2
make check
sudo make install
sudoedit /etc/softgred/softgred.conf
```

A default `/usr/local` installation reads `/usr/local/etc/softgred/softgred.conf`
instead. Use `--disable-web` to omit the dashboard and `--disable-service` to omit the TCP
API; the dashboard requires the API.

To build a Debian package of the checked-out source, install `debhelper` as well and run
`./autogen.sh && make deb`. The target runs the Debian build and regression tests and
writes `softgred_VERSION_ARCH.deb` to the parent directory; it does not install it.

## Run with systemd

Packages install and enable both units. After a source build, install them yourself:

```sh
sudo install -m 644 share/softgred.service /etc/systemd/system/softgred.service
sudo install -m 644 share/softgred-web.service /etc/systemd/system/softgred-web.service
sudo systemctl daemon-reload
sudo systemctl enable --now softgred.service softgred-web.service
sudo systemctl status softgred.service softgred-web.service
sudo journalctl -u softgred -u softgred-web -f
```

`softgred.service` runs the daemon in the foreground with `CAP_NET_ADMIN` and
`CAP_NET_RAW` and starts after the network is online. Add a dependency on your network
manager's address-ready target if `network-online.target` alone does not guarantee the
capture interface is ready. The daemon wants the web unit, which follows its starts, stops
and restarts and runs as an unprivileged dynamic user on `0.0.0.0:8080`. Mask
`softgred-web.service` to disable the dashboard. Stop a manually started instance before
enabling the unit, because the daemon's PID lock prevents duplicates.

Change the dashboard's address or API endpoint in `/etc/default/softgred-web`:

```sh
sudo tee /etc/default/softgred-web <<'CONF'
SOFTGRE_WEB_BIND=192.168.1.253
SOFTGRE_WEB_PORT=8080
SOFTGRE_API_HOST=127.0.0.1
SOFTGRE_API_PORT=8888
CONF
sudo systemctl restart softgred-web.service
```

## Run without systemd

The installed supervisor starts the daemon and the unprivileged dashboard together, stops
both when one exits and propagates shutdown. The production container uses it:

```sh
sudo python3 /usr/share/softgred/run-services.py
```

Open `http://SERVER_IP:8080/index.html`. Neither the dashboard nor the TCP API provides
authentication; use a trusted management network or an access-controlled reverse proxy.
