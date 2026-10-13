"""Build valid GRE Ethernet/VLAN test frames for the isolated lab interfaces."""
import socket
import struct
from pathlib import Path


def checksum(data):
    words = struct.unpack("!%dH" % (len(data) // 2), data)
    total = sum(words)
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def frame(
    remote="192.0.2.2",
    dest="192.0.2.1",
    vlan=1000,
    tags=0,
    flags=0,
    options=0,
    mac=b"\x02\x00\x00\x00\x00\x42",
):
    dst_mac = bytes.fromhex(Path("/sys/class/net/sg-test-in/address").read_text().replace(":", ""))
    peer_address = Path("/sys/class/net/sg-test-peer/address").read_text().replace(":", "")
    src_mac = bytes.fromhex(peer_address)
    inner = b"\xff" * 6 + mac + struct.pack("!HHH", 0x8100, 0xA000 | vlan, 0x0806)
    inner += struct.pack("!HHBBH", 1, 0x0800, 6, 4, 1)
    inner += mac + socket.inet_aton("198.51.100.2") + b"\0" * 6
    inner += socket.inet_aton("198.51.100.1") + b"\0" * 14
    gre = struct.pack("!HH", flags, 0x6558) + inner
    ip = struct.pack(
        "!BBHHHBBH4s4s",
        0x45 + options,
        0,
        20 + 4 * options + len(gre),
        42,
        0,
        64,
        47,
        0,
        socket.inet_aton(remote),
        socket.inet_aton(dest),
    )
    ip += b"\1" * (options * 4)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    ethernet = dst_mac + src_mac + struct.pack("!H", 0x8100 if tags else 0x0800)
    for tag in range(tags):
        ethernet += struct.pack("!HH", 123, 0x8100 if tag + 1 < tags else 0x0800)
    return ethernet + ip + gre
