"""Generate AppIcon.ico for In Out Voice Bridge.
Run: python generate_icon.py
"""
import struct, math

def make_icon_image(size):
    pixels = []
    corner_r = size * 0.22

    for y in range(size):
        row = []
        for x in range(size):
            dx = max(corner_r - x, 0, x - (size - 1 - corner_r))
            dy = max(corner_r - y, 0, y - (size - 1 - corner_r))
            inside = (dx*dx + dy*dy <= corner_r*corner_r) if (dx > 0 or dy > 0) else True

            if not inside:
                row.append((0, 0, 0, 0))
                continue

            t = y / max(size - 1, 1)
            bg_r = int(30 + 12 * (1 - t))
            bg_g = int(30 + 18 * (1 - t))
            bg_b = int(60 + 20 * (1 - t))
            r, g, b, a = bg_r, bg_g, bg_b, 255

            nx = x / size
            ny = y / size

            # Green circle (source)
            lcx, lcy, lr = 0.28, 0.5, 0.14
            dist_l = math.sqrt((nx - lcx)**2 + (ny - lcy)**2)
            if dist_l < lr:
                r, g, b = 29, 185, 84
            if dist_l < lr * 0.55:
                r, g, b = 20, 20, 38

            # Blue circle (destination)
            rcx, rcy, rr = 0.72, 0.5, 0.14
            dist_r = math.sqrt((nx - rcx)**2 + (ny - rcy)**2)
            if dist_r < rr:
                r, g, b = 137, 180, 250
            if dist_r < rr * 0.55:
                r, g, b = 20, 20, 38

            # Arrow body
            arrow_y = 0.5
            arrow_h = 0.06
            if 0.42 <= nx <= 0.62 and abs(ny - arrow_y) < arrow_h:
                at = (nx - 0.42) / 0.20
                r = int(29 + (137 - 29) * at)
                g = int(185 + (180 - 185) * at)
                b = int(84 + (250 - 84) * at)

            # Arrow head
            head_x = 0.62
            head_size = 0.10
            if head_x <= nx <= head_x + head_size:
                spread = (nx - head_x) / head_size
                if abs(ny - arrow_y) < head_size * (1 - spread) + 0.01:
                    r, g, b = 137, 180, 250

            row.append((b, g, r, a))
        pixels.append(row)
    return pixels

def pixels_to_bmp_data(pixels, size):
    xor_data = b''
    for row in reversed(pixels):
        for bgra in row:
            xor_data += struct.pack('BBBB', *bgra)
    mask_stride = ((size + 31) // 32) * 4
    and_mask = b'\x00' * (mask_stride * size)
    bih = struct.pack('<IIIHHIIIIII',
        40, size, size * 2, 1, 32, 0,
        len(xor_data) + len(and_mask), 0, 0, 0, 0)
    return bih + xor_data + and_mask

sizes = [16, 24, 32, 48, 64, 128, 256]
entries = []
offset = 6 + 16 * len(sizes)

for s in sizes:
    pixels = make_icon_image(s)
    data = pixels_to_bmp_data(pixels, s)
    w = 0 if s >= 256 else s
    h = 0 if s >= 256 else s
    entry = struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(data), offset)
    entries.append((entry, data))
    offset += len(data)

ico = struct.pack('<HHH', 0, 1, len(sizes))
for entry, _ in entries:
    ico += entry
for _, data in entries:
    ico += data

out = 'src/InOutVoiceBridge.App/Assets/AppIcon.ico'
with open(out, 'wb') as f:
    f.write(ico)

print(f'Created {out} ({len(ico)} bytes, {len(sizes)} sizes)')
