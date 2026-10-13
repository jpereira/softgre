#!/usr/bin/env python3
"""Require both the HTTP adapter and the daemon's TCP backend to respond."""
import json
import urllib.request

with urllib.request.urlopen("http://127.0.0.1:8080/api?action=status", timeout=6) as reply:
    if json.load(reply)["status"] != "OK":
        raise SystemExit(1)
