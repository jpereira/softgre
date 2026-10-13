#!/usr/bin/env python3
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("version", ROOT / "scripts/version.py")
version = importlib.util.module_from_spec(spec)
spec.loader.exec_module(version)


class VersionTests(unittest.TestCase):
    def test_bumps_and_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "debian").mkdir()
            (root / "VERSION").write_text("2.0.1\n")
            (root / "debian/changelog").write_text("softgred (2.0.1-1) unstable; urgency=medium\n")
            (root / "README.md").write_text("Download version 2.0.1\n")
            self.assertEqual(version.check(root), "2.0.1")
            self.assertEqual(version.bump(root, "patch"), "2.0.2")
            self.assertEqual(version.bump(root, "minor"), "2.1.0")
            self.assertEqual(version.bump(root, "major"), "3.0.0")
            self.assertEqual(version.bump(root, "4.5.6"), "4.5.6")
            self.assertEqual(version.check(root), "4.5.6")
            self.assertIn("4.5.6", (root / "README.md").read_text())
            before = (root / "debian/changelog").read_text()
            for invalid in ("4.5.6", "1.0.0", "01.2.3", "a.b.c", "1.2", "1.2.3-rc1"):
                with self.assertRaises(ValueError):
                    version.bump(root, invalid)
                self.assertEqual((root / "debian/changelog").read_text(), before)
            (root / "VERSION").write_text("9.0.0\n")
            with self.assertRaises(ValueError):
                version.check(root)


if __name__ == "__main__":
    unittest.main()
