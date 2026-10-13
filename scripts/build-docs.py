#!/usr/bin/env python3
"""Render the project's deliberately small Markdown subset with no third-party packages."""
import html
import re
import shutil
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LANGUAGES = {
    "en_US": {
        "lang": "en-US", "label": "English",
        "eyebrow": "LINUX NETWORKING / DYNAMIC GRE",
        "title": "Tunnels on demand.\nLinux in control.",
        "intro": "Discover GRE peers, connect VLANs to bridges and let the kernel forward traffic.",
        "search": "Search documentation", "contents": "In this guide", "copy": "Copy",
        "copied": "Copied", "empty": "No sections found.", "guide": "Complete guide",
        "source": "Source code", "footer": "Project documentation · LGPL-3.0-or-later",
        "nodes": ["GRE peer", "SoftGRE", "Linux bridge"],
        "detail": "IPv4 + Ethernet + VLAN → discovery → gretap + VLAN → kernel forwarding",
    },
}


def slug(text):
    plain = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "-", plain).strip("-")


def inline(text):
    # Tokenize code/links before escaping, so generated tags never become Markdown input.
    pattern = r"`([^`]+)`|\[([^\]]+)\]\(([^)]+)\)"
    output, offset = [], 0
    for match in re.finditer(pattern, text):
        output.append(html.escape(text[offset:match.start()]))
        if match.group(1) is not None:
            output.append(f"<code>{html.escape(match.group(1))}</code>")
        else:
            label, url = match.group(2), match.group(3)
            if not re.match(r"^(https?://|#|\.?\.?/)", url):
                raise ValueError(f"Unsupported documentation link: {url}")
            output.append(f'<a href="{html.escape(url, quote=True)}">{html.escape(label)}</a>')
        offset = match.end()
    output.append(html.escape(text[offset:]))
    return "".join(output)


def render(markdown, labels):
    lines = markdown.splitlines()
    sections, toc, current, paragraph = [], [], [], []
    ids = set()
    index = 0

    def flush():
        if paragraph:
            current.append(f'<p>{inline(" ".join(paragraph))}</p>')
            paragraph.clear()

    while index < len(lines):
        line = lines[index]
        if line.startswith("# "):
            index += 1
            continue
        if line.startswith("## "):
            flush()
            if current:
                closing = "</section>" if current[0].startswith("<section") else ""
                sections.append("\n".join(current) + closing)
            title = line[3:]
            anchor = slug(title)
            if anchor in ids:
                raise ValueError(f"Duplicate section: {title}")
            ids.add(anchor)
            toc.append((anchor, title))
            current = [f'<section class="doc-section" id="{anchor}">',
                       f'<h2><a href="#{anchor}">{html.escape(title)}</a></h2>']
        elif line.startswith("### "):
            flush()
            title = line[4:]
            anchor = slug(title)
            if anchor in ids:
                raise ValueError(f"Duplicate heading: {title}")
            ids.add(anchor)
            current.append(f'<h3 id="{anchor}">{html.escape(title)}</h3>')
        elif line.startswith("```"):
            flush()
            language = line[3:]
            code = []
            index += 1
            while index < len(lines) and not lines[index].startswith("```"):
                code.append(lines[index])
                index += 1
            if index == len(lines):
                raise ValueError("Unclosed code fence")
            current.append('<div class="code-block">'
                           f'<div class="code-bar"><span>{html.escape(language or "text")}</span>'
                           f'<button type="button" class="copy">{labels["copy"]}</button></div>'
                           f'<pre><code>{html.escape(chr(10).join(code))}</code></pre></div>')
        elif line.startswith("| "):
            flush()
            rows = []
            while index < len(lines) and lines[index].startswith("|"):
                row = [cell.strip() for cell in lines[index].strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?", cell) for cell in row):
                    rows.append(row)
                index += 1
            if not rows:
                raise ValueError("Empty table")
            header = "".join(f'<th scope="col">{inline(cell)}</th>' for cell in rows[0])
            body = "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>"
                           for row in rows[1:])
            current.append(f'<div class="table-wrap"><table><thead><tr>{header}</tr></thead>'
                           f'<tbody>{body}</tbody></table></div>')
            continue
        elif line.startswith("- "):
            flush()
            items = []
            while index < len(lines) and lines[index].startswith("- "):
                item = lines[index][2:]
                index += 1
                while index < len(lines) and lines[index].startswith("  "):
                    item += " " + lines[index].strip()
                    index += 1
                items.append(f"<li>{inline(item)}</li>")
            current.append("<ul>" + "".join(items) + "</ul>")
            continue
        elif not line.strip():
            flush()
        else:
            paragraph.append(line.strip())
        index += 1
    flush()
    if current:
        # Intro paragraphs precede the first titled section.
        closing = "</section>" if current[0].startswith("<section") else ""
        sections.append("\n".join(current) + closing)
    return "\n".join(sections), toc


