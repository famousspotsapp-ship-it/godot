#!/usr/bin/env python3
"""Generate pixel art sprites and chiptune SFX for Steel Streets.

Game Boy palette (4 colors only):
    0 = #e0f8d0 (lightest)
    1 = #88c070 (light)
    2 = #346856 (dark)
    3 = #081820 (darkest)

Run from repo root:
    python3 steel_streets/tools/gen_assets.py
"""
from __future__ import annotations

import math
import os
import struct
import wave
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SPR = ROOT / "assets" / "sprites"
AUD = ROOT / "assets" / "audio"

GB = [
    (224, 248, 208, 255),
    (136, 192, 112, 255),
    (52, 104, 86, 255),
    (8, 24, 32, 255),
]
TR = (0, 0, 0, 0)


def img_from_rows(rows):
    """Build an RGBA image from rows of '.' (transparent) or '0'-'3' chars."""
    h = len(rows)
    w = len(rows[0])
    im = Image.new("RGBA", (w, h), TR)
    px = im.load()
    for y, row in enumerate(rows):
        assert len(row) == w, f"row {y} has length {len(row)}, expected {w}"
        for x, ch in enumerate(row):
            if ch == ".":
                continue
            px[x, y] = GB[int(ch)]
    return im


def hcat(images):
    h = images[0].height
    w = sum(i.width for i in images)
    out = Image.new("RGBA", (w, h), TR)
    x = 0
    for im in images:
        out.paste(im, (x, 0), im)
        x += im.width
    return out


def save(im, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path)
    print(f"  wrote {path.relative_to(ROOT)}  {im.size}")


# --------------------------------------------------------------------------
# Player sprites (16x24, facing right). All frames share head+torso silhouette
# for consistency. The character is a turtle-like figure with a bandana mask.
# --------------------------------------------------------------------------

# Helper: assemble player frame from head/body/leg sub-rows.
PLAYER_HEAD_A = [  # rows 0..9 (10 rows). standing head pose.
    "................",
    ".....333333.....",
    "....32222223....",
    "...3211211123...",
    "...3133133123...",  # eyes (mask)
    "...3211211123...",
    "...3222222223...",
    "....32222223....",
    ".....322223.....",
    "......3333......",
]
PLAYER_HEAD_B = [  # 1px down (breathing idle).
    "................",
    "................",
    ".....333333.....",
    "....32222223....",
    "...3211211123...",
    "...3133133123...",
    "...3211211123...",
    "...3222222223...",
    "....32222223....",
    ".....322223.....",
]


def player_torso_arms(arm_l, arm_r):
    """Return 9 rows (player rows 10..18): shoulders, torso, arms.

    arm_l, arm_r: 'down' | 'mid' | 'up' | 'punch'.
    Local row indices 0..8 map to absolute rows 10..18.
    """
    rows = [
        ".....332233.....",  # 0  (abs 10) shoulders
        "....32212223....",  # 1  (abs 11)
        "....32212223....",  # 2  (abs 12)
        "....32212223....",  # 3  (abs 13)
        "....32212223....",  # 4  (abs 14)
        "....32222223....",  # 5  (abs 15)
        "....32212223....",  # 6  (abs 16)
        "....32212223....",  # 7  (abs 17)
        ".....322223.....",  # 8  (abs 18)
    ]

    def paint(side_cols, pose, current):
        out = [list(r) for r in current]
        c0, c1 = side_cols
        if pose == "down":
            for y in (1, 2, 3, 4):
                out[y][c0] = "3"
                out[y][c1] = "2"
            out[5][c0] = "3"
            out[5][c1] = "3"
        elif pose == "mid":
            for y in (2, 3):
                out[y][c0] = "3"
                out[y][c1] = "2"
            if c0 == 12:
                out[3][14] = "3"
            else:
                out[3][1] = "3"
        elif pose == "up":
            for y in (0, 1, 2):
                out[y][c0] = "3"
                out[y][c1] = "2"
        elif pose == "punch":
            for y in (2, 3):
                out[y][c0] = "3"
                out[y][c1] = "2"
            if c0 == 12:
                out[2][14] = "3"
                out[2][15] = "3"
                out[3][14] = "2"
                out[3][15] = "3"
            else:
                out[2][0] = "3"
                out[2][1] = "3"
                out[3][0] = "3"
                out[3][1] = "2"
        return ["".join(r) for r in out]

    rows = paint((2, 3), arm_l, rows)
    rows = paint((12, 13), arm_r, rows)
    return rows


