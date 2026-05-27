#!/usr/bin/env python3
"""
convert_avatar.py – Convert a PNG avatar to RGB565 C array for KidsBoard

Usage:
    python3 convert_avatar.py avatar.png
    python3 convert_avatar.py avatar.png --name AVATAR_MYNAME
    python3 convert_avatar.py avatar.png --saturation 2.2 --contrast 1.4

The output is a C array ready to paste into retro_assets.h.

Requirements:
    pip install Pillow numpy
"""

import sys
import argparse
try:
    from PIL import Image, ImageEnhance
    import numpy as np
except ImportError:
    print("Error: dependencies not installed. Run: pip install Pillow numpy")
    sys.exit(1)


def convert_avatar(path, name, saturation, contrast, brightness):
    img = Image.open(path).convert('RGB')

    # Resize to 32x32 using NEAREST for pixel art style
    img = img.resize((32, 32), Image.NEAREST)

    # Enhance for display (ILI9341 renders darker than a monitor)
    img = ImageEnhance.Color(img).enhance(saturation)
    img = ImageEnhance.Contrast(img).enhance(contrast)
    img = ImageEnhance.Brightness(img).enhance(brightness)

    arr = np.array(img)
    r = arr[:,:,0].astype(np.uint32) >> 3
    g = arr[:,:,1].astype(np.uint32) >> 2
    b = arr[:,:,2].astype(np.uint32) >> 3
    rgb565 = ((r << 11) | (g << 5) | b).flatten().astype(np.uint16)

    # Byte swap for TFT_eSPI pushImage compatibility
    swapped = ((rgb565 & 0xFF) << 8) | (rgb565 >> 8)

    # Output C array
    print(f'// {name}: 32x32px, {len(swapped)*2} bytes')
    print(f'const uint16_t {name}[{len(swapped)}] PROGMEM = {{')
    for i in range(0, len(swapped), 12):
        chunk = swapped[i:i+12]
        print('  ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',')
    print('};')
    print()
    print(f'// Done! Paste the above into retro_assets.h')
    print(f'// Then add {name} to the AVATAR_DATA[] array in display_ui.h')


def main():
    parser = argparse.ArgumentParser(description='Convert PNG avatar to RGB565 C array for KidsBoard')
    parser.add_argument('image', help='Path to PNG image (ideally 32x32px pixel art)')
    parser.add_argument('--name', default=None,
                        help='C array name, e.g. AVATAR_MYNAME (default: derived from filename)')
    parser.add_argument('--saturation', type=float, default=2.2,
                        help='Saturation enhancement (default: 2.2)')
    parser.add_argument('--contrast', type=float, default=1.4,
                        help='Contrast enhancement (default: 1.4)')
    parser.add_argument('--brightness', type=float, default=1.1,
                        help='Brightness enhancement (default: 1.1)')
    args = parser.parse_args()

    if args.name is None:
        # Derive name from filename: my_avatar.png → AVATAR_MY_AVATAR
        base = args.image.split('/')[-1].split('.')[0].upper()
        args.name = f'AVATAR_{base}'

    print(f'// Converting {args.image} → {args.name}')
    print(f'// Settings: saturation={args.saturation}, contrast={args.contrast}, brightness={args.brightness}')
    print()
    convert_avatar(args.image, args.name, args.saturation, args.contrast, args.brightness)


if __name__ == '__main__':
    main()