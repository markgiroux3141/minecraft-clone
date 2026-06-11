"""Generates the UI bitmap font (assets/fonts/ascii.png): ASCII 32..127 in a
16x6 grid of fixed-size cells, white-on-transparent, thresholded to 1-bit so
nearest-filter scaling stays crisp. vox::UiRenderer derives the glyph size
from the image dimensions and the grid (16 columns, 6 rows).

Requires Pillow and a Windows monospace TTF (Consolas, Courier fallback)."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

GRID_COLS = 16
GRID_ROWS = 6
FIRST_CHAR = 32
FONT_SIZE = 16
ALPHA_THRESHOLD = 110
FONT_CANDIDATES = ["consola.ttf", "cour.ttf", "lucon.ttf"]


def load_font() -> ImageFont.FreeTypeFont:
    for name in FONT_CANDIDATES:
        try:
            return ImageFont.truetype(name, FONT_SIZE)
        except OSError:
            continue
    raise SystemExit(f"none of {FONT_CANDIDATES} found")


def main() -> None:
    font = load_font()
    ascent, descent = font.getmetrics()
    cell_w = int(round(font.getlength("M")))
    cell_h = ascent + descent

    image = Image.new("RGBA", (cell_w * GRID_COLS, cell_h * GRID_ROWS), (255, 255, 255, 0))

    # Render each glyph antialiased, then threshold the alpha to 1-bit.
    for index in range(GRID_COLS * GRID_ROWS):
        ch = chr(FIRST_CHAR + index)
        if not ch.isprintable():
            continue
        cell = Image.new("L", (cell_w, cell_h), 0)
        ImageDraw.Draw(cell).text((0, 0), ch, fill=255, font=font)
        col, row = index % GRID_COLS, index // GRID_COLS
        for y in range(cell_h):
            for x in range(cell_w):
                if cell.getpixel((x, y)) >= ALPHA_THRESHOLD:
                    image.putpixel((col * cell_w + x, row * cell_h + y), (255, 255, 255, 255))

    out = Path(__file__).resolve().parent.parent / "assets" / "fonts" / "ascii.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    image.save(out)
    print(f"wrote {out} ({image.width}x{image.height}, glyph {cell_w}x{cell_h})")


if __name__ == "__main__":
    main()
