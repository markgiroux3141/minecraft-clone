"""Imports real Minecraft 1.12 textures from the local MCP source tree into
assets/mc/ (gitignored — personal use only, zero distribution). The game
prefers assets/mc/<path> over assets/<path> when it exists, so the committed
placeholder assets keep working on a clean clone.

Block atlas: writes assets/mc/textures/atlas.png as a horizontal strip of
16x16 tiles in the layer order of blocks::RegisterDefaults() (same contract
as scripts/gen_textures.py). Biome tints (grass/foliage colormaps) are baked
at import using the plains climate point; leaves are baked over an opaque
backdrop because the game renders them opaque (cutout pass not built yet).

Requires Pillow (same as gen_font.py). Usage:
    python scripts/import_mc_assets.py [path-to-minecraft-assets-root]
"""

import shutil
import sys
from pathlib import Path

from PIL import Image

DEFAULT_MC_ASSETS = Path(r"D:\Minecraft source code\mcp940\src\minecraft\assets\minecraft")
TILE = 16

# Plains climate point (temperature 0.8, rainfall 0.4) -> colormap pixel.
PLAINS_X = int((1.0 - 0.8) * 255)
PLAINS_Y = int((1.0 - 0.4 * 0.8) * 255)

# Straight copies: (source under the MC assets root, destination under
# assets/mc/). The font is the real 8x8-cell 16x16 ASCII grid; the GUI
# sheets hold the crosshair (icons) and hotbar/buttons (widgets).
COPIES = [
    ("textures/font/ascii.png", "fonts/ascii.png"),
    ("textures/gui/icons.png", "textures/gui/icons.png"),
    ("textures/gui/widgets.png", "textures/gui/widgets.png"),
    # Celestial sheets: opaque with black backgrounds, drawn additively.
    ("textures/environment/sun.png", "textures/environment/sun.png"),
    ("textures/environment/moon_phases.png", "textures/environment/moon_phases.png"),
]


def load_tile(path: Path) -> Image.Image:
    img = Image.open(path).convert("RGBA")
    if img.size != (TILE, TILE):
        raise SystemExit(f"{path}: expected {TILE}x{TILE}, got {img.size}")
    return img


def colormap_tint(path: Path) -> tuple[int, int, int]:
    r, g, b, _ = Image.open(path).convert("RGBA").getpixel((PLAINS_X, PLAINS_Y))
    return (r, g, b)


def tinted(img: Image.Image, tint: tuple[int, int, int]) -> Image.Image:
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            px[x, y] = (r * tint[0] // 255, g * tint[1] // 255, b * tint[2] // 255, a)
    return out


def build_atlas(mc: Path, out_path: Path) -> None:
    blocks = mc / "textures" / "blocks"
    grass_tint = colormap_tint(mc / "textures" / "colormap" / "grass.png")
    foliage_tint = colormap_tint(mc / "textures" / "colormap" / "foliage.png")

    # Grass side: base texture + biome-tinted overlay strands on top.
    grass_side = load_tile(blocks / "grass_side.png")
    overlay = tinted(load_tile(blocks / "grass_side_overlay.png"), grass_tint)
    grass_side.alpha_composite(overlay)

    # Leaves render opaque in-game, so bake the cutout holes onto a dark
    # backdrop (what vanilla's "fast graphics" leaves look like).
    leaves = tinted(load_tile(blocks / "leaves_oak.png"), foliage_tint)
    backdrop = Image.new(
        "RGBA", (TILE, TILE),
        (foliage_tint[0] * 2 // 5, foliage_tint[1] * 2 // 5, foliage_tint[2] * 2 // 5, 255))
    backdrop.alpha_composite(leaves)
    leaves = backdrop

    # Water: frame 0 of the 16x512 animation strip (already tinted + alpha).
    water = Image.open(blocks / "water_still.png").convert("RGBA").crop((0, 0, TILE, TILE))

    # Layer index in the texture array == position in this list — must match
    # blocks::RegisterDefaults() (and gen_textures.py's TILES).
    tiles = [
        load_tile(blocks / "stone.png"),
        load_tile(blocks / "dirt.png"),
        grass_side,
        tinted(load_tile(blocks / "grass_top.png"), grass_tint),
        load_tile(blocks / "glowstone.png"),
        load_tile(blocks / "sand.png"),
        load_tile(blocks / "log_oak.png"),
        load_tile(blocks / "log_oak_top.png"),
        leaves,
        water,
        load_tile(blocks / "snow.png"),
        load_tile(blocks / "grass_side_snowed.png"),
    ]

    strip = Image.new("RGBA", (TILE * len(tiles), TILE))
    for i, tile in enumerate(tiles):
        strip.paste(tile, (i * TILE, 0))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    strip.save(out_path)
    print(f"wrote {out_path} ({len(tiles)} layers, "
          f"grass tint {grass_tint}, foliage tint {foliage_tint})")


def main() -> None:
    mc = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_MC_ASSETS
    if not (mc / "textures" / "blocks").is_dir():
        raise SystemExit(f"Minecraft assets not found at {mc}")
    assets = Path(__file__).resolve().parent.parent / "assets"
    build_atlas(mc, assets / "mc" / "textures" / "atlas.png")
    for src, dst in COPIES:
        target = assets / "mc" / dst
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(mc / src, target)
        print(f"copied {src} -> {target}")


if __name__ == "__main__":
    main()
