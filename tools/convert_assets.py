#!/usr/bin/env python3
"""
generate_assets.py – Generate retro_assets.h for KidsBoard

Converts all pixel art assets (background, avatars, cloud) to RGB565 C arrays
for use with TFT_eSPI's pushImage(). All assets are byte-swapped.

Sky color is automatically sampled from the enhanced background and applied
consistently to the cloud asset, so cloud sky pixels are invisible over the
background.

Usage:
    python3 generate_assets.py
    python3 generate_assets.py --bg my_background.png
    python3 generate_assets.py --bg bg.png --cloud cloud.png --output retro_assets.h

Requirements:
    pip install Pillow numpy
"""

import argparse
import sys
import numpy as np
try:
    from PIL import Image, ImageEnhance
except ImportError:
    print("Error: dependencies not installed. Run: pip install Pillow numpy")
    sys.exit(1)

# ── Enhancement settings ──────────────────────────────────────────────────────
BG_SAT    = 1.8   # Background saturation
BG_CON    = 1.35  # Background contrast
BG_BRI    = 1.1   # Background brightness

AV_SAT    = 2.2   # Avatar saturation
AV_CON    = 1.4   # Avatar contrast
AV_BRI    = 1.1   # Avatar brightness

CL_BRI    = 1.05  # Cloud brightness (sky pixels are replaced, so sat/con don't matter)

SKY_RGB   = (91, 198, 232)  # Sky color in source images
SKY_TOL   = 30              # Tolerance for sky detection

# ── Helpers ───────────────────────────────────────────────────────────────────

def enhance(img, sat=1.0, con=1.0, bri=1.0):
    if sat != 1.0: img = ImageEnhance.Color(img).enhance(sat)
    if con != 1.0: img = ImageEnhance.Contrast(img).enhance(con)
    if bri != 1.0: img = ImageEnhance.Brightness(img).enhance(bri)
    return img

def clean_sky(img):
    """Replace all sky-ish pixels with exact SKY_RGB."""
    arr = np.array(img.convert('RGB'))
    sky = np.array(SKY_RGB)
    is_sky = np.all(np.abs(arr.astype(int) - sky) < SKY_TOL, axis=2)
    arr[is_sky] = sky
    return Image.fromarray(arr.astype(np.uint8)), is_sky

def to_rgb565_swapped(arr):
    """Convert RGB888 numpy array to byte-swapped RGB565 array."""
    arr = np.array(arr)
    r = arr[:,:,0].astype(np.uint32) >> 3
    g = arr[:,:,1].astype(np.uint32) >> 2
    b = arr[:,:,2].astype(np.uint32) >> 3
    rgb565 = ((r << 11) | (g << 5) | b).flatten().astype(np.uint16)
    return ((rgb565 & 0xFF) << 8) | (rgb565 >> 8)

