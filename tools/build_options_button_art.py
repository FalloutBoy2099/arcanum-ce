"""Convert a transparent 2x2 navigation-button sheet into Arcanum ART files.

Grid: left normal, right normal / left pressed, right pressed.
Requires Pillow. Run from the repository root.
"""

from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/art/options-buttons/OptionsMenuButtons-source.png"
OUTPUT = ROOT / "art/interface"
PREVIEWS = ROOT / "docs/art/options-buttons"
SIZE = 40
# Exact arrow colors from Arcanum's SldrButt_L_Arrow.ART (frames 0, 1, 2).
# Normal, pressed, hover; each pair is outline then fill.
ARROW_COLORS = ((0x54000D, 0x8E0000), (0xD6A2AF, 0xD60000), (0x8F0023, 0xD60000))
BASE_COLORS = 224


def is_arrow_red(color):
    red, green, blue = color[:3]
    return red > 50 and red > green * 2.5 and red > blue * 2.5


def arrow_contains(x, y):
    # Center the triangle's area (visual mass), rather than its bounding box.
    # The right button is mirrored from this master, preserving the correction.
    tip = 11.5
    base = 24.5
    return tip <= x < base and abs(y - 20) <= (x - tip) * 8 / (base - tip)


def read_sprite(sheet, column, row):
    width, height = sheet.size
    cell = sheet.crop((column * width // 2, row * height // 2,
                       (column + 1) * width // 2, (row + 1) * height // 2))
    # Ignore nearly invisible edge noise before fitting to the native 40px cell.
    bounds = cell.getchannel("A").point(lambda value: 255 if value >= 128 else 0).getbbox()
    if bounds is None:
        raise ValueError("An expected sprite cell is empty")
    return cell.crop(bounds).resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def write_button(name, normal, pressed, mirror=False):
    # ART uses one shared 256-color palette; index 0 is transparency.
    pair = Image.new("RGB", (SIZE * 2, SIZE))
    pair.paste(normal.convert("RGB"), (0, 0))
    pair.paste(pressed.convert("RGB"), (SIZE, 0))
    indexed = pair.quantize(colors=BASE_COLORS, method=Image.Quantize.MEDIANCUT,
                            dither=Image.Dither.NONE)
    rgb_palette = indexed.getpalette()
    palette = [0x00FF00] + [
        (rgb_palette[i] << 16) | (rgb_palette[i + 1] << 8) | rgb_palette[i + 2]
        for i in range(0, BASE_COLORS * 3, 3)
    ]
    for colors in ARROW_COLORS:
        palette.extend(colors)
    palette.extend([0] * (256 - len(palette)))
    states = []
    for state, sprite in enumerate((normal, pressed)):
        indices = indexed.crop((state * SIZE, 0, (state + 1) * SIZE, SIZE)).tobytes()
        indices = bytearray(indices)
        arrow_colors = [i for i in range(BASE_COLORS) if is_arrow_red(rgb_palette[i * 3:i * 3 + 3])]
        face_colors = [i for i in range(BASE_COLORS)
                        if rgb_palette[i * 3 + 1] >= rgb_palette[i * 3] * 0.8
                        and rgb_palette[i * 3] < 85
                        and rgb_palette[i * 3 + 1] < 85
                        and rgb_palette[i * 3 + 2] < 45]
        assert arrow_colors and face_colors
        original = bytes(indices)
        for y in range(SIZE):
            for x in range(SIZE):
                position = y * SIZE + x
                source_index = original[position]
                if arrow_contains(x + 0.5, y + 0.5):
                    outline = any(not arrow_contains(x + 0.5 + dx, y + 0.5 + dy)
                                  for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)))
                    indices[position] = BASE_COLORS + state * 2 + (0 if outline else 1)
                elif source_index in arrow_colors and (x + 0.5 - 20) ** 2 + (y + 0.5 - 20) ** 2 < 14 ** 2:
                    # Remove residual source arrow pixels outside the polygon.
                    target = original[33 * SIZE + 20]
                    indices[position] = min(face_colors, key=lambda i: sum(
                        (rgb_palette[i * 3 + c] - rgb_palette[target * 3 + c]) ** 2
                        for c in range(3)
                    ))
        # Rasterize the same exact circle in every native frame. Pixel centers
        # are symmetric around (19.5, 19.5), giving a true 40px diameter.
        pixels = bytes(
            index + 1 if (x + 0.5 - SIZE / 2) ** 2 + (y + 0.5 - SIZE / 2) ** 2 <= (SIZE / 2) ** 2 else 0
            for y in range(SIZE) for x in range(SIZE)
            for index in [indices[y * SIZE + x]]
        )
        if mirror:
            pixels = b"".join(pixels[y * SIZE:(y + 1) * SIZE][::-1] for y in range(SIZE))
        states.append(pixels)
    # Hover preserves the normal face, changing only the triangle's native colors.
    hover_colors = {BASE_COLORS + 1: BASE_COLORS + 5, BASE_COLORS + 2: BASE_COLORS + 6}
    states.append(bytes(hover_colors.get(index, index) for index in states[0]))

    header = [0] * 33
    header[0:4] = [1, 10, 8, 1]  # One rotation, 10 FPS, 8 bpp, first palette present.
    header[8] = 3
    header[17] = SIZE * SIZE * 3
    frame_info = struct.pack("<7i", SIZE, SIZE, SIZE * SIZE, 0, 0, 0, 0)
    data = (struct.pack("<33I", *header) + struct.pack("<256I", *palette)
            + frame_info * 3 + b"".join(states))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"{name}.art"
    path.write_bytes(data)

    # Decode the written native data to verify the header, frames, and exports.
    loaded = path.read_bytes()
    assert struct.unpack_from("<I", loaded, 32)[0] == 3
    offset = 132 + 1024 + 3 * 28
    assert len(loaded) == offset + 3 * SIZE * SIZE
    assert states[0] != states[1] and states[0] != states[2] and states[1] != states[2]
    assert all(0 in pixels and any(pixels) for pixels in states)
    for state, label in enumerate(("normal", "pressed", "hover")):
        pixels = loaded[offset + state * SIZE * SIZE:offset + (state + 1) * SIZE * SIZE]
        assert pixels == states[state]
        preview = Image.new("RGBA", (SIZE, SIZE))
        preview.putdata([
            ((palette[index] >> 16) & 255, (palette[index] >> 8) & 255,
             palette[index] & 255, 255 if index else 0)
            for index in pixels
        ])
        preview.save(PREVIEWS / f"{name}-{label}.png")
    print(f"{path}: 40x40, 3 frames, {len(data)} bytes")


if __name__ == "__main__":
    frames_directory = PREVIEWS / "frames"
    if (frames_directory / "OptionsMenuLeftBtn_001.png").exists():
        # Hand-edited frames take precedence over the historical generated sheet.
        # Do not recolor, redraw, mirror, or resize the user's frames.
        from assemble_art_frames import assemble
        for side in ("Left", "Right"):
            name = f"OptionsMenu{side}Btn"
            assemble([frames_directory / f"{name}_{i:03d}.png" for i in range(1, 4)], OUTPUT / f"{name}.art")
    else:
        sheet = Image.open(SOURCE).convert("RGBA")
        normal = read_sprite(sheet, 0, 0)
        pressed = read_sprite(sheet, 0, 1)
        write_button("OptionsMenuLeftBtn", normal, pressed)
        write_button("OptionsMenuRightBtn", normal, pressed, mirror=True)
