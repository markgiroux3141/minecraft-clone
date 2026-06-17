"""Imports real Minecraft 1.12 textures from the local MCP source tree into
assets/mc/ (gitignored — personal use only, zero distribution). The game
prefers assets/mc/<path> over assets/<path> when it exists, so the committed
placeholder assets keep working on a clean clone.

Block atlas: writes assets/mc/textures/atlas.png as a horizontal strip of
16x16 tiles in the layer order of blocks::RegisterDefaults() (same contract
as scripts/gen_textures.py). Biome tints (grass/foliage colormaps) are baked
at import using the plains climate point; leaf alpha holes are preserved
(the game alpha-tests them — cutout pass, M16).

Requires Pillow (same as gen_font.py). Usage:
    python scripts/import_mc_assets.py [path-to-minecraft-assets-root]
"""

import json
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
    # Survival inventory panel (M17): 176x166 at (0,0) in a 256x256 sheet.
    ("textures/gui/container/inventory.png", "textures/gui/container/inventory.png"),
    # Crafting table panel (M19): same layout family, 3x3 grid + result.
    ("textures/gui/container/crafting_table.png",
     "textures/gui/container/crafting_table.png"),
    # Furnace panel (M21): flame/arrow progress overlays sit right of the
    # panel art at x 176.
    ("textures/gui/container/furnace.png", "textures/gui/container/furnace.png"),
    # Celestial sheets: opaque with black backgrounds, drawn additively.
    ("textures/environment/sun.png", "textures/environment/sun.png"),
    ("textures/environment/moon_phases.png", "textures/environment/moon_phases.png"),
    # Steve skin (M20): the first-person bare arm samples it.
    ("textures/entity/steve.png", "textures/entity/steve.png"),
    # M32 mob skins: pig (64x32) and zombie (64x64, reuses the biped model).
    ("textures/entity/pig/pig.png", "textures/entity/pig/pig.png"),
    ("textures/entity/zombie/zombie.png", "textures/entity/zombie/zombie.png"),
    # Fire overlay (M30): the first-person "on fire" flames — vanilla's
    # ItemRenderer.renderFireInFirstPerson uses fire_layer_1. 16x512 vertical
    # animation strip (32 frames of 16x16); the HUD tiles a frame across the
    # bottom of the view.
    ("textures/blocks/fire_layer_1.png", "textures/fire_layer_1.png"),
    # M33 worn-armor model layers for the inventory player doll. layer_1 =
    # helmet/chest/arms, layer_2 = leggings/boots; leather ships a grayscale
    # base + an untinted overlay (the doll tints the base like the icons).
    *[(f"textures/models/armor/{m}_layer_{n}.png",
       f"textures/models/armor/{m}_layer_{n}.png")
      for m in ("leather", "chainmail", "iron", "gold", "diamond") for n in (1, 2)],
    ("textures/models/armor/leather_layer_1_overlay.png",
     "textures/models/armor/leather_layer_1_overlay.png"),
    ("textures/models/armor/leather_layer_2_overlay.png",
     "textures/models/armor/leather_layer_2_overlay.png"),
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


def crack_cutout(img: Image.Image) -> Image.Image:
    # destroy_stage_N: white background at alpha ~1, gray crack pixels
    # opaque. The game alpha-tests (vanilla's alpha test) and draws the
    # survivors with the 2*src*dst Crumble blend (dark grays darken the
    # block, light grays highlight). Threshold to clean 0/255 alpha so
    # the cutout and its mips stay crisp.
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            px[x, y] = (r, g, b, 255) if a >= 140 else (0, 0, 0, 0)
    return out


def torch_flame_cap(path: Path) -> Image.Image:
    # The torch top cap: crop the flame core (vanilla template_torch's
    # up-face uv [7,6,9,8]) and scale it to a full opaque 16x16 tile, so
    # our cap quad samples it with a plain 0..1 UV.
    img = Image.open(path).convert("RGBA")
    cap = img.crop((7, 6, 9, 8)).resize((TILE, TILE), Image.NEAREST)
    cap.putalpha(255)
    return cap


