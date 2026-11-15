#!/usr/bin/env python3
"""Builds a fake console tree for the host preview: a webMAN game list in the wrong order,
PS3ISO / PS2ISO / PSXISO folders with empty image files and generated covers."""
import colorsys
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

PS3 = [
    "Alpha Protocol (USA) (En,Fr,De,Es,It)", "Drakengard 3 (USA) (En,Fr)", "Eternal Sonata (USA) (En,Fr)",
    "God of War - Ascension (USA) (En,Fr,Es,Pt)", "Infamous (USA) (En,Fr,Es) (v02.00)", "Infamous 2 (USA) (En,Fr,Es,Pt) (v02.00)",
    "Killzone 2 (USA) (En,Fr,De,Es,It)", "Killzone 3 (USA, Brazil) (En,Ja,Fr,De,Es,It)", "LittleBigPlanet (USA) (En,Ja,Fr,De,Es,It) (v02.00)",
    "LittleBigPlanet 2 (USA) (En,Fr,Es,Pt)", "LittleBigPlanet 3 (USA) (En,Fr)", "LittleBigPlanet Karting (USA, Brazil) (En,Fr,Es,Pt)",
    "Puppeteer (USA) (En,Fr,Es,Pt)", "Ratchet & Clank - Into the Nexus (USA) (En,Fr,Es)",
    "Ratchet & Clank Future - A Crack in Time (USA) (En,Fr,Es)", "Ratchet & Clank Future - Tools of Destruction (USA) (En,Ja,Fr)",
    "Resistance - Fall of Man (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Ko)", "Resistance 2 (USA) (En,Ja,Fr,De,Es,It,Nl,Pt)", "Resistance 3 (USA) (En,Fr,Es)",
    "Silent Hill - Downpour (USA, Asia) (En,Fr,De,Es,It,Pt)", "Silent Hill - Homecoming (USA) (En,Fr,De,Es,It)",
    "Sly Cooper - Thieves in Time (USA) (En,Fr,Es,Pt)", "Sorcery (USA) (En,Fr,Es)", "Tales of Graces f (USA)",
    "Tales of Xillia (USA) (En,Fr,Es)", "Tales of Xillia 2 (USA) (En,Fr,Es)", "Yakuza - Dead Souls (USA)",
]
PS2 = ["Final Fantasy X (USA)", "Final Fantasy X-2 (USA)", "Suikoden III (USA)", "Suikoden V (USA)", "Wild Arms 3 (USA)",
       "Wild Arms - Alter Code F (USA)", "Shadow Hearts - Covenant (USA)", "Tales of Legendia (USA)"]
PSX = ["Final Fantasy VII (USA) (Disc 1)", "Final Fantasy VII (USA) (Disc 2)", "Final Fantasy VII (USA) (Disc 3)",
       "Final Fantasy IX (USA) (Disc 1)", "Chrono Cross (USA) (Disc 1)", "Suikoden II (USA)", "Vagrant Story (USA)"]


def cover(name: str, size: int = 160, alpha: int = 255) -> Image.Image:
    rnd = random.Random(name)
    hue = rnd.random()
    top = tuple(int(c * 255) for c in colorsys.hsv_to_rgb(hue, 0.55, 0.85))
    bot = tuple(int(c * 255) for c in colorsys.hsv_to_rgb((hue + 0.08) % 1, 0.7, 0.35))
    im = Image.new("RGBA", (size, size))
    px = im.load()
    for y in range(size):
        for x in range(size):
            t = y / (size - 1)
            px[x, y] = tuple(int(top[k] * (1 - t) + bot[k] * t) for k in range(3)) + (alpha,)
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 40)
    except OSError:
        font = ImageFont.load_default()
    initials = "".join(w[0] for w in name.split()[:2]).upper()
    d.text((size // 2, size // 2 + 14), initials, fill=(255, 255, 255, 235), font=font, anchor="mm")
    return im


def mark(im: Image.Image, ratio: float = 0.15) -> Image.Image:
    """Draws the completion badge the same way the ps3tweaks FTP cover marker does."""
    im = im.convert("RGBA")
    d = ImageDraw.Draw(im)
    base = min(im.size)
    size, margin = max(18, int(base * ratio)), max(6, int(base * 0.02))
    x0 = y0 = margin
    d.rounded_rectangle([x0, y0, x0 + size, y0 + size], radius=max(4, size // 6), fill=(27, 158, 50, 230),
                        outline=(255, 255, 255, 240), width=max(1, size // 15))
    pts = [(x0 + int(size * 0.22), y0 + int(size * 0.55)), (x0 + int(size * 0.43), y0 + int(size * 0.75)),
           (x0 + int(size * 0.78), y0 + int(size * 0.30))]
    d.line(pts, fill=(255, 255, 255, 255), width=max(2, size // 9), joint="curve")
    return im


def entry(key: str, title: str, folder: str, fname: str) -> str:
    return (f'<T key="{key}" include="inc"><P key="icon"><>/dev_hdd0/{folder}/{fname}.png</></P>'
            f'<P key="title"><>{title}</></P><P key="module_action"><>/mount_ps3/dev_hdd0/{folder}/{fname}.iso</></P>'
            f'<P key="info"><>hdd0/{folder}</></P></T>')


def segment(name: str, folder: str, titles: list, shuffle_seed: int) -> str:
    order = list(range(len(titles)))
    random.Random(shuffle_seed).shuffle(order)
    defs = "".join(entry(f"{i:04d}", titles[i], folder, titles[i]) for i in range(len(titles)))
    tags = "".join(f'<I class="type:x-xmb/module-action" key="{i:04d}" attr="{i:04d}"/>' for i in order)
    return f'<V id="seg_wm_{name}_items"><A><T key="inc"><P key="module_name"><>idle_plugin</></P></T>{defs}</A><I>{tags}</I></V>'


def main(root: Path) -> None:
    hdd = root / "dev_hdd0"
    for folder, titles in (("PS3ISO", PS3), ("PS2ISO", PS2), ("PSXISO", PSX)):
        d = hdd / folder
        d.mkdir(parents=True, exist_ok=True)
        for i, t in enumerate(titles):
            (d / f"{t}.iso").write_bytes(b"iso")
            if i % 7 != 3:  # a few games without a cover
                cover(t).save(d / f"{t}.png")
    # one cover already marked by another tool (the ps3tweaks FTP marker draws this same badge)
    mark(cover(PS3[4])).save(hdd / "PS3ISO" / f"{PS3[4]}.png")
    xml = ('<?xml version="1.0" encoding="UTF-8"?><X version="1.0"><V id="seg_mygames"><A><T key="wm_ps3"><P key="title"><>PLAYSTATION®3</></P>'
           f'<P key="info"><>{len(PS3)} Jogos PS3</></P></T></A><I><Q class="type:x-xmb/folder-pixmap" key="wm_ps3" attr="wm_ps3" src="#seg_wm_ps3_items"/></I></V>'
           + segment("ps3", "PS3ISO", PS3, 3) + segment("ps2", "PS2ISO", PS2, 5) + segment("psx", "PSXISO", PSX, 7) + "</X>")
    (hdd / "xmlhost" / "game_plugin").mkdir(parents=True, exist_ok=True)
    (hdd / "xmlhost" / "game_plugin" / "mygames.xml").write_text(xml, encoding="utf-8")
    print(f"fake console at {root}")


if __name__ == "__main__":
    main(Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/hometidy_fake"))