def player_legs(pose):
    """Return rows 19..23 (5 rows). pose in {'stand','step_l','step_r','jump','crouch'}."""
    if pose == "stand":
        return [
            "....333..333....",
            "....322..223....",
            "....322..223....",
            "....332..233....",
            "...3333..3333...",
        ]
    if pose == "step_l":
        return [
            "....3333.333....",
            "...32223.223....",
            "...32223.223....",
            "...332333233....",
            "...3333.3333....",
        ]
    if pose == "step_r":
        return [
            "....333.3333....",
            "....322.32223...",
            "....322.32223...",
            "....332333233...",
            "....3333.3333...",
        ]
    if pose == "jump":  # knees pulled up
        return [
            "...3333..3333...",
            "..32232..23223..",
            "..32232..23223..",
            "...3333..3333...",
            "................",
        ]
    if pose == "crouch":
        return [
            "...3333..3333...",
            "...3223..3223...",
            "...3333..3333...",
            "................",
            "................",
        ]
    raise ValueError(pose)


def assemble_player(head, torso, legs):
    return img_from_rows(head + torso + legs)


def gen_player():
    out_dir = SPR / "player"
    # Idle: 2 frames (subtle head bob)
    f0 = assemble_player(
        PLAYER_HEAD_A,
        player_torso_arms("down", "down"),
        player_legs("stand"),
    )
    f1 = assemble_player(
        PLAYER_HEAD_B,
        player_torso_arms("down", "down"),
        player_legs("stand"),
    )
    save(hcat([f0, f1]), out_dir / "idle.png")

    # Walk: 4 frames
    w0 = assemble_player(PLAYER_HEAD_A, player_torso_arms("mid", "down"), player_legs("step_l"))
    w1 = assemble_player(PLAYER_HEAD_A, player_torso_arms("down", "down"), player_legs("stand"))
    w2 = assemble_player(PLAYER_HEAD_A, player_torso_arms("down", "mid"), player_legs("step_r"))
    w3 = assemble_player(PLAYER_HEAD_A, player_torso_arms("down", "down"), player_legs("stand"))
    save(hcat([w0, w1, w2, w3]), out_dir / "walk.png")

    # Jump: 2 frames (rising / falling)
    j0 = assemble_player(PLAYER_HEAD_A, player_torso_arms("up", "up"), player_legs("jump"))
    j1 = assemble_player(PLAYER_HEAD_A, player_torso_arms("mid", "mid"), player_legs("jump"))
    save(hcat([j0, j1]), out_dir / "jump.png")

    # Attack: 3 frames (windup, swing, recover). Right arm extends with a weapon.
    # For the swing frame we extend the canvas an extra 8 pixels to fit the staff.
    a0 = assemble_player(PLAYER_HEAD_A, player_torso_arms("up", "down"), player_legs("stand"))
    a1_base = assemble_player(PLAYER_HEAD_A, player_torso_arms("down", "punch"), player_legs("stand"))
    # add a staff/sword extending to the right
    a1 = Image.new("RGBA", (16, 24), TR)
    a1.paste(a1_base, (0, 0), a1_base)
    # weapon: rows 12-13, cols 14-15 already painted dark; extend a little glow
    for y in range(12, 14):
        for x in range(14, 16):
            a1.putpixel((x, y), GB[3])
    a2 = assemble_player(PLAYER_HEAD_A, player_torso_arms("down", "mid"), player_legs("stand"))
    save(hcat([a0, a1, a2]), out_dir / "attack.png")

    # Climb: 2 frames (back-facing-ish, arms up alternately)
    c_head = [
        "................",
        ".....333333.....",
        "....32222223....",
        "....32222223....",  # back of head, no eyes
        "....32222223....",
        "....32222223....",
        "....32222223....",
        ".....322223.....",
        "......3333......",
        "................",
    ]
    c0 = img_from_rows(
        c_head
        + player_torso_arms("up", "down")
        + player_legs("step_l")
    )
    c1 = img_from_rows(
        c_head
        + player_torso_arms("down", "up")
        + player_legs("step_r")
    )
    save(hcat([c0, c1]), out_dir / "climb.png")

    # Hurt: 1 frame (knocked back)
    h0 = assemble_player(PLAYER_HEAD_A, player_torso_arms("up", "up"), player_legs("crouch"))
    save(h0, out_dir / "hurt.png")


