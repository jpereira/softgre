#!/usr/bin/env python3
"""Validate English site generation, escaping, local navigation and headings."""
import importlib.util
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("build_docs", ROOT / "scripts/build-docs.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class Page(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.ids, self.links, self.lang = set(), [], None
        self.duplicate_ids = []
        self.stack, self.unbalanced = [], []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        if tag not in {"meta", "link", "input", "br", "hr", "img"}:
            self.stack.append(tag)
        attrs = dict(attrs)
        if "id" in attrs:
            if attrs["id"] in self.ids:
                self.duplicate_ids.append(attrs["id"])
            self.ids.add(attrs["id"])
        for key in ("href", "src"):
            if key in attrs:
                self.links.append(attrs[key])
        if tag == "html":
            self.lang = attrs.get("lang")

    def handle_endtag(self, tag):
        if not self.stack or self.stack[-1] != tag:
            self.unbalanced.append(tag)
        else:
            self.stack.pop()


class DocsTests(unittest.TestCase):
    def test_generated_navigation(self):
        with tempfile.TemporaryDirectory() as directory:
            output = builder.build(directory)
            pages = {path: Page(path.read_text()) for path in output.rglob("*.html")}
            self.assertEqual(len(pages), 2)
            for path, page in pages.items():
                self.assertFalse(page.duplicate_ids)
                self.assertFalse(page.unbalanced)
                self.assertFalse(page.stack)
                for link in page.links:
                    url = urlsplit(link)
                    if url.scheme or url.netloc:
                        continue
                    target = (path.parent / unquote(url.path)).resolve() if url.path else path
                    self.assertTrue(target.is_relative_to(output.resolve()))
                    self.assertTrue(target.exists(), link)
                    if url.fragment:
                        self.assertIn(url.fragment, pages[target].ids)
            self.assertEqual(set(builder.LANGUAGES), {"en_US"})
            counts = []
            for locale, labels in builder.LANGUAGES.items():
                source = (ROOT / "docs" / locale / "index.md").read_text()
                article, toc = builder.render(source, labels)
                self.assertGreater(len(toc), 8)
                counts.append(len(toc))
                self.assertEqual(pages[output / locale / "index.html"].lang, labels["lang"])
                self.assertIn("AF_PACKET", article)
                self.assertIn("NETLINK_ROUTE", article)
            self.assertEqual(len(counts), 1)
            self.assertFalse((output / "pt_BR").exists())
            self.assertEqual(pages[output / "index.html"].lang, "en-US")

    def test_escaping_and_invalid_markdown(self):
        labels = builder.LANGUAGES["en_US"]
        source = "## Safe\n\n<script>alert(1)</script>\n\n```c\n<&>\n```"
        article, _ = builder.render(source, labels)
        self.assertNotIn("<script>", article)
        self.assertIn("&lt;script&gt;", article)
        self.assertIn("&lt;&amp;&gt;", article)
        with self.assertRaises(ValueError):
            builder.render("## A\n\n```c\nunfinished", labels)
        with self.assertRaises(ValueError):
            builder.render("## A\n\n## A\n", labels)
        with self.assertRaises(ValueError):
            builder.inline("[bad](javascript:alert)")
        article, _ = builder.render("## A\n\n### Detail\n\n```sh\npython3 server.py\n```", labels)
        self.assertIn('<h3 id="detail">Detail</h3>', article)
        self.assertIn("python3 server.py", article)
        with self.assertRaises(ValueError):
            builder.render("## A\n\n### A\n", labels)
        self.assertIn('href="https://example.com"', builder.inline("[ok](https://example.com)"))


if __name__ == "__main__":
    unittest.main()
