#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw — Copyright (C) 2026 FontDaw contributors
#
# Check the FontDaw app icon set in share/icons/FontDawIcon/ (see the README there).
#
# Idea: take reference points from the original 32-unit drawing (src/*.svg), map them onto each
# rendered icon and check the colour there. A tiny glyph, a wrong variant, a shifted or wrongly
# scaled drawing all fail. The Icon Composer layers (AppIcon.icon/Assets) are checked statically,
# because actool only runs on a Mac.
#
# Usage: tools/fontdaw/check_app_icons.py [--dir DIR] [--verbose]
#                                         [--iconset DIR --variant light|dark [--tile-inset F]]
#                                         [--self-test]
#   --dir DIR        icon directory (default: share/icons/FontDawIcon)
#   --verbose        print every sampled point
#   --iconset DIR    also check a macOS iconset (iconutil -c iconset AppIcon.icns); NOT YET
#                    VERIFIED ON A MAC: the mapping and tolerance are assumptions
#   --variant V      which variant the iconset shows (light or dark); needed with --iconset
#   --tile-inset F   tile size as a fraction of the canvas for --iconset (default 824/1024)
#   --self-test      build broken icon sets in a temporary directory and check that they fail
# Exit status 0 only if every check passed.
#
# Python 3 with Pillow. Reads repository files, changes nothing.

import argparse
import contextlib
import io
import json
import math
import os
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass

try:
    from PIL import Image, ImageChops, IcoImagePlugin
except ImportError:
    sys.exit("check_app_icons.py needs Pillow (pip install pillow)")

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEFAULT_DIR = os.path.join(ROOT, "share", "icons", "FontDawIcon")

# ---- Reference data: the 32-unit drawing and how the icon sets place it ---------------------

SIZES = (16, 24, 32, 48, 64, 96, 128, 256, 512, 1024)
ICO_SIZES = (16, 24, 32, 48, 256)

# Flat icon set (PNG, ICO, SVG): the tile fills the image, the glyph is drawn with
# transform="translate(3.2 3.2) scale(0.8)" in the 32-unit tile.
GLYPH_OFFSET = 3.2   # units of the 32-unit tile
GLYPH_SCALE = 0.8
UNITS = 32.0

# Icon Composer layers: a 1024 canvas, glyph drawn with translate(102.4 102.4) scale(25.6).
LAYER_CANVAS = 1024
LAYER_OFFSET = 102.4
LAYER_SCALE = 25.6

# macOS rendering (NOT YET VERIFIED ON A MAC): assumed tile size as a fraction of the canvas, the
# tile centred; the glyph is assumed to sit on the canvas like in the layer file.
MAC_TILE_INSET = 824 / 1024
# macOS adds a glass effect and a shadow, so colours are only compared loosely.
MAC_TOLERANCE = 48        # per channel, for features that are big enough to be solid
MAC_DILUTED_RATIO = 0.85  # for thin features, see DILUTED_RATIO

COLORS = {
    "light": {"wave": "#dd5a3b", "note": "#1c1b18", "tile": "#fbfaf6"},
    "dark": {"wave": "#f08a63", "note": "#ece9e1", "tile": "#141311"},
}

EXACT_TOLERANCE = 8      # per channel, for features at least SOLID_PX wide
SOLID_PX = 3.0           # feature width in pixels from which a point must show the exact colour
# Thinner features are diluted by anti-aliasing (at 16 px the stroke is under a pixel wide). The
# sampled colour must then be clearly on the expected side: its distance to the expected colour
# must be at most DILUTED_RATIO times the distance between the tile colour and the expected colour
# (1.0 would accept the plain tile colour). Calibrated against the committed PNGs, see --verbose.
DILUTED_RATIO = 0.6
CORNER_ALPHA_MAX = 8     # the four corner pixels must be (nearly) transparent
STEM_MIN_PX = 1.5        # the stem point is checked from the size where the stem is this wide


@dataclass(frozen=True)
class Point:
    name: str
    x: float          # 32-unit drawing coordinates
    y: float
    kind: str         # "glyph" (drawn with the glyph transform) or "tile" (tile space)
    color: str        # key in COLORS
    width: float      # width of the feature in drawing units (for the solid/diluted rule)