# --------------------------------------------------------------------------
# Enemies
# --------------------------------------------------------------------------

def gen_foot_soldier():
    """16x16, 2-frame walk. Hooded grunt with a small dagger."""
    f0 = img_from_rows([
        "................",
        ".....3333.......",
        "....322223......",
        "....311123......",  # eye band
        "....322223......",
        "....333333......",
        "...32222233.....",
        "...32222203.....",  # belt buckle (light)
        "...32222223.....",
        "...32222223.....",
        "....32222233....",
        "....32232223....",
        "....3323.323....",
        "....333..323....",
        "....33...33.....",
        "................",
    ])
    f1 = img_from_rows([
        "................",
        ".....3333.......",
        "....322223......",
        "....311123......",
        "....322223......",
        "....333333......",
        "...32222233.....",
        "...32222203.....",
        "...32222223.....",
        "...32222223.....",
        "....32222233....",
        "....32232223....",
        "....3.32323.....",
        "....3..32323....",
        "....33..3.33....",
        "................",
    ])
    save(hcat([f0, f1]), SPR / "enemies" / "foot_soldier.png")


def gen_shuriken_thrower():
    """16x16, 2-frame idle. Robed thrower."""
    f0 = img_from_rows([
        "................",
        "....333333......",
        "...32222223.....",
        "...31133113.....",  # mask eyes
        "...32222223.....",
        "....333333......",
        "...3322223......",
        "..32222223......",
        "..32222223......",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "...3322223......",
        "...33...33......",
    ])
    f1 = img_from_rows([
        "................",
        "....333333......",
        "...32222223.....",
        "...31133113.....",
        "...32222223.....",
        "....333333......",
        "...3322223......",
        "..32222223......",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "...32222223.....",
        "....3322223.....",
        "....33...33.....",
    ])
    save(hcat([f0, f1]), SPR / "enemies" / "shuriken_thrower.png")


def gen_boss():
    """24x32, 2-frame menacing idle. Burly armored brute."""
    rows_a = [
        "........................",
        "........................",
        ".......333333333........",
        "......32222222223.......",
        "......32222222223.......",
        "......31133113323.......",  # eyes
        "......32222222223.......",
        "......32232232223.......",  # snarl
        "......32222222223.......",
        ".......3333333333.......",
        ".....333222222333.......",  # spiky shoulders
        "....3322222222233.......",
        "....32222222222223......",
        "....32222002222223......",  # chest spike (light)
        "....32222002222223......",
        "....32222222222223......",
        "....32222222222223......",
        "....32222222222223......",
        "....32222222222223......",
        "....32222222222223......",
        "....33222222222233......",
        ".....332222222233.......",
        "......333222233.........",
        "......32222223..........",
        "......322..223..........",
        "......322..223..........",
        "......332..233..........",
        ".....3333..3333.........",
        "................",
        "................",
        "................",
        "................",
    ]
    # pad to 24 wide explicitly (some lines short above)
    rows_a = [r.ljust(24, ".")[:24] for r in rows_a]
    f0 = img_from_rows(rows_a)
    # frame B: shift weight 1 px (legs apart)
    rows_b = list(rows_a)
    rows_b[24] = ".....3322..2233........."
    rows_b[25] = ".....3222..2223........."
    rows_b[26] = ".....3322..2233........."
    rows_b[27] = "....3333....3333........"
    f1 = img_from_rows(rows_b)
    save(hcat([f0, f1]), SPR / "enemies" / "boss.png")


