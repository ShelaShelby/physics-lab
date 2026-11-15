# 0.26: a true-colour map of Saturn, Uranus or Neptune from Hubble OPAL global maps (FITS, one per filter;
# see assets/maps/README.md). For every pixel: FITS value x the README's I/F factor = I/F (limb darkening
# already removed by OPAL's Minnaert correction) at each filter's pivot wavelength -> a spectrum, linear
# between the filters and flat outside them -> CIE 1931 2 deg XYZ under D65 -> linear sRGB.
# The result is put on the app's layout: 2048 x 1024, east longitude 0..360 from the left edge (180 E in the
# middle), planetocentric latitude (tan(lat_c) = (Rp/Re)^2 tan(lat_g), radii from the README), by bilinear
# interpolation from the source grid (each README states the source's longitude direction; the files are
# either cell-centred, 1800 x 900, or on a grid that stores both 0 and 360, 721 x 361).
# Pixels without clean data (a filter missing, within 3 pixels of a gap, or within 15 deg of one and jumping in colour or brightness)
# are filled: inside a row along the row; whole rows by blending the clean rows above and below (thin gaps),
# or past the last clean row (a pole, a hemisphere in darkness) with the mean colour of the last 2 deg of data.
#   map_opal.py planet out.jpg   (the FITS files and pivots.txt in the current folder)
# pivots.txt (a copy is tools/map_opal_pivots.txt): "filter nm" per line, the pivot wavelengths of the WFC3
# Instrument Handbook, Table 6.2 (hst-docs.stsci.edu/wfc3ihb, 6.5 UVIS Spectral Elements).
import sys, glob, numpy as np, colour
from astropy.io import fits
from PIL import Image
planet, out = sys.argv[1], sys.argv[2]
P = {   # the READMEs: I/F scale factors, ellipsoid radii (km), longitude direction of the map
 'saturn':  dict(fac={'f395n': .00231, 'f467m': .00507, 'f502n': .00285, 'f631n': .00362, 'fq727n': .00422, 'f763m': .00376}, re=60268, rp=54364, west=True),
 'uranus':  dict(fac={'f467m': .00317, 'f547m': .00266, 'fq619n': .00171, 'f657n': .00440, 'fq727n': .00044, 'f763m': .00118}, re=25559, rp=24973, west=False, south_limit=4.0),
 'neptune': dict(fac={'f467m': .00496, 'f547m': .00257, 'fq619n': .00161, 'f657n': .00413, 'fq727n': .00206, 'f763m': .00184}, re=24764, rp=24341, west=True),
}[planet]
# west=True: "left edge = 360 W longitude, decreasing to the right" (Saturn, Neptune): east longitude grows to the right from 0 at the left.
# west=False: "right edge = 0 E longitude, increasing to the left" (Uranus): east longitude grows to the LEFT from 0 at the right.
piv = dict(l.split() for l in open('pivots.txt') if l.strip())
# The quad filters FQ619N and FQ727N are only 6 nm wide and sit in the cores of methane absorption bands, so a line through them
# would make the whole band (and the brighter light between the bands, which the eye sees too) as dark as its core; they are left out
# of the colour (tested on Uranus: with them red/green 0.18, measured 0.57). The broad and medium filters sample the continuum.
filt = sorted([k for k in P['fac'] if not k.startswith('fq')], key=lambda k: float(piv[k]))
maps = []
for k in filt:
    f = glob.glob(f'*_{k}_v1_globalmap.fits'); assert len(f) == 1, (k, f)
    maps.append(fits.getdata(f[0]).astype(float) * P['fac'][k])
M = np.stack(maps, -1)
H, W, n = M.shape
grid = (W % 2 == 1) and np.allclose(M[:, 0], M[:, -1])     # 0 and 360 both stored: a grid, not cells
if grid: M = M[:, :-1]; W -= 1
wl = np.array([float(piv[k]) for k in filt])
# ---- clean data ----
valid = (M > 0).all(-1)
def grow(m, steps):
    for _ in range(steps):
        m = m | np.roll(m, 1, 1) | np.roll(m, -1, 1) | np.vstack([m[1:], m[-1:]]) | np.vstack([m[:1], m[:-1]])
    return m
valid &= ~grow(~valid, 3)
# next to a large gap (10 deg of latitude or more: a hemisphere in darkness, a pole) the last 4 deg of data are looked at almost edge-on,
# where the Minnaert correction no longer holds and the filters fall off differently: they are dropped too (a thin gap such as Saturn's
# ring strip keeps its edges)
dlat = 180 / (H - 1) if grid else 180 / H
# Uranus 2025: its southern half is in darkness and unseen; towards that edge the F547M brightness starts to slide at 0 to 3 deg N in
# every longitude (checked column by column) and falls almost to zero by 10 deg S: the view there is edge-on. Only data north of
# 4 deg N (planetographic) are used.
if P.get('south_limit') is not None:
    lat_rows = 90 - (np.arange(H) * dlat if grid else (np.arange(H) + 0.5) * dlat)
    valid[lat_rows < P['south_limit']] = False
big, trim = int(round(10 / dlat)), int(round(4 / dlat))
for c in range(W):
    col = ~valid[:, c]; r = 0
    while r < H:
        if col[r]:
            e = r
            while e < H and col[e]: e += 1
            if e - r >= big: valid[max(0, r - trim):r, c] = False; valid[e:e + trim, c] = False
            r = e
        else: r += 1
