#!/usr/bin/env python3
"""Render the project's deliberately small Markdown subset with no third-party packages."""
import html
import re
import shutil
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Guide pages in reading order; each is docs/<locale>/<slug>.md with a "# Title" line.
PAGES = (
    "index",
    "try-it",
    "dhcp",
    "install",
    "configuration",
    "api",
    "architecture",
    "development",
)
REPOSITORY = "https://github.com/jpereira/softgre"
LANGUAGES = {
    "en_US": {
        "lang": "en-US",
        "label": "English",
        "eyebrow": "LINUX NETWORKING / DYNAMIC GRE",
        "title": "Tunnels on demand.\nLinux in control.",
        "intro": "Discover GRE peers, connect VLANs to bridges and let the kernel forward traffic.",
        "search": "Filter this page",
        "pages": "Guide",
        "contents": "On this page",
        "previous": "Previous",
        "next": "Next",
        "copy": "Copy",
        "copied": "Copied",
        "empty": "No sections found.",
        "guide": "Complete guide",
        "source": "Source code",
        "footer": "Project documentation · LGPL-3.0-or-later",
        "nodes": ["GRE peer", "SoftGRE", "Linux bridge"],
        "detail": "IPv4 + Ethernet + VLAN → discovery → gretap + VLAN → kernel forwarding",
    },
}


def slug(text):
    plain = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "-", plain).strip("-")


def inline(text):
    # Tokenize code/links before escaping, so generated tags never become Markdown input.
    pattern = r"`([^`]+)`|\[([^\]]+)\]\(([^)]+)\)|\*\*([^*]+)\*\*"
    output, offset = [], 0
    for match in re.finditer(pattern, text):
        output.append(html.escape(text[offset : match.start()]))
        if match.group(1) is not None:
            output.append(f"<code>{html.escape(match.group(1))}</code>")
        elif match.group(4) is not None:
            output.append(f"<strong>{inline(match.group(4))}</strong>")
        else:
            label, url = match.group(2), match.group(3)
            page = re.fullmatch(r"([a-z0-9-]+)\.md(#[a-z0-9-]+)?", url)
            if page:
                # Links between guide pages work on GitHub (.md) and on the site (.html).
                if page.group(1) not in PAGES:
                    raise ValueError(f"Unknown guide page: {url}")
                url = page.group(1) + ".html" + (page.group(2) or "")
            elif not re.match(r"^(https?://|#|\.?\.?/)", url):
                raise ValueError(f"Unsupported documentation link: {url}")
            output.append(f'<a href="{html.escape(url, quote=True)}">{inline(label)}</a>')
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
            current = [
                f'<section class="doc-section" id="{anchor}">',
                f'<h2><a href="#{anchor}">{html.escape(title)}</a></h2>',
            ]
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
            current.append(
                '<div class="code-block">'
                f'<div class="code-bar"><span>{html.escape(language or "text")}</span>'
                f'<button type="button" class="copy">{labels["copy"]}</button></div>'
                f"<pre><code>{html.escape(chr(10).join(code))}</code></pre></div>"
            )
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
            body = "".join(
                "<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>" for row in rows[1:]
            )
            current.append(
                f'<div class="table-wrap"><table><thead><tr>{header}</tr></thead>'
                f"<tbody>{body}</tbody></table></div>"
            )
            continue
        elif line.startswith("- ") or re.match(r"\d+\. ", line):
            flush()
            ordered = not line.startswith("- ")
            marker = r"\d+\. " if ordered else r"- "
            items = []
            while index < len(lines) and re.match(marker, lines[index]):
                item = re.sub(marker, "", lines[index], count=1)
                index += 1
                while index < len(lines) and lines[index].startswith("  "):
                    item += " " + lines[index].strip()
                    index += 1
                items.append(f"<li>{inline(item)}</li>")
            tag = "ol" if ordered else "ul"
            current.append(f"<{tag}>" + "".join(items) + f"</{tag}>")
            continue
        elif line.startswith("!["):
            flush()
            image = re.fullmatch(r"!\[([^\]]+)\]\((\.\./assets/[\w.-]+)\)", line.strip())
            if not image:
                raise ValueError(f"Images must reference ../assets/ with alt text: {line}")
            alt, source = html.escape(image.group(1), quote=True), image.group(2)
            current.append(
                f'<figure><a href="{source}"><img src="{source}" alt="{alt}" loading="lazy">'
                "</a></figure>"
            )
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


def title_of(markdown, name):
    for line in markdown.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    raise ValueError(f"Guide page without a title: {name}")