def gen_projectile():
    """8x8 spinning shuriken, 2 frames."""
    a = img_from_rows([
        "...33...",
        "..3223..",
        ".322223.",
        "33233233",
        ".322223.",
        "..3223..",
        "...33...",
        "........",
    ])
    b = img_from_rows([
        "...3....",
        ".3.3.3..",
        "..323...",
        "3332333.",
        "..323...",
        ".3.3.3..",
        "....3...",
        "........",
    ])
    save(hcat([a, b]), SPR / "projectiles" / "shuriken.png")


# --------------------------------------------------------------------------
# Tileset (16x16 tiles in a horizontal strip)
# --------------------------------------------------------------------------

def tile_brick_solid():
    return img_from_rows([
        "3333333333333333",
        "3221222122212223",
        "3221222122212223",
        "3221222122212223",
        "3333333333333333",
        "2122212221222122",
        "2122212221222122",
        "2122212221222122",
        "3333333333333333",
        "3221222122212223",
        "3221222122212223",
        "3221222122212223",
        "3333333333333333",
        "2122212221222122",
        "2122212221222122",
        "3333333333333333",
    ])


def tile_concrete():
    return img_from_rows([
        "3333333333333333",
        "3211111111111123",
        "3211211211211123",
        "3212112112112123",
        "3211111111111123",
        "3211211211211123",
        "3212112112112123",
        "3211111111111123",
        "3211111111111123",
        "3211211211211123",
        "3212112112112123",
        "3211111111111123",
        "3211211211211123",
        "3212112112112123",
        "3211111111111123",
        "3333333333333333",
    ])


def tile_rooftop():
    return img_from_rows([
        "3333333333333333",
        "2333333333333332",
        "2222222222222222",
        "2122212221222122",
        "2222222222222222",
        "2122212221222122",
        "2222222222222222",
        "3333333333333333",
        "3221122112211223",
        "3221122112211223",
        "3221122112211223",
        "3333333333333333",
        "2122212221222122",
        "2122212221222122",
        "2122212221222122",
        "3333333333333333",
    ])


def tile_brick_bg():
    """Background brick pattern, lighter (no top edge)."""
    return img_from_rows([
        "1212121212121212",
        "1100110011001100",
        "1212121212121212",
        "1100110011001100",
        "1212121212121212",
        "0011001100110011",
        "1212121212121212",
        "0011001100110011",
        "1212121212121212",
        "1100110011001100",
        "1212121212121212",
        "1100110011001100",
        "1212121212121212",
        "0011001100110011",
        "1212121212121212",
        "0011001100110011",
    ])


def tile_window():
    return img_from_rows([
        "1111111111111111",
        "1222222222222221",
        "1233333223333321",
        "1233333223333321",
        "1233333223333321",
        "1233333223333321",
        "1233333223333321",
        "1222222222222221",
        "1222222222222221",
        "1233333223333321",
        "1233333223333321",
        "1233333223333321",
        "1233333223333321",
        "1233333223333321",
        "1222222222222221",
        "1111111111111111",
    ])


def tile_door():
    return img_from_rows([
        "1111111111111111",
        "1333333333333331",
        "1322222222222231",
        "1322222222222231",
        "1322222222222231",
        "1322222222222231",
        "1322222222222231",
        "1322222222222231",
        "1322222222222231",
        "1322222002222231",
        "1322222002222231",
        "1322222002222231",
        "1322222222222231",
        "1322222222222231",
        "1322222222222231",
        "1333333333333331",
    ])


