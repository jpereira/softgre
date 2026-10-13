#!/usr/bin/env python3
"""Serve the SoftGRE dashboard and its read-only TCP adapter using Python 3.9+."""
import argparse
import ipaddress
import json
import os
import re
import signal
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from socketserver import ThreadingMixIn
from urllib.parse import parse_qs, urlsplit

ASSETS = {"/": ("index.html", "text/html"), "/index.html": ("index.html", "text/html"),
          "/dashboard.js": ("dashboard.js", "text/javascript"),
          "/dashboard.css": ("dashboard.css", "text/css")}
COMMANDS = {"status": "STAT", "tunnels": "STUN", "ues": "LMIP", "cpe": "GTMC",
            "version": "VERS", "help": "HELP", "overview": "OVERVIEW"}
MAX_RESPONSE = 1024 * 1024
BACKEND_TIMEOUT = 5


class AdapterError(Exception):
    def __init__(self, code, status, body):
        self.code, self.status, self.body = code, status, body


def ipv4(value):
    try:
        return str(ipaddress.IPv4Address(value))
    except ipaddress.AddressValueError as error:
        raise argparse.ArgumentTypeError("Expected an IPv4 address") from error


def port(value):
    try:
        number = int(value)
        if not 1 <= number <= 65535:
            raise ValueError
        return number
    except ValueError as error:
        raise argparse.ArgumentTypeError("Expected a port between 1 and 65535") from error


def command_for(query):
    try:
        params = parse_qs(query, keep_blank_values=True, max_num_fields=2,
                          encoding="utf-8", errors="strict")
        if any(key not in {"action", "arg"} or len(value) != 1
               for key, value in params.items()):
            raise ValueError
        action = params.get("action", ["status"])[0]
        arg = params.get("arg", [""])[0]
        command = COMMANDS[action]
        if action == "ues":
            arg = ipv4(arg)
        elif action == "cpe":
            if not re.fullmatch(r"[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}", arg):
                raise ValueError
        elif arg:
            raise ValueError
    except (KeyError, ValueError, UnicodeError, argparse.ArgumentTypeError) as error:
        raise AdapterError(400, "INVALID", "Invalid action or argument") from error
    if action in {"ues", "cpe"}:
        command += " " + arg
    return action, (command + "\n").encode("ascii")


def backend_query(address, action, command, deadline=None):
    if deadline is None:
        deadline = time.monotonic() + BACKEND_TIMEOUT
    try:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout
        with socket.create_connection(address, timeout=min(2, remaining)) as backend:
            backend.settimeout(max(0.001, deadline - time.monotonic()))
            backend.sendall(command)
            chunks, size = [], 0
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise socket.timeout
                backend.settimeout(remaining)
                chunk = backend.recv(min(8192, MAX_RESPONSE + 1 - size))
                if not chunk:
                    break
                size += len(chunk)
                if size > MAX_RESPONSE:
                    raise AdapterError(502, "ERROR", "Backend response too large")
                chunks.append(chunk)
    except socket.timeout as error:
        raise AdapterError(504, "ERROR", "Backend timeout") from error
    except OSError as error:
        raise AdapterError(502, "ERROR", "Backend unavailable") from error
    try:
        reply = b"".join(chunks).decode("utf-8", errors="strict")
    except UnicodeError as error:
        raise AdapterError(502, "ERROR", "Invalid backend response") from error
    if action == "help":
        return {"status": "OK", "body": reply}
    match = re.fullmatch(r"RESULT: (OK|NULL|NOTFOUND|INVALID)\n(?:BODY: ([\s\S]*))?", reply)
    if not match:
        raise AdapterError(502, "ERROR", "Invalid backend response")
    return {"status": match[1], "body": (match[2] or "").rstrip("\r\n")}


def overview(address):
    """Bound the complete snapshot to five seconds and 64 tunnel/MAC queries."""
    deadline = time.monotonic() + BACKEND_TIMEOUT
    status = backend_query(address, "status", b"STAT\n", deadline)
    listing = backend_query(address, "tunnels", b"STUN\n", deadline)
    if status["status"] != "OK" or listing["status"] not in {"OK", "NULL"}:
        raise AdapterError(502, "ERROR", "Invalid overview response")
    entries = [entry for entry in listing["body"].strip().split(";") if entry]
    tunnels = []
    for entry in entries[:64]:
        remote, separator, name = entry.partition("@")
        try:
            remote = ipv4(remote)
        except argparse.ArgumentTypeError as error:
            raise AdapterError(502, "ERROR", "Invalid tunnel response") from error
        if not separator or not re.fullmatch(r"[a-zA-Z0-9_.-]{1,15}", name):
            raise AdapterError(502, "ERROR", "Invalid tunnel response")
        macs = backend_query(address, "ues", f"LMIP {remote}\n".encode(), deadline)
        tunnels.append({"remote": remote, "interface": name,
                        "macs": [mac for mac in macs["body"].split(";") if mac]})
    interfaces = []
    for path in sorted(Path("/sys/class/net").iterdir())[:128]:
        try:
            master = path / "master"
            interfaces.append({"name": path.name,
                               "state": (path / "operstate").read_text().strip(),
                               "bridge": (master.resolve().name
                                          if (master / "bridge").exists() else ""),
                               "rx_packets": int((path / "statistics/rx_packets").read_text()),
                               "tx_packets": int((path / "statistics/tx_packets").read_text())})
        except (OSError, ValueError):
            continue  # Interfaces can disappear while the snapshot is collected.
    return {"status": "OK", "body": status["body"], "tunnels": tunnels,
            "tunnel_count": len(entries), "truncated": len(entries) > 64,
            "interfaces": interfaces}


class DashboardHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "SoftGRE"
    sys_version = ""

    def setup(self):
        self.request.settimeout(5)
        super().setup()
        self.deadline_timer = threading.Timer(10, self.expire)
        self.deadline_timer.daemon = True
        self.deadline_timer.start()

    def expire(self):
        try:
            self.request.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

    def finish(self):
        self.deadline_timer.cancel()
        super().finish()

    def reply(self, code, content, content_type, head=False, allow=False):
        self.close_connection = True
        self.send_response(code)
        self.send_header("Content-Type", content_type + "; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Connection", "close")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; frame-ancestors 'none'")
        if allow:
            self.send_header("Allow", "GET")
        self.end_headers()
        if not head:
            self.wfile.write(content)

    def json_reply(self, code, result, **kwargs):
        content = json.dumps(result, ensure_ascii=True).encode("utf-8")
        self.reply(code, content, "application/json", **kwargs)

    def send_error(self, code, message=None, explain=None):
        if code == 501:
            self.unsupported()
        else:
            super().send_error(code, message, explain)

    def do_GET(self):
        self.route()

    def do_HEAD(self):
        self.route(head=True)

    def route(self, head=False):
        try:
            if len(self.path) > 2048:
                raise AdapterError(414, "INVALID", "Request target too long")
            try:
                target = urlsplit(self.path)
            except ValueError as error:
                raise AdapterError(400, "INVALID", "Invalid request target") from error
            if target.scheme or target.netloc or target.fragment:
                raise AdapterError(400, "INVALID", "Invalid request target")
            if target.path in {"/api", "/api.php"}:
                if head:
                    raise AdapterError(405, "INVALID", "Use GET")
                action, command = command_for(target.query)
                result = (overview(self.server.backend) if action == "overview" else
                          backend_query(self.server.backend, action, command))
                self.json_reply(200, result)
            elif target.path in ASSETS:
                filename, content_type = ASSETS[target.path]
                self.reply(200, self.server.assets[filename], content_type, head=head)
            else:
                raise AdapterError(404, "NOTFOUND", "Not found")
        except AdapterError as error:
            try:
                self.json_reply(error.code, {"status": error.status, "body": error.body},
                                head=head, allow=error.code == 405)
            except OSError:
                self.close_connection = True
        except (BrokenPipeError, ConnectionResetError, socket.timeout):
            self.close_connection = True

    def unsupported(self):
        try:
            self.json_reply(405, {"status": "INVALID", "body": "Use GET"}, allow=True)
        except (OSError, socket.timeout):
            self.close_connection = True

    do_POST = do_PUT = do_DELETE = do_PATCH = do_OPTIONS = do_TRACE = do_CONNECT = unsupported


class DashboardServer(ThreadingMixIn, HTTPServer):
    allow_reuse_address = True
    daemon_threads = False
    request_queue_size = 16

    def __init__(self, address, backend, directory):
        self.backend = backend
        self.assets = {name: (directory / name).read_bytes() for name, _ in ASSETS.values()}
        self.slots = threading.BoundedSemaphore(16)
        super().__init__(address, DashboardHandler)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            try:
                request.settimeout(0.1)
                request.sendall(b"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n"
                                b"Connection: close\r\n\r\n")
            except OSError:
                pass
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", type=ipv4,
                        default=os.environ.get("SOFTGRE_WEB_BIND", "127.0.0.1"),
                        help="HTTP IPv4 address (default: 127.0.0.1)")
    parser.add_argument("--port", type=port, default=os.environ.get("SOFTGRE_WEB_PORT", "8080"),
                        help="HTTP port (default: 8080)")
    parser.add_argument("--api-host", type=ipv4,
                        default=os.environ.get("SOFTGRE_API_HOST", "127.0.0.1"))
    parser.add_argument("--api-port", type=port,
                        default=os.environ.get("SOFTGRE_API_PORT", "8888"))
    args = parser.parse_args()
    try:
        server = DashboardServer((args.bind, args.port), (args.api_host, args.api_port),
                                 Path(__file__).resolve().parent)
    except OSError as error:
        parser.exit(1, f"Cannot start dashboard: {error}\n")

    def stop(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, stop)
    print(f"SoftGRE dashboard: http://{args.bind}:{args.port}/", flush=True)
    try:
        server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