# ---- colour ----
g5 = np.arange(360, 781, 5.0)
shape = colour.SpectralShape(360, 780, 5)
cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer'].copy().align(shape).values
d65 = colour.SDS_ILLUMINANTS['D65'].copy().align(shape).values
kk = d65[:, None] * cmfs / (d65 * cmfs[:, 1]).sum()
B = np.zeros((n, 3))
for j in range(n):
    e = np.zeros(n); e[j] = 1
    B[j] = (np.interp(g5, wl, e)[:, None] * kk).sum(0)     # linear interpolation is linear in the filter values: XYZ = I/F . B
RGB = colour.XYZ_to_RGB((M @ B).reshape(-1, 3), colour.RGB_COLOURSPACES['sRGB'], apply_cctf_encoding=False).reshape(H, W, 3)
RGB = np.clip(RGB, 0, None)
near = grow(~valid, int(round(15 / dlat)))   # within 15 deg of missing data
Yv = (M @ B)[..., 1]
with np.errstate(divide='ignore', invalid='ignore'):
    lrg, lbg = np.log(RGB[..., 0] / RGB[..., 1]), np.log(RGB[..., 2] / RGB[..., 1])
nodata = ~valid
for r in range(H):
    ok = valid[r]
    if ok.sum() < W // 4: continue
    mr, mb = np.median(lrg[r, ok]), np.median(lbg[r, ok])
    my = np.median(Yv[r, ok])
    with np.errstate(divide='ignore', invalid='ignore'):
        dy = np.abs(np.log(Yv[r] / my))
    # near the edges of the data: a colour jump (0.1 in log r/g or b/g) or a brightness jump (more than 15 %) against the row is an
    # edge or a mosaic seam (Neptune 2025: brightness ringing from 43 to 56 N exactly at the seams, 182 E and 290 E), not the planet
    nodata[r] |= near[r] & ok & ((np.abs(lrg[r] - mr) > 0.1) | (np.abs(lbg[r] - mb) > 0.1) | (dy > np.log(1.15)))
print('edge pixels dropped for colour: %d' % (nodata & valid).sum())
# ---- fill gaps on the source grid ----
lat_g = 90 - (np.arange(H) * 180 / (H - 1) if grid else (np.arange(H) + 0.5) * 180 / H)
good = nodata.mean(1) < 0.05
x = np.arange(W)
for r in np.where(good)[0]:
    if nodata[r].any():
        ok = ~nodata[r]
        for c in range(3): RGB[r, ~ok, c] = np.interp(x[~ok], x[ok], RGB[r, ok, c], period=W)
gi = np.where(good)[0]
edge_rows = max(1, int(round(2 * (H - (1 if grid else 0)) / 180)))
for r in np.where(~good)[0]:
    above, below = gi[gi < r], gi[gi > r]
    if len(above) and len(below):
        a, b = above[-1], below[0]; s = (r - a) / (b - a)
        RGB[r] = RGB[a] * (1 - s) + RGB[b] * s
    else:
        edge = above[-edge_rows:] if len(above) else below[:edge_rows]
        RGB[r] = RGB[edge].reshape(-1, 3).mean(0)
print('clean data, planetographic latitude: %.1f to %.1f' % (lat_g[good].max(), lat_g[good].min()))
# ---- onto the app's layout ----
OW, OH = 2048, 1024
lon_e = (np.arange(OW) + 0.5) * 360 / OW                   # east longitude of each output column
lat_c = 90 - (np.arange(OH) + 0.5) * 180 / OH              # planetocentric latitude of each output row
lat_s = np.degrees(np.arctan(np.tan(np.radians(lat_c)) * (P['re'] / P['rp']) ** 2))   # its planetographic latitude
step = 360 / W
if P['west']:  col = lon_e / step - (0 if grid else 0.5)             # column j holds east longitude j*step (grid) or (j+0.5)*step (cells)
else:          col = (360 - lon_e) / step - (0 if grid else 0.5)     # Uranus: column j holds east longitude 360 - j*step
row = (90 - lat_s) / (180 / (H - 1)) if grid else (90 - lat_s) / (180 / H) - 0.5
row = np.clip(row, 0, H - 1)
c0 = np.floor(col).astype(int); tc = (col - c0)[None, :, None]; c0 %= W; c1 = (c0 + 1) % W
r0 = np.floor(row).astype(int); tr = (row - r0)[:, None, None]; r1 = np.minimum(r0 + 1, H - 1)
O = (RGB[r0][:, c0] * (1 - tc) + RGB[r0][:, c1] * tc) * (1 - tr) + (RGB[r1][:, c0] * (1 - tc) + RGB[r1][:, c1] * tc) * tr
w = np.cos(np.radians(lat_c))[:, None]
m = (O * w[..., None]).sum((0, 1)) / (w.sum() * OW)
print('filters', filt, 'pivots', wl)
print('area-mean linear sRGB', m.round(4), ' r/g %.2f b/g %.2f' % (m[0] / m[1], m[2] / m[1]))
print('pixels brighter than white (clipped): %.3f %%' % ((O > 1).any(-1).mean() * 100))
enc = colour.cctf_encoding(np.clip(O, 0, 1), function='sRGB')
Image.fromarray(np.round(enc * 255).astype(np.uint8)).save(out, 'JPEG', quality=90, optimize=True)
