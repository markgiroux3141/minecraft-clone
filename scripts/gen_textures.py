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


SANDSTONE = (213, 202, 159)


def sandstone_side(x, y):
    # Smooth cap rows top and bottom, horizontal strata between.
    if y < 2 or y >= 14:
        return speckle(SANDSTONE, x, y, 25, 5)
    if hash01(0, y, 25) > 0.6:
        return speckle((196, 184, 138), x, y, 25, 7)
    return speckle(SANDSTONE, x, y, 25, 8)


def sandstone_top(x, y):
    return speckle(SANDSTONE, x, y, 26, 6)


def sandstone_bottom(x, y):
    # Coarser, slightly darker underside.
    return speckle((201, 190, 147), x, y, 27, 10)


def bedrock(x, y):
    # Harsh dark/light gray blotches.
    if hash01(x // 2, y // 2, 28) > 0.5:
        return speckle((40, 40, 40), x, y, 28, 18)
    return speckle((110, 110, 110), x, y, 28, 22)


def cobblestone(x, y):
    # Rounded stone lumps on a 4px grid with dark mortar seams between.
    if x % 5 == 0 or y % 5 == 0:
        return speckle((78, 78, 78), x, y, 29, 10)
    return speckle((132, 132, 132), x, y, 29, 16)


def crack_stage(stage):
    # M18 destroy_stage_0..9 (kFirstCrackTile in Block.h): transparent
    # tile with dark crack pixels, density rising with the stage; rendered
    # as an alpha-tested cube over the block being dug.
    def pixel(x, y):
        # A fixed jagged X of cracks plus stage-keyed spread around it.
        on_seam = abs(x - y) <= 0 or abs(x + y - 15) <= 0
        spread = hash01(x, y, 40 + stage) < (stage + 1) / 14.0
        if (on_seam and stage >= 1) or spread:
            return (30, 30, 30, 255)
        return (0, 0, 0, 0)

    return pixel


PLANK = (157, 127, 78)


def planks(x, y):
    # Horizontal boards (4px rows) with dark seams and butt joints.
    if y % 4 == 0 or (x == (y // 4 * 7) % 16):
        return speckle((110, 86, 50), x, y, 30, 8)
    return speckle(PLANK, x, y, 30, 10)


def crafting_table_top(x, y):
    # Planks with a dark tool-grid border.
    if x in (0, 15) or y in (0, 15) or (3 < x < 12 and 3 < y < 12 and (x % 4 == 0 or y % 4 == 0)):
        return speckle((74, 56, 34), x, y, 31, 8)
    return speckle(PLANK, x, y, 31, 8)


def crafting_table_side(x, y):
    # Planks with a saw + hammer silhouette blob.
    if 4 <= x <= 11 and 5 <= y <= 10 and hash01(x, y, 32) > 0.5:
        return speckle((58, 44, 28), x, y, 32, 8)
    return planks(x, y)


def crafting_table_front(x, y):
    if 5 <= x <= 10 and 4 <= y <= 11 and hash01(x, y, 33) > 0.45:
        return speckle((58, 44, 28), x, y, 33, 8)
    return planks(x, y)


STICK = (104, 82, 50)


def stick_sprite(x, y):
    # Diagonal stick from bottom-left to upper-right, 2px thick.
    d = (15 - y) - x
    if -1 <= d <= 1 and 2 <= x <= 12:
        return speckle(STICK, x, y, 34, 10)
    return (0, 0, 0, 0)


def tool_sprite(head_color, salt):
    # Generic placeholder tool: diagonal stick handle + a material-colored
    # head blob in the upper-right (shape detail comes from the real
    # assets; this just reads as "a tool of that material").
    def pixel(x, y):
        d = (15 - y) - x
        if -1 <= d <= 1 and 1 <= x <= 9:
            return speckle(STICK, x, y, salt, 10)
        if (x - 11) ** 2 + (y - 4) ** 2 <= 11:
            return speckle(head_color, x, y, salt, 12)
        return (0, 0, 0, 0)

    return pixel


WOOD_HEAD = (157, 127, 78)
STONE_HEAD = (125, 125, 125)
IRON_HEAD = (222, 222, 226)


def ore(fleck, salt):
    # Stone with clustered ore flecks (2x2-ish blobs on a hash gate).
    def pixel(x, y):
        if hash01(x // 2, y // 2, salt) > 0.78:
            return speckle(fleck, x, y, salt, 12)
        return speckle(STONE, x, y, 0)

    return pixel


coal_ore = ore((38, 38, 38), 50)
iron_ore = ore((216, 175, 147), 51)


def furnace_side(x, y):
    # Cobble-ish body with darker top/bottom caps.
    if y < 1 or y >= 15:
        return speckle((62, 62, 62), x, y, 52, 8)
    return speckle((104, 104, 104), x, y, 52, 14)


def furnace_top(x, y):
    return speckle((118, 118, 118), x, y, 53, 12)


def furnace_front(lit):
    def pixel(x, y):
        # The mouth: a dark opening in the lower half, glowing when lit.
        if 4 <= x <= 11 and 8 <= y <= 13:
            if lit:
                hot = hash01(x, y, 55) * 0.5 + 0.5
                return (int(255 * hot), int(140 * hot), int(28 * hot))
            return speckle((28, 28, 28), x, y, 54, 6)
        return furnace_side(x, y)

    return pixel


def glass(x, y):
    # White frame + sparse interior glints; everything else transparent
    # (cutout, like leaves).
    if x in (0, 15) or y in (0, 15):
        return speckle((216, 230, 234), x, y, 56, 6)
    if (x + y) % 7 == 0 and hash01(x, y, 56) > 0.7:
        return (236, 248, 250, 255)
    return (0, 0, 0, 0)


def coal_sprite(x, y):
    # A jagged black lump.
    if (x - 8) ** 2 + (y - 8) ** 2 <= 24 and hash01(x, y, 57) > 0.15:
        return speckle((42, 42, 42), x, y, 57, 14)
    return (0, 0, 0, 0)


def iron_ingot_sprite(x, y):
    # A flat trapezoid bar with a highlight ridge.
    if 2 <= x <= 13 and 6 <= y <= 11:
        if y == 6 or x in (2, 13):
            return speckle((238, 238, 242), x, y, 58, 4)
        return speckle((198, 198, 205), x, y, 58, 8)
    return (0, 0, 0, 0)


def torch(x, y):
    # 2px stick up the middle with a glowing head (the mesher's inset
    # planes show only these middle columns). Flame top at row 6 to match
    # the MC torch_on tile, so the mesher's top cap lines up either way.
    if 7 <= x <= 8 and 9 <= y <= 15:
        return speckle((124, 98, 60), x, y, 62, 10)
    if 7 <= x <= 8 and 6 <= y <= 8:
        return (255, 222, 120) if y == 7 else (244, 168, 54)
    return (0, 0, 0, 0)


def torch_flame(x, y):
    # The torch top cap (mapped onto the small post-top quad): a hot
    # yellow core fading to orange, fully opaque so the alpha test keeps
    # it. Looking down at a torch shows this square.
    if abs(x - 7.5) + abs(y - 7.5) < 4:
        return (255, 232, 138, 255)
    return (242, 158, 48, 255)


def lava(x, y):
    # M26: molten rock — dark crust veined with hot yellow cracks. Mostly
    # opaque dark orange with hash-gated bright spots, so it reads glowing
    # (the block emits light 15) and fully opaque (alpha 255).
    if hash01(x // 2, y // 2, 64) > 0.78:
        return speckle((255, 224, 96), x, y, 64, 20)
    if hash01(x, y, 64) > 0.6:
        return speckle((226, 110, 24), x, y, 64, 24)
    return speckle((158, 54, 12), x, y, 64, 18)


def obsidian(x, y):
    # M26: near-black volcanic glass with a faint purple sheen and sparse
    # glints.
    if hash01(x, y, 65) > 0.88:
        return speckle((78, 60, 104), x, y, 65, 16)
    return speckle((24, 18, 34), x, y, 65, 10)


IRON_PAIL = (170, 170, 178)
IRON_PAIL_DK = (120, 120, 128)


def bucket_sprite(fill):
    # A gray iron pail: trapezoid body (wider at the top) with a rim, on a
    # transparent background. `fill` is an optional (r,g,b) for the liquid
    # sitting in the top of the pail (None = empty).
    def pixel(x, y):
        # Body spans y 4..14; left/right edges slope inward toward the base.
        if y < 4 or y > 14:
            return (0, 0, 0, 0)
        inset = (y - 4) // 4  # 0 near the rim, grows toward the base
        left = 3 + inset
        right = 12 - inset
        if x < left or x > right:
            return (0, 0, 0, 0)
        if y == 4 or x == left or x == right or y == 14:
            return speckle(IRON_PAIL_DK, x, y, 66, 8)  # rim + outline
        if fill is not None and y <= 6:
            return speckle(fill, x, y, 67, 12)  # liquid surface in the pail
        return speckle(IRON_PAIL, x, y, 66, 10)

    return pixel


bucket_empty = bucket_sprite(None)
bucket_water = bucket_sprite((60, 110, 210))
bucket_lava = bucket_sprite((230, 110, 24))


def porkchop_sprite(x, y):
    # M32: a pink raw-meat blob with a paler fat rim.
    d = (x - 8) ** 2 + (y - 8) ** 2
    if d <= 30:
        return speckle((226, 130, 132), x, y, 69, 12)
    if d <= 42:
        return speckle((236, 198, 196), x, y, 69, 8)  # fat edge
    return (0, 0, 0, 0)


def rotten_flesh_sprite(x, y):
    # M32: a ragged greenish-brown scrap.
    if (x - 8) ** 2 + (y - 8) ** 2 <= 34 and hash01(x, y, 70) > 0.18:
        return speckle((124, 116, 78), x, y, 70, 16)
    return (0, 0, 0, 0)


# M34 placeholders (real art comes from import_mc_assets.py): a full-tile wool
# block + simple item blobs for the passive-mob drops + shears.
def wool_white(x, y):
    return speckle((222, 222, 224), x, y, 95, 6)


def _blob(color, salt, radius2=34):
    def pixel(x, y):
        return speckle(color, x, y, salt, 12) if (x - 8) ** 2 + (y - 8) ** 2 <= radius2 \
            else (0, 0, 0, 0)
    return pixel


def feather_sprite(x, y):
    # A thin pale diagonal quill.
    return (236, 236, 240, 255) if abs((x) - (15 - y)) <= 1 and 2 <= x <= 13 else (0, 0, 0, 0)


def egg_sprite(x, y):
    # A cream oval, taller than wide.
    if ((x - 8) ** 2) / 16.0 + ((y - 8) ** 2) / 28.0 <= 1.0:
        return speckle((226, 214, 180), x, y, 101, 8)
    return (0, 0, 0, 0)


def shears_sprite(x, y):
    # Two crossed gray blades.
    if (abs(x - y) <= 1 or abs(x - (15 - y)) <= 1) and 3 <= x <= 12:
        return speckle((150, 152, 160), x, y, 102, 8)
    return (0, 0, 0, 0)


# M35 placeholders (real art comes from import_mc_assets.py): a TNT block (red
# body with a white-on-dark band, lighter top/bottom caps) + gunpowder/flint &
# steel item blobs.
def tnt_side(x, y):
    if 6 <= y <= 9:  # the white "TNT" band
        return speckle((228, 228, 228), x, y, 103, 6)
    return speckle((182, 54, 44), x, y, 103, 12)


def tnt_top(x, y):
    return speckle((196, 80, 66), x, y, 104, 10)


def tnt_bottom(x, y):
    return speckle((120, 64, 52), x, y, 105, 10)


def gunpowder_sprite(x, y):
    # A dark-gray granular pile.
    if (x - 8) ** 2 + (y - 9) ** 2 <= 30 and hash01(x, y, 106) > 0.2:
        return speckle((72, 72, 76), x, y, 106, 16)
    return (0, 0, 0, 0)


def flint_and_steel_sprite(x, y):
    # A gray steel bar (upper-left) + a dark flint chip (lower-right).
    if 2 <= x <= 8 and 3 <= y <= 6:
        return speckle((176, 176, 182), x, y, 107, 8)
    if (x - 10) ** 2 + (y - 11) ** 2 <= 10:
        return speckle((54, 50, 58), x, y, 107, 10)
    return (0, 0, 0, 0)


# M36 placeholders (real art comes from import_mc_assets.py): a bow (108) with
# three draw frames (109..111), an arrow (112), and a bone (113).
def _bow(salt, string_x):
    def pixel(x, y):
        d = (x - 12) ** 2 + (y - 8) ** 2
        if 20 <= d <= 44 and x >= 5:  # the wooden arc
            return speckle((120, 80, 40), x, y, salt, 8)
        if x == string_x and 2 <= y <= 13:  # the string
            return (210, 210, 210, 255)
        if salt != 108 and y == 8 and string_x <= x <= 12:  # nocked shaft (pulling frames)
            return (170, 150, 110, 255)
        return (0, 0, 0, 0)

    return pixel


def arrow_sprite(x, y):
    # A pale diagonal shaft tipped at the top-right.
    if abs((x) - (15 - y)) <= 1 and 2 <= y <= 13:
        return speckle((184, 184, 188), x, y, 112, 6)
    return (0, 0, 0, 0)


def bone_sprite(x, y):
    # A white diagonal shaft with knobby ends.
    if abs(x - y) <= 1 and 3 <= x <= 12:
        return speckle((234, 234, 226), x, y, 113, 4)
    if x in (2, 3, 12, 13) and y in (2, 3, 12, 13) and abs(x - y) >= 9:
        return (234, 234, 226, 255)
    return (0, 0, 0, 0)


# M33 armor placeholder silhouettes (the real icons come from
# import_mc_assets.py). Each shape is a flat material-colored stencil; the
# four shapes mirror the equip order helmet/chestplate/leggings/boots.
def helmet_sprite(color, salt):
    def pixel(x, y):
        if 3 <= x <= 12 and 2 <= y <= 10:
            if 7 <= y <= 10 and 6 <= x <= 9:  # face opening
                return (0, 0, 0, 0)
            if y == 2 and (x < 4 or x > 11):  # round the crown
                return (0, 0, 0, 0)
            return speckle(color, x, y, salt, 10)
        return (0, 0, 0, 0)

    return pixel


def chest_sprite(color, salt):
    def pixel(x, y):
        if 3 <= y <= 4 and 2 <= x <= 13:  # shoulders
            return speckle(color, x, y, salt, 10)
        if 5 <= y <= 13 and 4 <= x <= 11:  # torso
            return speckle(color, x, y, salt, 10)
        return (0, 0, 0, 0)

    return pixel


def legs_sprite(color, salt):
    def pixel(x, y):
        if 4 <= y <= 5 and 4 <= x <= 11:  # belt
            return speckle(color, x, y, salt, 10)
        if 6 <= y <= 14 and (4 <= x <= 6 or 9 <= x <= 11):  # two legs
            return speckle(color, x, y, salt, 10)
        return (0, 0, 0, 0)

    return pixel


def boots_sprite(color, salt):
    def pixel(x, y):
        if 9 <= y <= 12 and (3 <= x <= 6 or 9 <= x <= 12):  # uppers
            return speckle(color, x, y, salt, 10)
        if 13 <= y <= 14 and (3 <= x <= 7 or 9 <= x <= 13):  # soles
            return speckle(color, x, y, salt, 10)
        return (0, 0, 0, 0)

    return pixel


# Material colors (leather, chainmail, iron, gold, diamond) + the empty-slot
# placeholder color, and the four shapes in equip order.
ARMOR_COLORS = [(160, 101, 64), (150, 152, 160), (200, 200, 206),
                (240, 212, 80), (120, 222, 214)]
ARMOR_SHAPES = [helmet_sprite, chest_sprite, legs_sprite, boots_sprite]
EMPTY_SLOT_COLOR = (78, 78, 84)


# Layer index in the texture array == position in this list.
TILES = [stone, dirt, grass_side, grass_top, glowstone, sand, log_side, log_top, leaves, water,
         snow, grass_side_snowed, tall_grass, dandelion, poppy, dead_bush,
         birch_side, birch_top, birch_leaves, spruce_side, spruce_top, spruce_leaves,
         cactus_side, cactus_top, sandstone_side, sandstone_top, sandstone_bottom, bedrock,
         cobblestone] + [crack_stage(i) for i in range(10)] + [
         planks, crafting_table_top, crafting_table_side, crafting_table_front,
         stick_sprite,
         tool_sprite(WOOD_HEAD, 35), tool_sprite(WOOD_HEAD, 36), tool_sprite(WOOD_HEAD, 37),
         tool_sprite(STONE_HEAD, 38), tool_sprite(STONE_HEAD, 39), tool_sprite(STONE_HEAD, 40),
         # M21: ores (50/51), furnace (52 front off / 53 front on / 54 side /
         # 55 top), glass (56), then the item sprites (57..61).
         coal_ore, iron_ore, furnace_front(False), furnace_front(True), furnace_side,
         furnace_top, glass,
         coal_sprite, iron_ingot_sprite,
         tool_sprite(IRON_HEAD, 59), tool_sprite(IRON_HEAD, 60), tool_sprite(IRON_HEAD, 61),
         # M21 follow-up: torch sides (62) + the flame-top cap tile (63,
         # the torch block's +Y face, drawn on the small post-top quad).
         torch, torch_flame,
         # M26: lava (64, still — used for source and all flow levels, like
         # water) and obsidian (65, lava+water product), then bucket sprites
         # (66 empty / 67 water / 68 lava).
         lava, obsidian, bucket_empty, bucket_water, bucket_lava,
         # M32 mob drops (69 porkchop / 70 rotten flesh).
         porkchop_sprite, rotten_flesh_sprite] + [
         # M33 armor icons (71..90): material-major x slot (helmet, chestplate,
         # leggings, boots), matching Item.cpp's RegisterDefaults.
         ARMOR_SHAPES[s](color, 71 + m * 4 + s)
         for m, color in enumerate(ARMOR_COLORS) for s in range(4)] + [
         # Empty-slot placeholders (91..94, Head/Chest/Legs/Feet).
         ARMOR_SHAPES[s](EMPTY_SLOT_COLOR, 91 + s) for s in range(4)] + [
         # M34: white wool BLOCK (95) + passive-mob drop / shears sprites
         # (96..102), matching Block.cpp / Item.cpp RegisterDefaults.
         wool_white,
         _blob((196, 70, 70), 96),    # raw beef
         _blob((150, 101, 64), 97),   # leather
         _blob((222, 130, 130), 98),  # raw mutton
         _blob((236, 200, 170), 99),  # raw chicken
         feather_sprite,              # feather (100)
         egg_sprite,                  # egg (101)
         shears_sprite] + [           # shears (102)
         # M35: TNT block (103 side / 104 top / 105 bottom) + gunpowder (106) +
         # flint & steel (107), matching Block.cpp / Item.cpp RegisterDefaults.
         tnt_side, tnt_top, tnt_bottom, gunpowder_sprite, flint_and_steel_sprite] + [
         # M36: bow (108) + three draw frames (109..111, used by the view model) +
         # arrow (112) + bone (113), matching Item.cpp RegisterDefaults.
         _bow(108, 4), _bow(109, 6), _bow(110, 8), _bow(111, 10), arrow_sprite, bone_sprite] + [
         # M37: cooked foods (114..117) — browned blobs, smelted from the raw
         # meat drops; matching Item.cpp RegisterDefaults.
         _blob((150, 96, 56), 114),   # cooked porkchop
         _blob((120, 74, 44), 115),   # cooked beef (steak)
         _blob((158, 108, 64), 116),  # cooked mutton
         _blob((176, 130, 78), 117)]  # cooked chicken


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
