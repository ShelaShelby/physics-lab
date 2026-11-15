# 0.26: part of making assets/maps (see assets/maps/README.md).
# Shrink a huge strip-stored equirectangular TIFF to W x H by reading only k rows per output row (HTTP range reads),
# then averaging those rows and boxes of columns. Writes a PNG (lossless) for the next step.
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
print('source', SW, SH, C, flush=True)
xs = (np.arange(W + 1) * SW / W).astype(int)
def row(j):
    y0, y1 = j * SH / H, (j + 1) * SH / H
    acc = None
    for t in range(k):
        y = min(SH - 1, int(y0 + (t + 0.5) * (y1 - y0) / k))
        a = ds().read(window=Window(0, y, SW, 1)).astype(np.float64)[:, 0, :]   # C x SW
        acc = a if acc is None else acc + a
    acc /= k
    cs = np.cumsum(np.concatenate([np.zeros((C, 1)), acc], axis=1), axis=1)
    return j, ((cs[:, xs[1:]] - cs[:, xs[:-1]]) / (xs[1:] - xs[:-1])).T     # W x C
img = np.zeros((H, W, C))
done = 0
with ThreadPoolExecutor(24) as ex:
    for j, r in ex.map(row, range(H)):
        img[j] = r; done += 1
        if done % 128 == 0: print(done, flush=True)
a = np.clip(np.rint(img), 0, 255).astype(np.uint8)
Image.fromarray(a[:, :, 0] if C == 1 else a[:, :, :3]).save(out)
print('wrote', out)
