#!/usr/bin/env python3
"""Test MAC ownership at the concentrator's local bridge input in an isolated netns."""
import json
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from integration import query, run, wait_for
from packets import checksum, frame


def udp_frame(remote, mac, bridge_mac, marker):
    inner_ip = struct.pack(
        "!BBHHHBBH4s4s",
        0x45,
        0,
        28 + len(marker),
        1,
        0,
        64,
        17,
        0,
        socket.inet_aton("198.51.100.2"),
        socket.inet_aton("198.51.100.1"),
    )
    inner_ip = inner_ip[:10] + struct.pack("!H", checksum(inner_ip)) + inner_ip[12:]
    udp = struct.pack("!HHHH", 19000, 19001, 8 + len(marker), 0) + marker
    inner = bridge_mac + mac + struct.pack("!HHH", 0x8100, 1000, 0x0800) + inner_ip + udp
    outer = bytearray(frame(remote=remote, mac=mac)[:38])
    struct.pack_into("!H", outer, 16, 24 + len(inner))
    outer[24:26] = b"\0\0"
    outer[24:26] = struct.pack("!H", checksum(outer[14:34]))
    return bytes(outer) + inner


def main():
    if os.environ.get("SOFTGRE_TEST_NAMESPACE") != "isolated":
        raise SystemExit("Run in a disposable netns with SOFTGRE_TEST_NAMESPACE=isolated")
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else "bin/softgred").resolve())
    for name in ("sg-test-in", "sg-test-peer", "br-security"):
        assert not Path("/sys/class/net", name).exists(), "test interface already exists"
    run("ip", "link", "set", "lo", "up")
    run("ip", "link", "add", "sg-test-in", "type", "veth", "peer", "name", "sg-test-peer")
    run("ip", "addr", "add", "192.0.2.1/24", "dev", "sg-test-in")
    for name in ("sg-test-in", "sg-test-peer"):
        run("ip", "link", "set", name, "up")
    with tempfile.TemporaryDirectory() as directory:
        config = Path(directory, "config")
        config.write_text(
            "[global]\ninterface=sg-test-in\nbridge-map=1000@br-security\nbridge-force=true\n"
            "allow-remotes=192.0.2.2,192.0.2.3\n"
            f"pid-file={directory}/pid\nlog-file={directory}/events.log\n"
            "[service]\nbind-in=lo\nport=18888\n"
        )
        with Path(directory, "daemon.log").open("w+") as log:
            daemon = subprocess.Popen([binary, "-f", "-c", str(config)], stdout=log, stderr=log)
            try:
                wait_for(lambda: "RESULT: NULL" in query("STUN"))
                mac_a = bytes.fromhex("020000000042")
                mac_b = bytes.fromhex("020000000043")
                with socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(3)) as raw:
                    raw.bind(("sg-test-peer", 0))
                    raw.send(frame(remote="192.0.2.4"))
                    time.sleep(0.1)
                    assert "RESULT: NULL" in query("STUN"), "unauthorized remote provisioned"
                    for remote, mac in (("192.0.2.2", mac_a), ("192.0.2.3", mac_b)):
                        raw.send(frame(remote=remote, mac=mac))
                        wait_for(lambda: remote in query("STUN"))
                    run("ip", "addr", "add", "198.51.100.1/24", "dev", "br-security")
                    bridge_mac = bytes.fromhex(
                        Path("/sys/class/net/br-security/address").read_text().replace(":", "")
                    )
                    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sink:
                        sink.bind(("198.51.100.1", 19001))
                        sink.settimeout(1)
                        raw.send(udp_frame("192.0.2.2", mac_a, bridge_mac, b"authorized"))
                        assert sink.recv(64) == b"authorized", "authorized path failed"
                        raw.send(udp_frame("192.0.2.3", mac_a, bridge_mac, b"spoofed"))
                        try:
                            leaked = sink.recv(64)
                        except socket.timeout:
                            leaked = None
                        assert leaked is None, f"MAC ownership bypass at bridge INPUT: {leaked!r}"
                        assert "192.0.2.2@" in query("GTMC 02:00:00:00:00:42")
                        entries = json.loads(
                            run("bridge", "-j", "fdb", "show", "br", "br-security")
                        )
                        owners = {
                            entry.get("ifname", entry.get("dev"))
                            for entry in entries
                            if entry.get("mac") == "02:00:00:00:00:42"
                        }
                        assert owners == {
                            "sgre0.1000"
                        }, f"Spoofed MAC poisoned bridge FDB: {owners}"
                print("Security integration: remote allowlist and local MAC ownership passed")
            finally:
                daemon.terminate()
                daemon.wait(timeout=15)
                log.seek(0)
                print(log.read())


if __name__ == "__main__":
    main()
