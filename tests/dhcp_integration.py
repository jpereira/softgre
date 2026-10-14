#!/usr/bin/env python3
"""Lease per-bridge DHCP networks through real remote APs, gretap tunnels and dnsmasq."""
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from integration import query, run, wait_for

# Each bridge is a separate service network with its own subnet, mask, gateway and DNS.
NETWORKS = {
    1000: {
        "bridge": "br-radius",
        "server": "radius",
        "gateway": "10.100.0.1",
        "prefix": 24,
        "range": ("10.100.0.100", "10.100.0.199"),
        "subnet": "255.255.255.0",
        "dns": "1.1.1.1",
    },
    10: {
        "bridge": "br-hotspot",
        "server": "hotspot",
        "gateway": "172.16.10.1",
        "prefix": 23,
        "range": ("172.16.10.100", "172.16.11.199"),
        "subnet": "255.255.254.0",
        "dns": "8.8.8.8",
    },
}
# Remote access points: every AP carries one client on each mapped VLAN.
APS = {"192.0.2.2": "sg-ap2", "192.0.2.3": "sg-ap3"}

HOOK = """#!/bin/sh
# udhcpc event hook: apply the lease and record the offered options for the test.
case "$1" in
    deconfig) ip addr flush dev "$interface"; ip link set "$interface" up ;;
    bound | renew)
        ip addr flush dev "$interface"
        ip addr add "$ip/$mask" dev "$interface"
        printf 'ip=%s\\nsubnet=%s\\nrouter=%s\\ndns=%s\\nserverid=%s\\n' \\
            "$ip" "$subnet" "$router" "$dns" "$serverid" >"$SOFTGRE_LEASE"
        ;;
esac
"""


def ns(name, *args):
    return run("ip", "netns", "exec", name, *args)


def ap_mac(remote):
    return f"02:a0:00:00:00:{int(remote.split('.')[-1]):02x}"


def client_mac(remote, vlan):
    return f"02:10:00:00:{int(remote.split('.')[-1]):02x}:{1 if vlan == 1000 else 2:02x}"


def netns(name):
    run("ip", "netns", "add", name)
    # No IPv6 autoconfiguration noise: only subscriber DHCP traffic may teach MACs.
    for scope in ("all", "default"):
        ns(name, "sh", "-c", f"echo 1 >/proc/sys/net/ipv6/conf/{scope}/disable_ipv6")
    ns(name, "ip", "link", "set", "lo", "up")


def build_topology():
    run("ip", "link", "set", "lo", "up")
    # The capture interface is a bridge acting as the provider network shared by all APs.
    run("ip", "link", "add", "sg-wan", "type", "bridge")
    run("ip", "addr", "add", "192.0.2.1/24", "dev", "sg-wan")
    run("ip", "link", "set", "sg-wan", "up")
    # Operator-owned service bridges: the daemon attaches tunnels, it does not create them.
    for network in NETWORKS.values():
        run("ip", "link", "add", network["bridge"], "type", "bridge")
        run("ip", "link", "set", network["bridge"], "up")
    # One DHCP server namespace with a leg on each service bridge.
    netns("sg-dhcp")
    for network in NETWORKS.values():
        port = "sg-dhcp-" + network["server"][0]
        run("ip", "link", "add", port, "type", "veth", "peer", "name", network["server"])
        run("ip", "link", "set", network["server"], "netns", "sg-dhcp")
        run("ip", "link", "set", port, "master", network["bridge"], "up")
        address = f"{network['gateway']}/{network['prefix']}"
        ns("sg-dhcp", "ip", "addr", "add", address, "dev", network["server"])
        ns("sg-dhcp", "ip", "link", "set", network["server"], "up")
    for remote, ap in APS.items():
        netns(ap)
        run("ip", "link", "add", ap + "-wan", "type", "veth", "peer", "name", "wan")
        run("ip", "link", "set", "wan", "netns", ap)
        run("ip", "link", "set", ap + "-wan", "master", "sg-wan", "up")
        ns(ap, "ip", "addr", "add", remote + "/24", "dev", "wan")
        ns(ap, "ip", "link", "set", "wan", "up")
        # The AP side is statically configured; only the concentrator learns on demand.
        # The AP's own frames (its bridges use the tunnel MAC) may also be learned.
        gretap = ("softgre", "address", ap_mac(remote), "type", "gretap")
        ns(ap, "ip", "link", "add", *gretap, "local", remote, "remote", "192.0.2.1")
        ns(ap, "ip", "link", "set", "softgre", "up")
        for vlan in NETWORKS:
            tagged, lan, client = f"softgre.{vlan}", f"lan{vlan}", f"{ap}-c{vlan}"
            ns(
                ap,
                "ip",
                "link",
                "add",
                "link",
                "softgre",
                "name",
                tagged,
                "type",
                "vlan",
                "id",
                str(vlan),
            )
            ns(ap, "ip", "link", "add", lan, "address", ap_mac(remote), "type", "bridge")
            ns(ap, "ip", "link", "set", tagged, "master", lan, "up")
            ns(ap, "ip", "link", "set", lan, "up")
            # A subscriber device behind the AP, untagged on the VLAN's local bridge.
            netns(client)
            ns(ap, "ip", "link", "add", f"port{vlan}", "type", "veth", "peer", "name", "eth0")
            ns(ap, "ip", "link", "set", "eth0", "netns", client)
            ns(ap, "ip", "link", "set", f"port{vlan}", "master", lan, "up")
            ns(client, "ip", "link", "set", "eth0", "address", client_mac(remote, vlan))
            ns(client, "ip", "link", "set", "eth0", "up")


