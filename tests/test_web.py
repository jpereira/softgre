#!/usr/bin/env python3
"""Run the optional PHP adapter against a fake backend and real HTTP connections."""
import json
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

ROOT = Path(__file__).resolve().parent.parent


@unittest.skipUnless(shutil.which("php"), "PHP is optional")
class WebTests(unittest.TestCase):
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
        cls.php = subprocess.Popen(["php", "-S", f"127.0.0.1:{cls.http_port}",
                                    "-t", str(ROOT / "share/webinterface")], env=env,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", cls.http_port), timeout=0.1):
                    break
            except OSError:
                time.sleep(0.02)
        else:
            raise AssertionError("PHP server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.php.terminate()
        cls.php.wait(timeout=5)
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
        for action, command in (("status", "STAT"), ("tunnels", "STUN"), ("version", "VERS")):
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


if __name__ == "__main__":
    unittest.main()
