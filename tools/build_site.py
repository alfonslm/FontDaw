#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
#
# FontDaw
# The notation-first DAW
#
# Copyright (C) 2026 FontDaw contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Builds the wiki pages of the website: wiki/*.md -> web/wiki/*.html.
#
#   python3 tools/build_site.py
#
# Python 3 only, nothing to install. The wiki's pages are kept as Markdown in wiki/, one file
# per page, with the navigation in wiki/_Sidebar.md. Links between pages use the page name without
# ".md" (as a GitHub wiki does): [FAQ](FAQ), [files](FontDaw-and-MuseScore#files-and-formats).
# The converter covers the Markdown these pages use: headings, paragraphs, bulleted and numbered
# lists, **bold**, *italic*, `code` and links, and pictures on a line of their own, written
# ![what the picture shows](images/name.png "caption"); the pictures live in web/wiki/images/.
# Run it after changing wiki/ and commit the result.
# The Pages workflow publishes web/ as it is.

import html
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "wiki"
OUT = ROOT / "web" / "wiki"
SITE_URL = "https://alfonslm.github.io/FontDaw/"


def page_file(name):
    """The HTML file of a wiki page. Home is the wiki's index page."""
    return "index.html" if name == "Home" else name + ".html"


def slugify(text):
    """GitHub-style heading anchor: lower case, punctuation dropped, spaces to hyphens."""
    text = re.sub(r"<[^>]+>", "", text).lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.strip().replace(" ", "-")


def link_target(url):
    if re.match(r"^[a-z]+:", url) or url.startswith("#"):
        return url
    name, _, anchor = url.partition("#")
    if not (SRC / (name + ".md")).exists():
        sys.exit("broken wiki link: " + url)
    return page_file(name) + ("#" + anchor if anchor else "")


def inline(text):
    """Inline Markdown to HTML. Code spans are cut out first so nothing inside them is changed."""
    codes = []

    def keep_code(m):
        codes.append("<code>" + html.escape(m.group(1), quote=False) + "</code>")
        return "\x00%d\x00" % (len(codes) - 1)

    text = re.sub(r"`([^`]+)`", keep_code, text)
    text = html.escape(text, quote=False)

    def make_link(m):
        url = link_target(html.unescape(m.group(2)))
        external = re.match(r"^https?:", url) is not None
        attrs = ' rel="noopener"' if external else ""
        return '<a href="%s"%s>%s</a>' % (html.escape(url), attrs, m.group(1))

    text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", make_link, text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<em>\1</em>", text)
    text = re.sub(r"(?<!\w)_(?!\s)(.+?)(?<!\s)_(?!\w)", r"<em>\1</em>", text)
    return re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], text)


def markdown(source):
    """Block-level Markdown to HTML. Returns (title, body html)."""
    out, para, items, kind = [], [], [], None
    title = None

    def flush():
        nonlocal para, items, kind
        if para:
            out.append("<p>" + inline(" ".join(para)) + "</p>")
            para = []
        if items:
            out.append("<%s>" % kind)
            out.extend("  <li>" + inline(i) + "</li>" for i in items)
            out.append("</%s>" % kind)
            items, kind = [], None

    for line in source.splitlines():
        stripped = line.strip()
        heading = re.match(r"^(#{1,3})\s+(.*)$", stripped)
        bullet = re.match(r"^[-*]\s+(.*)$", stripped)
        image = re.match(r'^!\[([^\]]*)\]\((\S+)\s+"([^"]*)"\)$', stripped)
        number = re.match(r"^\d+\.\s+(.*)$", stripped)
        if not stripped:
            flush()
        elif image:
            flush()
            alt, src, caption = image.groups()
            if not (OUT / src).exists():
                sys.exit("missing image: web/wiki/" + src)
            out.append('<figure>\n  <a href="%s"><img src="%s" alt="%s" loading="lazy"></a>\n  <figcaption>%s</figcaption>\n</figure>'
                       % (html.escape(src), html.escape(src), html.escape(alt), inline(caption)))
        elif heading:
            flush()
            level, text = len(heading.group(1)), heading.group(2)
            if level == 1:
                title = text
                out.append("<h1>" + inline(text) + "</h1>")
            else:
                out.append('<h%d id="%s">%s</h%d>' % (level, slugify(text), inline(text), level))
        elif bullet or number:
            if para:
                flush()
            new_kind = "ul" if bullet else "ol"
            if kind and kind != new_kind:
                flush()
            kind = new_kind
            items.append((bullet or number).group(1))
        elif items and line.startswith(" "):
            items[-1] += " " + stripped  # continuation of a list item
        else:
            if items:
                flush()
            para.append(stripped)
    flush()
    if not title:
        sys.exit("page has no '# Title' line")
    return title, "\n".join(out)


