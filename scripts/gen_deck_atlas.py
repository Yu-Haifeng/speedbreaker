#!/usr/bin/env python3
"""Pack the deck's button art into the one texture the overlay draws, and write
the metadata that drawing and hit testing share.

Two stages, either of which can be run on its own:

    python3 scripts/gen_deck_atlas.py --sources ~/Downloads/NFS.MW.IOS
        Packs the source pictures into runtime/ui/touch/deck_atlas.png and
        writes the header. The pack bakes the plate's translucency (BAKE
        below), so the file is marked as baked and refuses to be baked twice.

    python3 scripts/gen_deck_atlas.py
        Reads the committed atlas and rewrites runtime/hid/touch_art.h. No
        sources needed: the metadata is always reproducible from the PNG.

Both outputs are committed -- the game embeds the PNG at build time
(runtime/CMakeLists.txt), so nothing here runs while building, and the build
never needs Python.

BAKE. The overlay used to draw each picture twice: its plate is about 30%
alpha, which over a sunlit road reads as a bare outline, and a second pass took
it to about 51%. Compositing a straight-alpha picture over itself gives
`a' = 2a - a²` with the colour unchanged, so the same look costs one draw if
the alpha is raised here instead. A file that already carries the marker is not
baked again (that would double it).

The atlas is kCols x kRows cells of 272 (256 of picture and an 8-pixel gutter,
so a bilinear sample at a cell's edge cannot reach its neighbour). A cell's
picture is trimmed to its own alpha before the UV and the distance field are
taken: the plates reach the edge of their art, and the gutter is not part of
the button.

The distance field is what makes a button's touch area follow its picture: a
quantised signed distance to the silhouette, holes filled (the gears are rings
around a cut-out cross), so `Inside(x, y, pad)` is "on the picture, or within
`pad` of it" for any shape, round or not.
"""
import argparse
import math
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

# The pictures the deck draws, in the order hid/touch_art.h's Id has them. The
# file names are the NFS.MW.IOS deck's own -- or, where the deck has no such
# picture, the deck's own folder of them (--art, runtime/ui/touch/art).
#
# The source set has 23; the six the deck does not draw (up, down, left, right,
# race, stats) are left out, and quick-rear-view is drawn rather than taken from
# it (scripts/gen_touch_icon.py), so the atlas is 4 rows rather than 5.
ART = [
    ("AnalogBase", "analog base"),
    ("AnalogStick", "analog stick"),
    ("Gas", "gas"),
    ("Brake", "brake"),
    ("Handbrake", "handbrake"),
    ("Nos", "nos"),
    ("Speedbreaker", "speedbreaker"),
    ("LookBack", "look back"),
    ("QuickRearView", "quick rear view"),
    ("Camera", "camera"),
    ("GearUp", "gear up"),
    ("GearDown", "gear down"),
    ("Pause", "pause"),
    ("Reset", "reset"),
    ("Enter", "enter"),
    ("List", "list"),
    ("Map", "map"),
    ("Message", "message"),
]

ART_SIZE = 256   # of a cell's picture, in pixels
GUTTER = 8       # transparent room around it
CELL = ART_SIZE + GUTTER * 2
COLS = 5

SDF_WIDTH = 40   # samples across a picture's width
BAKED_KEY = b"SpeedBreaker-baked"

PNG = "runtime/ui/touch/deck_atlas.png"
HEADER = "runtime/hid/touch_art.h"


def kebab(name):
    """AnalogBase -> analog-base: the name a file and a settings row use."""
    out = []
    for index, ch in enumerate(name):
        if ch.isupper() and index:
            out.append("-")
        out.append(ch.lower())
    return "".join(out)