def tile_sky_diamond():
    """Background sky pattern (light)."""
    return img_from_rows([
        "0000000000000000",
        "0000000110000000",
        "0000001111000000",
        "0000011001100000",
        "0000110000110000",
        "0001100000011000",
        "0011000000001100",
        "0110000000000110",
        "0011000000001100",
        "0001100000011000",
        "0000110000110000",
        "0000011001100000",
        "0000001111000000",
        "0000000110000000",
        "0000000000000000",
        "0000000000000000",
    ])


def tile_sky_zigzag():
    return img_from_rows([
        "0000000000000000",
        "0000000000000000",
        "1100110011001100",
        "0011001100110011",
        "0000000000000000",
        "0000000000000000",
        "0011001100110011",
        "1100110011001100",
        "0000000000000000",
        "0000000000000000",
        "1100110011001100",
        "0011001100110011",
        "0000000000000000",
        "0000000000000000",
        "0011001100110011",
        "1100110011001100",
    ])


def tile_ladder():
    return img_from_rows([
        "....3333....3333",
        "....3223....3223",
        "....3223....3223",
        "....3223....3223",
        "3333322333333223",
        "3223322333333223",
        "....3223....3223",
        "....3223....3223",
        "....3223....3223",
        "3333322333333223",
        "3223322333333223",
        "....3223....3223",
        "....3223....3223",
        "....3223....3223",
        "3333322333333223",
        "3223322333333223",
    ])


def tile_question_block():
    return img_from_rows([
        "3333333333333333",
        "3000000000000003",
        "3022222222222203",
        "3022233333322203",
        "3022232222322203",
        "3022232023322203",
        "3022222203322203",
        "3022222203322203",
        "3022222203322203",
        "3022222033322203",
        "3022222203322203",
        "3022222033322203",
        "3022222033322203",
        "3022222222222203",
        "3000000000000003",
        "3333333333333333",
    ])


def tile_spike():
    return img_from_rows([
        "................",
        "................",
        "................",
        "...3...3...3....",
        "..323.323.323...",
        "..323.323.323...",
        ".32323323323323.",
        "32232323232323.3",
        "32222222222222.3",
        "32222222222222.3",
        "33333333333333.3",
        "................",
        "................",
        "................",
        "................",
        "................",
    ])


def gen_tileset():
    tiles = [
        tile_brick_solid(),       # 0
        tile_concrete(),          # 1
        tile_rooftop(),           # 2
        tile_brick_bg(),          # 3
        tile_window(),            # 4
        tile_door(),              # 5
        tile_sky_diamond(),       # 6
        tile_sky_zigzag(),        # 7
        tile_ladder(),            # 8
        tile_question_block(),    # 9
        tile_spike(),             # 10
    ]
    save(hcat(tiles), SPR / "tiles" / "tileset.png")
    # Also export the interactive tiles as standalone images for scene instances.
    save(tile_ladder(), SPR / "items" / "ladder.png")
    save(tile_question_block(), SPR / "items" / "question.png")
    save(tile_spike(), SPR / "items" / "spike.png")


# --------------------------------------------------------------------------
# UI: heart, bar, logo
# --------------------------------------------------------------------------

