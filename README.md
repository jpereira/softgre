# SoftGRE

Dynamic IPv4 GRE Ethernet provisioning for Linux. SoftGRE discovers remote peers, creates gretap
and VLAN interfaces, connects them to Linux bridges and exposes a read-only TCP status API.
Capture uses AF_PACKET with a kernel BPF filter; interface control uses native NETLINK_ROUTE.

- [Documentation in English](docs/en_US/index.md)
- [Debian/Ubuntu packages](https://github.com/jpereira/softgre/releases/tag/v2.0.1)
- [Documentation website](https://jpereira.github.io/softgre/)

```sh
sudo apt-get install build-essential autoconf automake pkg-config libglib2.0-dev ebtables iptables iproute2
./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var
make -j2
make check
sudo make install
```

Edit `/etc/softgred/softgred.conf`, then run `sudo /usr/sbin/softgred -f`.
GRE discovery requires trusted ingress; the management API defaults to loopback.

```sh
scripts/check-linux.sh
python3 scripts/build-docs.py
python3 tests/test_docs.py
```

The full Linux validation script also needs Clang and Python. See the guides for configuration,
systemd, containers, optional Python/PHP dashboard, integration tests, protocol limits and performance.
Licensed under LGPL-3.0-or-later; see [LICENSE.md](LICENSE.md) and [COPYING](COPYING).
