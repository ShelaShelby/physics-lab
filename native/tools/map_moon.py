# 0.50 (review item 10): a moon's map for the app from a USGS global mosaic (equirectangular / simple cylindrical GeoTIFF, east to the right, north up),
# read over HTTP row by row (as map_shrink.py) and shrunk to W x H by averaging k source rows per output row and boxes of columns, counting ONLY valid
# pixels (the file's nodata value, 0, is left out). A 3-band (colour) mosaic is turned into grey: the luminance of its decoded sRGB (IEC 61966-2-1,
# 0.2126 R + 0.7152 G + 0.0722 B), encoded again; no measured colour is claimed. Output pixels with no valid source pixel (areas never imaged) get the
# area-weighted mean grey of all valid pixels (as map_pluto_fill.py did for Pluto). Saves a baseline JPEG, quality 90, and prints the coverage.
#   python3 tools/map_moon.py URL out.jpg W H k
import sys, threading, numpy as np, rasterio
from rasterio.windows import Window
from concurrent.futures import ThreadPoolExecutor
from PIL import Image
url, out, W, H, k = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
local = threading.local()
def ds():
    if not hasattr(local, 'd'): local.d = rasterio.open('/vsicurl/' + url)
    return local.d
d0 = rasterio.open('/vsicurl/' + url); SW, SH, C = d0.width, d0.height, d0.count
nod = d0.nodata if d0.nodata is not None else 0
print('source', SW, SH, C, 'bounds', d0.bounds, 'nodata', nod, flush=True)
dec = lambda c: np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
enc = lambda l: np.where(l <= 0.0031308, 12.92 * l, 1.055 * np.clip(l, 0, 1) ** (1 / 2.4) - 0.055)
xs = (np.arange(W + 1) * SW / W).astype(int)
def row(j):
    y0, y1 = j * SH / H, (j + 1) * SH / H
    acc = np.zeros(SW); cnt = np.zeros(SW)
    for t in range(k):
        y = min(SH - 1, int(y0 + (t + 0.5) * (y1 - y0) / k))
        a = ds().read(window=Window(0, y, SW, 1)).astype(np.float64)[:, 0, :]   # C x SW
        valid = np.all(a != nod, axis=0)
        if C >= 3: g = enc(0.2126 * dec(a[0] / 255) + 0.7152 * dec(a[1] / 255) + 0.0722 * dec(a[2] / 255)) * 255
        else: g = a[0]
        acc += np.where(valid, g, 0); cnt += valid
    cs = np.concatenate([[0], np.cumsum(acc)]); cc = np.concatenate([[0], np.cumsum(cnt)])
    s = cs[xs[1:]] - cs[xs[:-1]]; n = cc[xs[1:]] - cc[xs[:-1]]
    return j, np.where(n > 0, s / np.maximum(n, 1), np.nan)
img = np.zeros((H, W))
with ThreadPoolExecutor(24) as ex:
    for j, r in ex.map(row, range(H)): img[j] = r
w = np.cos((np.arange(H) + 0.5) / H * np.pi - np.pi / 2)[:, None] * np.ones((1, W))
ok = ~np.isnan(img)
mean = (img[ok] * w[ok]).sum() / w[ok].sum()
print('covered (area) %.1f %%, filled with mean grey %.1f' % (100 * w[ok].sum() / w.sum(), mean))
img[~ok] = mean
Image.fromarray(np.clip(np.round(img), 0, 255).astype(np.uint8), 'L').convert('RGB').save(out, 'JPEG', quality=90, optimize=True)
print(out, W, H)
