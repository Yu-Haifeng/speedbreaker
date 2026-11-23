#!/usr/bin/env python3
"""Draw the one picture the deck's own set does not have: a quick rear view.

The NFS.MW.IOS deck has one mirror (look back.png) and no second, quicker
glance, so the deck's LB and its Y both wore that picture. This draws a second
one in the first one's material: the same disc, vertical fade, rim ring, tab and
mirror, lifted straight out of look back.png, with the reflections inside the
glass replaced by a pair of chevrons looking back over your shoulder -- the
motion of a glance rather than the stillness of a look.

Why not the handoff's own quick-rear-view SVG (design/touch-deck-v2.html)? That
is a 24x24 line drawing -- a 2.1-unit stroke on a 15.8-unit mirror, a third of a
button across here -- and beside the deck's bold solid pictures (the camera a
filled body, the mirror's reflections wedges of 50 pixels in 256) it reads as a
different set. This keeps everything the sibling has and changes only what is
inside the glass, in strokes at the mirror's own 7-pixel weight, so the two
buttons are one family by construction.

    python3 scripts/gen_touch_icon.py --source ~/Downloads/NFS.MW.IOS/"look back.png"

writes runtime/ui/touch/art/quick rear view.png, which is committed and packed
by scripts/gen_deck_atlas.py like any other picture. Run it again only to redraw
the chevrons: the plate and the mirror come from the source each time.
"""
import argparse
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_deck_atlas import read_png, scale_box, write_png  # noqa: E402

SIZE = 256   # what the packer wants (scripts/gen_deck_atlas.py's ART_SIZE)
# The mirror's interior: its outline is 7 pixels thick at 39..216 by 97..174, so
# this box holds the reflections and nothing of the outline.
INTERIOR = (56, 105, 204, 167)  # x0, y0, x1, y1


def plate_fade(px, w, h, top, bottom):
    """The plate's alpha as `a0 + slope * (y - h / 2)`.

    The disc is black at a vertical fade, so two clean rows of the middle column
    -- above and below everything drawn on it -- give the whole thing.
    """
    def alpha(y):
        return px[((y * w) + w // 2) * 4 + 3]

    a_top, a_bottom = alpha(top), alpha(bottom)
    slope = (a_bottom - a_top) / float(bottom - top)
    return a_top + (h // 2 - top) * slope, slope


def over_white(px, w, x, y, cover):
    """White at `cover` (0..1) over the plate under it, straight alpha."""
    i = (y * w + x) * 4
    da = px[i + 3] / 255.0
    out_a = cover + da * (1.0 - cover)
    for c in range(3):
        px[i + c] = max(0, min(255, int(round((255.0 * cover + px[i + c] * da * (1.0 - cover)) / out_a))))
    px[i + 3] = max(0, min(255, int(round(out_a * 255.0))))


def stroke(px, w, h, a, b, width):
    """A line with round ends, 4x4 supersampled."""
    x0, y0 = a
    x1, y1 = b
    dx, dy = x1 - x0, y1 - y0
    length = dx * dx + dy * dy
    half = width * 0.5
    for y in range(max(0, int(min(y0, y1) - half - 1)), min(h, int(max(y0, y1) + half + 2))):
        for x in range(max(0, int(min(x0, x1) - half - 1)), min(w, int(max(x0, x1) + half + 2))):
            cover = 0
            for sy in range(4):
                for sx in range(4):
                    fx, fy = x + (sx + 0.5) / 4, y + (sy + 0.5) / 4
                    t = 0.0 if length == 0 else max(0.0, min(1.0, ((fx - x0) * dx + (fy - y0) * dy) / length))
                    if math.hypot(fx - (x0 + t * dx), fy - (y0 + t * dy)) <= half:
                        cover += 1
            if cover > 0:
                over_white(px, w, x, y, cover / 16.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--source", required=True, help='the deck\'s "look back.png"')
    ap.add_argument("--out", default="runtime/ui/touch/art/quick rear view.png")
    ap.add_argument("--stroke", type=float, default=9.0, help="the chevrons' stroke, in pixels of 256")
    ap.add_argument("--arm", type=float, default=19.0, help="one chevron's arm, in pixels of 256")
    ap.add_argument("--gap", type=float, default=12.0, help="between the two chevrons' tips")
    args = ap.parse_args()

    w, h, rgba = scale_box(*read_png(args.source)[:3], SIZE)
    px = bytearray(rgba)

    # The reflections out: the plate behind them is black at the fade's alpha,
    # so the erase is exact rather than a smear.
    a0, slope = plate_fade(px, w, h, INTERIOR[1] - 30, INTERIOR[3] + 30)
    for y in range(INTERIOR[1], INTERIOR[3]):
        alpha = max(0, min(255, int(round(a0 + slope * (y - h // 2)))))
        for x in range(INTERIOR[0], INTERIOR[2]):
            i = (y * w + x) * 4
            px[i] = px[i + 1] = px[i + 2] = 0
            px[i + 3] = alpha

    # Two chevrons pointing back, where the reflections were: at the mirror's
    # own weight, inside its glass with room to spare.
    cx, cy = 128.0, 135.0
    for side in (-0.5, 0.5):
        x = cx + side * (args.arm + args.gap)
        stroke(px, w, h, (x + args.arm * 0.5, cy - args.arm), (x - args.arm * 0.5, cy), args.stroke)
        stroke(px, w, h, (x - args.arm * 0.5, cy), (x + args.arm * 0.5, cy + args.arm), args.stroke)
    print(f"chevrons {args.arm * 2:.0f} tall, {args.stroke:.0f} wide, {args.gap:.0f} apart")

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    write_png(args.out, w, h, bytes(px))
    print(f"{args.out}: {w}x{h}")


if __name__ == "__main__":
    main()
