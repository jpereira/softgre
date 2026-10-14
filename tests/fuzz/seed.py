#!/usr/bin/env python3
"""Write valid GRE frames (several VLANs, outer tags, IPv4 options) as parser fuzz seeds."""
import socket
import struct
import sys
from pathlib import Path


def build(vlan, outer_tags, ihl):
    inner = (
        b"\xff" * 6
        + b"\x02\x00\x00\x00\x00\x42"
        + struct.pack("!HHH", 0x8100, 0xA000 | vlan, 0x0806)
    )
    gre = struct.pack("!HH", 0, 0x6558) + inner
    ip = struct.pack(
        "!BBHHHBBH4s4s",
        0x40 | ihl,
        0,
        ihl * 4 + len(gre),
        42,
        0,
        64,
        47,
        0,
        socket.inet_aton("192.0.2.2"),
        socket.inet_aton("192.0.2.1"),
    ) + b"\0" * ((ihl - 5) * 4)
    eth = b"\xaa" * 6 + b"\xbb" * 6
    if outer_tags:
        eth += struct.pack("!H", 0x8100)
        for tag in range(outer_tags):
            eth += struct.pack("!HH", 100, 0x8100 if tag + 1 < outer_tags else 0x0800)
    else:
        eth += struct.pack("!H", 0x0800)
    return eth + ip + gre


directory = Path(sys.argv[1])
directory.mkdir(parents=True, exist_ok=True)
for index, (vlan, tags, ihl) in enumerate([(1000, 0, 5), (10, 1, 5), (4094, 2, 7), (1, 0, 6)]):
    (directory / f"seed{index}").write_bytes(build(vlan, tags, ihl))

# Seed typed configuration paths, not just the INI reader's early rejection branches.
if len(sys.argv) > 2:
    config = Path(sys.argv[2])
    config.mkdir(parents=True, exist_ok=True)
    base = b"[global]\ninterface=eth0\nbridge-map=1000@br0,10@br1\n"
    for index, tail in enumerate(
        (
            b"",
            b"allow-remotes=192.0.2.0/24,203.0.113.5\n",
            b"allow-remotes=\n",
            b"\0allow-remotes=192.0.2.0/24\n",
            b"[service]\nport=8888\nmax-listen=128\n[debug]\nenable=true\n",
        )
    ):
        (config / f"seed{index}").write_bytes(base + tail)
