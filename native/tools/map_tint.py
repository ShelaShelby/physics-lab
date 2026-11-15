# 0.50 (review items 8 and 10): give a black-and-white map a measured disc colour. Each pixel's grey (sRGB-decoded, IEC 61966-2-1) is multiplied, in LINEAR
# light, by one factor per channel, (r/g, 1, b/g) x s, with s chosen so the luminance (0.2126 R + 0.7152 G + 0.0722 B) is unchanged; then encoded and saved
# (baseline JPEG, quality 90, as map_jpg.py). A constant tint gives every average of the map, over the sphere or over a disc, the same r/g and b/g.
#   python3 tools/map_tint.py in.jpg out.jpg r_over_g b_over_g
import sys
from PIL import Image
src, out, rg, bg = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
s = 1 / (0.2126 * rg + 0.7152 + 0.0722 * bg)
k = (rg * s, s, bg * s)
dec = [(i / 255 / 12.92) if i / 255 <= 0.04045 else ((i / 255 + 0.055) / 1.055) ** 2.4 for i in range(256)]
def enc(l):
    l = min(max(l, 0.0), 1.0)
    return 12.92 * l if l <= 0.0031308 else 1.055 * l ** (1 / 2.4) - 0.055
im = Image.open(src).convert('L')
luts = [[int(round(enc(dec[i] * kc) * 255)) for i in range(256)] for kc in k]
clipped = sum(n for i, n in enumerate(im.histogram()) if dec[i] * max(k) > 1) / (im.size[0] * im.size[1])
rgb = Image.merge('RGB', [im.point(l) for l in luts])
rgb.save(out, 'JPEG', quality=90, optimize=True)
print(out, im.size, 'factors %.4f %.4f %.4f' % k, 'clipped pixels %.4f %%' % (100 * clipped))
