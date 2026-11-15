# 0.26: part of making assets/maps (see assets/maps/README.md).
# Pluto: the part New Horizons did not see (no data, 0 in the USGS mosaic; the south was in polar night in July 2015)
# is filled with the mean grey of the mapped part, so it does not look like dark ground.
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('L')).astype(float); H, W = a.shape
lat = 90 - (np.arange(H) + 0.5) * 180 / H
mask = (a < 4) & (lat[:, None] < -10)
mean = a[~mask].mean()
print('unseen fraction of pixels', mask.mean().round(3), 'fill grey', round(mean, 1))
a[mask] = mean
Image.fromarray(np.rint(a).astype(np.uint8)).convert('RGB').save(sys.argv[2], 'JPEG', quality=90, optimize=True)
