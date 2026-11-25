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
# Checks the website in web/: no link may point to a missing file, and every file must be
# referenced by another file.
#
#   python3 tools/check_site.py
#
# Python 3 only, nothing to install. Links are read from href, src and srcset in .html files and
# from url(...) in .css files. External links (http:, https:, mailto:, //), other schemes, pure
# anchors and query strings are ignored; a link to a directory means its index.html. Relative links
# resolve against the file that holds them, root-absolute ones against web/ (a leading /FontDaw/,
# the project path the site is served under, is dropped first).
#
# Exempt from the "unreferenced" check: index.html (any directory), 404.html, CNAME, .nojekyll,
# robots.txt, sitemap.xml, favicon.ico, and the paths listed in web/.site-check-allow (one path
# relative to web/ per line, # starts a comment). Exit status 1 if anything is reported, else 0.

import re
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parent.parent / "web"
SITE_PREFIX = "/FontDaw/"
EXEMPT_NAMES = {"index.html", "404.html", "CNAME", ".nojekyll", "robots.txt", "sitemap.xml",
                "favicon.ico", ".site-check-allow"}
SCHEME = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
CSS_URL = re.compile(r"url\(\s*(['\"]?)(.*?)\1\s*\)", re.S)


class LinkParser(HTMLParser):
    """Collects (line, target) for every href, src and srcset value."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.links = []

    def handle_starttag(self, tag, attrs):
        for name, value in attrs:
            if not value:
                continue
            line = self.getpos()[0]
            if name in ("href", "src"):
                self.links.append((line, value.strip()))
            elif name == "srcset":
                for candidate in value.split(","):
                    if candidate.strip():
                        self.links.append((line, candidate.split()[0]))


def extract_links(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    if path.suffix == ".html":
        parser = LinkParser()
        parser.feed(text)
        return parser.links
    if path.suffix == ".css":
        return [(text.count("\n", 0, m.start()) + 1, m.group(2).strip())
                for m in CSS_URL.finditer(text)]
    return []


def resolve(source, target):
    """Returns the local file a link points to, or None for links to ignore."""
    if not target or target.startswith(("#", "//")) or SCHEME.match(target):
        return None
    target = unquote(re.split(r"[?#]", target, maxsplit=1)[0])
    if not target:
        return None
    if target.startswith(SITE_PREFIX):
        target = target[len(SITE_PREFIX) - 1:]
    base = ROOT if target.startswith("/") else source.parent
    path = (base / target.lstrip("/")).resolve()
    return path / "index.html" if path.is_dir() else path


def read_allow_list():
    allow_file = ROOT / ".site-check-allow"
    if not allow_file.is_file():
        return set()
    lines = (l.split("#", 1)[0].strip() for l in allow_file.read_text(encoding="utf-8").splitlines())
    return {(ROOT / l).resolve() for l in lines if l}


def main():
    files = sorted(p for p in ROOT.rglob("*") if p.is_file())
    referenced = set()
    broken = []
    for source in files:
        for line, target in extract_links(source):
            dest = resolve(source, target)
            if dest is None:
                continue
            if dest.is_file() and ROOT in dest.parents:
                if dest != source.resolve():
                    referenced.add(dest)
            else:
                broken.append(f"{source.relative_to(ROOT.parent)}:{line} -> {target}")

    allowed = read_allow_list()
    orphans = [p for p in files
               if p.name not in EXEMPT_NAMES and p.resolve() not in allowed
               and p.resolve() not in referenced]

    for entry in broken:
        print(f"BROKEN: {entry}")
    for path in orphans:
        print(f"UNREFERENCED: {path.relative_to(ROOT.parent)}")
    print(f"Checked {len(files)} files: {len(broken)} broken links, "
          f"{len(orphans)} unreferenced files.")
    return 1 if broken or orphans else 0


if __name__ == "__main__":
    sys.exit(main())