def sidebar(current):
    """The navigation from wiki/_Sidebar.md: **Group** lines and * [Title](Page) items."""
    groups = []
    for line in (SRC / "_Sidebar.md").read_text(encoding="utf-8").splitlines():
        group = re.match(r"^\*\*(.+)\*\*$", line.strip())
        item = re.match(r"^\*\s+\[(.+)\]\((.+)\)$", line.strip())
        if group and group.group(1) != "FontDaw":
            groups.append((group.group(1), []))
        elif item and groups:
            groups[-1][1].append((item.group(1), item.group(2)))
    parts = []
    for name, links in groups:
        parts.append('<h3 class="nav-group">%s</h3>' % html.escape(name))
        parts.append("<ul>")
        for title, page in links:
            here = ' aria-current="page"' if page == current else ""
            parts.append('  <li><a href="%s"%s>%s</a></li>' % (page_file(page), here, html.escape(title)))
        parts.append("</ul>")
    return "\n".join(parts), [page for _, links in groups for _, page in links]


TEMPLATE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{page_title}</title>
<meta name="description" content="{description}">
<meta name="color-scheme" content="light dark">
<link rel="icon" href="../icons/favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="../style.css">
</head>
<body class="wiki-body">
<header class="wiki-header">
  <a class="wiki-home" href="../index.html"><img src="../icons/favicon.svg" alt="" width="28" height="28"><span>FontDaw</span></a>
  <a class="wiki-jump" href="#wiki-nav">All wiki pages</a>
</header>
<div class="wiki">
  <main class="wiki-content">
{body}
  </main>
  <nav id="wiki-nav" class="wiki-nav" aria-label="Wiki pages">
    <h2 class="nav-title">Wiki pages</h2>
{nav}
  </nav>
</div>
<footer class="footer wiki-footer">
  <p>Based on MuseScore Studio by MuseScore Limited and contributors. FontDaw is not affiliated with or endorsed by MuseScore Limited or Muse Group.</p>
  <p>Free software under the GNU GPL v3. See <a href="Licence-and-credits.html">Licence and credits</a>.</p>
</footer>
</body>
</html>
"""


def main():
    if not (SRC / "_Sidebar.md").exists():
        sys.exit("missing wiki/_Sidebar.md")
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.html"):
        old.unlink()
    pages = sorted(p for p in SRC.glob("*.md") if not p.name.startswith("_"))
    _, listed = sidebar(None)
    for p in pages:
        if p.stem not in listed:
            sys.exit("page %s is not in wiki/_Sidebar.md" % p.stem)
    for p in pages:
        title, body = markdown(p.read_text(encoding="utf-8"))
        nav, _ = sidebar(p.stem)
        first_para = re.search(r"<p>(.*?)</p>", body, re.S)
        description = re.sub(r"<[^>]+>", "", first_para.group(1)) if first_para else title
        page_title = "FontDaw wiki" if p.stem == "Home" else "%s – FontDaw wiki" % html.unescape(re.sub(r"<[^>]+>", "", inline(title)))
        indent = lambda s, n: "\n".join((" " * n + l) if l else l for l in s.splitlines())
        (OUT / page_file(p.stem)).write_text(TEMPLATE.format(
            page_title=html.escape(page_title),
            description=html.escape(html.unescape(description), quote=True),
            nav=indent(nav, 4),
            body=indent(body, 4),
        ), encoding="utf-8")
    print("Built %d pages in %s" % (len(pages), OUT.relative_to(ROOT)))


if __name__ == "__main__":
    main()
