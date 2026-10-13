#!/usr/bin/env python3
"""Supervise the installed SoftGRE daemon and its unprivileged Python dashboard."""
import os
import pwd
import signal
import subprocess
import sys
import time
from pathlib import Path


def main():
    stopping = False

    def stop(signum, frame):
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    daemon = None
    web = None
    result = 1
    try:
        binary = Path(__file__).resolve().parents[2] / "sbin/softgred"
        daemon = subprocess.Popen([str(binary), "-f", *sys.argv[1:]])
        # Informational CLI invocations do not start a listener.
        if any(arg in {"-h", "--help", "-v", "--version"} for arg in sys.argv[1:]):
            return daemon.wait()
        environment = dict(os.environ)
        environment.setdefault("SOFTGRE_WEB_BIND", "0.0.0.0")
        credentials = {}
        if os.geteuid() == 0:
            account = pwd.getpwnam("nobody")
            credentials = dict(user=account.pw_uid, group=account.pw_gid, extra_groups=[])
        server = Path(__file__).resolve().parent / "webinterface/server.py"
        web = subprocess.Popen([sys.executable, str(server)], env=environment, **credentials)
        while not stopping:
            for child in (daemon, web):
                status = child.poll()
                if status is not None:
                    result = status if status > 0 else 1
                    stopping = True
                    break
            time.sleep(0.1)
        if all(child.poll() is None for child in (daemon, web)):
            result = 0
    except (OSError, KeyError) as error:
        print(f"Cannot start SoftGRE services: {error}", file=sys.stderr)
    finally:
        children = [child for child in (web, daemon) if child is not None]
        for child in children:
            if child.poll() is None:
                child.terminate()
        deadline = time.monotonic() + 85
        for child in children:
            try:
                child.wait(timeout=max(0.1, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
                result = 1
    return result


if __name__ == "__main__":
    raise SystemExit(main())
