#!/usr/bin/env python3
"""Keep the source and Debian versions consistent, without committing or creating tags."""
import argparse
import os
import re
import tempfile
from datetime import datetime, timezone
from email.utils import format_datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def parse(value):
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", value):
        raise ValueError("Use MAJOR.MINOR.PATCH without leading zeroes")
    return tuple(map(int, value.split(".")))


def atomic_write(path, content):
    fd, temporary = tempfile.mkstemp(prefix=".version-", dir=path.parent)
    try:
        with os.fdopen(fd, "w") as file:
            file.write(content)
        os.chmod(temporary, path.stat().st_mode & 0o777 if path.exists() else 0o644)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def bump(root, requested):
    current = (root / "VERSION").read_text().strip()
    parts = list(parse(current))
    if requested in ("major", "minor", "patch"):
        index = ("major", "minor", "patch").index(requested)
        parts[index] += 1
        for following in range(index + 1, 3):
            parts[following] = 0
        requested = ".".join(map(str, parts))
    if parse(requested) <= parse(current):
        raise ValueError("The new version must be greater than the current version")
    path = root / "debian/changelog"
    previous = path.read_text()
    date = format_datetime(datetime.now(timezone.utc))
    stanza = (
        f"softgred ({requested}-1) unstable; urgency=medium\n\n"
        f"  * Release {requested}.\n\n"
        f" -- Jorge Pereira <jpereiran@gmail.com>  {date}\n\n"
    )
    documents = []
    for relative in ("README.md", "docs/en_US/index.md"):
        document = root / relative
        if document.exists():
            documents.append((document, document.read_text().replace(current, requested)))
    atomic_write(path, stanza + previous)
    for document, content in documents:
        atomic_write(document, content)
    atomic_write(root / "VERSION", requested + "\n")
    return requested


def check(root):
    current = (root / "VERSION").read_text().strip()
    parse(current)
    first = (root / "debian/changelog").read_text().splitlines()[0]
    distro = r"(?:debian12|debian13|ubuntu22\.04|ubuntu24\.04)"
    package = rf"softgred \({re.escape(current)}-1(?:\+{distro})?\) "
    if not re.match(package, first):
        raise ValueError("VERSION and Debian changelog do not match")
    return current


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "version",
        nargs="?",
        default="patch",
        help="major, minor, patch (default), or an explicit X.Y.Z",
    )
    parser.add_argument("--check", action="store_true", help="Validate version consistency")
    args = parser.parse_args()
    try:
        result = check(ROOT) if args.check else bump(ROOT, args.version)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(result)


if __name__ == "__main__":
    main()
