#!/usr/bin/env python3
"""Keep a blank line after C include blocks, including before global declarations."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
paths = sorted((root / "src").glob("*.[ch]")) + sorted((root / "tests").glob("*.c"))
dirty = False
for path in paths:
    source = path.read_text()
    formatted = re.sub(r"(^#include[^\n]*\n)(?!\n|#include)", r"\1\n", source, flags=re.MULTILINE)
    if source == formatted:
        continue
    dirty = True
    if args.check:
        print(f"{path.relative_to(root)}: missing blank line after include block")
    else:
        path.write_text(formatted)
raise SystemExit(1 if args.check and dirty else 0)
