#!/usr/bin/env python3
"""Generates ICON0.PNG (320x176, the XMB icon) and files/PIC1.PNG (1920x1080, the XMB background) with the version.

Usage: tools/make_art.py [OUT_DIR]   (default build/art; the version is read from VERSION)
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
BG, CARD, GREEN, GREEN_DARK, WHITE, MUTED = (15, 23, 42), (25, 33, 52), (34, 197, 94), (21, 128, 61), (248, 250, 252), (148, 163, 184)
BOLD, REGULAR = ROOT / "data" / "inter_bold.ttf", ROOT / "data" / "inter_regular.ttf"
VERSION = "v" + (ROOT / "VERSION").read_text().strip()


def font(path: Path, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(path), size)


def badge(d: ImageDraw.ImageDraw, cx: float, cy: float, r: float, disc=GREEN, mark=BG) -> None:
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=disc)
    w = max(3, int(r / 4))
    pts = [(cx - r * 0.45, cy + r * 0.02), (cx - r * 0.1, cy + r * 0.4), (cx + r * 0.5, cy - r * 0.38)]
    d.line(pts, fill=mark, width=w, joint="curve")
    for x, y in (pts[0], pts[2]):
        d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=mark)


def bars(d: ImageDraw.ImageDraw, x: float, y: float, unit: float) -> None:
    for i, w in enumerate((5.2, 4.2, 3.2, 2.2)):  # shrinking bars = a sorted list
        d.rounded_rectangle((x, y + i * 2.0 * unit, x + w * unit * 2.4, y + i * 2.0 * unit + 1.2 * unit), radius=0.6 * unit,
                            fill=GREEN if i == 0 else MUTED)


def icon() -> Image.Image:
    s = 4
    w, h = 320 * s, 176 * s
    im = Image.new("RGBA", (w, h), BG + (255,))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((12 * s, 12 * s, w - 12 * s, h - 12 * s), radius=22 * s, fill=CARD + (255,))
    bars(d, 36 * s, 34 * s, 7 * s)
    badge(d, 238 * s, 66 * s, 38 * s)
    d.text((w // 2, 127 * s), "HomeTidy", font=font(BOLD, 28 * s), fill=WHITE, anchor="mm")
    d.text((w // 2, 154 * s), VERSION, font=font(REGULAR, 15 * s), fill=MUTED, anchor="mm")
    return im.resize((320, 176), Image.LANCZOS)


def background() -> Image.Image:
    w, h = 1920, 1080
    im = Image.new("RGB", (w, h), BG)
    px = im.load()
    for y in range(h):  # soft vertical gradient
        t = y / (h - 1)
        c = tuple(int(BG[k] * (1 - t) + (8, 12, 26)[k] * t) for k in range(3))
        for x in range(w):
            px[x, y] = c
    d = ImageDraw.Draw(im)
    badge(d, w // 2, h // 2 - 70, 120, GREEN_DARK, WHITE)
    d.text((w // 2, h // 2 + 120), "HomeTidy", font=font(BOLD, 110), fill=WHITE, anchor="mm")
    d.text((w // 2, h // 2 + 210), "Sort your game list. Mark what you finished.", font=font(REGULAR, 42), fill=MUTED, anchor="mm")
    d.text((w // 2, h // 2 + 290), VERSION, font=font(REGULAR, 38), fill=GREEN, anchor="mm")
    return im


if __name__ == "__main__":
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "art"
    (out / "files").mkdir(parents=True, exist_ok=True)
    icon().save(out / "ICON0.PNG")
    background().save(out / "files" / "PIC1.PNG")
    print(f"art {VERSION} written to {out}")
