#!/usr/bin/env python3
"""Exercise real Linux GRE, VLAN, firewall, API and signal lifecycles in an isolated netns."""
import json
import os
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def wait_for(test, timeout=6):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            value = test()
            if value:
                return value
        except (OSError, AssertionError):
            pass
        time.sleep(0.02)
    raise AssertionError("condition did not become true")


def query(command):
    with socket.create_connection(("127.0.0.1", 18888), timeout=2) as client:
        client.sendall((command + "\n").encode())
        result = bytearray()
        while True:
            chunk = client.recv(65536)
            if not chunk:
                return result.decode()
            result.extend(chunk)


def checksum(data):
    words = struct.unpack("!%dH" % (len(data) // 2), data)
    total = sum(words)
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def frame(remote="192.0.2.2", dest="192.0.2.1", vlan=1000, tags=0,
          flags=0, options=0, mac=b"\x02\x00\x00\x00\x00\x42"):
    dst_mac = bytes.fromhex(Path("/sys/class/net/sg-test-in/address").read_text().replace(":", ""))
    peer_address = Path("/sys/class/net/sg-test-peer/address").read_text().replace(":", "")
    src_mac = bytes.fromhex(peer_address)
    inner = b"\xff" * 6 + mac + struct.pack("!HHH", 0x8100, 0xA000 | vlan, 0x0806)
    inner += struct.pack("!HHBBH", 1, 0x0800, 6, 4, 1)
    inner += mac + socket.inet_aton("198.51.100.2") + b"\0" * 6
    inner += socket.inet_aton("198.51.100.1") + b"\0" * 14
    gre = struct.pack("!HH", flags, 0x6558) + inner
    ip = struct.pack("!BBHHHBBH4s4s", 0x45 + options, 0, 20 + 4 * options + len(gre),
                     42, 0, 64, 47, 0, socket.inet_aton(remote), socket.inet_aton(dest))
    ip += b"\1" * (options * 4)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    ethernet = dst_mac + src_mac + struct.pack("!H", 0x8100 if tags else 0x0800)
    for tag in range(tags):
        ethernet += struct.pack("!HH", 123, 0x8100 if tag + 1 < tags else 0x0800)
    return ethernet + ip + gre


def main():
    if os.environ.get("SOFTGRE_TEST_NAMESPACE") != "isolated":
        raise SystemExit("Run in a disposable netns with SOFTGRE_TEST_NAMESPACE=isolated")
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else "bin/softgred").resolve())
    for name in ("sg-test-in", "sg-test-peer", "br-radius", "br-hotspot"):
        assert not Path("/sys/class/net", name).exists(), "test interface already exists"
    run("ip", "link", "set", "lo", "up")
    run("ip", "link", "add", "sg-test-in", "type", "veth", "peer", "name", "sg-test-peer")
    run("ip", "addr", "add", "192.0.2.1/24", "dev", "sg-test-in")
    run("ip", "link", "set", "sg-test-in", "up")
    run("ip", "link", "set", "sg-test-peer", "up")
    # An unrelated rule/policy must survive daemon startup, SIGUSR1 and shutdown.
    run("ebtables", "-A", "FORWARD", "-p", "IPv4", "-j", "ACCEPT")
    original = run("ebtables", "-L", "FORWARD")
    with tempfile.TemporaryDirectory() as directory:
        config = Path(directory, "softgred.conf")
        pidfile = Path(directory, "softgred.pid")
        config.write_text(f"""[global]
interface=sg-test-in
tunnel-prefix=sgre
bridge-map=1000@br-radius,10@br-hotspot,11@br-radius
bridge-force=true
pid-file={pidfile}
[service]
bind-in=lo
port=18888
max-listen=4
""")
        logfile = Path(directory, "daemon.log")
        with logfile.open("w+") as log:
            daemon = subprocess.Popen([binary, "-f", "-c", str(config)], stdout=log, stderr=log)
            try:
                wait_for(lambda: "RESULT: NULL" in query("STUN"))
                second = subprocess.run([binary, "-f", "-c", str(config)], capture_output=True)
                assert second.returncode != 0, "second daemon bypassed PID lock"
                assert Path("/sys/class/net/br-radius/bridge").exists()
                assert Path("/sys/class/net/br-hotspot/bridge").exists()
                with socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(3)) as raw:
                    raw.bind(("sg-test-peer", 0))
                    # Foreign destinations, unsupported key flags, unknown VLANs: no state.
                    for packet in (frame(dest="192.0.2.9"), frame(vlan=999), frame(flags=0x2000)):
                        raw.send(packet)
                    time.sleep(0.1)
                    assert "RESULT: NULL" in query("STUN")
                    raw.send(frame())
                    wait_for(lambda: "192.0.2.2@sgre0" in query("STUN"))
                    wait_for(lambda: "02:00:00:00:00:42" in query("LMIP 192.0.2.2"))
                    links = json.loads(run("ip", "-j", "-d", "link", "show", "sgre0.1000"))
                    assert links[0]["master"] == "br-radius", "first bridge mapping corrupted (#5)"
                    assert links[0]["linkinfo"]["info_data"]["id"] == 1000
                    # Test the data path: LAN broadcast ARP must cross the VLAN and gretap.
                    run("ip", "link", "add", "sg-test-lan", "type", "veth",
                        "peer", "name", "sg-test-server")
                    run("ip", "link", "set", "sg-test-lan", "master", "br-radius")
                    run("ip", "link", "set", "sg-test-lan", "up")
                    run("ip", "link", "set", "sg-test-server", "up")
                    peer_mac = Path("/sys/class/net/sg-test-peer/address").read_text().strip()
                    run("ip", "neigh", "replace", "192.0.2.2", "lladdr", peer_mac,
                        "nud", "permanent", "dev", "sg-test-in")
                    gateway_mac = bytes.fromhex("02aabbccddee")
                    arp = b"\xff" * 6 + gateway_mac + struct.pack("!H", 0x0806)
                    arp += struct.pack("!HHBBH", 1, 0x0800, 6, 4, 1)
                    arp += gateway_mac + socket.inet_aton("198.51.100.1") + b"\0" * 6
                    arp += socket.inet_aton("198.51.100.2") + b"\0" * 18
                    with socket.socket(socket.AF_PACKET, socket.SOCK_RAW) as lan:
                        lan.bind(("sg-test-server", 0))
                        lan.send(arp)
                    raw.settimeout(2)
                    forwarded = False
                    deadline = time.monotonic() + 3
                    while time.monotonic() < deadline:
                        received = raw.recv(65536)
                        if len(received) < 58 or received[12:14] != b"\x08\x00":
                            continue
                        if received[23] != 47 or received[26:30] != socket.inet_aton("192.0.2.1"):
                            continue
                        inner_offset = 14 + (received[14] & 15) * 4 + 4
                        if received[inner_offset + 6:inner_offset + 12] == gateway_mac:
                            forwarded = True
                            break
                    assert forwarded, "bridge broadcast was blocked before GRE encapsulation"
                    raw.settimeout(None)
                    first_rules = run("ebtables", "-L", "SOFTGRE_IN")
                    start = time.monotonic()
                    for _ in range(10000):
                        raw.send(frame())
                    assert "192.0.2.2@sgre0" in query("STUN")
                    assert run("ebtables", "-L", "SOFTGRE_IN") == first_rules
                    print(f"10,000 known-client packets sent in {time.monotonic() - start:.3f}s")
                    # Fresh clients exercise both kernel BPF VLAN branches and IPv4 options.
                    for tags in (1, 2):
                        mac = bytes((2, 0, 0, 0, 0, 0x42 + tags))
                        raw.send(frame(tags=tags, options=tags, mac=mac))
                        wait_for(lambda m=mac: m.hex(":") in query("LMIP 192.0.2.2"))
                    daemon.send_signal(signal.SIGUSR2)
                    daemon.send_signal(signal.SIGUSR1)
                    wait_for(lambda: "RESULT: NULL" in query("STUN"))
                    assert not Path("/sys/class/net/sgre0").exists()
                    assert run("ebtables", "-L", "FORWARD") == original
                    assert "02:00:00:00:00:42" not in run("ebtables", "-L", "SOFTGRE_IN")
                    # Existing interfaces are never adopted or removed; skip occupied slots.
                    run("ip", "link", "add", "sgre0", "type", "dummy")
                    raw.send(frame())
                    wait_for(lambda: "192.0.2.2@sgre1" in query("STUN"))
                # Fragmented TCP commands, invalid names and abrupt disconnects.
                with socket.create_connection(("127.0.0.1", 18888), timeout=2) as client:
                    client.sendall(b"VE"); time.sleep(0.05); client.sendall(b"RS\r\n")
                    assert b"RESULT: OK" in client.recv(1024)
                assert "RESULT: INVALID" in query("VERSextra")
                assert "192.0.2.2@sgre1" in query("GTMC 02:00:00:00:00:42")
                daemon.send_signal(signal.SIGTERM)
                assert daemon.wait(timeout=10) == 0
                assert not pidfile.exists()
                assert not Path("/sys/class/net/sgre1").exists()
                assert Path("/sys/class/net/sgre0").exists()
                run("ip", "link", "delete", "sgre0")
                assert run("ebtables", "-L", "FORWARD") == original
                assert "SOFTGRE" not in run("ebtables", "-L")
                # The daemon parent reports real startup failure rather than false success.
                bad = config.read_text().replace("bridge-force=true", "bridge-force=false")
                bad = bad.replace("br-radius", "br-missing")
                config.write_text(bad)
                failed = subprocess.run([binary, "-c", str(config)], timeout=10,
                                        capture_output=True)
                assert failed.returncode != 0
                assert not pidfile.exists()
                print("Linux integration: GRE/VLAN/firewall/API/signals/rollback passed")
            finally:
                if daemon.poll() is None:
                    daemon.terminate()
                    daemon.wait(timeout=10)
                log.seek(0)
                print(log.read())


if __name__ == "__main__":
    main()
