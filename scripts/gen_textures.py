"""Generates placeholder block textures as a horizontal strip PNG
(assets/textures/atlas.png) — one 16x16 tile per block texture, loaded into a
GL texture array at runtime. Layer order must match blocks::RegisterDefaults().
Pure stdlib (zlib/struct); no Pillow needed."""

import struct
import zlib
from pathlib import Path

TILE = 16

STONE = (125, 125, 125)
DIRT = (134, 96, 67)
GRASS = (106, 170, 64)


def hash01(x: int, y: int, salt: int) -> float:
    h = (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
    h = ((h ^ (h >> 13)) * 0x5BD1E995) & 0xFFFFFFFF
    return ((h >> 8) & 0xFF) / 255.0


def speckle(base, x, y, salt, amount=14):
    d = int((hash01(x, y, salt) - 0.5) * 2 * amount)
    return tuple(min(255, max(0, c + d)) for c in base)


def stone(x, y):
    return speckle(STONE, x, y, 0)


def dirt(x, y):
    return speckle(DIRT, x, y, 1)


def grass_side(x, y):
    # Dirt with a ragged grass fringe along the top of the tile (y = 0 is the
    # top of the image; the loader flips tiles for GL's bottom-left origin).
    fringe = 2 + int(hash01(x, 0, 2) * 3)
    if y < fringe:
        return speckle(GRASS, x, y, 2, 12)
    return speckle(DIRT, x, y, 1)


def grass_top(x, y):
    return speckle(GRASS, x, y, 3, 12)


GLOWSTONE = (228, 188, 102)


def glowstone(x, y):
    # Bright mottled tile with hot spots, like packed glowing dust.
    if hash01(x // 2, y // 2, 5) > 0.72:
        return speckle((255, 236, 170), x, y, 4, 10)
    return speckle(GLOWSTONE, x, y, 4, 18)


SAND = (219, 207, 163)


def sand(x, y):
    return speckle(SAND, x, y, 6, 10)


LOG = (104, 82, 50)
LOG_DARK = (82, 64, 38)
LOG_RING = (151, 124, 81)


def log_side(x, y):
    # Vertical bark striations: stripe choice keyed on x only.
    if hash01(x, 0, 7) > 0.6:
        return speckle(LOG_DARK, x, y, 7, 8)
    return speckle(LOG, x, y, 7, 10)


def log_top(x, y):
    # Square growth rings inside a dark bark rim.
    ring = max(abs(2 * x - 15), abs(2 * y - 15)) // 2  # 0..7 from center
    if ring >= 7:
        return speckle(LOG_DARK, x, y, 8, 8)
    return speckle(LOG_RING if ring % 2 == 0 else LOG, x, y, 8, 6)


LEAF = (58, 118, 41)


def leaves(x, y):
    # Mottled foliage with cutout holes (alpha 0 — the chunk shader
    # alpha-tests them away). Keep coverage high (~80%) so box-filtered
    # mips stay above the 0.5 cutoff and distant leaves fill in solid.
    if hash01(x, y, 9) > 0.8:
        return (0, 0, 0, 0)
    return speckle(LEAF, x, y, 9, 14)


WATER = (52, 96, 200)


def water(x, y):
    # Semi-transparent blue with faint wave streaks (alpha keyed per tile;
    # opaque tiles get alpha 255 in write_png).
    base = (70, 116, 216) if hash01(0, y, 10) > 0.7 else WATER
    return speckle(base, x, y, 10, 8) + (168,)


SNOW = (240, 244, 248)


def snow(x, y):
    return speckle(SNOW, x, y, 11, 6)


def grass_side_snowed(x, y):
    # Dirt with a ragged snow cap (the snowy twin of grass_side).
    fringe = 2 + int(hash01(x, 0, 12) * 3)
    if y < fringe:
        return speckle(SNOW, x, y, 12, 5)
    return speckle(DIRT, x, y, 1)


def tall_grass(x, y):
    # Upright blades on a transparent background: each column hash-picks a
    # blade height; most columns are empty.
    if hash01(x, 0, 13) > 0.55:
        return (0, 0, 0, 0)
    blade = 6 + int(hash01(x, 1, 13) * 9)  # blade tops 6..14 px tall
    if y < TILE - blade:
        return (0, 0, 0, 0)
    return speckle(GRASS, x, y, 13, 16)


def flower(x, y, salt, petal):
    # Simple sprite: a 4x4 petal head up top, a 2px stem to the ground.
    if 5 <= x <= 9 and 2 <= y <= 6 and abs(x - 7) + abs(y - 4) <= 3:
        return speckle(petal, x, y, salt, 12)
    if 7 <= x <= 8 and 6 < y < TILE:
        return speckle((62, 140, 42), x, y, salt, 10)
    return (0, 0, 0, 0)


def dandelion(x, y):
    return flower(x, y, 14, (236, 220, 64))


def poppy(x, y):
    return flower(x, y, 15, (212, 56, 40))


DEADBUSH = (124, 94, 50)


def dead_bush(x, y):
    # Dry twigs: a short trunk fanning into hash-gated diagonal branches.
    if 7 <= x <= 8 and y >= 11:
        return speckle(DEADBUSH, x, y, 16, 14)
    d = abs(x - 7) + abs(x - 8)  # distance from the 2px center
    rise = TILE - 1 - y
    if 3 <= y <= 12 and abs(rise - d) <= 1 and hash01(x, 0, 16) > 0.35:
        return speckle(DEADBUSH, x, y, 16, 14)
    return (0, 0, 0, 0)


BIRCH = (222, 219, 209)
BIRCH_DARK = (66, 64, 58)


def birch_side(x, y):
    # Pale bark with scattered dark eye patches.
    if hash01(x // 3, y // 3, 17) > 0.82:
        return speckle(BIRCH_DARK, x, y, 17, 8)
    return speckle(BIRCH, x, y, 17, 8)


def birch_top(x, y):
    ring = max(abs(2 * x - 15), abs(2 * y - 15)) // 2
    if ring >= 7:
        return speckle(BIRCH_DARK, x, y, 18, 8)
    return speckle(BIRCH if ring % 2 == 0 else (196, 188, 168), x, y, 18, 6)


BIRCH_LEAF = (116, 168, 88)


def birch_leaves(x, y):
    if hash01(x, y, 19) > 0.8:
        return (0, 0, 0, 0)
    return speckle(BIRCH_LEAF, x, y, 19, 14)


SPRUCE = (74, 56, 34)
SPRUCE_DARK = (52, 40, 24)


def spruce_side(x, y):
    if hash01(x, 0, 20) > 0.6:
        return speckle(SPRUCE_DARK, x, y, 20, 8)
    return speckle(SPRUCE, x, y, 20, 10)


def spruce_top(x, y):
    ring = max(abs(2 * x - 15), abs(2 * y - 15)) // 2
    if ring >= 7:
        return speckle(SPRUCE_DARK, x, y, 21, 8)
    return speckle((118, 92, 58) if ring % 2 == 0 else SPRUCE, x, y, 21, 6)


SPRUCE_LEAF = (46, 88, 52)


def spruce_leaves(x, y):
    if hash01(x, y, 22) > 0.84:
        return (0, 0, 0, 0)
    return speckle(SPRUCE_LEAF, x, y, 22, 12)


CACTUS = (62, 130, 64)


def cactus_side(x, y):
    # Vertical ribs with pale spine dots; fully opaque (full-cube cactus).
    if x % 4 == 1 and y % 4 == 2:
        return speckle((196, 214, 160), x, y, 23, 6)
    if x % 4 == 3:
        return speckle((48, 104, 50), x, y, 23, 8)
    return speckle(CACTUS, x, y, 23, 10)


def cactus_top(x, y):
    ring = max(abs(2 * x - 15), abs(2 * y - 15)) // 2
    if ring >= 7:
        return speckle((48, 104, 50), x, y, 24, 8)
    return speckle(CACTUS, x, y, 24, 8)


# Layer index in the texture array == position in this list.
TILES = [stone, dirt, grass_side, grass_top, glowstone, sand, log_side, log_top, leaves, water,
         snow, grass_side_snowed, tall_grass, dandelion, poppy, dead_bush,
         birch_side, birch_top, birch_leaves, spruce_side, spruce_top, spruce_leaves,
         cactus_side, cactus_top]


def png_chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data))
    )


def write_png(path: Path, width: int, height: int, pixel) -> None:
    rows = []
    for y in range(height):
        row = bytearray(b"\x00")  # filter type: none
        for x in range(width):
            px = pixel(x, y)
            row.extend(px if len(px) == 4 else (*px, 255))
        rows.append(bytes(row))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
        + png_chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
    print(f"wrote {path} ({len(png)} bytes)")


def main() -> None:
    textures = Path(__file__).resolve().parent.parent / "assets" / "textures"
    write_png(
        textures / "atlas.png",
        TILE * len(TILES),
        TILE,
        lambda x, y: TILES[x // TILE](x % TILE, y),
    )


if __name__ == "__main__":
    main()
