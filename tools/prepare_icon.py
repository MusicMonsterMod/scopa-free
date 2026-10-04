#!/usr/bin/env python3
"""Prepare SDL window BMP and desktop/hicolor PNGs from assets/icons/scopa-icon.png."""
import shutil, struct
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent.parent
icons = root / 'assets/icons'
source = icons / 'scopa-icon.png'
if not source.exists():
    raise SystemExit(f'Missing icon source: {source}')

im = Image.open(source).convert('RGBA')

# Desktop / AppImage root icon: clean 256x256 (not a huge full-res source).
desktop = im.resize((256, 256), Image.Resampling.LANCZOS)
desktop.save(root / 'assets/scopa-free.png')

# SDL_SetWindowIcon / panel: 64x64 BITMAPV4 with alpha (not full-size source).
window = im.resize((64, 64), Image.Resampling.LANCZOS)
w, h = window.size
pixels = window.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes('raw', 'BGRA')
header = struct.pack('<IiiHHIIiiII', 108, w, h, 1, 32, 3, len(pixels), 2835, 2835, 0, 0)
header += struct.pack('<IIIII', 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, 0x73524742) + bytes(48)
(root / 'assets/icon.bmp').write_bytes(
    b'BM' + struct.pack('<IHHI', 14 + len(header) + len(pixels), 0, 0, 14 + len(header)) + header + pixels
)

for size in [16, 32, 48, 64, 128, 256, 512]:
    im.resize((size, size), Image.Resampling.LANCZOS).save(icons / f'{size}.png')

print(f'Prepared {source.relative_to(root)} -> icon.bmp {w}x{h}, scopa-free.png 256x256, hicolor PNGs')
