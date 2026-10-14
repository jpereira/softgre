#!/usr/bin/env python3
"""dnsmasq dhcp-script: record the SoftGRE remote and tunnel behind each DHCP lease.

dnsmasq runs it as `dhcp-lease-hook.py add|old|del MAC IP [HOSTNAME]`. The hook asks the
softgred API which remote learned the client MAC and prints one line, which dnsmasq
logs. Set SOFTGRE_DHCP_LOG to also append the lines to a file. The API address comes
from SOFTGRE_API_HOST and SOFTGRE_API_PORT (default 127.0.0.1:8888).
"""
import os
import re
import socket
import sys
import time

ACTIONS = {"add", "old", "del"}
MAC = re.compile(r"[0-9a-f]{2}(:[0-9a-f]{2}){5}")
REPLY = re.compile(r"RESULT: OK\nBODY: ([0-9.]+)@([\w.-]+)\n")
MAX_REPLY = 4096


def clean(value):
    # Hostnames and relay IDs come from clients or APs; keep log lines single-token.
    return re.sub(r"[^\w.:@/+-]", "_", value)[:64]


def relay_id(name):
    # dnsmasq passes option 82 sub-options as raw bytes, cut at the first zero byte.
    raw = os.environb.get(name.encode(), b"")
    if all(0x20 <= byte < 0x7F for byte in raw):
        return raw.decode("ascii")
    return raw.hex(":")


def tunnel_of(mac, address):
    try:
        with socket.create_connection(address, timeout=2) as api:
            api.settimeout(2)
            api.sendall(f"GTMC {mac}\n".encode("ascii"))
            reply = b""
            while len(reply) <= MAX_REPLY:
                chunk = api.recv(MAX_REPLY)
                if not chunk:
                    break
                reply += chunk
    except OSError:
        return None
    match = REPLY.fullmatch(reply.decode("ascii", errors="replace"))
    return match.groups() if match else None


def main(argv):
    if len(argv) < 4 or argv[1] not in ACTIONS:
        return 0
    action, mac, ip = argv[1], argv[2].lower(), argv[3]
    host = os.environ.get("SOFTGRE_API_HOST", "127.0.0.1")
    try:
        port = int(os.environ.get("SOFTGRE_API_PORT", "8888"))
    except ValueError:
        port = 8888
    learned = tunnel_of(mac, (host, port)) if MAC.fullmatch(mac) else None
    remote, tunnel = learned or ("-", "-")
    fields = {
        "action": action,
        "ip": ip,
        "mac": mac,
        "interface": os.environ.get("DNSMASQ_INTERFACE", ""),
        "remote": remote,
        "tunnel": tunnel,
        "hostname": argv[4] if len(argv) > 4 else "",
        "circuit-id": relay_id("DNSMASQ_CIRCUIT_ID"),
        "remote-id": relay_id("DNSMASQ_REMOTE_ID"),
    }
    line = "DHCP lease " + " ".join(f"{k}={clean(v)}" for k, v in fields.items() if v)
    print(line, flush=True)
    path = os.environ.get("SOFTGRE_DHCP_LOG")
    if path:
        stamp = time.strftime("%Y-%m-%dT%H:%M:%S%z")
        try:
            with open(path, "a", encoding="utf-8") as log:
                log.write(f"[{stamp}] {line}\n")
        except OSError as error:
            print(f"Cannot append to {path}: {error}", file=sys.stderr)
    # Never fail the lease because the concentrator or log is unavailable.
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
