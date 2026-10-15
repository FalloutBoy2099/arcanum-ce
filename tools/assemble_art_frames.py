"""Losslessly assemble numbered PNG frames into single-rotation Arcanum ARTs."""

from pathlib import Path
import argparse
import struct

from PIL import Image


def assemble(paths, output):
    frames = [Image.open(path).convert("RGBA") for path in paths]
    width, height = frames[0].size
    if any(frame.size != (width, height) for frame in frames):
        raise ValueError("All frames must have identical dimensions")
    colors = {}
    pixels = []
    for frame in frames:
        encoded = bytearray()
        for red, green, blue, alpha in frame.getdata():
            if alpha not in (0, 255):
                raise ValueError("ART requires binary transparency; partial alpha found")
            if alpha == 0:
                encoded.append(0)
                continue
            color = (red << 16) | (green << 8) | blue
            if color not in colors:
                colors[color] = len(colors) + 1
            if len(colors) > 255:
                raise ValueError("More than 255 opaque colors; cannot encode losslessly")
            encoded.append(colors[color])
        pixels.append(bytes(encoded))

    # A unique color key prevents an opaque pixel sharing the transparency RGB.
    key = 0x00FF00
    while key in colors:
        key = (key + 1) & 0xFFFFFF
    palette = [key] + list(colors)
    palette.extend([0] * (256 - len(palette)))
    header = [0] * 33
    header[:4] = [1, 10, 8, 1]
    header[8] = len(frames)
    header[17] = width * height * len(frames)
    metadata = struct.pack("<7i", width, height, width * height, 0, 0, 0, 0)
    data = (struct.pack("<33I", *header) + struct.pack("<256I", *palette)
            + metadata * len(frames) + b"".join(pixels))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)

    # Verify every visible RGB pixel and transparency against the original PNGs.
    loaded = output.read_bytes()
    offset = 132 + 1024 + len(frames) * 28
    for number, frame in enumerate(frames):
        encoded = loaded[offset + number * width * height:offset + (number + 1) * width * height]
        assert len(encoded) == width * height
        for index, (red, green, blue, alpha) in zip(encoded, frame.getdata()):
            assert (index != 0) == (alpha == 255)
            if index:
                assert palette[index] == (red << 16) | (green << 8) | blue
    print(f"{output}: {width}x{height}, {len(frames)} frames, {len(colors)} opaque colors, lossless verification passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    groups = {}
    for path in sorted(args.source.glob("*_???.png")):
        name, number = path.stem.rsplit("_", 1)
        if number.isdigit():
            groups.setdefault(name, []).append(path)
    if not groups:
        parser.error("No numbered PNG frames found")
    for name, paths in groups.items():
        numbers = [int(path.stem.rsplit("_", 1)[1]) for path in paths]
        if numbers != list(range(1, len(paths) + 1)):
            raise ValueError(f"Missing or duplicate frame numbers for {name}")
        assemble(paths, args.output / f"{name}.art")