def read_png(path):
    """A PNG as (width, height, 4 channels, text chunks)."""
    d = Path(path).read_bytes()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    i, idat, plte, texts = 8, b"", None, {}
    w = h = ct = None
    while i < len(d):
        (length,) = struct.unpack_from(">I", d, i)
        kind = d[i + 4 : i + 8]
        data = d[i + 8 : i + 8 + length]
        i += 12 + length
        if kind == b"IHDR":
            w, h, depth, ct, _, _, interlace = struct.unpack(">IIBBBBB", data)
            if depth != 8 or interlace != 0:
                sys.exit(f"{path}: only 8-bit non-interlaced PNGs")
        elif kind == b"IDAT":
            idat += data
        elif kind == b"PLTE":
            plte = data
        elif kind == b"tEXt":
            key, _, value = data.partition(b"\0")
            texts[key] = value
        elif kind == b"IEND":
            break
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    stride = w * channels
    out = bytearray(h * stride)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        f = raw[pos]
        pos += 1
        line = bytearray(raw[pos : pos + stride])
        pos += stride
        if f == 1:
            for x in range(channels, stride):
                line[x] = (line[x] + line[x - channels]) & 255
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                a = line[x - channels] if x >= channels else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 255
        elif f == 4:
            for x in range(stride):
                a = line[x - channels] if x >= channels else 0
                b = prev[x]
                c = prev[x - channels] if x >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out[y * stride : (y + 1) * stride] = line
        prev = line

    if channels == 4:
        return w, h, bytes(out), texts
    rgba = bytearray(w * h * 4)
    for i in range(w * h):
        if channels == 3:
            rgba[i * 4 : i * 4 + 3] = out[i * 3 : i * 3 + 3]
            rgba[i * 4 + 3] = 255
        elif channels == 1:
            v = out[i]
            rgba[i * 4 : i * 4 + 4] = bytes((v, v, v, 255))
        else:  # a palette's index
            idx = out[i] * 3
            rgba[i * 4 : i * 4 + 4] = plte[idx : idx + 3] + b"\xff"
    return w, h, bytes(rgba), texts


def write_png(path, w, h, rgba, texts=None):
    """An RGBA PNG, filter 0 (the rows are a screenshot's, not a photo's)."""
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgba[y * w * 4 : (y + 1) * w * 4]

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    body = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    for key, value in (texts or {}).items():
        body += chunk(b"tEXt", key + b"\0" + value)
    body += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    Path(path).write_bytes(body)


def scale_box(w, h, rgba, want):
    """`rgba` at most `want` a side, averaging the pixels each one covers."""
    f = min(want / w, want / h, 1.0)
    tw, th = max(1, int(round(w * f))), max(1, int(round(h * f)))
    out = bytearray(tw * th * 4)
    for ty in range(th):
        y0, y1 = ty * h / th, (ty + 1) * h / th
        for tx in range(tw):
            x0, x1 = tx * w / tw, (tx + 1) * w / tw
            r = g = b = a = 0.0
            area = 0.0
            for sy in range(int(y0), min(h, int(math.ceil(y1)))):
                dy = min(y1, sy + 1) - max(y0, sy)
                if dy <= 0:
                    continue
                for sx in range(int(x0), min(w, int(math.ceil(x1)))):
                    dx = min(x1, sx + 1) - max(x0, sx)
                    if dx <= 0:
                        continue
                    weight = dx * dy
                    i = (sy * w + sx) * 4
                    # (Straight alpha: weight the colour by its own alpha, so
                    # the transparent surround cannot darken an edge.)
                    alpha = rgba[i + 3] / 255.0
                    r += rgba[i] * alpha * weight
                    g += rgba[i + 1] * alpha * weight
                    b += rgba[i + 2] * alpha * weight
                    a += alpha * weight
                    area += weight
            o = (ty * tw + tx) * 4
            if area > 0 and a > 0:
                out[o] = min(255, int(r / a + 0.5))
                out[o + 1] = min(255, int(g / a + 0.5))
                out[o + 2] = min(255, int(b / a + 0.5))
                out[o + 3] = min(255, int(a / area * 255 + 0.5))
    return tw, th, bytes(out)