def page(locale, name, markdown, pages):
    labels = LANGUAGES[locale]
    article, toc = render(markdown, labels)
    titles = dict(pages)
    position = [slug for slug, _ in pages].index(name)
    nav = "".join(f'<a href="#{anchor}">{html.escape(title)}</a>' for anchor, title in toc)
    current = ' aria-current="page"'
    guide = "".join(
        f'<a href="{slug}.html"{current if slug == name else ""}>{html.escape(title)}</a>'
        for slug, title in pages
    )
    pager = []
    if position > 0:
        slug, title = pages[position - 1]
        pager.append(
            f'<a class="previous" href="{slug}.html"><span>{labels["previous"]}</span>'
            f"{html.escape(title)}</a>"
        )
    if position + 1 < len(pages):
        slug, title = pages[position + 1]
        pager.append(
            f'<a class="next" href="{slug}.html"><span>{labels["next"]}</span>'
            f"{html.escape(title)}</a>"
        )
    nodes = '<span class="arrow" aria-hidden="true">→</span>'.join(
        f'<span class="node">{node}</span>' for node in labels["nodes"]
    )
    if name == "index":
        heading = "<br>".join(html.escape(part) for part in labels["title"].split("\n"))
        header = f"""<section class="hero">
<p class="eyebrow">{labels['eyebrow']}</p><h1>{heading}</h1><p class="lead">{labels['intro']}</p>
<div class="badges"><span>Linux</span><span>AF_PACKET + BPF</span><span>Native netlink</span></div>
<div class="network" aria-label="{html.escape(labels['detail'])}">{nodes}</div>
<p class="diagram-caption">{labels['detail']}</p>
<p class="actions"><a class="primary" href="try-it.html">{html.escape(titles['try-it'])}</a>
<a href="install.html">{html.escape(titles['install'])}</a></p></section>"""
    else:
        header = f"""<header class="page-header"><p class="eyebrow">{labels['pages']} \
{position + 1}/{len(pages)}</p><h1>{html.escape(titles[name])}</h1></header>"""
    dark_label = "Dark theme"
    light_label = "Light theme"
    source = f"{REPOSITORY}/blob/master/docs/{locale}/{name}.md"
    return f"""<!doctype html>
<html lang="{labels['lang']}">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{html.escape(labels['intro'])}">
<title>{html.escape(titles[name])} · SoftGRE</title>
<link rel="icon" type="image/svg+xml" href="../assets/softgre-logo.svg">
<link rel="stylesheet" href="../assets/site.css">
<script src="../assets/theme.js"></script>
<script src="../assets/site.js" defer></script>
</head>
<body data-copy="{labels['copy']}" data-copied="{labels['copied']}">
<a class="skip" href="#documentation">{labels['guide']}</a>
<header class="topbar"><a class="brand" href="index.html">
<img class="brand-logo" src="../assets/softgre-logo.svg" alt="" width="42" height="42">
SoftGRE</a>
<nav aria-label="Site navigation"><a href="try-it.html">{html.escape(titles['try-it'])}</a>
<a href="install.html">{html.escape(titles['install'])}</a>
<a href="{REPOSITORY}">GitHub ↗</a>
<button id="theme-toggle" class="theme-toggle" type="button" aria-pressed="false"
data-dark="{dark_label}" data-light="{light_label}" hidden>{dark_label}</button></nav></header>
<div class="shell">{header}
<div class="docs-layout"><aside><div class="sidebar">
<p class="eyebrow">{labels['pages']}</p><nav class="pages" aria-label="{labels['pages']}">{guide}</nav>
<p class="eyebrow">{labels['contents']}</p>
<label for="search">{labels['search']}</label><input id="search" type="search" autocomplete="off">
<nav class="toc" aria-label="{labels['contents']}">{nav}</nav>
<a class="source" href="{source}">{labels['source']} ↗</a></div></aside>
<main id="documentation" tabindex="-1">
<p class="empty" role="status" hidden>{labels['empty']}</p>{article}
<nav class="pager" aria-label="{labels['pages']}">{"".join(pager)}</nav></main></div>
<footer>{labels['footer']}<a href="{REPOSITORY}">GitHub →</a>
</footer></div></body></html>
"""


def redirect(target):
    return f"""<!doctype html>
<html lang="en-US">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="0; url={target}">
<link rel="canonical" href="{target}">
<title>SoftGRE</title>
</head>
<body><p><a href="{target}">SoftGRE documentation</a></p></body></html>
"""


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
        sources = {name: (ROOT / "docs" / locale / f"{name}.md").read_text() for name in PAGES}
        pages = [(name, title_of(sources[name], name)) for name in PAGES]
        for name in PAGES:
            (target / f"{name}.html").write_text(page(locale, name, sources[name], pages))
    (output / "index.html").write_text(redirect("en_US/index.html"))
    stale = output / "pt_BR"
    if stale.is_dir():
        shutil.rmtree(stale)
    (output / ".nojekyll").touch()
    print(f"Documentation built: {output}")
    return output


if __name__ == "__main__":
    build()