# From share/icons/FontDawIcon/src/*.svg. Wave: M3.5 16 c1.9 0 2.3-6 4.2-6 ... crests at y=10
# (x 7.7, 16.1, 24.5), ends at x 3.5 and 28.5; stroke 2. Head: ellipse at (14.9, 20.9), ry 2.71.
# Stem: x 18.21 from y 20.35 up to 6.64, stroke 1.38.
POINTS = (
    Point("wave end left", 3.5, 16.0, "glyph", "wave", 2.0),
    Point("wave end right", 28.5, 16.0, "glyph", "wave", 2.0),
    Point("wave crest left", 7.7, 10.0, "glyph", "wave", 2.0),
    Point("wave crest right", 24.5, 10.0, "glyph", "wave", 2.0),
    Point("note head centre", 14.9, 20.9, "glyph", "note", 5.4),
    Point("note stem", 18.21, 8.0, "glyph", "note", 1.38),
    Point("tile top", 16.0, 2.5, "tile", "tile", 100.0),
    Point("tile bottom", 16.0, 29.5, "tile", "tile", 100.0),
)


@dataclass(frozen=True)
class Mapping:
    """Where the tile and the glyph are on the canvas, as fractions of the canvas size."""
    tile_origin: float
    tile_size: float
    glyph_origin: float
    glyph_size: float
    tolerance: int
    diluted_ratio: float
    name: str

    def frac(self, p):
        if p.kind == "tile":
            return (self.tile_origin + self.tile_size * p.x / UNITS,
                    self.tile_origin + self.tile_size * p.y / UNITS)
        return (self.glyph_origin + self.glyph_size * p.x / UNITS,
                self.glyph_origin + self.glyph_size * p.y / UNITS)


FLAT = Mapping(0.0, 1.0, GLYPH_OFFSET / UNITS, GLYPH_SCALE, EXACT_TOLERANCE, DILUTED_RATIO, "flat")


def mac_mapping(tile_inset=MAC_TILE_INSET):
    return Mapping((1 - tile_inset) / 2, tile_inset, GLYPH_OFFSET / UNITS, GLYPH_SCALE,
                   MAC_TOLERANCE, MAC_DILUTED_RATIO, "mac")


# ---- Helpers --------------------------------------------------------------------------------

def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def to_hex(c):
    return "#%02x%02x%02x" % tuple(c[:3])


