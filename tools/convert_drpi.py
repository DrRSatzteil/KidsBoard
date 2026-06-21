#!/usr/bin/env python3
"""
convert_drpi.py – Converts Dr. Pi PNG-Assets to RGB565 C-Arrays for ILI9341.

Usage:
  python3 convert_drpi.py drpi_normal.png DR_PI
  python3 convert_drpi.py drpi_happy.png DR_PI_HAPPY

Features:
  - Keeps true alpha transparency (PNG with transparent background)
  - Fallback: Flood-Fill background removal if no alpha is present
  - Byte-Swap for ILI9341 (pushImage expects Little-Endian)
  - Transparency marker 0x0001 (not 0x0000 due to real black pixels)
  - Saturation/contrast increased for ILI9341 (dark display)
  - Output: <name>_sprite.h directly to current directory
"""

import sys
import numpy as np
from collections import deque

try:
    from PIL import Image, ImageEnhance
except ImportError:
    print("Pillow fehlt. Installieren mit: pip install Pillow")
    sys.exit(1)

SIZE        = 64
SATURATION  = 2.3
CONTRAST    = 1.4
BRIGHTNESS  = 1.3
ALPHA_THRESHOLD = 30   # Pixel mit Alpha < 30 werden transparent
BG_TOLERANCE    = 20   # Flood-Fill Toleranz für Hintergrundfarbe


def remove_bg_floodfill(data, tolerance=BG_TOLERANCE):
    """Entfernt zusammenhängenden Hintergrund via Flood-Fill von den 4 Ecken."""
    h, w = data.shape[:2]
    mask = np.zeros((h, w), dtype=bool)

    corners = [data[0, 0, :3], data[0, w-1, :3],
               data[h-1, 0, :3], data[h-1, w-1, :3]]
    bg_color = np.mean(corners, axis=0)
    print(f"  Flood-Fill: Hintergrundfarbe ~{bg_color.astype(int)}, Toleranz={tolerance}")

    queue = deque()
    for seed in [(0, 0), (0, w-1), (h-1, 0), (h-1, w-1)]:
        if not mask[seed]:
            mask[seed] = True
            queue.append(seed)

    while queue:
        y, x = queue.popleft()
        for dy, dx in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and not mask[ny, nx]:
                pixel = data[ny, nx, :3].astype(float)
                if np.max(np.abs(pixel - bg_color)) <= tolerance:
                    mask[ny, nx] = True
                    queue.append((ny, nx))

    removed = int(np.sum(mask))
    print(f"  Flood-Fill: {removed} background pixels removed")
    return mask


def convert(input_path, name, size=SIZE):
    print(f"\nConverting: {input_path} → {name}")

    img = Image.open(input_path).convert("RGBA")
    print(f"  Original: {img.size}")

    data = np.array(img)
    alpha_orig = data[:, :, 3]
    has_transparency = int(np.sum(alpha_orig < 128)) > 0

    if has_transparency:
        print(f"  PNG has true transparency ({int(np.sum(alpha_orig == 0))} fully transparent pixels)")
    else:
        print(f"  No transparency found – using Flood-Fill")
        bg_mask = remove_bg_floodfill(data)
        data[bg_mask, 3] = 0
        img = Image.fromarray(data)

    # Auf Zielgröße skalieren
    img = img.resize((size, size), Image.LANCZOS)

    # Sättigung + Kontrast für ILI9341
    rgb = img.convert("RGB")
    rgb = ImageEnhance.Color(rgb).enhance(SATURATION)
    rgb = ImageEnhance.Contrast(rgb).enhance(CONTRAST)
    rgb = ImageEnhance.Brightness(rgb).enhance(BRIGHTNESS)

    pixels = np.array(rgb)
    alpha  = np.array(img)[:, :, 3]

    transparent_count = int(np.sum(alpha < ALPHA_THRESHOLD))
    print(f"  After resize: {transparent_count} transparent pixels (threshold {ALPHA_THRESHOLD})")

    # C-Array generieren
    lines = []
    lines.append(f"// {name}: {size}x{size}px, RGB565 pre-swapped for ILI9341")
    lines.append(f"// Transparent-Marker: 0x0001 (not 0x0000!)")
    lines.append(f"// Generated with convert_drpi.py")
    lines.append(f"const uint16_t {name}[{size * size}] PROGMEM = {{")

    row_strings = []
    for y in range(size):
        row = []
        for x in range(size):
            if alpha[y, x] < ALPHA_THRESHOLD:
                row.append("0x0001")  # Transparenz-Marker
            else:
                r5 = (int(pixels[y, x, 0]) >> 3) & 0x1F
                g6 = (int(pixels[y, x, 1]) >> 2) & 0x3F
                b5 = (int(pixels[y, x, 2]) >> 3) & 0x1F
                rgb565  = (r5 << 11) | (g6 << 5) | b5
                swapped = ((rgb565 & 0xFF) << 8) | (rgb565 >> 8)
                # Echter schwarzer Pixel darf nicht mit Marker kollidieren
                if swapped == 0x0001:
                    swapped = 0x0821  # sehr dunkles Grau – visuell identisch
                row.append(f"0x{swapped:04X}")
        row_strings.append("  " + ", ".join(row))

    lines.append(",\n".join(row_strings))
    lines.append("};")

    out_path = f"{name.lower()}_sprite.h"
    with open(out_path, "w") as f:
        f.write("\n".join(lines))
    print(f"  → {out_path} ({size*size*2} Bytes)")


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    input_path = sys.argv[1]
    name       = sys.argv[2].upper()
    convert(input_path, name)
    print("\Done! Insert into retro_assets.h.")


if __name__ == "__main__":
    main()