def flattened(img: Image.Image) -> Image.Image:
    # Bake transparent pixels opaque over the tile's own body color — used
    # for the full-cube cactus, whose vanilla texture leaves the model's
    # 14/16 inset margin transparent (alpha-testing it on a full cube
    # would punch see-through slits).
    out = Image.new("RGBA", img.size, img.getpixel((img.width // 2, img.height // 2)))
    out.alpha_composite(img)
    out.putalpha(255)
    return out


# Vanilla's default un-dyed leather color (ItemArmor: 0xA06540), applied to
# the grayscale leather icons/layers before compositing the untinted overlay.
LEATHER_DEFAULT = (160, 101, 64)


def armor_icon(items: Path, material: str, slot: str) -> Image.Image:
    icon = load_tile(items / f"{material}_{slot}.png")
    if material == "leather":
        icon = tinted(icon, LEATHER_DEFAULT)
        icon.alpha_composite(load_tile(items / f"leather_{slot}_overlay.png"))
    return icon


def build_atlas(mc: Path, out_path: Path) -> None:
    blocks = mc / "textures" / "blocks"
    items = mc / "textures" / "items"
    grass_tint = colormap_tint(mc / "textures" / "colormap" / "grass.png")
    foliage_tint = colormap_tint(mc / "textures" / "colormap" / "foliage.png")

    # Grass side: base texture + biome-tinted overlay strands on top.
    grass_side = load_tile(blocks / "grass_side.png")
    overlay = tinted(load_tile(blocks / "grass_side_overlay.png"), grass_tint)
    grass_side.alpha_composite(overlay)

    # Cutout leaves (M16): alpha holes survive into the atlas — the chunk
    # shader alpha-tests them (vanilla "fancy graphics" look).
    leaves = tinted(load_tile(blocks / "leaves_oak.png"), foliage_tint)

    # Water: frame 0 of the 16x512 animation strip (already tinted + alpha).
    water = Image.open(blocks / "water_still.png").convert("RGBA").crop((0, 0, TILE, TILE))

    # Lava: frame 0 of lava_still's animation strip, forced opaque (vanilla
    # lava is a full opaque block; we render it in the liquid pass but want
    # alpha 255 so it doesn't read see-through). M26.
    lava = Image.open(blocks / "lava_still.png").convert("RGBA").crop((0, 0, TILE, TILE))
    lava.putalpha(255)

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
        # M16 plants: tallgrass ships grayscale — bake the grass tint, like
        # grass_top. Flowers and the dead bush are already colored.
        tinted(load_tile(blocks / "tallgrass.png"), grass_tint),
        load_tile(blocks / "flower_dandelion.png"),
        load_tile(blocks / "flower_rose.png"),
        load_tile(blocks / "deadbush.png"),
        # M16 tree species + cactus. Birch and spruce leaves use vanilla's
        # FIXED foliage constants (ColorizerFoliage), not the colormap.
        load_tile(blocks / "log_birch.png"),
        load_tile(blocks / "log_birch_top.png"),
        tinted(load_tile(blocks / "leaves_birch.png"), (128, 167, 85)),
        load_tile(blocks / "log_spruce.png"),
        load_tile(blocks / "log_spruce_top.png"),
        tinted(load_tile(blocks / "leaves_spruce.png"), (97, 153, 97)),
        flattened(load_tile(blocks / "cactus_side.png")),
        flattened(load_tile(blocks / "cactus_top.png")),
        # Sandstone: the vanilla buffer band under sand surfaces.
        load_tile(blocks / "sandstone_normal.png"),
        load_tile(blocks / "sandstone_top.png"),
        load_tile(blocks / "sandstone_bottom.png"),
        load_tile(blocks / "bedrock.png"),
        # M18: cobblestone (stone's mining drop), then the ten crack
        # stages (kFirstCrackTile in Block.h).
        load_tile(blocks / "cobblestone.png"),
        *[crack_cutout(load_tile(blocks / f"destroy_stage_{i}.png")) for i in range(10)],
        # M19 crafting: planks + crafting table (39..42), then the item
        # sprites (43..49 — tiles in Item.cpp's RegisterDefaults).
        load_tile(blocks / "planks_oak.png"),
        load_tile(blocks / "crafting_table_top.png"),
        load_tile(blocks / "crafting_table_side.png"),
        load_tile(blocks / "crafting_table_front.png"),
        load_tile(items / "stick.png"),
        load_tile(items / "wood_pickaxe.png"),
        load_tile(items / "wood_axe.png"),
        load_tile(items / "wood_shovel.png"),
        load_tile(items / "stone_pickaxe.png"),
        load_tile(items / "stone_axe.png"),
        load_tile(items / "stone_shovel.png"),
        # M21: ores (50/51), furnace (52..55), glass (56 — cutout like
        # leaves, interior alpha survives), then coal/iron item sprites.
        load_tile(blocks / "coal_ore.png"),
        load_tile(blocks / "iron_ore.png"),
        load_tile(blocks / "furnace_front_off.png"),
        load_tile(blocks / "furnace_front_on.png"),
        load_tile(blocks / "furnace_side.png"),
        load_tile(blocks / "furnace_top.png"),
        load_tile(blocks / "glass.png"),
        load_tile(items / "coal.png"),
        load_tile(items / "iron_ingot.png"),
        load_tile(items / "iron_pickaxe.png"),
        load_tile(items / "iron_axe.png"),
        load_tile(items / "iron_shovel.png"),
        # M21 follow-up: torch sides (62) — alpha survives, the mesher's
        # inset planes and the alpha test carve out the post.
        load_tile(blocks / "torch_on.png"),
        # Torch top cap (63): vanilla's template_torch maps the post's
        # upper face to texels (7,6)-(9,8) — the flame core. Crop that and
        # scale to a full opaque tile for our cap quad's 0..1 UV.
        torch_flame_cap(blocks / "torch_on.png"),
        # M26: lava still (64 — used for the source and every flow level,
        # like water) and obsidian (65).
        lava,
        load_tile(blocks / "obsidian.png"),
        # M26 buckets (66 empty / 67 water / 68 lava) — real item sprites.
        load_tile(items / "bucket_empty.png"),
        load_tile(items / "bucket_water.png"),
        load_tile(items / "bucket_lava.png"),
        # M32 mob drops (69 porkchop / 70 rotten flesh) — real item sprites.
        load_tile(items / "porkchop_raw.png"),
        load_tile(items / "rotten_flesh.png"),
        # M33 armor icons (71..90) — material-major (leather, chainmail, iron,
        # gold, diamond) x slot (helmet, chestplate, leggings, boots), matching
        # Item.cpp's RegisterDefaults. Leather is grayscale + a tinted overlay.
        *[armor_icon(items, mat, slot)
          for mat in ("leather", "chainmail", "iron", "gold", "diamond")
          for slot in ("helmet", "chestplate", "leggings", "boots")],
        # Empty-slot placeholders (91..94, Head/Chest/Legs/Feet) for the
        # inventory armor slots.
        *[load_tile(items / f"empty_armor_slot_{slot}.png")
          for slot in ("helmet", "chestplate", "leggings", "boots")],
    ]

    strip = Image.new("RGBA", (TILE * len(tiles), TILE))
    for i, tile in enumerate(tiles):
        strip.paste(tile, (i * TILE, 0))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    strip.save(out_path)
    print(f"wrote {out_path} ({len(tiles)} layers, "
          f"grass tint {grass_tint}, foliage tint {foliage_tint})")


# M22 audio. Vanilla sounds DO ship with the MCP source — not as loose .ogg
# files, but in the launcher-style HASHED object store at mcp940/jars/assets:
# indexes/<ver>.json maps a name ("minecraft/sounds/dig/stone1.ogg") to a
# hash, and the bytes live at objects/<hash[:2]>/<hash> with NO extension
# (which is why a plain *.ogg search turns up nothing). We resolve the 1.12
# index and copy the families the game uses into gitignored assets/mc/sounds/
# (zero distribution, same as the textures). Same source tree as the textures
# — no launcher install needed.
SOUND_INDEX = "1.12"


def sounds_store_for(mc: Path) -> Path:
    # mc = .../mcp940/src/minecraft/assets/minecraft  ->  .../mcp940/jars/assets
    return mc.parents[3] / "jars" / "assets"


def want_sound(rel: str) -> str | None:
    # rel = the index name minus "minecraft/sounds/". Returns the destination
    # path under assets/mc/sounds/, or None to skip. Music is flattened from
    # music/game/<name>.ogg to music/<name>.ogg (the game loads it flat).
    for family in ("dig/", "step/", "ambient/cave/", "liquid/", "fire/", "item/bucket/",
                   "damage/", "mob/pig/", "mob/zombie/"):
        if rel.startswith(family):
            return rel
    if rel in ("random/pop.ogg", "random/glass1.ogg", "random/glass2.ogg",
               "random/glass3.ogg"):
        return rel
    if rel.startswith("music/game/") and rel.count("/") == 2:
        return "music/" + rel.rsplit("/", 1)[1]
    return None


def import_textures(mc: Path, assets: Path) -> None:
    build_atlas(mc, assets / "mc" / "textures" / "atlas.png")
    for src, dst in COPIES:
        target = assets / "mc" / dst
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(mc / src, target)
        print(f"copied {src} -> {target}")
    # M33: the leather armor MODEL layers ship grayscale + a separate overlay
    # (like the icons), so the raw copies are colorless. Bake the tinted base +
    # overlay over them so the inventory doll shows brown leather.
    armor_out = assets / "mc" / "textures" / "models" / "armor"
    for n in (1, 2):
        base = tinted(Image.open(mc / f"textures/models/armor/leather_layer_{n}.png")
                      .convert("RGBA"), LEATHER_DEFAULT)
        base.alpha_composite(Image.open(mc / f"textures/models/armor/leather_layer_{n}_overlay.png")
                             .convert("RGBA"))
        base.save(armor_out / f"leather_layer_{n}.png")
        print(f"baked tinted leather_layer_{n}.png")


def import_sounds(store: Path, assets: Path) -> None:
    index = store / "indexes" / f"{SOUND_INDEX}.json"
    if not index.is_file():
        print(f"(skipping sounds: no index at {index})")
        return
    objects = json.loads(index.read_text())["objects"]
    out = assets / "mc" / "sounds"
    prefix = "minecraft/sounds/"
    count = 0
    for name, meta in objects.items():
        if not (name.startswith(prefix) and name.endswith(".ogg")):
            continue
        dst_rel = want_sound(name[len(prefix):])
        if dst_rel is None:
            continue
        h = meta["hash"]
        src = store / "objects" / h[:2] / h
        if not src.is_file():
            continue
        target = out / dst_rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, target)
        count += 1
    print(f"copied {count} sound files -> {out}")


def main() -> None:
    args = sys.argv[1:]
    sounds_store: Path | None = None
    if "--sounds-store" in args:
        i = args.index("--sounds-store")
        sounds_store = Path(args[i + 1])
        del args[i : i + 2]
    mc = Path(args[0]) if args else DEFAULT_MC_ASSETS

    assets = Path(__file__).resolve().parent.parent / "assets"

    # Each source is optional so the script still runs with only one present.
    if (mc / "textures" / "blocks").is_dir():
        import_textures(mc, assets)
    else:
        print(f"(skipping textures: not found at {mc})")

    store = sounds_store or sounds_store_for(mc)
    if store.is_dir():
        import_sounds(store, assets)
    else:
        print(f"(skipping sounds: no asset store at {store})")


if __name__ == "__main__":
    main()
