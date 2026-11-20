#!/usr/bin/env bash
# Generates the FontDaw app icon set in share/icons/FontDawIcon/ from the website's SVGs
# (Batch 1 decision A-3: placeholder until the v2 redesign, FD-043). Run by hand when the icon
# changes; the results are committed, so the build needs no SVG renderer.
#
# Usage: tools/fontdaw/make_app_icons.sh [<dir with fontdaw-icon-white-note.svg and -black-note.svg>]
#        Without an argument it uses the copies in share/icons/FontDawIcon/src/.
# Needs: rsvg-convert (librsvg2-bin), icotool (icoutils), python3.
#
# Writes: fontdaw-icon-{dark,light}.svg (note on a rounded tile), PNGs at 16…1024 px for both,
# fontdaw-icon-dark.ico (16, 24, 32, 48, 256 px). The macOS Icon Composer bundle AppIcon.icon/
# gets its two layer files (Assets/note-{light,dark}.svg) generated here: the 32-unit drawing on
# a 1024 canvas (width/height 1024, translate(102.4 102.4) scale(25.6), plain presentation
# attributes, no <style>). A bare 32-unit file draws a 32-point dot in actool (FD-052), so the
# source SVGs must not be copied into Assets/.
# At the end tools/fontdaw/check_app_icons.py checks the whole set; the script fails if it does.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$root/share/icons/FontDawIcon"
src="${1:-$out/src}"

for f in fontdaw-icon-white-note.svg fontdaw-icon-black-note.svg; do
    [ -f "$src/$f" ] || { echo "missing $src/$f" >&2; exit 1; }
done
mkdir -p "$out/src" "$out/AppIcon.icon/Assets"
if [ "$(cd "$src" && pwd)" != "$out/src" ]; then
    cp "$src"/fontdaw-icon-white-note.svg "$src"/fontdaw-icon-black-note.svg "$out/src/"
fi

# Icon Composer layers: the drawing on a 1024 canvas, classes turned into plain attributes
# (nothing then depends on how actool reads a style sheet). 102.4 = 3.2 * 32 and
# 25.6 = 0.8 * 32: the same place as in the tile.
python3 -I - "$out" <<'EOF'
import re, sys
d = sys.argv[1]
for layer, src in (("light", "fontdaw-icon-black-note.svg"), ("dark", "fontdaw-icon-white-note.svg")):
    s = open(f"{d}/src/{src}").read()
    title = re.search(r"<title>.*?</title>", s).group(0)
    style = re.search(r"<style>(.*?)</style>", s).group(1)
    col = {m.group(1): m.group(3) for m in re.finditer(r"\.(\w+)\{(stroke|fill):(#\w+)\}", style)}
    # wave, note head, flag, stem: the order of the source
    wave = re.search(r'<path class="w" fill="none" (.*?)/>', s).group(1)
    g = re.search(r'<g class="n">(.*?)</g>', s).group(1)
    head = re.search(r"<ellipse (.*?)/>", g).group(1)
    flag = re.search(r"<path (d=.*?)/>", g).group(1)
    stem = re.search(r'<path class="ns" fill="none" (.*?)/>', s).group(1)
    body = (f'<path fill="none" stroke="{col["w"]}" {wave}/>'
            f'<ellipse fill="{col["n"]}" {head}/>'
            f'<path fill="{col["n"]}" {flag}/>'
            f'<path fill="none" stroke="{col["ns"]}" {stem}/>')
    open(f"{d}/AppIcon.icon/Assets/note-{layer}.svg", "w").write(
        '<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024" fill="none">'
        f'{title}<g transform="translate(102.4 102.4) scale(25.6)">{body}</g></svg>\n')
EOF

# Tile colours: the website's page background in dark and light mode (docs/style.css).
python3 -I - "$out" <<'EOF'
import re, sys
d = sys.argv[1]
for variant, src, tile in (("dark", "fontdaw-icon-white-note.svg", "#141311"),
                           ("light", "fontdaw-icon-black-note.svg", "#fbfaf6")):
    s = open(f"{d}/src/{src}").read()
    style = re.search(r"<style>(.*?)</style>", s).group(1)
    body = s.split("</style>", 1)[1].rsplit("</svg>", 1)[0]
    open(f"{d}/fontdaw-icon-{variant}.svg", "w").write(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">'
        f"<title>FontDaw app icon, {variant} (placeholder from the website's icon)</title>"
        f'<style>{style}</style><rect width="32" height="32" rx="7" fill="{tile}"/>'
        f'<g transform="translate(3.2 3.2) scale(0.8)">{body}</g></svg>\n')
EOF

for v in dark light; do
    for n in 16 24 32 48 64 96 128 256 512 1024; do
        rsvg-convert -w "$n" -h "$n" "$out/fontdaw-icon-$v.svg" -o "$out/fontdaw-icon-${v}_${n}x${n}.png"
    done
done
icotool -c -o "$out/fontdaw-icon-dark.ico" \
    "$out"/fontdaw-icon-dark_{16x16,24x24,32x32,48x48}.png -r "$out/fontdaw-icon-dark_256x256.png"

ls -l "$out" | sed 1d
python3 -I "$root/tools/fontdaw/check_app_icons.py" --dir "$out"
echo "Check 16 and 24 px by eye: below 32 px the note's stem is thinner than a pixel (FD-043)."