def scale_sips(source, want):
    """`sips` scales better and far faster, where it exists."""
    out = Path(tempfile.mkdtemp()) / "s.png"
    subprocess.run(["sips", "-Z", str(want), "-s", "format", "png", str(source), "--out", str(out)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return read_png(out)[:3]


def bake_alpha(rgba):
    """`a' = 2a - a²`: the picture composited over itself, straight alpha."""
    out = bytearray(rgba)
    for i in range(3, len(out), 4):
        a = out[i]
        out[i] = a * (510 - a) // 255
    return bytes(out)


def pack(sources, art, builtin=False):
    """The atlas: every picture scaled into its cell, its alpha baked."""
    src = Path(sources)
    have_sips = not builtin and subprocess.run(["which", "sips"], capture_output=True).returncode == 0
    rows = (len(ART) + COLS - 1) // COLS
    w, h = COLS * CELL, rows * CELL
    atlas = bytearray(w * h * 4)  # transparent

    for index, (name, file) in enumerate(ART):
        # The deck's folder first, then the deck's own art (the pictures the
        # source set does not have).
        path = next((p for p in (src / f"{file}.png", Path(art) / f"{file}.png") if p.exists()), None)
        if path is None:
            sys.exit(f"missing {file}.png in {src} or {art}")
        iw, ih, rgba = scale_sips(path, ART_SIZE) if have_sips else scale_box(*read_png(path)[:3], ART_SIZE)
        # Centred in its cell: a cell is square, the picture keeps its shape.
        cx = (index % COLS) * CELL + GUTTER + (ART_SIZE - iw) // 2
        cy = (index // COLS) * CELL + GUTTER + (ART_SIZE - ih) // 2
        for y in range(ih):
            dst = ((cy + y) * w + cx) * 4
            atlas[dst : dst + iw * 4] = rgba[y * iw * 4 : (y + 1) * iw * 4]
        print(f"  {name:14} {file}.png -> {iw}x{ih} at cell {index % COLS},{index // COLS}")

    return w, h, bake_alpha(bytes(atlas))


def edt1d(f):
    """Squared Euclidean distance transform of one line (Felzenszwalb)."""
    n = len(f)
    d = [0.0] * n
    v = [0] * n
    z = [0.0] * (n + 1)
    k = 0
    v[0] = 0
    z[0] = -1e20
    z[1] = 1e20
    for q in range(1, n):
        s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k])
        while s <= z[k]:
            k -= 1
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k])
        k += 1
        v[k] = q
        z[k] = s
        z[k + 1] = 1e20
    k = 0
    for q in range(n):
        while z[k + 1] < q:
            k += 1
        d[q] = (q - v[k]) ** 2 + f[v[k]]
    return d


def distance_to(grid, gw, gh, want):
    """For every sample, the distance (in samples) to the nearest `want` one."""
    far = 1e12
    rows = [[0.0 if grid[y * gw + x] == want else far for x in range(gw)] for y in range(gh)]
    rows = [edt1d(row) for row in rows]
    cols = [edt1d([rows[y][x] for y in range(gh)]) for x in range(gw)]
    return [[math.sqrt(cols[x][y]) if cols[x][y] > 0 else 0.0 for x in range(gw)] for y in range(gh)]


def fill_holes(mask, bw, bh):
    """Every background sample the outside cannot reach is inside the shape.

    The gears are rings: a silhouette that stopped at the cut-out would leave
    the middle of the button unpressed, which is not what a thumb does.
    """
    outside = bytearray(bw * bh)
    queue = []
    for x in range(bw):
        for y in (0, bh - 1):
            if not mask[y * bw + x] and not outside[y * bw + x]:
                outside[y * bw + x] = 1
                queue.append((x, y))
    for y in range(bh):
        for x in (0, bw - 1):
            if not mask[y * bw + x] and not outside[y * bw + x]:
                outside[y * bw + x] = 1
                queue.append((x, y))
    while queue:
        x, y = queue.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < bw and 0 <= ny < bh and not mask[ny * bw + nx] and not outside[ny * bw + nx]:
                outside[ny * bw + nx] = 1
                queue.append((nx, ny))
    return bytes(1 if mask[i] or not outside[i] else 0 for i in range(bw * bh))