def dist(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def max_diff(a, b):
    return max(abs(a[i] - b[i]) for i in range(3))


class Report:
    def __init__(self, verbose):
        self.verbose = verbose
        self.failed = 0
        self.checked = 0

    def file(self, label, problems, notes=()):
        self.checked += 1
        if problems:
            self.failed += 1
            print(f"FAIL {label}")
            for p in problems:
                print(f"       {p}")
        else:
            print(f"ok   {label}")
        if self.verbose:
            for n in notes:
                print(f"       {n}")

    def warn(self, text):
        print(f"warn {text}")


def check_point(img, p, mapping, variant, n):
    """Return (problem or None, verbose note) for one point of an RGBA image of size n."""
    expected = hex_rgb(COLORS[variant][p.color])
    tile = hex_rgb(COLORS[variant]["tile"])
    fx, fy = mapping.frac(p)
    px = min(n - 1, max(0, int(fx * n)))
    py = min(n - 1, max(0, int(fy * n)))
    r = max(1, n // 128)
    candidates = []
    for yy in range(max(0, py - r), min(n, py + r + 1)):
        for xx in range(max(0, px - r), min(n, px + r + 1)):
            c = img.getpixel((xx, yy))
            if c[3] >= 128:
                candidates.append(c)
    if not candidates:
        return (f"{p.name} ({px},{py}): transparent, expected {to_hex(expected)}",
                f"{p.name} ({px},{py}): transparent")
    found = min(candidates, key=lambda c: dist(c, expected))
    width_px = p.width * mapping.glyph_size * n / UNITS if p.kind == "glyph" else 1e9
    if p.kind == "tile":
        width_px = 1e9
    solid = width_px >= SOLID_PX
    if solid:
        ok = max_diff(found, expected) <= mapping.tolerance
        rule = f"exact +-{mapping.tolerance}"
        detail = f"max diff {max_diff(found, expected)}"
    else:
        d_exp = dist(found, expected)
        d_tile = dist(tile, expected)
        ratio = d_exp / d_tile if d_tile else 0.0
        ok = ratio <= mapping.diluted_ratio
        rule = f"diluted ratio <= {mapping.diluted_ratio}"
        detail = f"ratio {ratio:.2f}"
    note = (f"{p.name} ({px},{py}) expected {to_hex(expected)} found {to_hex(found)} "
            f"[{rule}; {detail}]")
    if ok:
        return None, note
    return (f"{p.name} ({px},{py}): expected {to_hex(expected)}, found {to_hex(found)} "
            f"[{rule}; {detail}]"), note


def check_image(img, n, mapping, variant):
    """Point checks and the four corners on one RGBA image. Returns (problems, notes)."""
    problems, notes = [], []
    if img.size != (n, n):
        return [f"size is {img.size[0]}x{img.size[1]}, expected {n}x{n}"], notes
    if img.mode != "RGBA":
        return [f"mode is {img.mode}, expected RGBA"], notes
    for p in POINTS:
        if p.name == "note stem" and p.width * mapping.glyph_size * n / UNITS < STEM_MIN_PX:
            notes.append(f"{p.name}: skipped, stem under {STEM_MIN_PX} px wide")
            continue
        problem, note = check_point(img, p, mapping, variant, n)
        notes.append(note)
        if problem:
            problems.append(problem)
    for cx, cy in ((0, 0), (n - 1, 0), (0, n - 1), (n - 1, n - 1)):
        a = img.getpixel((cx, cy))[3]
        notes.append(f"corner ({cx},{cy}) alpha {a}")
        if a > CORNER_ALPHA_MAX:
            problems.append(f"corner ({cx},{cy}): alpha {a}, expected transparent")
    return problems, notes


# ---- The checks -----------------------------------------------------------------------------

def check_pngs(d, rep):
    for variant in ("dark", "light"):
        for n in SIZES:
            name = f"fontdaw-icon-{variant}_{n}x{n}.png"
            path = os.path.join(d, name)
            if not os.path.isfile(path):
                rep.file(name, ["file is missing"])
                continue
            try:
                img = Image.open(path)
                img.load()
            except Exception as e:  # noqa: BLE001
                rep.file(name, [f"cannot read: {e}"])
                continue
            problems, notes = check_image(img, n, FLAT, variant)
            rep.file(name, problems, notes)


def check_ico(d, rep):
    name = "fontdaw-icon-dark.ico"
    path = os.path.join(d, name)
    if not os.path.isfile(path):
        rep.file(name, ["file is missing"])
        return
    try:
        with open(path, "rb") as f:
            ico = IcoImagePlugin.IcoFile(f)
            images = {w: ico.getimage((w, h)).convert("RGBA") for w, h in ico.sizes()}
    except Exception as e:  # noqa: BLE001
        rep.file(name, [f"cannot read: {e}"])
        return
    have = sorted(images)
    problems, notes = [], []
    if have != sorted(ICO_SIZES):
        problems.append(f"sizes are {have}, expected {sorted(ICO_SIZES)}")
    for n in ICO_SIZES:
        if n not in images:
            continue
        img = images[n]
        notes.append(f"-- embedded {n}x{n}")
        pr, nt = check_image(img, n, FLAT, "dark")
        problems += [f"{n}px: {x}" for x in pr]
        notes += nt
    rep.file(name, problems, notes)


def check_variants_differ(d, rep):
    problems = []
    for n in SIZES:
        a = os.path.join(d, f"fontdaw-icon-dark_{n}x{n}.png")
        b = os.path.join(d, f"fontdaw-icon-light_{n}x{n}.png")
        if not (os.path.isfile(a) and os.path.isfile(b)):
            continue
        ia, ib = Image.open(a).convert("RGBA"), Image.open(b).convert("RGBA")
        if ia.size == ib.size and ImageChops.difference(ia, ib).getbbox(alpha_only=False) is None:
            problems.append(f"{n}px: dark and light PNG are the same image")
    rep.file("dark and light variants differ", problems)


def parse_transform(t):
    m = re.fullmatch(r"\s*translate\(\s*([-\d.]+)[ ,]+([-\d.]+)\s*\)\s*scale\(\s*([-\d.]+)\s*\)\s*", t or "")
    if not m:
        return None
    return float(m.group(1)), float(m.group(2)), float(m.group(3))


def strip_ns(tag):
    return tag.rsplit("}", 1)[-1]


def glyph_group(root):
    """The top-level group that carries the glyph transform."""
    for el in root.iter():
        if strip_ns(el.tag) == "g" and el.get("transform", "").startswith("translate"):
            return el
    return None


def check_flat_svgs(d, rep):
    for variant in ("dark", "light"):
        name = f"fontdaw-icon-{variant}.svg"
        path = os.path.join(d, name)
        problems = []
        try:
            root = ET.parse(path).getroot()
        except Exception as e:  # noqa: BLE001
            rep.file(name, [f"cannot read: {e}"])
            continue
        rect = next((e for e in root.iter() if strip_ns(e.tag) == "rect"), None)
        tile = COLORS[variant]["tile"]
        if rect is None:
            problems.append("no tile rect")
        else:
            if (rect.get("fill") or "").lower() != tile:
                problems.append(f"tile fill is {rect.get('fill')}, expected {tile}")
            if rect.get("width") != "32" or rect.get("height") != "32":
                problems.append(f"tile rect is {rect.get('width')}x{rect.get('height')}, expected 32x32")
        if root.get("viewBox") != "0 0 32 32":
            problems.append(f"viewBox is {root.get('viewBox')}, expected 0 0 32 32")
        g = glyph_group(root)
        t = parse_transform(g.get("transform")) if g is not None else None
        if t is None:
            problems.append("no glyph group with translate(..) scale(..)")
        elif not (abs(t[0] - GLYPH_OFFSET) < 1e-6 and abs(t[1] - GLYPH_OFFSET) < 1e-6
                  and abs(t[2] - GLYPH_SCALE) < 1e-6):
            problems.append(f"glyph transform is translate({t[0]} {t[1]}) scale({t[2]}), "
                            f"expected translate({GLYPH_OFFSET} {GLYPH_OFFSET}) scale({GLYPH_SCALE})")
        rep.file(name, problems)


def parse_srgb(s):
    m = re.fullmatch(r"srgb:([\d.]+),([\d.]+),([\d.]+),([\d.]+)", s or "")
    return tuple(float(x) for x in m.groups()) if m else None


def check_bundle(d, rep):
    bundle = os.path.join(d, "AppIcon.icon")
    jpath = os.path.join(bundle, "icon.json")
    try:
        with open(jpath) as f:
            spec = json.load(f)
    except Exception as e:  # noqa: BLE001
        rep.file("AppIcon.icon/icon.json", [f"cannot read: {e}"])
        return
    problems = []
    # Which layer file belongs to which variant: no appearance = light, "dark" = dark.
    layer_files = {}
    for grp in spec.get("groups", []):
        for layer in grp.get("layers", []):
            for s in layer.get("image-name-specializations", []):
                layer_files.setdefault(s.get("appearance", "light"), []).append(s["value"])
    for variant in ("light", "dark"):
        if not layer_files.get(variant):
            problems.append(f"icon.json names no {variant} layer image")
    fills = {s.get("appearance", "light"): parse_srgb(s["value"].get("solid"))
             for s in spec.get("fill-specializations", [])}
    for variant in ("light", "dark"):
        want = [v / 255 for v in hex_rgb(COLORS[variant]["tile"])]
        got = fills.get(variant)
        if got is None:
            problems.append(f"icon.json has no {variant} fill")
        elif any(abs(got[i] - want[i]) > 0.002 for i in range(3)) or abs(got[3] - 1) > 1e-6:
            problems.append(f"{variant} fill is {got}, expected the tile colour {COLORS[variant]['tile']}")
    for variant, files in layer_files.items():
        for fname in files:
            problems += [f"{fname}: {x}" for x in check_layer_svg(os.path.join(bundle, "Assets", fname), variant)]
    rep.file("AppIcon.icon (icon.json and layer SVGs)", problems)


def check_layer_svg(path, variant):
    if not os.path.isfile(path):
        return ["file is missing"]
    try:
        root = ET.parse(path).getroot()
    except Exception as e:  # noqa: BLE001
        return [f"cannot read: {e}"]
    problems = []
    for a in ("width", "height"):
        if root.get(a) != str(LAYER_CANVAS):
            problems.append(f"{a} is {root.get(a)}, expected {LAYER_CANVAS}")
    vb = f"0 0 {LAYER_CANVAS} {LAYER_CANVAS}"
    if root.get("viewBox") != vb:
        problems.append(f"viewBox is {root.get('viewBox')}, expected {vb}")
    if any(strip_ns(e.tag) == "style" for e in root.iter()):
        problems.append("has a <style> element (colours must be plain attributes)")
    g = glyph_group(root)
    t = parse_transform(g.get("transform")) if g is not None else None
    if t is None:
        problems.append("no glyph group with translate(..) scale(..)")
    elif not (abs(t[0] - LAYER_OFFSET) < 1e-6 and abs(t[1] - LAYER_OFFSET) < 1e-6
              and abs(t[2] - LAYER_SCALE) < 1e-6):
        problems.append(f"glyph transform is translate({t[0]} {t[1]}) scale({t[2]}), "
                        f"expected translate({LAYER_OFFSET} {LAYER_OFFSET}) scale({LAYER_SCALE})")
    fills = {(e.get("fill") or "").lower() for e in root.iter()} | \
            {(e.get("stroke") or "").lower() for e in root.iter()}
    for key in ("wave", "note"):
        if COLORS[variant][key] not in fills:
            problems.append(f"no element with the {key} colour {COLORS[variant][key]}")
    other = "dark" if variant == "light" else "light"
    for key in ("wave", "note"):
        if COLORS[other][key] in fills:
            problems.append(f"has the {other} variant's {key} colour {COLORS[other][key]}")
    return problems


def check_iconset(path, variant, tile_inset, rep):
    """NOT YET VERIFIED ON A MAC. Check the PNGs of `iconutil -c iconset AppIcon.icns`."""
    mapping = mac_mapping(tile_inset)
    files = sorted(f for f in os.listdir(path) if f.lower().endswith(".png"))
    if not files:
        rep.file(f"iconset {path}", ["no PNG files"])
        return
    seen = set()
    for f in files:
        try:
            img = Image.open(os.path.join(path, f))
            img.load()
        except Exception as e:  # noqa: BLE001
            rep.file(f, [f"cannot read: {e}"])
            continue
        n = img.size[0]
        seen.add(n)
        if img.size[0] != img.size[1]:
            rep.file(f, [f"not square: {img.size}"])
            continue
        problems, notes = check_image(img.convert("RGBA"), n, mapping, variant)
        rep.file(f"{f} ({n}px, mac mapping, tile {tile_inset:.4f})", problems, notes)
    print(f"iconset holds pixel sizes: {sorted(seen)}")
    for n in (256, 512, 1024):
        if n not in seen:
            rep.warn(f"iconset has no {n}px image")


def check_dir(d, rep):
    check_pngs(d, rep)
    check_ico(d, rep)
    check_variants_differ(d, rep)
    check_flat_svgs(d, rep)
    check_bundle(d, rep)


# ---- Self test ------------------------------------------------------------------------------

def _plain_tile(src, variant):
    """A copy of `src` with the glyph removed: tile colour everywhere the icon is opaque."""
    base = Image.new("RGBA", src.size, hex_rgb(COLORS[variant]["tile"]) + (255,))
    base.putalpha(src.getchannel("A"))
    return base


def _over(base, top):
    out = base.copy()
    out.paste(top, (0, 0), top.getchannel("A"))
    # keep the base's alpha (rounded tile)
    out.putalpha(base.getchannel("A"))
    return out


def mutate_pngs(d, fn):
    for variant in ("dark", "light"):
        for n in SIZES:
            p = os.path.join(d, f"fontdaw-icon-{variant}_{n}x{n}.png")
            fn(p, variant, n).save(p)


def mut_tiny(p, variant, n):
    src = Image.open(p).convert("RGBA")
    base = _plain_tile(src, variant)
    small = max(1, round(n / 30))
    glyph = src.resize((small, small), Image.LANCZOS)
    base.paste(glyph, ((n - small) // 2, (n - small) // 2))
    return base


def mut_shift(p, variant, n):
    src = Image.open(p).convert("RGBA")
    base = _plain_tile(src, variant)
    shift = round(0.15 * n)
    return _over(base, ImageChops.offset(src, shift, shift))


def mut_blank(p, variant, n):
    return _plain_tile(Image.open(p).convert("RGBA"), variant)


def self_test():
    failures = []

    def run(d):
        buf = io.StringIO()
        rep = Report(False)
        with contextlib.redirect_stdout(buf):
            check_dir(d, rep)
        return rep.failed, buf.getvalue()

    # The committed set passes.
    failed, out = run(DEFAULT_DIR)
    print(f"committed set: {'ok' if not failed else 'FAIL'}")
    if failed:
        failures.append("the committed set does not pass")
        print(out)

    with tempfile.TemporaryDirectory() as tmp:
        def fresh(name):
            d = os.path.join(tmp, name)
            shutil.copytree(DEFAULT_DIR, d)
            return d

        def expect_fail(name, d, must_mention):
            failed, out = run(d)
            hit = [ln for ln in out.splitlines() if ln.startswith("FAIL") and must_mention in ln]
            good = failed > 0 and hit
            print(f"mutant {name}: {'caught' if good else 'NOT CAUGHT'} "
                  f"({failed} failing files: {', '.join(ln[5:] for ln in out.splitlines() if ln.startswith('FAIL'))[:150]})")
            if not good:
                failures.append(f"mutant not caught: {name}")

        d = fresh("tiny")
        mutate_pngs(d, mut_tiny)
        expect_fail("(a) glyph shrunk to 1/30 on a plain tile", d, "fontdaw-icon-")
        # every size must catch it on its own
        for variant in ("dark", "light"):
            for n in SIZES:
                name = f"fontdaw-icon-{variant}_{n}x{n}.png"
                pr, _ = check_image(Image.open(os.path.join(d, name)), n, FLAT, variant)
                if not pr:
                    failures.append(f"(a) not caught at {name}")
                    print(f"  (a) NOT caught at {name}")

        d = fresh("shift")
        mutate_pngs(d, mut_shift)
        expect_fail("(b) glyph shifted by 15% of the tile", d, "fontdaw-icon-")
        for variant in ("dark", "light"):
            for n in SIZES:
                name = f"fontdaw-icon-{variant}_{n}x{n}.png"
                pr, _ = check_image(Image.open(os.path.join(d, name)), n, FLAT, variant)
                if not pr:
                    failures.append(f"(b) not caught at {name}")
                    print(f"  (b) NOT caught at {name}")

        d = fresh("variant")
        for n in SIZES:
            shutil.copyfile(os.path.join(d, f"fontdaw-icon-dark_{n}x{n}.png"),
                            os.path.join(d, f"fontdaw-icon-light_{n}x{n}.png"))
        expect_fail("(c) dark image under the light name", d, "fontdaw-icon-light")
        failed, out = run(d)
        if "dark and light variants differ" not in "\n".join(l for l in out.splitlines() if l.startswith("FAIL")):
            failures.append("(c) the 'variants differ' check did not fail")

        d = fresh("blank")
        mutate_pngs(d, mut_blank)
        expect_fail("(d) blank tile", d, "fontdaw-icon-")
        for variant in ("dark", "light"):
            for n in SIZES:
                name = f"fontdaw-icon-{variant}_{n}x{n}.png"
                pr, _ = check_image(Image.open(os.path.join(d, name)), n, FLAT, variant)
                if not pr:
                    failures.append(f"(d) not caught at {name}")
                    print(f"  (d) NOT caught at {name}")

        d = fresh("layer")
        # The original broken layer files: the 32-unit drawings, no size.
        for layer, src in (("note-light.svg", "fontdaw-icon-black-note.svg"),
                           ("note-dark.svg", "fontdaw-icon-white-note.svg")):
            shutil.copyfile(os.path.join(d, "src", src), os.path.join(d, "AppIcon.icon", "Assets", layer))
        expect_fail("(e) Assets SVG with viewBox 0 0 32 32 and no size", d, "AppIcon.icon")

    if failures:
        print("SELF-TEST FAILED:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("self-test passed")
    return 0


# ---- Main -----------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="Check the FontDaw app icon set by sampling colours at points mapped from the "
                    "32-unit drawing. The --iconset part is NOT YET VERIFIED ON A MAC.")
    ap.add_argument("--dir", default=DEFAULT_DIR, help="icon directory (default: %(default)s)")
    ap.add_argument("--verbose", action="store_true", help="print every sampled point")
    ap.add_argument("--iconset", metavar="DIR",
                    help="also check a macOS iconset made with 'iconutil -c iconset AppIcon.icns'. "
                         "NOT YET VERIFIED ON A MAC: mapping and tolerance are assumptions; use "
                         "--verbose to read the measured colours")
    ap.add_argument("--variant", choices=("light", "dark"), help="variant shown by the iconset")
    ap.add_argument("--tile-inset", type=float, default=MAC_TILE_INSET,
                    help="tile size as a fraction of the canvas for --iconset (default 824/1024 = %(default).4f)")
    ap.add_argument("--self-test", action="store_true",
                    help="build broken icon sets in a temporary directory and check that they fail")
    a = ap.parse_args()

    if a.self_test:
        return self_test()
    if a.iconset and not a.variant:
        ap.error("--iconset needs --variant light|dark")

    rep = Report(a.verbose)
    check_dir(a.dir, rep)
    if a.iconset:
        print("-- macOS iconset (not yet verified on a Mac)")
        check_iconset(a.iconset, a.variant, a.tile_inset, rep)
    print(f"{rep.checked - rep.failed} of {rep.checked} checks passed"
          + ("" if not rep.failed else f", {rep.failed} FAILED"))
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