def write_dnsmasq(directory):
    lines = [
        "port=0",
        "no-resolv",
        "no-hosts",
        "no-ping",
        "bind-interfaces",
        "except-interface=lo",
        "dhcp-authoritative",
        "log-dhcp",
        "user=root",
        f"dhcp-leasefile={directory}/dnsmasq.leases",
        f"log-facility={directory}/dnsmasq.log",
    ]
    for network in NETWORKS.values():
        tag, (first, last) = network["server"], network["range"]
        lines += [
            f"interface={tag}",
            f"dhcp-range=set:{tag},{first},{last},{network['subnet']},1h",
            f"dhcp-option=tag:{tag},option:router,{network['gateway']}",
            f"dhcp-option=tag:{tag},option:dns-server,{network['dns']}",
        ]
    config = Path(directory, "dnsmasq.conf")
    config.write_text("\n".join(lines) + "\n")
    return config


def in_range(address, first, last):
    def value(text):
        return int.from_bytes(bytes(map(int, text.split("."))), "big")

    return value(first) <= value(address) <= value(last)


def main():
    if os.environ.get("SOFTGRE_TEST_NAMESPACE") != "isolated":
        raise SystemExit(
            "Run in a disposable privileged container with SOFTGRE_TEST_NAMESPACE=isolated"
        )
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else "bin/softgred").resolve())
    for name in ("sg-wan", "br-radius", "br-hotspot"):
        assert not Path("/sys/class/net", name).exists(), "test interface already exists"
    processes = []
    with tempfile.TemporaryDirectory() as directory:
        try:
            build_topology()
        except subprocess.CalledProcessError as error:
            raise SystemExit(f"Topology setup failed (needs --privileged): {error.output}")
        mapping = ",".join(f"{vlan}@{n['bridge']}" for vlan, n in NETWORKS.items())
        config = Path(directory, "softgred.conf")
        config.write_text(
            f"""[global]
interface=sg-wan
tunnel-prefix=sgre
bridge-map={mapping}
bridge-force=false
pid-file={directory}/softgred.pid
log-file={directory}/events.log
[service]
bind-in=lo
port=18888
max-listen=4
"""
        )
        hook = Path(directory, "udhcpc.sh")
        hook.write_text(HOOK)
        hook.chmod(0o755)
        log = Path(directory, "daemon.log").open("w+")
        try:
            daemon = subprocess.Popen([binary, "-f", "-c", str(config)], stdout=log, stderr=log)
            processes.append(daemon)
            wait_for(lambda: "RESULT: OK" in query("VERS"))
            dnsmasq = subprocess.Popen(
                ["ip", "netns", "exec", "sg-dhcp", "dnsmasq", "--keep-in-foreground"]
                + [f"--conf-file={write_dnsmasq(directory)}"],
                stdout=log,
                stderr=log,
            )
            processes.append(dnsmasq)
            time.sleep(0.5)
            assert dnsmasq.poll() is None, "dnsmasq failed to start"
            # All four subscribers ask for a lease at once. The first DISCOVER from each
            # client is what teaches the daemon its tunnel and MAC; udhcpc retries until
            # the kernel path exists and the firewall permits the learned MAC.
            clients = {}
            start = time.monotonic()
            for remote, ap in APS.items():
                for vlan in NETWORKS:
                    client = f"{ap}-c{vlan}"
                    lease = Path(directory, client + ".lease")
                    command = ["ip", "netns", "exec", client, "busybox", "udhcpc", "-i", "eth0"]
                    command += ["-f", "-q", "-n", "-t", "20", "-T", "1", "-A", "1", "-s", str(hook)]
                    env = dict(os.environ, SOFTGRE_LEASE=str(lease))
                    clients[client] = (remote, vlan, lease)
                    processes.append(subprocess.Popen(command, env=env, stdout=log, stderr=log))
            for process in processes[2:]:
                assert process.wait(timeout=40) == 0, "a client did not obtain a DHCP lease"
            print(f"4 DHCP leases through 2 learned tunnels in {time.monotonic() - start:.2f}s")
            tunnels = query("STUN")
            for client, (remote, vlan, lease) in clients.items():
                network = NETWORKS[vlan]
                offered = dict(line.split("=", 1) for line in lease.read_text().splitlines())
                assert in_range(offered["ip"], *network["range"]), (client, offered)
                assert offered["subnet"] == network["subnet"], (client, offered)
                assert offered["router"] == network["gateway"], (client, offered)
                assert offered["dns"] == network["dns"], (client, offered)
                assert offered["serverid"] == network["gateway"], (client, offered)
                # The client is reachable only after it was learned on its own remote.
                mac = client_mac(remote, vlan)
                assert remote + "@" in tunnels, f"{remote} has no tunnel"
                assert mac in query(f"LMIP {remote}"), f"{mac} not learned on {remote}"
                learned = query(f"GTMC {mac}")
                tunnel = learned.split(remote + "@", 1)[1].split()[0]
                attached = run("ip", "-d", "link", "show", f"{tunnel}.{vlan}")
                assert f"master {network['bridge']}" in attached, attached
                # Data path, not just DHCP: reach the gateway through gretap and the bridge.
                ns(client, "ping", "-c", "1", "-W", "2", network["gateway"])
                print(
                    f"{client}: {mac} via {remote}/{tunnel}.{vlan} -> {network['bridge']} "
                    f"ip={offered['ip']} mask={offered['subnet']} dns={offered['dns']}"
                )
            for remote in APS:
                expected = {client_mac(remote, vlan) for vlan in NETWORKS}
                body = query(f"LMIP {remote}").split("BODY: ", 1)[1].strip()
                learned = set(body.split(";"))
                assert expected <= learned <= expected | {ap_mac(remote)}, (remote, learned)
            leases = Path(directory, "dnsmasq.leases").read_text().splitlines()
            assert len(leases) == len(clients), leases
            events = Path(directory, "events.log").read_text()
            for remote in APS:
                assert f"Tunnel created remote={remote}" in events, events
            print("DHCP integration: per-bridge networks, masks, gateways and DNS passed")
        except BaseException:
            for name in ("events.log", "dnsmasq.log"):
                path = Path(directory, name)
                if path.exists():
                    print(f"--- {name}\n{path.read_text()}")
            raise
        finally:
            for process in reversed(processes):
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
            log.seek(0)
            print(log.read())
            log.close()
            for name in run("ip", "netns", "list").split():
                if name.startswith("sg-"):
                    subprocess.run(["ip", "netns", "delete", name], check=False)


if __name__ == "__main__":
    main()
