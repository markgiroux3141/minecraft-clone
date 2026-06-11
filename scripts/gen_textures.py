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


# Layer index in the texture array == position in this list.
TILES = [stone, dirt, grass_side, grass_top]


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
            row.extend(pixel(x, y))
        rows.append(bytes(row))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
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