def write_array(f, name, data, w, h, comment=''):
    f.write(f'// {comment}: {w}x{h}px\n')
    f.write(f'const uint16_t {name}[{len(data)}] PROGMEM = {{\n')
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        f.write('  ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',\n')
    f.write('};\n\n')

# ── Asset generation ──────────────────────────────────────────────────────────

def generate_background(path):
    """Load, clean sky, resize to 120x160, enhance, return data + sky reference."""
    img, _ = clean_sky(Image.open(path))
    img = img.resize((120, 160), Image.NEAREST)
    enhanced = enhance(img, sat=BG_SAT, con=BG_CON, bri=BG_BRI)
    arr = np.array(enhanced)
    # Sample sky reference from top-right corner (guaranteed sky area)
    sky_ref = arr[2, 118]
    print(f'  Sky reference (enhanced): RGB({sky_ref[0]},{sky_ref[1]},{sky_ref[2]})')
    return to_rgb565_swapped(arr), sky_ref

def generate_night(bg_path):
    """Generate night version of background."""
    img, _ = clean_sky(Image.open(bg_path))
    img = img.resize((120, 160), Image.NEAREST)
    arr = np.array(img, dtype=np.float32)
    r_n = arr[:,:,0]*0.25
    g_n = arr[:,:,1]*0.35
    b_n = np.clip(arr[:,:,2]*0.65+25, 0, 255)
    gray = r_n*0.299 + g_n*0.587 + b_n*0.114
    f = 0.65
    arr[:,:,0] = np.clip(gray*(1-f) + r_n*f, 0, 255)
    arr[:,:,1] = np.clip(gray*(1-f) + g_n*f, 0, 255)
    arr[:,:,2] = np.clip(gray*(1-f) + b_n*f, 0, 255)
    return to_rgb565_swapped(arr.astype(np.uint8))

def generate_avatar(path, name):
    """Load, resize to 32x32, enhance."""
    img = Image.open(path).convert('RGB').resize((32, 32), Image.NEAREST)
    img = enhance(img, sat=AV_SAT, con=AV_CON, bri=AV_BRI)
    print(f'  Avatar {name}: sat={AV_SAT} con={AV_CON} bri={AV_BRI}')
    return to_rgb565_swapped(np.array(img))

def generate_cloud(path, sky_ref):
    """Load, clean sky, enhance, replace sky pixels with BG sky reference."""
    img, is_sky_full = clean_sky(Image.open(path))
    arr = np.array(img)
    is_sky = np.all(arr == np.array(SKY_RGB), axis=2)
    rows = np.any(~is_sky, axis=1); cols = np.any(~is_sky, axis=0)
    y0=np.argmax(rows); y1=len(rows)-np.argmax(rows[::-1])
    x0=np.argmax(cols); x1=len(cols)-np.argmax(cols[::-1])

    cloud_crop = img.crop((x0,y0,x1,y1)).resize((80,30), Image.NEAREST)
    sky_mask = np.array(
        Image.fromarray(is_sky[y0:y1,x0:x1].astype(np.uint8)*255).resize((80,30), Image.NEAREST)
    ) > 127

    cloud_arr = np.array(enhance(cloud_crop, bri=CL_BRI))
    cloud_arr[sky_mask] = sky_ref  # replace sky with BG sky reference
    return to_rgb565_swapped(cloud_arr)

# ── Main ──────────────────────────────────────────────────────────────────────

def generate_sky_mask(bg_path, output_path):
    """Generate sky_mask.h from background PNG.
    Sky pixels (matching SKY_RGB ± SKY_TOL) become 1, everything else 0.
    Output is a 1-bit packed array, 1 byte per 8 pixels."""
    img, is_sky = clean_sky(Image.open(bg_path))
    img = img.resize((120, 160), Image.NEAREST)

    # Resize mask to 120x160
    mask = np.array(
        Image.fromarray(is_sky.astype(np.uint8)*255).resize((120,160), Image.NEAREST)
    ) > 127

    # Scale to 240x320 (2x)
    mask_full = np.repeat(np.repeat(mask, 2, axis=0), 2, axis=1)
    H, W = mask_full.shape  # 320 x 240

    # Find Y offset and height of sky region
    sky_rows = np.any(mask_full, axis=1)
    if not sky_rows.any():
        print('  Warning: no sky pixels found in mask!')
        return

    y_offset = int(np.argmax(sky_rows))
    y_end    = int(len(sky_rows) - np.argmax(sky_rows[::-1]))
    sky_height = y_end - y_offset
    sky_width_bytes = W // 8  # = 30

    print(f'  Sky mask: y_offset={y_offset}, height={sky_height}, width={W}px ({sky_width_bytes} bytes/row)')

    # Pack bits: MSB first
    rows_data = []
    for y in range(y_offset, y_end):
        row = mask_full[y]
        packed = []
        for x in range(0, W, 8):
            byte = 0
            for bit in range(8):
                if x+bit < W and row[x+bit]:
                    byte |= (1 << (7 - bit))
            packed.append(byte)
        rows_data.append(packed)

    with open(output_path, 'w') as f:
        f.write('#pragma once\n#include <pgmspace.h>\n\n')
        f.write(f'#define SKY_MASK_Y_OFFSET  {y_offset}\n')
        f.write(f'#define SKY_MASK_HEIGHT    {sky_height}\n')
        f.write(f'#define SKY_MASK_WIDTH     {sky_width_bytes}\n\n')
        f.write(f'const uint8_t SKY_MASK[{sky_height * sky_width_bytes}] PROGMEM = {{\n')
        for row in rows_data:
            f.write('  ' + ', '.join(f'0x{b:02X}' for b in row) + ',\n')
        f.write('};\n\n')
        f.write('// Returns true if pixel (x,y) is sky\n')
        f.write('bool isSky(int x, int y) {\n')
        f.write('  if (y < SKY_MASK_Y_OFFSET) return true;\n')
        f.write('  if (y >= SKY_MASK_Y_OFFSET + SKY_MASK_HEIGHT) return false;\n')
        f.write('  int row = y - SKY_MASK_Y_OFFSET;\n')
        f.write('  uint8_t b = pgm_read_byte(&SKY_MASK[row * SKY_MASK_WIDTH + x / 8]);\n')
        f.write('  return (b & (1 << (7 - x % 8))) != 0;\n')
        f.write('}\n')

    print(f'  Written to {output_path}')


def main():
    parser = argparse.ArgumentParser(description='Generate retro_assets.h for KidsBoard')
    parser.add_argument('--bg',      default='bg_day.png',        help='Day background PNG')
    parser.add_argument('--cloud',   default='cloud.png',          help='Cloud PNG/JPEG')
    parser.add_argument('--mila',    default='avatar_mila.png',    help='Mila avatar PNG')
    parser.add_argument('--felix',   default='avatar_felix.png',   help='Felix avatar PNG')
    parser.add_argument('--mama',    default='avatar_mama.png',    help='Mama avatar PNG')
    parser.add_argument('--papa',    default='avatar_papa.png',    help='Papa avatar PNG')
    parser.add_argument('--output',  default='retro_assets.h',     help='Output file')
    parser.add_argument('--mask',    default='sky_mask.h',         help='Sky mask output file')
    parser.add_argument('--sky-rgb', default='91,198,232',          help='Sky color in source images (R,G,B) default: 91,198,232')
    args = parser.parse_args()

    # Parse sky RGB
    try:
        r, g, b = [int(x.strip()) for x in args.sky_rgb.split(',')]
        global SKY_RGB
        SKY_RGB = (r, g, b)
    except:
        print(f'Error: --sky-rgb must be R,G,B (e.g. 91,198,232)')
        sys.exit(1)
    print(f'Sky color: RGB{SKY_RGB}')
    print(f'Generating {args.output}...')

    print(f'Background: {args.bg}')
    day_data, sky_ref = generate_background(args.bg)
    night_data = generate_night(args.bg)

    print('Avatars:')
    mila_data  = generate_avatar(args.mila,  'Mila')
    felix_data = generate_avatar(args.felix, 'Felix')
    mama_data  = generate_avatar(args.mama,  'Mama')
    papa_data  = generate_avatar(args.papa,  'Papa')

    print(f'Cloud: {args.cloud}')
    cloud_data = generate_cloud(args.cloud, sky_ref)

    # Compute CLOUD_SKY from sky_ref
    r5,g6,b5 = int(sky_ref[0])>>3, int(sky_ref[1])>>2, int(sky_ref[2])>>3
    sky_rgb565 = (r5<<11)|(g6<<5)|b5
    sky_swapped = ((sky_rgb565&0xFF)<<8)|(sky_rgb565>>8)
    print(f'CLOUD_SKY = 0x{sky_swapped:04X}')

    with open(args.output, 'w') as f:
        f.write('#pragma once\n#include <pgmspace.h>\n\n')
        f.write('// All assets byte-swapped for TFT_eSPI pushImage\n')
        f.write('// Sky color sampled from enhanced background top-right corner\n')
        f.write('// Generated by tools/generate_assets.py\n\n')
        f.write('#define BG_W       120\n#define BG_H       160\n')
        f.write('#define AVATAR_W    32\n#define AVATAR_H    32\n')
        f.write('#define CLOUD_W     80\n#define CLOUD_H     30\n')
        f.write(f'#define CLOUD_SKY  0x{sky_swapped:04X}\n\n')
        write_array(f, 'BG_DAY',       day_data,   120, 160, 'Day background')
        write_array(f, 'BG_NIGHT',     night_data, 120, 160, 'Night background')
        write_array(f, 'AVATAR_MILA',  mila_data,   32,  32, 'Mila')
        write_array(f, 'AVATAR_FELIX', felix_data,  32,  32, 'Felix')
        write_array(f, 'AVATAR_MAMA',  mama_data,   32,  32, 'Mama')
        write_array(f, 'AVATAR_PAPA',  papa_data,   32,  32, 'Papa')
        write_array(f, 'SPRITE_CLOUD', cloud_data,  80,  30, 'Cloud')

    print(f'\nSky mask: {args.mask}')
    generate_sky_mask(args.bg, args.mask)

    print(f'\nDone!')
    print(f'Copy {args.output} and {args.mask} to your KidsBoard sketch folder.')

if __name__ == '__main__':
    main()