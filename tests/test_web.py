#!/usr/bin/env python3
"""Run both optional web adapters against a fake backend and real HTTP connections."""
import json
import sys
import importlib.util
import os
import shutil
import socket
import subprocess
import threading
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent


class WebTestCases:
    @classmethod
    def setUpClass(cls):
        cls.backend = socket.socket()
        cls.backend.bind(("127.0.0.1", 0))
        cls.backend.listen()
        cls.backend.settimeout(0.2)
        cls.port = cls.backend.getsockname()[1]
        cls.commands = []
        cls.running = True

        def serve():
            while cls.running:
                try:
                    client, _ = cls.backend.accept()
                except socket.timeout:
                    continue
                except OSError:
                    return
                with client:
                    query = client.recv(256).decode()
                    cls.commands.append(query)
                    if query.startswith("HELP"):
                        reply = "HELP - command list\n"
                    elif query.startswith("STUN"):
                        reply = "RESULT: OK\nBODY: 192.0.2.2@sgre0;\n"
                    elif query.startswith("LMIP"):
                        reply = "RESULT: OK\nBODY: 02:00:00:00:00:42\n"
                    elif query.startswith("GTMC"):
                        reply = "RESULT: NOTFOUND\n"
                    else:
                        reply = "RESULT: OK\nBODY: test-backend\n"
                    # Multiple writes exercise full response accumulation.
                    client.sendall(reply[:5].encode())
                    time.sleep(0.01)
                    client.sendall(reply[5:].encode())

        cls.thread = threading.Thread(target=serve, daemon=True)
        cls.thread.start()
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            cls.http_port = reserve.getsockname()[1]
        env = dict(os.environ, SOFTGRE_API_PORT=str(cls.port))
        cls.process = subprocess.Popen(cls.server_command(), env=env,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", cls.http_port), timeout=0.1):
                    break
            except OSError:
                time.sleep(0.02)
        else:
            raise AssertionError("HTTP server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        cls.process.wait(timeout=5)
        cls.running = False
        cls.thread.join(timeout=1)
        cls.backend.close()

    def request(self, params, method="GET"):
        url = f"http://127.0.0.1:{self.http_port}/api.php?{params}"
        request = urllib.request.Request(url, method=method)
        try:
            response = urllib.request.urlopen(request, timeout=3)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, json.loads(response.read())

    def test_commands(self):
        for action, command in (("status", "STAT"), ("version", "VERS")):
            status, result = self.request("action=" + action)
            self.assertEqual(status, 200)
            self.assertEqual(result, {"status": "OK", "body": "test-backend"})
            self.assertEqual(self.commands[-1], command + "\n")
        status, result = self.request("action=cpe&arg=02:00:00:00:00:42")
        self.assertEqual(result["status"], "NOTFOUND")
        self.assertEqual(status, 200)
        self.assertEqual(self.commands[-1], "GTMC 02:00:00:00:00:42\n")
        status, result = self.request("action=help")
        self.assertEqual(result["status"], "OK")
        self.assertIn("HELP", result["body"])

    def test_invalid_input(self):
        for params in ("action=unknown", "action[]=status", "action=ues&arg=999.1.1.1",
                       "action=cpe&arg=bad", "action=ues&arg=192.0.2.1%0ASTUN"):
            status, result = self.request(params)
            self.assertEqual(status, 400)
            self.assertEqual(result["status"], "INVALID")
        status, _ = self.request("action=status", "POST")
        self.assertEqual(status, 405)


@unittest.skipUnless(shutil.which("php"), "PHP is optional")
class PHPWebTests(WebTestCases, unittest.TestCase):
    @classmethod
    def server_command(cls):
        return ["php", "-S", f"127.0.0.1:{cls.http_port}",
                "-t", str(ROOT / "share/webinterface")]


class PythonWebTests(WebTestCases, unittest.TestCase):
    @classmethod
    def server_command(cls):
        return [sys.executable, str(ROOT / "share/webinterface/server.py"),
                "--port", str(cls.http_port)]

    def test_static_assets_and_isolation(self):
        for path in ("/", "/index.html", "/dashboard.css", "/dashboard.js"):
            with urllib.request.urlopen(f"http://127.0.0.1:{self.http_port}{path}") as reply:
                self.assertEqual(reply.status, 200)
                self.assertEqual(reply.headers["X-Content-Type-Options"], "nosniff")
                self.assertIn("frame-ancestors 'none'", reply.headers["Content-Security-Policy"])
                self.assertTrue(reply.read())
        for path in ("/server.py", "/../api.php", "/%2e%2e/api.php", "/.git/config"):
            with self.assertRaises(urllib.error.HTTPError) as raised:
                urllib.request.urlopen(f"http://127.0.0.1:{self.http_port}{path}")
            self.assertEqual(raised.exception.code, 404)
            raised.exception.close()
        url = f"http://127.0.0.1:{self.http_port}/api?action=status"
        with urllib.request.urlopen(url) as reply:
            self.assertEqual(json.load(reply)["status"], "OK")

    @unittest.skipUnless(Path("/sys/class/net").is_dir(), "Linux interface snapshot")
    def test_live_overview(self):
        status, result = self.request("action=overview")
        self.assertEqual(status, 200)
        self.assertEqual(result["tunnel_count"], 1)
        self.assertEqual(result["tunnels"], [{"remote": "192.0.2.2", "interface": "sgre0",
                                            "macs": ["02:00:00:00:00:42"]}])
        self.assertFalse(result["truncated"])
        self.assertTrue(any(link["name"] == "lo" for link in result["interfaces"]))
        self.assertTrue(all(isinstance(link["rx_packets"], int)
                            for link in result["interfaces"]))

    def test_strict_input(self):
        for params in ("action=status&action=help", "action=status&arg=x", "extra=x",
                       "action=status&arg=&extra=x", "action=cpe&arg=02:00:00:00:00:42%0A",
                       "action=ues&arg=192.0.2.002"):
            status, _ = self.request(params)
            self.assertEqual(status, 400)
        for method in ("HEAD", "TRACE", "PROPFIND"):
            url = f"http://127.0.0.1:{self.http_port}/api?action=status"
            with self.assertRaises(urllib.error.HTTPError) as raised:
                urllib.request.urlopen(urllib.request.Request(url, method=method))
            self.assertEqual(raised.exception.code, 405)
            self.assertEqual(raised.exception.headers["Allow"], "GET")
            raised.exception.close()
        before = len(self.commands)
        for _ in range(3):
            self.request("action=unknown")
        self.assertEqual(len(self.commands), before)

    def test_client_limit(self):
        clients = []
        try:
            for _ in range(16):
                clients.append(socket.create_connection(("127.0.0.1", self.http_port)))
                time.sleep(0.02)
            with socket.create_connection(("127.0.0.1", self.http_port)) as rejected:
                rejected.settimeout(2)
                self.assertIn(b"503 Service Unavailable", rejected.recv(1024))
        finally:
            for client in clients:
                client.close()
        time.sleep(0.1)
        self.assertEqual(self.request("action=status")[0], 200)


spec = importlib.util.spec_from_file_location("web_server", ROOT / "share/webinterface/server.py")
web = importlib.util.module_from_spec(spec)
spec.loader.exec_module(web)


class BackendTests(unittest.TestCase):
    def query_reply(self, reply, action="status", hold=0):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()

            def serve():
                with listener.accept()[0] as client:
                    client.recv(256)
                    try:
                        client.sendall(reply)
                        time.sleep(hold)
                    except OSError:
                        pass

            thread = threading.Thread(target=serve)
            thread.start()
            try:
                return web.backend_query(listener.getsockname(), action, b"STAT\n")
            finally:
                thread.join(timeout=2)

    def test_backend_statuses(self):
        for status in ("OK", "NULL", "NOTFOUND", "INVALID"):
            self.assertEqual(self.query_reply(f"RESULT: {status}\n".encode()),
                             {"status": status, "body": ""})
        self.assertEqual(self.query_reply(b"RESULT: OK\nBODY: a\nb\n")["body"], "a\nb")

    def test_backend_failures(self):
        for reply in (b"", b"RESULT: UNKNOWN\n", b"RESULT: OK\nBODY: \xff",
                      b"x" * (web.MAX_RESPONSE + 1)):
            with self.assertRaises(web.AdapterError) as raised:
                self.query_reply(reply)
            self.assertEqual(raised.exception.code, 502)
        old = web.BACKEND_TIMEOUT
        try:
            web.BACKEND_TIMEOUT = 0.1
            with self.assertRaises(web.AdapterError) as raised:
                self.query_reply(b"RESULT: OK\n", hold=0.25)
            self.assertEqual(raised.exception.code, 504)
        finally:
            web.BACKEND_TIMEOUT = old
        with socket.socket() as closed:
            closed.bind(("127.0.0.1", 0))
            address = closed.getsockname()
        with self.assertRaises(web.AdapterError) as raised:
            web.backend_query(address, "status", b"STAT\n")
        self.assertEqual(raised.exception.code, 502)

    @unittest.skipUnless(Path("/sys/class/net").is_dir(), "Linux interface snapshot")
    def test_overview_limits_and_deadline(self):
        listing = ";".join(f"192.0.2.{i + 1}@sgre{i}" for i in range(65))
        responses = [{"status": "OK", "body": "status"}, {"status": "OK", "body": listing}]
        responses += [{"status": "OK", "body": "02:00:00:00:00:42"}] * 64
        with patch.object(web, "backend_query", side_effect=responses) as backend:
            result = web.overview(("127.0.0.1", 8888))
        self.assertEqual(result["tunnel_count"], 65)
        self.assertEqual(len(result["tunnels"]), 64)
        self.assertTrue(result["truncated"])
        deadlines = [call.args[3] for call in backend.call_args_list]
        self.assertEqual(len(deadlines), 66)
        self.assertEqual(len(set(deadlines)), 1)
        for malformed in ("bad@sgre0", "192.0.2.2@../../etc", "192.0.2.2"):
            responses = [{"status": "OK", "body": "status"},
                         {"status": "OK", "body": malformed}]
            with patch.object(web, "backend_query", side_effect=responses):
                with self.assertRaises(web.AdapterError) as raised:
                    web.overview(("127.0.0.1", 8888))
                self.assertEqual(raised.exception.code, 502)

    def test_invalid_configuration(self):
        for arguments in (["--port", "0"], ["--port", "65536"],
                          ["--api-host", "example.com"], ["--bind", "::1"]):
            result = subprocess.run([sys.executable, str(ROOT / "share/webinterface/server.py"),
                                     *arguments], capture_output=True)
            self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
