# Try it with Docker

Two Docker setups run SoftGRE without touching your host network. Both build the Ubuntu
24.04 packages, install them and start `softgred` and the dashboard under systemd. Run the
commands from a clone of the repository on any Docker engine with Linux containers.

- **Gateway with simulated APs** is the complete picture: AP containers send real GRE
  over a Docker network, their clients request DHCP leases and reach the internet through
  the gateway.
- **Single-container lab** is the quickest look at provisioning: a script injects
  synthetic GRE frames into a test interface inside one container.

## Gateway with simulated APs

The `gateway` target of the lab image is a complete concentrator. It captures GRE on the
container's Docker network interface, so APs in other containers reach it from outside.
`br-radius` (`10.100.0.1/24`, DNS `1.1.1.1`) and `br-hotspot` (`172.16.10.1/23`, DNS
`8.8.8.8`) have gateway addresses and NAT to the uplink. dnsmasq serves both bridges with
the [lab configuration](https://github.com/jpereira/softgre/blob/master/scripts/lab/dnsmasq-softgre.conf)
and the [lease hook](dhcp.md#which-ap-is-behind-each-lease).

### Start the gateway

```sh
docker build -f docker/Dockerfile.lab --target gateway -t softgre-gateway:ubuntu24.04 .
docker network create softgre-net
docker run -d --name softgre-gw --network softgre-net --privileged --cgroupns=private \
  --tmpfs /run --tmpfs /run/lock -p 8080:8080 softgre-gateway:ubuntu24.04
docker inspect --format '{{.State.Health.Status}}' softgre-gw
```

Wait until the health status is `healthy`. Open `http://localhost:8080/index.html`; there
are no tunnels yet.

### Start access points

Each AP container runs
[`ap.sh`](https://github.com/jpereira/softgre/blob/master/scripts/lab/ap.sh): a `gretap`
to the gateway, VLANs 1000 and 10, and one client namespace per VLAN (`c1000`, `c10`) that
requests a DHCP lease. With `--option82`, the AP inserts DHCP option 82 (circuit-id =
SSID, remote-id = AP MAC):

```sh
docker run -d --name ap1 --network softgre-net --privileged --no-healthcheck \
  --entrypoint /opt/softgre-lab/ap.sh softgre-gateway:ubuntu24.04 softgre-gw
docker run -d --name ap2 --network softgre-net --privileged --no-healthcheck \
  --entrypoint /opt/softgre-lab/ap.sh softgre-gateway:ubuntu24.04 softgre-gw --option82
docker logs ap1
```

```text
AP 172.21.0.3 -> gateway 172.21.0.2, tunnel softgre (02:a0:00:00:00:03)
c1000 VLAN 1000 MAC 02:10:00:00:03:01: ip=10.100.0.100/24 router=10.100.0.1 dns=1.1.1.1
c10 VLAN 10 MAC 02:10:00:00:03:02: ip=172.16.10.137/23 router=172.16.10.1 dns=8.8.8.8
AP ready
```

Addresses and MACs derive from each AP's Docker IP. The dashboard now shows one tunnel per
AP with its learned MACs. Add more APs with new container names.

### Use the clients and follow the gateway

```sh
docker exec ap1 ip netns exec c1000 ping -c 3 1.1.1.1
docker exec ap1 ip netns exec c10 ping -c 3 10.100.0.1
docker exec softgre-gw tail -f /var/log/softgred-dhcp.log
docker exec softgre-gw journalctl -u softgred -u dnsmasq -f
docker exec softgre-gw /opt/softgre-lab/status.sh
```

The first ping leaves through the gateway's NAT; the second reaches the RADIUS bridge's
gateway address from the hotspot network. The DHCP log ties every lease to its AP and
tunnel, plus the option 82 IDs from `ap2`:

```text
DHCP lease action=add ip=10.100.0.199 mac=02:10:00:00:04:01 interface=br-radius remote=172.21.0.4 tunnel=sgre1 circuit-id=radius remote-id=02:a0:00:00:00:04
```

### Reprovision and clean up

Clear the learned state, then restart an AP to watch it be provisioned again:

```sh
docker exec softgre-gw sh -c 'kill -USR1 "$(cat /run/softgre-lab/softgred.pid)"'
docker restart ap1
```

Remove everything when finished:

```sh
docker rm -f softgre-gw ap1 ap2
docker network rm softgre-net
```

`scripts/check-gateway.sh` runs this flow with two APs on a temporary network. It checks
leases, gateway pings, the lease hook (including option 82) and the dashboard; CI runs it
on every push.

## Single-container lab

The default lab target creates a test veth pair and bridges inside one container. The
capture interface is `sg-test-in` at `192.0.3.1/24`; frames enter through `sg-test-peer`.
VLAN 1000 maps to `br-radius` and VLAN 10 to `br-hotspot`. The TCP API listens on container
loopback port 8888 and the dashboard on port 8080.

### Start the lab

```sh
docker build -f docker/Dockerfile.lab -t softgre-lab:ubuntu24.04 .
docker run -d --name test-softgre --restart unless-stopped --privileged --cgroupns=private \
  --tmpfs /run --tmpfs /run/lock -p 8080:8080 softgre-lab:ubuntu24.04
```

Choose an unused container name and port; to reach the dashboard from other machines,
publish it on the Docker host's address, such as `-p 192.168.1.253:8080:8080`. Open
`http://localhost:8080/index.html` and leave it open. The lab uses privileged mode and a
private cgroup namespace for systemd; it does not use host networking or mount the host
cgroup tree.

### Inject GRE packets

```sh
docker exec test-softgre python3 /opt/softgre-lab/send.py
```

The script sends five packets for each of three synthetic clients: two MACs on remote
`192.0.3.2` and one on `192.0.3.3`. It waits for learning, checks the dashboard's JSON and
verifies each VLAN interface's bridge. On a fresh lab the topology is:

```text
Remote 192.0.3.2 -> sgre0
  MACs: 02:00:00:00:00:42, 02:00:00:00:00:43
  sgre0.1000 -> br-radius
  sgre0.10   -> br-hotspot

Remote 192.0.3.3 -> sgre1
  MAC: 02:00:00:00:00:44
  sgre1.1000 -> br-radius
  sgre1.10   -> br-hotspot
```

Each new remote receives every configured VLAN interface, not only the VLAN from its
first packet. Interface slot names depend on which names are free.

### Inspect, extend and reset

```sh
docker exec test-softgre ip -d link show type gretap
docker exec test-softgre bridge link show
docker exec test-softgre /opt/softgre-lab/status.sh
docker exec test-softgre journalctl -u softgred -u softgred-web -f
docker exec test-softgre python3 /opt/softgre-lab/send.py --remote 192.0.3.4 --mac 02:00:00:00:00:45 --vlan 10 --count 20
docker exec test-softgre sh -c 'kill -USR1 "$(cat /run/softgre-lab/softgred.pid)"'
```

The dashboard refreshes every five seconds. Repeating `send.py` keeps learned clients and
increases packet counters; after `SIGUSR1`, send again to watch reprovisioning. A daemon
or container restart also clears learned state. The injected peers are synthetic: this
lab tests GRE decoding, provisioning and bridge attachment, not a remote LAN service. Use
the gateway setup above for end-to-end traffic.

All project containers include `brctl`, `ifconfig`, `netstat`, `htop`, `vim`, `strace`,
`lsof`, `systemctl`, `journalctl`, `service` and `logrotate`. Stop the lab with
`docker stop test-softgre`. `scripts/check-lab.sh` runs a temporary lab and checks
provisioning, the dashboard and the container lifecycle.
