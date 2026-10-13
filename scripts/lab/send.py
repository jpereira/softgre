#!/usr/bin/env python3
"""Inject GRE frames into an isolated SoftGRE lab and verify learned interfaces."""
import argparse
import ipaddress
import json
import os
import re
import socket
import subprocess
import time
import urllib.request
from packets import frame


def main():
    if os.environ.get("SOFTGRE_LAB") != "isolated":
        raise SystemExit("Run this script inside the test-softgre lab container")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=5, help="Packets per client (1..10000)")
    parser.add_argument("--interval", type=float, default=0.1, help="Seconds between packets")
    parser.add_argument("--remote", type=ipaddress.IPv4Address, help="Single remote IPv4")
    parser.add_argument("--mac", default="02:00:00:00:00:42", help="Single remote source MAC")
    parser.add_argument("--vlan", type=int, choices=(10, 1000), default=1000)
    args = parser.parse_args()
    if not 1 <= args.count <= 10000 or not 0 <= args.interval <= 10:
        parser.error("Count must be 1..10000 and interval 0..10")
    if not re.fullmatch(r"[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}", args.mac):
        parser.error("Expected a colon-separated MAC address")
    mac = bytes.fromhex(args.mac.replace(":", ""))
    if mac[0] & 1 or not any(mac):
        parser.error("Expected a nonzero unicast source MAC")
    clients = [(str(args.remote), mac, args.vlan)] if args.remote else [
        ("192.0.2.2", bytes.fromhex("020000000042"), 1000),
        ("192.0.2.2", bytes.fromhex("020000000043"), 10),
        ("192.0.2.3", bytes.fromhex("020000000044"), 1000)]
    with socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(3)) as raw:
        raw.bind(("sg-test-peer", 0))
        for remote, source, vlan in clients:
            for _ in range(args.count):
                raw.send(frame(remote=remote, mac=source, vlan=vlan))
                time.sleep(args.interval)
            print(f"Sent {args.count} GRE packets: remote={remote} VLAN={vlan} "
                  f"MAC={source.hex(':')}", flush=True)
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        url = "http://127.0.0.1:8080/api?action=overview"
        with urllib.request.urlopen(url, timeout=6) as reply:
            snapshot = json.load(reply)
        learned = {t["remote"]: t for t in snapshot["tunnels"]}
        if all(remote in learned and source.hex(':') in learned[remote]["macs"]
               for remote, source, _ in clients):
            break
        time.sleep(0.1)
    else:
        raise SystemExit("Daemon did not learn all clients")
    for remote, _, vlan in clients:
        name = learned[remote]["interface"] + f".{vlan}"
        links = json.loads(subprocess.check_output(["ip", "-j", "-d", "link", "show", name]))
        expected = "br-radius" if vlan == 1000 else "br-hotspot"
        if links[0].get("master") != expected:
            raise SystemExit(f"{name} is not attached to {expected}")
        print(f"Verified {remote}: {name} attached to {expected}")
    print(json.dumps(snapshot, indent=2))


if __name__ == "__main__":
    main()