def analyse(w, h, rgba, index):
    """One cell's UV, aspect, inscribed radius and quantised distance field."""
    cx = (index % COLS) * CELL + GUTTER
    cy = (index // COLS) * CELL + GUTTER

    # The picture's own bounds, at a threshold relative to its brightest sample:
    # the plates are about 30% alpha and must count as shape.
    peak = max(rgba[((cy + y) * w + cx + x) * 4 + 3] for y in range(ART_SIZE) for x in range(ART_SIZE))
    threshold = max(8, int(peak * 0.10))
    xs = [x for y in range(ART_SIZE) for x in range(ART_SIZE) if rgba[((cy + y) * w + cx + x) * 4 + 3] > threshold]
    if not xs:
        sys.exit(f"cell {index}: nothing drawn")
    x0, x1 = min(xs), max(xs)
    ys = [y for y in range(ART_SIZE) for x in range(ART_SIZE) if rgba[((cy + y) * w + cx + x) * 4 + 3] > threshold]
    y0, y1 = min(ys), max(ys)
    tw, th = x1 - x0 + 1, y1 - y0 + 1
    aspect = tw / th

    mask = bytearray(tw * th)
    for y in range(th):
        for x in range(tw):
            if rgba[((cy + y0 + y) * w + cx + x0 + x) * 4 + 3] > threshold:
                mask[y * tw + x] = 1
    mask = fill_holes(mask, tw, th)

    # Down to the field's grid: area coverage, then half of a sample's worth.
    sw = SDF_WIDTH
    sh = max(2, int(round((sw - 1) / aspect)) + 1)
    small = bytearray(sw * sh)
    for sy in range(sh):
        ya, yb = sy * th / sh, (sy + 1) * th / sh
        for sx in range(sw):
            xa, xb = sx * tw / sw, (sx + 1) * tw / sw
            covered = 0.0
            area = 0.0
            for y in range(int(ya), min(th, int(math.ceil(yb)))):
                dy = min(yb, y + 1) - max(ya, y)
                if dy <= 0:
                    continue
                for x in range(int(xa), min(tw, int(math.ceil(xb)))):
                    dx = min(xb, x + 1) - max(xa, x)
                    if dx <= 0:
                        continue
                    if mask[y * tw + x]:
                        covered += dx * dy
                    area += dx * dy
            small[sy * sw + sx] = 1 if area > 0 and covered / area >= 0.5 else 0

    inside = distance_to(small, sw, sh, 0)   # distance from a shape sample to the edge
    outside = distance_to(small, sw, sh, 1)  # distance from a gap sample to the shape
    span = float(sw)                         # one sample is 1/span of the width
    inradius = 0.0
    field = []
    for y in range(sh):
        for x in range(sw):
            if small[y * sw + x]:
                d = -inside[y][x] / span
                inradius = min(0.5, max(inradius, -d))
            else:
                d = outside[y][x] / span
            field.append(min(255, max(0, int(round((min(0.5, max(-0.5, d)) + 0.5) * 255)))))

    uv = ((cx + x0) / w, (cy + y0) / h, (cx + x1 + 1) / w, (cy + y1 + 1) / h)
    return uv, aspect, inradius, sw, sh, bytes(field)


def write_header(path, arts):
    rows = (len(ART) + COLS - 1) // COLS
    lines = [
        "// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).",
        "//",
        "// GENERATED by scripts/gen_deck_atlas.py -- do not edit.",
        "//",
        "// The deck's pictures in runtime/ui/touch/deck_atlas.png, and what both",
        "// drawing and hit testing need about each one: its cell in the atlas, its",
        "// trimmed bounds, its shape's inscribed radius and a quantised signed",
        "// distance field of its silhouette (holes filled), so that a control's",
        "// touch area follows the picture rather than the rectangle it sits in.",
        "//",
        "// The field is in units of the picture's width: negative inside the",
        "// silhouette, positive outside, saturated at half a width, and quantised",
        "// as (d + 0.5) * 255. SdfAt() interpolates it, Inside() compares it with a",
        "// pad. Info::field points at it: at kSdf here, at a buffer of its own for a",
        "// picture the player supplied (hid/touch_art_state.h), whose cell is in",
        "// the second texture Info::sheet names rather than in this atlas.",
        "#pragma once",
        "#include <algorithm>",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace hid::touch::art",
        "{",
        "    // The pictures scripts/gen_deck_atlas.py packs, in its order.",
        "    enum class Id : uint8_t",
        "    {",
    ]
    for row in range(0, len(ART), 5):
        names = ", ".join(name for name, _ in ART[row : row + 5])
        lines.append(f"        {names},")
    lines += [
        "        Count",
        "    };",
        "    constexpr size_t kCount = size_t(Id::Count);",
        "",
        f"    // The atlas: {COLS} x {rows} cells of {CELL} ({ART_SIZE} of picture and a",
        f"    // {GUTTER}-pixel gutter around it, so a sample at a cell's edge cannot reach its",
        "    // neighbour).",
        f"    constexpr int kCols = {COLS}, kRows = {rows}, kCell = {CELL};",
        "    constexpr int kAtlasWidth = kCols * kCell, kAtlasHeight = kRows * kCell;",
        "",
        "    struct Info",
        "    {",
        "        float u0, v0, u1, v1;  // the trimmed picture's place in the atlas, 0..1",
        "        float aspect;          // trimmed width / height",
        "        float inradius;        // its widest inscribed circle's radius, in widths",
        "        uint8_t sdfW, sdfH;    // the field's grid",
        "        uint8_t sheet;         // 0: the deck's atlas; 1: a picture the player supplied",
        "        const uint8_t* field;  // its first sample: kSdf, or a loaded picture's own",
        "    };",
        "",
        "    inline constexpr int kSdfW = %d;" % SDF_WIDTH,
        "",
    ]
    # kSdf first: kArt's entries point into it.
    fields = [field for (_, _, _, _, _, field) in arts]
    total = sum(len(field) for field in fields)
    lines += [
        f"    // {total // 1024} KB of signed distance, one byte a sample.",
        "    inline constexpr uint8_t kSdf[] = {",
    ]
    for field in fields:
        for row in range(0, len(field), 24):
            chunk = ", ".join(str(v) for v in field[row : row + 24])
            lines.append(f"        {chunk},")
    lines += [
        "    };",
        "",
        "    // The pictures' names: what touch-deck.toml and the icon picker call",
        "    // them, and what a picture the player supplies is named after.",
        "    inline constexpr const char* kArtKey[kCount] = {",
    ]
    for name, _ in ART:
        lines.append(f'        "{kebab(name)}",  // {name}')
    lines += [
        "    };",
        "",
        "    // What each picture looks like and where it sits in the atlas.",
        "    inline constexpr Info kArt[kCount] = {",
    ]
    offset = 0
    for (name, _), (uv, aspect, inradius, sw, sh, field) in zip(ART, arts):
        lines.append(f"        {{ {uv[0]:.6f}f, {uv[1]:.6f}f, {uv[2]:.6f}f, {uv[3]:.6f}f, {aspect:.6f}f,"
                     f" {inradius:.6f}f, {sw}, {sh}, 0, kSdf + {offset} }},  // {name}")
        offset += len(field)
    lines += [
        "    };",
        "",
        "    // The signed distance at (x, y), both fractions of the picture's",
        "    // rectangle, in units of its width. Bilinear, and clamped at the",
        "    // rectangle's edge: a point just outside the picture reads as its own",
        "    // edge, which is what a pad is for.",
        "    inline float SdfAt(const Info& info, float x, float y)",
        "    {",
        "        if (!info.field || info.sdfW < 2 || info.sdfH < 2)",
        "            return 0.5f;",
        "        // (A sample sits at the middle of its cell, as the packer's does.)",
        "        const float gx = std::clamp(x * float(info.sdfW) - 0.5f, 0.0f, float(info.sdfW - 1));",
        "        const float gy = std::clamp(y * float(info.sdfH) - 0.5f, 0.0f, float(info.sdfH - 1));",
        "        const int x0 = int(gx), y0 = int(gy);",
        "        const int x1 = std::min(x0 + 1, int(info.sdfW) - 1), y1 = std::min(y0 + 1, int(info.sdfH) - 1);",
        "        const float fx = gx - float(x0), fy = gy - float(y0);",
        "        const uint8_t* s = info.field;",
        "        auto at = [s, &info](int x, int y) {",
        "            return float(s[size_t(y) * info.sdfW + size_t(x)]) / 255.0f - 0.5f;",
        "        };",
        "        const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx;",
        "        const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;",
        "        return top + (bottom - top) * fy;",
        "    }",
        "",
        "    // On the picture, or within `pad` of it (both in units of its width).",
        "    inline bool Inside(const Info& info, float x, float y, float pad)",
        "    {",
        "        return SdfAt(info, x, y) <= pad;",
        "    }",
        "}",
        "",
    ]
    Path(path).write_text("\n".join(lines))
    return offset


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--sources", help="the folder the NFS.MW.IOS deck PNGs are in (packs first)")
    ap.add_argument("--art", default="runtime/ui/touch/art", help="the deck's own PNGs, for the pictures the deck has none of")
    ap.add_argument("--png", default=PNG)
    ap.add_argument("--header", default=HEADER)
    ap.add_argument("--builtin", action="store_true", help="scale in Python (no sips)")
    args = ap.parse_args()

    if args.sources:
        w, h, rgba = pack(args.sources, args.art, args.builtin)
        texts = {BAKED_KEY: b"1"}
        write_png(args.png, w, h, rgba, texts)
        print(f"{args.png}: {w}x{h}, {len(ART)} cells, {Path(args.png).stat().st_size // 1024} KB (baked)")
    else:
        w, h, rgba, texts = read_png(args.png)
        if BAKED_KEY not in texts:
            sys.exit(f"{args.png} is not baked: run with --sources")
    if (w, h) != (COLS * CELL, ((len(ART) + COLS - 1) // COLS) * CELL):
        sys.exit(f"{args.png} is {w}x{h}, not the {COLS}x{len(ART)} grid it should be")

    arts = []
    for index, (name, _) in enumerate(ART):
        uv, aspect, inradius, sw, sh, field = analyse(w, h, rgba, index)
        arts.append((uv, aspect, inradius, sw, sh, field))
        print(f"  {name:14} uv {uv[0]:.4f},{uv[1]:.4f} {uv[2]-uv[0]:.4f}x{uv[3]-uv[1]:.4f}"
              f" aspect {aspect:.3f} inradius {inradius:.3f} field {sw}x{sh}")

    size = write_header(args.header, arts)
    print(f"{args.header}: {len(ART)} pictures, {size // 1024} KB of distance")


if __name__ == "__main__":
    main()