def page(locale, markdown, prefix="../"):
    labels = LANGUAGES[locale]
    article, toc = render(markdown, labels)
    nav = "".join(f'<a href="#{anchor}">{html.escape(title)}</a>' for anchor, title in toc)
    nodes = '<span class="arrow" aria-hidden="true">→</span>'.join(
        f'<span class="node">{name}</span>' for name in labels["nodes"])
    dark_label = "Dark theme"
    light_label = "Light theme"
    title = "<br>".join(html.escape(part) for part in labels["title"].split("\n"))
    return f'''<!doctype html>
<html lang="{labels['lang']}">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{html.escape(labels['intro'])}">
<title>SoftGRE · {labels['label']}</title>
<link rel="icon" type="image/svg+xml" href="{prefix}assets/softgre-logo.svg">
<link rel="stylesheet" href="{prefix}assets/site.css">
<script src="{prefix}assets/theme.js"></script>
<script src="{prefix}assets/site.js" defer></script>
</head>
<body data-copy="{labels['copy']}" data-copied="{labels['copied']}">
<a class="skip" href="#documentation">{labels['guide']}</a>
<header class="topbar"><a class="brand" href="{prefix}index.html">
<img class="brand-logo" src="{prefix}assets/softgre-logo.svg" alt="" width="42" height="42">
SoftGRE</a>
<nav aria-label="Site navigation"><a href="#documentation">Guide</a>
<a href="https://github.com/jpereira/softgre">GitHub ↗</a>
<button id="theme-toggle" class="theme-toggle" type="button" aria-pressed="false"
data-dark="{dark_label}" data-light="{light_label}" hidden>{dark_label}</button></nav></header>
<div class="shell"><section class="hero">
<p class="eyebrow">{labels['eyebrow']}</p><h1>{title}</h1><p class="lead">{labels['intro']}</p>
<div class="badges"><span>Linux</span><span>AF_PACKET + BPF</span><span>Native netlink</span></div>
<div class="network" aria-label="{html.escape(labels['detail'])}">{nodes}</div>
<p class="diagram-caption">{labels['detail']}</p></section>
<div class="docs-layout"><aside><div class="sidebar">
<label for="search">{labels['search']}</label><input id="search" type="search" autocomplete="off">
<p class="eyebrow">{labels['contents']}</p><nav aria-label="{labels['contents']}">{nav}</nav>
<a class="source" href="https://github.com/jpereira/softgre/blob/master/docs/{locale}/index.md">
{labels['source']} ↗</a></div></aside><main id="documentation" tabindex="-1">
<p class="empty" role="status" hidden>{labels['empty']}</p>{article}</main></div>
<footer>{labels['footer']}<a href="https://github.com/jpereira/softgre">GitHub →</a>
</footer></div></body></html>
'''


def build(output=None):
    output = (Path(output) if output else ROOT / "_site").resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "assets").mkdir(exist_ok=True)
    for asset in (ROOT / "docs/assets").iterdir():
        if asset.is_file():
            shutil.copyfile(asset, output / "assets" / asset.name)
    for locale in LANGUAGES:
        target = output / locale
        target.mkdir(exist_ok=True)
        markdown = (ROOT / "docs" / locale / "index.md").read_text()
        (target / "index.html").write_text(page(locale, markdown))
    markdown = (ROOT / "docs/en_US/index.md").read_text()
    (output / "index.html").write_text(page("en_US", markdown, prefix=""))
    stale = output / "pt_BR"
    if stale.is_dir():
        shutil.rmtree(stale)
    (output / ".nojekyll").touch()
    print(f"Documentation built: {output}")
    return output


if __name__ == "__main__":
    build()