def gen_ui():
    heart = img_from_rows([
        "................",
        "..33....33......",
        ".3223..3223.....",
        ".3222.32223.....",
        ".322232222.3....",
        ".32222222..3....",
        "..32222...3.....",
        "...322..3.......",
        "....3.3.........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]).crop((0, 0, 12, 10))
    save(heart, SPR / "ui" / "heart.png")

    heart_empty = img_from_rows([
        "................",
        "..33....33......",
        ".3003..3003.....",
        ".3000.30003.....",
        ".300030000.3....",
        ".30000000..3....",
        "..30000...3.....",
        "...300..3.......",
        "....3.3.........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]).crop((0, 0, 12, 10))
    save(heart_empty, SPR / "ui" / "heart_empty.png")

    # Title-screen face: 80x80 circular character portrait
    portrait = Image.new("RGBA", (80, 80), TR)
    cx, cy = 40, 40
    for y in range(80):
        for x in range(80):
            d = math.hypot(x - cx, y - cy)
            if d > 38:
                continue
            if d > 36:
                portrait.putpixel((x, y), GB[3])
            elif d > 33:
                portrait.putpixel((x, y), GB[2])
            else:
                portrait.putpixel((x, y), GB[1])
    # Eyes (mask band)
    for x in range(14, 66):
        portrait.putpixel((x, 32), GB[3])
        portrait.putpixel((x, 38), GB[3])
    for y in range(33, 38):
        for x in range(14, 66):
            portrait.putpixel((x, y), GB[2])
    # Eye whites
    for x in range(22, 30):
        for y in range(34, 37):
            portrait.putpixel((x, y), GB[0])
    for x in range(50, 58):
        for y in range(34, 37):
            portrait.putpixel((x, y), GB[0])
    # Pupils
    for x in range(25, 27):
        for y in range(34, 37):
            portrait.putpixel((x, y), GB[3])
    for x in range(53, 55):
        for y in range(34, 37):
            portrait.putpixel((x, y), GB[3])
    # Smile
    for x in range(30, 50):
        portrait.putpixel((x, 56), GB[3])
    for x in range(32, 48):
        portrait.putpixel((x, 57), GB[3])
    portrait.putpixel((30, 55), GB[3])
    portrait.putpixel((49, 55), GB[3])
    save(portrait, SPR / "ui" / "portrait.png")


# --------------------------------------------------------------------------
# Audio: simple chiptune-style square-wave .wav files
# --------------------------------------------------------------------------

SAMPLE_RATE = 22050


def write_wav(path, samples):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        clipped = []
        for s in samples:
            v = max(-1.0, min(1.0, s))
            clipped.append(int(v * 32767))
        w.writeframes(struct.pack("<" + "h" * len(clipped), *clipped))
    print(f"  wrote {path.relative_to(ROOT)}  {len(samples)/SAMPLE_RATE:.2f}s")


def square_tone(freq, dur, vol=0.3, attack=0.005, release=0.05):
    n = int(SAMPLE_RATE * dur)
    out = []
    for i in range(n):
        t = i / SAMPLE_RATE
        s = 1.0 if (t * freq) % 1.0 < 0.5 else -1.0
        # simple ADSR: attack/release envelope
        env = 1.0
        if t < attack:
            env = t / attack
        elif t > dur - release:
            env = max(0.0, (dur - t) / release)
        out.append(s * vol * env)
    return out


def slide(f0, f1, dur, vol=0.3):
    n = int(SAMPLE_RATE * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SAMPLE_RATE
        f = f0 + (f1 - f0) * (t / dur)
        phase += f / SAMPLE_RATE
        s = 1.0 if phase % 1.0 < 0.5 else -1.0
        env = 1.0
        if t < 0.005:
            env = t / 0.005
        elif t > dur - 0.05:
            env = max(0.0, (dur - t) / 0.05)
        out.append(s * vol * env)
    return out


def noise_burst(dur, vol=0.3, decay=True):
    import random
    rng = random.Random(0xC0FFEE)
    n = int(SAMPLE_RATE * dur)
    out = []
    for i in range(n):
        t = i / SAMPLE_RATE
        s = rng.uniform(-1.0, 1.0)
        env = 1.0
        if decay:
            env = max(0.0, 1.0 - t / dur)
        out.append(s * vol * env)
    return out


# --------------------------------------------------------------------------
# 5x7 bitmap font (used to bake static title/UI text strings as PNGs).
# --------------------------------------------------------------------------

FONT_5x7 = {
    ' ': ('.....', '.....', '.....', '.....', '.....', '.....', '.....'),
    'A': ('.111.', '1...1', '1...1', '11111', '1...1', '1...1', '1...1'),
    'B': ('1111.', '1...1', '1...1', '1111.', '1...1', '1...1', '1111.'),
    'C': ('.1111', '1....', '1....', '1....', '1....', '1....', '.1111'),
    'D': ('1111.', '1...1', '1...1', '1...1', '1...1', '1...1', '1111.'),
    'E': ('11111', '1....', '1....', '1111.', '1....', '1....', '11111'),
    'F': ('11111', '1....', '1....', '1111.', '1....', '1....', '1....'),
    'G': ('.1111', '1....', '1....', '1.111', '1...1', '1...1', '.111.'),
    'H': ('1...1', '1...1', '1...1', '11111', '1...1', '1...1', '1...1'),
    'I': ('11111', '..1..', '..1..', '..1..', '..1..', '..1..', '11111'),
    'J': ('11111', '....1', '....1', '....1', '....1', '1...1', '.111.'),
    'K': ('1...1', '1..1.', '1.1..', '11...', '1.1..', '1..1.', '1...1'),
    'L': ('1....', '1....', '1....', '1....', '1....', '1....', '11111'),
    'M': ('1...1', '11.11', '1.1.1', '1.1.1', '1...1', '1...1', '1...1'),
    'N': ('1...1', '11..1', '11..1', '1.1.1', '1..11', '1..11', '1...1'),
    'O': ('.111.', '1...1', '1...1', '1...1', '1...1', '1...1', '.111.'),
    'P': ('1111.', '1...1', '1...1', '1111.', '1....', '1....', '1....'),
    'Q': ('.111.', '1...1', '1...1', '1...1', '1.1.1', '1..1.', '.11.1'),
    'R': ('1111.', '1...1', '1...1', '1111.', '1.1..', '1..1.', '1...1'),
    'S': ('.1111', '1....', '1....', '.111.', '....1', '....1', '1111.'),
    'T': ('11111', '..1..', '..1..', '..1..', '..1..', '..1..', '..1..'),
    'U': ('1...1', '1...1', '1...1', '1...1', '1...1', '1...1', '.111.'),
    'V': ('1...1', '1...1', '1...1', '1...1', '1...1', '.1.1.', '..1..'),
    'W': ('1...1', '1...1', '1...1', '1.1.1', '1.1.1', '11.11', '1...1'),
    'X': ('1...1', '1...1', '.1.1.', '..1..', '.1.1.', '1...1', '1...1'),
    'Y': ('1...1', '1...1', '.1.1.', '..1..', '..1..', '..1..', '..1..'),
    'Z': ('11111', '....1', '...1.', '..1..', '.1...', '1....', '11111'),
    '0': ('.111.', '1...1', '1..11', '1.1.1', '11..1', '1...1', '.111.'),
    '1': ('..1..', '.11..', '..1..', '..1..', '..1..', '..1..', '.111.'),
    '2': ('.111.', '1...1', '....1', '...1.', '..1..', '.1...', '11111'),
    '3': ('1111.', '....1', '....1', '.111.', '....1', '....1', '1111.'),
    '4': ('...1.', '..11.', '.1.1.', '1..1.', '11111', '...1.', '...1.'),
    '5': ('11111', '1....', '1111.', '....1', '....1', '1...1', '.111.'),
    '6': ('.111.', '1...1', '1....', '1111.', '1...1', '1...1', '.111.'),
    '7': ('11111', '....1', '...1.', '..1..', '.1...', '.1...', '.1...'),
    '8': ('.111.', '1...1', '1...1', '.111.', '1...1', '1...1', '.111.'),
    '9': ('.111.', '1...1', '1...1', '.1111', '....1', '1...1', '.111.'),
    '!': ('..1..', '..1..', '..1..', '..1..', '..1..', '.....', '..1..'),
    ':': ('.....', '..1..', '..1..', '.....', '..1..', '..1..', '.....'),
    "'": ('..1..', '..1..', '.....', '.....', '.....', '.....', '.....'),
    '.': ('.....', '.....', '.....', '.....', '.....', '..1..', '..1..'),
    ',': ('.....', '.....', '.....', '.....', '..1..', '..1..', '.1...'),
    '-': ('.....', '.....', '.....', '11111', '.....', '.....', '.....'),
    '?': ('.111.', '1...1', '....1', '...1.', '..1..', '.....', '..1..'),
    '/': ('....1', '....1', '...1.', '..1..', '.1...', '1....', '1....'),
    '@': ('.111.', '1...1', '1.111', '1.1.1', '1.111', '1....', '.111.'),
}


def render_text(text, color_idx=3, scale=1):
    text = text.upper()
    char_w = 5
    spacing = 1
    h = 7
    w = len(text) * (char_w + spacing) - spacing
    img = Image.new("RGBA", (w, h), TR)
    px = img.load()
    color = GB[color_idx]
    for i, ch in enumerate(text):
        glyph = FONT_5x7.get(ch, FONT_5x7[' '])
        for y, row in enumerate(glyph):
            for x, b in enumerate(row):
                if b == '1':
                    px[i * (char_w + spacing) + x, y] = color
    if scale > 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    return img


def gen_text_assets():
    out_ui = SPR / "ui"
    save(render_text("STEEL STREETS", scale=2), out_ui / "title_main.png")
    save(render_text("SEARCHING FOR", scale=1), out_ui / "title_sub_a.png")
    save(render_text("MASTER PLANK!", scale=1), out_ui / "title_sub_b.png")
    save(render_text("PRESS START", scale=1), out_ui / "press_start.png")
    save(render_text("PRESS ANY KEY", scale=1), out_ui / "press_any_key.png")
    save(render_text("GAME OVER", scale=2), out_ui / "game_over.png")
    save(render_text("LEVEL COMPLETE", scale=1), out_ui / "victory.png")
    save(render_text("BOSS", scale=1), out_ui / "boss_label.png")
    save(render_text("STEEL STREETS!", scale=1), out_ui / "splash_caption.png")


def gen_audio():
    write_wav(AUD / "jump.wav", slide(440, 880, 0.18))
    write_wav(AUD / "attack.wav", noise_burst(0.08, 0.25))
    write_wav(AUD / "hit.wav", slide(880, 220, 0.12, vol=0.35))
    write_wav(AUD / "hurt.wav", slide(330, 110, 0.30, vol=0.4))
    write_wav(AUD / "pickup.wav", square_tone(1320, 0.06) + square_tone(1760, 0.10))
    # Boss-defeat fanfare
    fanfare = []
    for f in [523, 659, 784, 1047]:
        fanfare += square_tone(f, 0.15, vol=0.35)
    fanfare += square_tone(1047, 0.4, vol=0.35)
    write_wav(AUD / "victory.wav", fanfare)
    # Game over
    write_wav(
        AUD / "game_over.wav",
        slide(440, 220, 0.25, vol=0.35) + slide(220, 110, 0.40, vol=0.35),
    )
    # Simple looping background music: 4-bar arpeggio in C major
    bgm = []
    notes = [262, 330, 392, 523, 392, 330, 262, 196] * 2
    for f in notes:
        bgm += square_tone(f, 0.18, vol=0.18, attack=0.002, release=0.02)
    write_wav(AUD / "bgm.wav", bgm)


def main():
    print("Generating sprites...")
    gen_player()
    gen_foot_soldier()
    gen_shuriken_thrower()
    gen_boss()
    gen_projectile()
    gen_tileset()
    gen_ui()
    print("Generating text assets...")
    gen_text_assets()
    print("Generating audio...")
    gen_audio()
    print("Done.")


if __name__ == "__main__":
    main()
