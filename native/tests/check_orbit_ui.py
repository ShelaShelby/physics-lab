#!/usr/bin/env python3
# 0.15 stage 4 and later: the Orbit Lab's pictures, checked in the REAL app (Linux, headless on Mesa's software Vulkan; not Windows, not a GPU). tests/TESTS.md lists the parts kept and why.
#   check_orbit_ui.py <PhysicsLab binary> <work folder> [part,part,...]
#   airedge   0.50: just outside Earth's limb the picture is the sky (no blue rim of air)
#   nightside 0.50: Jupiter's night side is the sky's colour (no ambient light)
#   planets   0.77.0: the GPU picture of planets and moons with maps (their measured brightness) against the CPU twin (tests/planet_picture.cpp)
#   review085 0.85.0: true sunlight, an eclipse with the Sun's measured edge, the Sun with its sunspots (0.94.0: filled), against the same twin
#   sun094    0.94.0: the Sun's surface close up (granulation, the spots filled) and with a photo in the cache (a made-up smooth picture), against the same twin
#   rings, ringslant, flyby, cardlines: Saturn's rings, the fly-by and the lines on the card against the processor or the formulas (see the parts)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

HERE = os.path.dirname(os.path.abspath(__file__))
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def seq(*cmds, start=10, step=6):
    out, f = [], start
    for c in cmds:
        out.append(f"{f}:{c}")
        f += step
    out.append(f"{f + 8}:state")
    return ";".join(out), f + 16

def state(r):
    if not r.states:
        return None
    d = {}
    for tok in r.states[-1].replace("|", " ").split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d

class Runner:
    def __init__(self, binary, work, size="1280x800"):
        self.binary, self.work, self.size, self.n = binary, work, size, 0
        self.base = {}
    def go(self, cmds, env=None, size=None, theme="dark", extra_frames=0):
        self.n += 1
        script, frames = seq(*cmds)
        e = {"PL_NEWUI": "on", "PL_SCRIPT": script}
        e.update(env or {})
        return run_app(self.binary, self.work, f"orb{self.n}", e, frames=frames + extra_frames, size=size or self.size, theme=theme)
    def base_state(self, env=None):
        key = tuple(sorted((env or {}).items()))
        if key not in self.base:
            self.base[key] = state(self.go([], env))
        return self.base[key]

def missing(r):
    return r.scripted_missing()

# ------------------------------------------------------------------ the card
CARD_THINGS = [("Sun", "Sun"), ("Earth", "Earth"), ("Jupiter", "Jupiter"), ("Pluto", "Pluto"), ("Moon", "Moon"), ("Halley", "Halley"), ("Asteroid belt", "Asteroid belt"), ("Alpha Centauri", "Alpha Centauri")]

# ------------------------------------------------------------------ 0.50: lighting and the edge of air (pictures; dark theme, Stars Off so the sky is one colour)
def body_shot(binary, work, name, sel, zoom, extra=None):
    env = {"PL_NEWUI": "on", "PL_SELECT": sel, "PL_ZOOM": zoom, "PL_LOG_BODYPX": "1", "PL_SKYOFF": "1"}
    env.update(extra or {})
    r = run_app(binary, work, name, env, frames=40)
    ln = [l.split() for l in r.out.splitlines() if l.startswith("PLBODYPX " + sel + " ")]
    if not ln:
        return None, None
    x, y, rad, lx, ly, lz = map(float, ln[-1][2:8])
    return Bmp(r.bmp), (x, y, rad, lx, ly, lz)

def near(a, b, tol=4):
    return all(abs(p - q) <= tol for p, q in zip(a, b))

def compare_cpu(tool, work, r, name, args, minpx=5000):
    # the GPU's picture (r.bmp) against tests/planet_picture.cpp's: at most 1 % of the pixels inside 0.95 of the outline differ by more than 8 levels in a channel
    import subprocess
    out = os.path.join(work, "planet_cpu_" + name + ".bin")
    p = subprocess.run([tool, out] + args, capture_output=True, text=True)
    check(p.returncode == 0, f"{name}: planet_picture ran ({p.stderr.strip()})")
    if p.returncode != 0:
        return
    d = open(out, "rb").read()
    nl = d.index(b"\n")
    _, w, h, x0, y0 = d[:nl].split()
    w, h, x0, y0 = int(w), int(h), int(x0), int(y0)
    px = d[nl + 1:]
    bmp = Bmp(r.bmp)
    n = bad = 0
    worst = 0
    for j in range(h):
        for i in range(w):
            o = 4 * (j * w + i)
            if px[o + 3] == 0:
                continue
            x, y = x0 + i, y0 + j
            if not (0 <= x < bmp.w and 0 <= y < bmp.h):
                continue
            g = bmp.px(x, y)
            dmax = max(abs(g[c] - px[o + c]) for c in range(3))
            n += 1
            worst = max(worst, dmax)
            bad += dmax > 8
    check(n > minpx and bad <= 0.01 * n, f"{name}: GPU against CPU, {bad} of {n} pixels differ by more than 8 levels (at most 1 %; largest {worst})")
    print(f"planets {name}: {bad} of {n} pixels over 8 levels, largest {worst}")

def part_planets(binary, work):
    # 0.77.0 (outside review item 1): each map is scaled to its body's measured geometric albedo (render/planet_maps.h mapAlbedo) in kPlanetFS. The GPU's
    # picture of the ball against the CPU twin (render/shading.h albedoScaled, ellipsoid.h; tests/planet_picture.cpp): at most 1 % of the pixels inside
    # 0.95 of the outline differ by more than 8 levels in a channel. Bodies without air (ring shadows and eclipses are in the twin): Jupiter (clipped bright zones), Mars, the Moon
    # (McEwen's law, a factor below 1), Europa (a factor its ceiling cannot reach), Pluto, Earth (its clouds and air, 0.77.0 item 2). The axis line and the label may cross a few pixels.
    # (0.85.0: the true sunlight, the eclipse with the measured edge and the Sun are part review085)
    tool = os.path.join(os.path.dirname(os.path.abspath(binary)), "planet_picture")
    check(os.path.exists(tool), "the CPU twin planet_picture is built next to the app")
    if not os.path.exists(tool):
        return
    balls(binary, work, tool, [(sel, zoom, {}) for sel, zoom in (("Jupiter", "3.4"), ("Mars", "4.5"), ("moon:Earth", "6"), ("moon:Jupiter:1", "7"), ("Pluto", "6"), ("Earth", "4.5"))])

def part_review085(binary, work):
    # 0.85.0 (outside review R1-R3, PLAN_REVIEW.md): the GPU against the CPU twin (tests/planet_picture.cpp), at most 1 % of the pixels over 8 levels:
    # True sunlight with Earth, Jupiter and Neptune followed (the exposure set for each; the PLPLANET line's factor is read by the twin); Earth in the Moon's
    # shadow on 8 Apr 2024 18:18 UT (the eclipse with the Sun's measured edge, per channel); the Sun close up on 10 May 2024 00:00 UT (Neckel & Labs's edge
    # and that day's 7 sunspot groups as outlines: kPlanetFS uMode 4).
    tool = os.path.join(os.path.dirname(os.path.abspath(binary)), "planet_picture")
    check(os.path.exists(tool), "the CPU twin planet_picture is built next to the app")
    if not os.path.exists(tool):
        return
    balls(binary, work, tool, [("Earth", "4.5", {"PL_TRUELIGHT": "1", "tag": "true_Earth"}), ("Jupiter", "3.4", {"PL_TRUELIGHT": "1", "tag": "true_Jupiter"}),
                               ("Neptune", "3.9", {"PL_TRUELIGHT": "1", "tag": "true_Neptune"}),
                               ("Earth", "4.5", {"PL_DATE": "2024-04-08", "PL_TIME": "18:18", "PL_PAUSED": "1", "tag": "eclipse_Earth"})])
    r = run_app(binary, work, "planets_sun", {"PL_NEWUI": "on", "PL_SELECT": "Sun", "PL_ZOOM": "2.6", "PL_LOG_PLANET": "1", "PL_SKYOFF": "1", "PL_DATE": "2024-05-10",
                                              "PL_TIME": "00:00", "PL_PAUSED": "1"}, frames=40)
    suns = [l.split()[1:] for l in r.out.splitlines() if l.startswith("PLSUN ")]
    check(bool(suns), f"the Sun: the app logged its ball ({len(suns)} PLSUN lines)")
    if suns:
        f = suns[-1]
        check(float(f[2]) > 60, f"the Sun: drawn big enough to compare ({f[2]} px)")
        check(int(f[12]) == 7, f"the Sun on 10 May 2024: the record's 7 groups (NOAA SRS of that date) are drawn ({f[12]})")
        compare_cpu(tool, work, r, "sun", ["sun"] + f)

def write_grey_png(path, w, h, rows):
    # an 8-bit grey PNG (no filter), standard library only
    import struct, zlib
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    def chunk(k, d):
        c = struct.pack(">I", len(d)) + k + d
        return c + struct.pack(">I", zlib.crc32(k + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

def fake_photo(folder, day):
    # a smooth made-up "photo" with the reply's and header's form (orbit/sun_photo.cpp reads them): 1024 x 1024 (scale 4 of 4096), the disc darkened
    # towards its edge, two soft dark spots and a soft bright patch. The observer at Carrington 313.565, -3.201 (the real header of 10 May 2024).
    import math
    os.makedirs(folder, exist_ok=True)
    n, s = 1024, 4
    cx = cy = (2048.5 - (s + 1) / 2) / s
    rpx = 950.44952 / (0.6 * s)
    blobs = [(0.30, -0.25, 0.06, -0.55), (-0.35, 0.20, 0.04, -0.4), (0.55, 0.40, 0.05, 0.12)]
    rows = []
    for j in range(n):
        row = bytearray(n)
        for i in range(n):
            x, y = (i - cx) / rpx, (cy - j) / rpx
            r2 = x * x + y * y
            if r2 >= 1:
                continue
            mu = math.sqrt(1 - r2)
            v = 1 - 0.55 * (1 - mu) - 0.15 * (1 - mu) ** 2
            for bx, by, bw, a in blobs:
                v *= 1 + a * math.exp(-((x - bx) ** 2 + (y - by) ** 2) / (2 * bw * bw))
            row[i] = max(0, min(255, int(round(220 * v))))
        rows.append(row)
    write_grey_png(os.path.join(folder, day + ".png"), n, n, rows)
    open(os.path.join(folder, day + ".xml"), "w").write("<fits><NAXIS1>4096</NAXIS1><NAXIS2>4096</NAXIS2><CRPIX1>2048.50</CRPIX1><CRPIX2>2048.50</CRPIX2><CDELT1>0.60000002</CDELT1>"
        "<CROTA2>0.00000</CROTA2><RSUN_OBS>950.44952</RSUN_OBS><CRLN_OBS>313.56503</CRLN_OBS><CRLT_OBS>-3.2012870</CRLT_OBS><WAVELNTH>6173.00</WAVELNTH>"
        "<CONTENT>CONTINUUM INTENSITY</CONTENT></fits>")
    open(os.path.join(folder, day + ".json"), "w").write('{"id":"1","date":"' + day + ' 00:00:05","name":"HMI Int"}')

def part_sun094(binary, work):
    # 0.94.0: the Sun's surface (render/sun_surface.h, orbit/sun_photo.h): the GPU against the CPU twin (tests/planet_picture.cpp sun), at most 1 % of the pixels
    # over 8 levels per view. 10 May 2024 00:00 UT, paused, 2D at true scale. (1) Zoom 4 (the Sun's radius about 5,080 px, the window its middle): the
    # granulation (cells about 9 px), no photo (an empty cache, the download off). (2) The same with a made-up smooth photo in the cache for that day.
    # (3) Zoom 2.6 (the whole disc, 202 px): the photo shrunk (the GPU's mip levels; the twin reads the full level: the photo is smooth) and the far side's spots.
    # (The 3D camera's closest view on this 1280 x 800 window makes the cells 1.5 px: faded out. review085 checks the 7 groups filled at zoom 2.6 without a photo.)
    tool = os.path.join(os.path.dirname(os.path.abspath(binary)), "planet_picture")
    check(os.path.exists(tool), "the CPU twin planet_picture is built next to the app")
    if not os.path.exists(tool):
        return
    for tag, photo, zoom in (("drawn", False, "4.0"), ("photo_close", True, "4.0"), ("photo_disc", True, "2.6")):
        cache = os.path.join(work, "sun094_cache_" + tag)
        if os.path.isdir(cache):
            import shutil
            shutil.rmtree(cache)
        os.makedirs(cache, exist_ok=True)
        if photo:
            fake_photo(cache, "2024-05-10")
        env = {"PL_NEWUI": "on", "PL_SELECT": "Sun", "PL_ZOOM": zoom, "PL_LOG_PLANET": "1", "PL_SKYOFF": "1", "PL_DATE": "2024-05-10", "PL_TIME": "00:00",
               "PL_PAUSED": "1", "PL_SUNPHOTO_OFFLINE": "1", "PL_SUNPHOTO_CACHE": cache}
        r = run_app(binary, work, "sun094_" + tag, env, frames=60)
        suns = [l.split()[1:] for l in r.out.splitlines() if l.startswith("PLSUN ")]
        check(bool(suns), f"sun094 {tag}: the app logged its ball ({len(suns)} PLSUN lines)")
        if not suns:
            continue
        f = suns[-1]
        n = int(f[12])
        check(n == 7, f"sun094 {tag}: the 7 groups of 10 May 2024 are drawn ({n})")
        tail = f[13 + 6 * n:]
        check(len(tail) == 7 and tail[0] == "1", f"sun094 {tag}: the granulation is on ({tail[:1]})")
        check((tail[5] == "1") == photo, f"sun094 {tag}: the photo is {'shown' if photo else 'not shown'} ({tail[5]}, {tail[6]})")
        print(f"sun094 {tag}: the Sun's radius {float(f[2]):.0f} px, cells per radian {tail[1]}")
        if zoom == "4.0":   # the disc is far larger than the window: the twin draws only a part no panel covers (the card and Controls are on the right)
            os.environ["PL_PICTURE_CLIP"] = "40 120 580 740"
        compare_cpu(tool, work, r, "sun094_" + tag, ["sun"] + f, minpx=20000)
        os.environ.pop("PL_PICTURE_CLIP", None)

def balls(binary, work, tool, cases):
    for sel, zoom, extra in cases:
        tag = extra.pop("tag", sel.replace(":", "_"))
        env = {"PL_NEWUI": "on", "PL_SELECT": sel, "PL_ZOOM": zoom, "PL_LOG_PLANET": "1", "PL_LOG_BODYPX": "1", "PL_SKYOFF": "1"}
        env.update(extra)
        r = run_app(binary, work, "planets_" + tag, env, frames=40)
        rows = r.out.splitlines()
        lines = [rows[i].split() + rows[i + 1].split()[1:] for i in range(len(rows) - 1) if rows[i].startswith("PLPLANET ") and rows[i + 1].startswith("PLPLANETX")]
        # the selected body: the biggest ball drawn in the last frame with this body's map
        want = {"Jupiter": 5, "Mars": 4, "moon:Earth": 3, "moon:Jupiter:1": 11, "Pluto": 9, "Earth": 2, "Neptune": 8}[sel]
        mine = [l for l in lines[len(lines) * 3 // 4:] if int(l[1]) == want and 0 <= float(l[2]) < 1280 and 0 <= float(l[3]) < 800]
        mine.sort(key=lambda l: float(l[4]))
        check(bool(mine), f"{tag}: the app logged its ball ({len(lines)} PLPLANET lines)")
        if not mine:
            continue
        f = mine[-1][1:]
        check(float(f[3]) > 60, f"{tag}: drawn big enough to compare ({f[3]} px)")
        if tag.startswith("true_"):
            check(abs(float(f[26]) - 1) < 1e-5, f"{tag}: the followed body's sunlight factor is 1 (the exposure is set for it): {f[26]}")
        if tag.startswith("eclipse_"):
            check(int(f[21]) > 0, f"{tag}: the Moon is among the bodies that can eclipse the Sun there ({f[21]})")
        compare_cpu(tool, work, r, tag, f)

def part_airedge(binary, work):
    # review item 2: the blue ring from 0.96 R to 1.16 R round Earth (about 1,100-1,300 km on screen, on the night side too) had no source; the air's scale height is 8.5 km
    # (data/nssdc/earthfact.html), far under a pixel here. Earth about 147 px in radius: just outside the limb (1.07-1.12 R, outside the selection ring) the picture is the sky.
    import math
    bmp, at = body_shot(binary, work, "airedge", "Earth", "4.5")
    check(at is not None and at[2] > 100, f"Earth drawn big enough to look at its edge ({at})")
    if not at:
        return
    x, y, rad = at[:3]
    good = n = 0
    for k in range(72):
        t = 2 * math.pi * k / 72
        p15 = (int(round(x + 1.5 * rad * math.cos(t))), int(round(y + 1.5 * rad * math.sin(t))))
        if not (0 <= p15[0] < bmp.w and 0 <= p15[1] < bmp.h):
            continue
        sky = bmp.px(*p15)
        for f in (1.07, 1.09, 1.12):
            n += 1
            good += near(bmp.px(int(round(x + f * rad * math.cos(t))), int(round(y + f * rad * math.sin(t)))), sky)
    check(n >= 100 and good >= 0.9 * n, f"just outside Earth's limb the picture is the sky's colour: {good} of {n} samples (orbit lines and the label may cross a few)")

def part_nightside(binary, work):
    # review item 3: the lit ball had a 12 % ambient floor, so Jupiter's night side showed its bands. Space gives no light from all round: a pixel whose surface faces
    # away from the Sun (cos i < -0.15, a margin for the planet's flattening and the smooth edge) inside 0.95 R is the sky's colour (the sky taken at 1.5 R on the night side).
    import math
    bmp, at = body_shot(binary, work, "nightside", "Jupiter", "3.4")
    check(at is not None and at[2] > 80, f"Jupiter drawn big enough ({at})")
    if not at:
        return
    x, y, rad, lx, ly, lz = at
    ln = math.sqrt(lx * lx + ly * ly + lz * lz) or 1
    lx, ly, lz = lx / ln, ly / ln, lz / ln
    skyp = (int(round(x - 1.5 * rad * lx / max(math.hypot(lx, ly), 1e-9))), int(round(y - 1.5 * rad * ly / max(math.hypot(lx, ly), 1e-9))))
    check(0 <= skyp[0] < bmp.w and 0 <= skyp[1] < bmp.h, f"sky sample on screen {skyp}")
    if not (0 <= skyp[0] < bmp.w and 0 <= skyp[1] < bmp.h):
        return
    sky = bmp.px(*skyp)
    good = n = 0
    step = max(1, int(rad / 25))
    for py in range(int(y - rad), int(y + rad) + 1, step):
        for px_ in range(int(x - rad), int(x + rad) + 1, step):
            u, v = (px_ - x) / rad, (py - y) / rad
            d2 = u * u + v * v
            if d2 > 0.95 ** 2 or not (0 <= px_ < bmp.w and 0 <= py < bmp.h):
                continue
            if u * lx + v * ly + math.sqrt(1 - d2) * lz < -0.15:
                n += 1
                good += near(bmp.px(px_, py), sky)
    check(n >= 50 and good >= 0.95 * n, f"Jupiter's night side is the sky's colour: {good} of {n} pixels (sky {sky}; the axis line and the label may cross a few)")

def part_rings(binary, work):
    # 0.64.0: Saturn's rings per pixel on the card (Vulkan): the start-up check passes here (five views, each at most 1 % of the ring pixels over 8 levels
    # from the processor's triangles), the benchmark's Path names the card; made to fail on purpose (PL_RINGS_CARDFAIL), the processor's triangles are kept
    # and the Path says why. Then the two pictures (card, processor) of the same moment differ in under 2 % of the pixels by more than 8 levels.
    import re
    BENCH = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0.016", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": "orbit.saturn",
             "PL_BENCH_WARMUP": "0.3", "PL_BENCH_MEASURE": "0.7", "PL_BENCH_SHOT": "0.5"}
    # (0.78.0: 0.7 s counted, the picture at 0.5 s; was 2.0 s and 1.5 s. The CPU run draws about 5 frames a second on the build machine's software GPU,
    # so the part took 243-300 s on GitHub, at the 5-minute limit. The checks are the same: the start-up check, both Paths, and one moment in both runs.)
    runs = {}
    for tag, extra in (("card", {}), ("cpu", {"PL_RINGS_CARDFAIL": "1"})):
        d = os.path.join(work, "rings_" + tag); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            if f.endswith(".bmp"): os.remove(os.path.join(d, f))
        runs[tag] = run_app(binary, d, "rings_" + tag, dict(BENCH, **extra), frames=100000, size="1280x800", timeout=900)
    out = runs["card"].out
    cases = [ln for ln in out.splitlines() if ln.startswith("PLRINGCARDCASE ")]
    pct = [float(m.group(1)) for m in (re.search(r"\(([\d.]+) %, limit 1 %\)", ln) for ln in cases) if m]
    check("PLRINGCARD state=1" in out and len(pct) == 5 and all(p <= 1.0 for p in pct), f"rings: the start-up check passes, five views within 1 % ({cases})")   # (0.75.0, on purpose: a fourth view, the pinhole camera close by; 0.75.1: a fifth, 30 degrees below the plane, orbit/ring_check.h)
    rep = out[out.find("PLBENCH-RESULT-BEGIN"):out.find("PLBENCH-RESULT-END")]
    path = [ln.strip() for ln in rep.splitlines() if ln.strip().startswith("Path:")]
    check(any("Saturn's rings: the graphics card (Vulkan; its start-up check passed" in p for p in path), f"rings: the benchmark's Path names the card ({path})")
    out2 = runs["cpu"].out
    rep2 = out2[out2.find("PLBENCH-RESULT-BEGIN"):out2.find("PLBENCH-RESULT-END")]
    path2 = [ln.strip() for ln in rep2.splitlines() if ln.strip().startswith("Path:")]
    check("PLRINGCARD state=-1" in out2 and any("Saturn's rings: the processor, because the check was made to fail" in p for p in path2), f"rings: a failed check keeps the processor's triangles and says why ({path2})")
    try:
        a, b = Bmp(os.path.join(work, "rings_card", "orbit.saturn.bmp")), Bmp(os.path.join(work, "rings_cpu", "orbit.saturn.bmp"))
        n = over = 0
        for y in range(0, a.h, 2):
            for x in range(0, a.w, 2):
                n += 1
                over += max(abs(p - q) for p, q in zip(a.px(x, y)[:3], b.px(x, y)[:3])) > 8
        check(n > 0 and over <= 0.02 * n, f"rings: the card's picture and the processor's of the same moment: {over} of {n} pixels over 8 levels")
    except Exception as e:
        check(False, f"rings: both pictures saved ({e})")

def part_ringslant(binary, work):
    # 0.75.1: close up the rings' near and far parts are seen at very different slants, so their opacity and light differ as the formulas say. (Not from the closest zoom, 1.5 radii:
    # there the camera sits at the B ring's inner edge, the B ring is past it or behind the planet's ball, and the picture holds only the D and C rings' edge; the closest camera's
    # views are the card check's fifth and fourth views and part_flyby. Here: zoom 3.7, the camera 4.4 radii from the middle, 29 degrees above the ring plane, the Sun behind it.) The reference is written here from the logged plane (PL_RINGS_LOG: the ring plane's axes, the pole, the middle, the pinhole, the Sun) and the saved
    # Cassini profile (src/orbit/saturn_profile.inc, the data the app reads), by the textbook pinhole and single-scattering arithmetic, not the app's functions: for a block of
    # 9 x 9 pixels at the B ring's near-most and far-most lit points that are on the picture, each pixel's ray meets the ring plane (the plane's normal n, the camera at the origin:
    # mu = |n . P| / |P|), its band's opacity is the mean of 1 - exp(-tau / mu) and its light the mean of albedo / 0.5 x the Chandrasekhar slab (blocked over the band's
    # 10 km measurements), the colour the tint x light in linear light, the picture's black between. Limits (written before the first look): the mean of a block within 8 levels
    # in each channel of the picture's (the card check's level), the two blocks' slants over 20 % apart, and the brighter of the two the one the formulas make brighter.
    import math, re, bisect
    inc = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "orbit", "saturn_profile.inc")).read()
    arr = lambda name: [float(v) for v in re.search(name + r"\[kSatProfileN\] = \{([^}]*)\}", inc).group(1).split(",")]
    km, tau = arr("kSatProfileKm"), arr("kSatProfileTau")
    n = len(km)
    elo = [km[0] - 0.5 * (km[1] - km[0])] + [0.5 * (km[i - 1] + km[i]) for i in range(1, n)]
    ehi = [elo[i + 1] for i in range(n - 1)] + [km[-1] + 0.5 * (km[-1] - km[-2])]
    EQ = 60268.0
    def alb(k):                                    # NASA's fact sheet's middle albedo of each ring region (saturn_rings.h): C (and D) 0.21, B 0.5, Division 0.3, A 0.5, beyond 0.6
        return 0.21 if k < 91975 else 0.5 if k < 117507 else 0.3 if k < 122340 else 0.5 if k < 136780 else 0.6
    def dec(c): return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    def enc(l): l = min(max(l, 0.0), 1.0); return 12.92 * l if l <= 0.0031308 else 1.055 * l ** (1 / 2.4) - 0.055
    TINT = (0.91, 0.83, 0.66)
    env = {"PL_NEWUI": "on", "PL_SELECT": "Saturn", "PL_3D": "1", "PL_ZOOM": "3.7", "PL_SKYOFF": "1", "PL_RINGS_LOG": "1", "PL_NO_MSAA": "1", "PL_LOG_BODYPX": "1",
           "PL_SCRIPT": "10:view=3.7,1.25,4.4"}   # (8.6 degrees above the ring plane, the Sun behind the camera: the lit face, the rings across the lower left)
    r = run_app(binary, work, "ringslant", env, frames=50, size="1920x1080")   # (wide: the rings fall in the free part of the picture, left of the cards)
    ln = [l for l in r.out.splitlines() if l.startswith("PLRINGFRAME ")]
    check(bool(ln), "ringslant: the ring plane is logged")
    if not ln:
        return
    v = [float(t) for t in ln[-1].replace("|", " ").split()[1:]]
    x, y, rad, e1, e2 = v[0], v[1], v[2], v[3:6], v[6:9]
    ppx, ppy, zr, wn, pole, mu0, sunlit = v[9], v[10], v[11], v[12], v[13:16], v[16], int(v[17])
    K, sz, Lb = v[18:27], v[27], v[28:31]
    F = rad / zr
    ux, uy = (x - ppx) / F, -(y - ppy) / F                      # the middle over the focal length, in units of its distance
    nd = (pole[0], pole[1], -pole[2])                           # the pole in (right, up, depth)
    S = (ux, uy, 1.0)
    h = sum(a * b for a, b in zip(nd, S))
    def ray_point(sx, sy):                                      # the pixel's ray meets the plane: P (units of the middle's distance), its q (radii, screen frame), mu
        d = ((sx - ppx) / F, -(sy - ppy) / F, 1.0)
        den = sum(a * b for a, b in zip(nd, d))
        if abs(den) < 1e-9: return None
        t = h / den
        if t <= 0: return None
        P = (t * d[0], t * d[1], t * d[2])
        q = ((P[0] - S[0]) / zr, (P[1] - S[1]) / zr, -(P[2] - S[2]) / zr)
        if not (1 - zr * q[2] >= wn): return None
        return q, abs(h) / math.sqrt(sum(c * c for c in P))
    def shadow(q):
        qb = [K[3 * i] * q[0] + K[3 * i + 1] * q[1] + K[3 * i + 2] * q[2] for i in range(3)]
        qb[2] /= sz
        LL = sum(c * c for c in Lb); t = -sum(a * b for a, b in zip(qb, Lb)) / LL
        d = [qb[i] + t * Lb[i] for i in range(3)]; f = sum(c * c for c in d) - 1
        return max(f, 1e-3) if t <= 0 else f
    def stretch(vv):                                            # a vector from the middle (radii, screen frame) in the planet's own frame, the pole axis divided by sz: the ball is a unit ball there
        return [K[0] * vv[0] + K[1] * vv[1] + K[2] * vv[2], K[3] * vv[0] + K[4] * vv[1] + K[5] * vv[2], (K[6] * vv[0] + K[7] * vv[1] + K[8] * vv[2]) / sz]
    def hidden(q):                                              # the planet's ball is between the camera and the ring point (the ball hides the half of the rings behind it)
        a, b = stretch((-ux / zr, -uy / zr, 1 / zr)), stretch(q)
        dd = [b[i] - a[i] for i in range(3)]
        u = min(max(-sum(a[i] * dd[i] for i in range(3)) / sum(c * c for c in dd), 0.0), 1.0)
        return sum((a[i] + u * dd[i]) ** 2 for i in range(3)) < 1.0
    # the drawn groups as the app cuts them (width: at least 10 km, a pixel, a 700th of the profile; each starting where the one before ends)
    wkm = max(10.0, EQ / rad, (km[-1] - km[0]) / 700)
    groups, i = [], 0
    while i < n:
        j = i
        while j < n and (j == i or ehi[j] - elo[i] <= wkm): j += 1
        groups.append((i, j)); i = j
    bands, prev = [], 0.0
    for i, j in groups:
        a, b = elo[i] / EQ, ehi[j - 1] / EQ
        wpx = (b - a) * rad; dpx = max(wpx, 1.0); mid = 0.5 * (a + b)
        a0 = max(mid - 0.5 * dpx / rad, prev); a1 = mid + 0.5 * dpx / rad
        if a1 > a0: bands.append((a0, a1, i, j, wpx / dpx)); prev = a1
    starts = [b[0] for b in bands]
    def pixel(sx, sy):                                          # the expected picture's value at a pixel centre (the picture's black between the rings), 0..255 a channel
        rp = ray_point(sx, sy)
        if rp is None: return (0.0, 0.0, 0.0), 0.0
        q, mu = rp
        if hidden(q): return (0.0, 0.0, 0.0), 0.0
        mu = max(mu, 0.004)
        rr = math.sqrt(sum(c * c for c in q))
        bi = bisect.bisect_right(starts, rr) - 1
        for a0, a1, i, j, cov in bands[bi:bi + 1] if bi >= 0 else []:
            if a0 <= rr < a1:
                sb = ss = 0.0
                for m in range(i, j):
                    t = tau[m]
                    sb += 1 - math.exp(-t / mu)
                    sh = (mu0 / (mu0 + mu) * (1 - math.exp(-t * (1 / mu + 1 / mu0))) if sunlit else
                          (t / mu * math.exp(-t / mu) if abs(mu0 - mu) < 1e-9 else mu0 / (mu0 - mu) * (math.exp(-t / mu0) - math.exp(-t / mu)))) / 0.5
                    ss += alb(km[m]) / 0.5 * sh
                alpha, light = sb / (j - i) * cov, ss / (j - i) * cov
                k = min(1.0, light / alpha) if alpha > 1e-6 else 0.0
                lit = shadow(q) >= 0
                return tuple(255 * alpha * (enc(dec(c) * k) if lit else 0.0) for c in TINT), alpha
        return (0.0, 0.0, 0.0), 0.0
    # the near-most and far-most points of the B ring (1.54 to 1.93 radii) that are on the picture clear of the panels and bars, lit, and (so the planet behind does not show
    # through) opaque in every pixel of their 9 x 9 block (opacity over 0.95 by the formulas)
    bmp = Bmp(r.bmp)
    cand = []
    for ri in range(14):
        rr = 1.54 + 0.03 * ri
        for deg in range(0, 360):
            t = math.radians(deg)
            q = [rr * (math.cos(t) * e1[k] + math.sin(t) * e2[k]) for k in range(3)]
            w = 1 - zr * q[2]
            if not (w >= wn): continue
            X, Y = ppx + (x - ppx + rad * q[0]) / w, ppy + (y - ppy - rad * q[1]) / w
            if not (14 <= X < bmp.w - 690 and 64 <= Y < bmp.h - 14) or shadow(q) < 0 or hidden(q): continue   # (the left of the picture: the Saturn card and the Controls panel stand over the right)
            rp = ray_point(X, Y)
            if rp and all(pixel(int(X) + dx + 0.5, int(Y) + dy + 0.5)[1] > 0.95 for dx in range(-4, 5, 2) for dy in range(-4, 5, 2)): cand.append((rp[1], X, Y))
    check(len(cand) >= 2, f"ringslant: opaque lit B ring points are on the picture ({len(cand)})")
    if len(cand) < 2:
        return
    cand.sort()
    far, near = cand[0], cand[-1]
    check(near[0] > 1.2 * far[0], f"ringslant: the near and far parts are seen at slants over 20 % apart (mu {near[0]:.3f} and {far[0]:.3f})")
    got = {}
    for tag, (mu, X, Y) in (("near", near), ("far", far)):
        ex = [0.0, 0.0, 0.0]; ms = [0.0, 0.0, 0.0]; cnt = 0
        for dy in range(-4, 5):
            for dx in range(-4, 5):
                sx, sy = int(X) + dx, int(Y) + dy
                e = pixel(sx + 0.5, sy + 0.5)[0]; g = bmp.px(sx, sy)[:3]
                for c in range(3): ex[c] += e[c]; ms[c] += g[c]
                cnt += 1
        ex = [c / cnt for c in ex]; ms = [c / cnt for c in ms]
        got[tag] = (ex, ms)
        print(f"  ringslant {tag}: mu {mu:.3f}, expected block mean {[round(c, 1) for c in ex]}, picture {[round(c, 1) for c in ms]}")
        check(all(abs(a - b) <= 8 for a, b in zip(ex, ms)), f"ringslant: the {tag} part of the B ring is as bright as the formulas say (expected {[round(c, 1) for c in ex]}, picture {[round(c, 1) for c in ms]}; 8 levels)")
    de = sum(got["near"][0]) - sum(got["far"][0]); dm = sum(got["near"][1]) - sum(got["far"][1])
    check(abs(de) > 30 and (de > 0) == (dm > 0), f"ringslant: the near and far parts differ in brightness as the formulas say (formulas {de:+.0f}, picture {dm:+.0f}, summed over the channels)")

def part_flyby(binary, work):
    # 0.75.0: the new benchmark scene orbit.flyby (Saturn from the closest zoom, the camera through the ring plane: lines, rings and rocks cut at the near plane on the
    # card): the card's picture against the processor's (PL_NO_LINES_CARD, PL_RINGS_CARDFAIL; the rocks on the card in both: tests/vk_rocks.inc checks them) of the same moment, multisampling off, validation on, no
    # errors: at most 1 % of the pixels over 8 levels (the Vulkan picture limit)
    BENCH = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0.016", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": "orbit.flyby", "PL_NO_MSAA": "1",
             "PL_BENCH_WARMUP": "0.3", "PL_BENCH_MEASURE": "2.0", "PL_BENCH_SHOT": "1.5"}
    runs = {}
    for tag, extra in (("card", {"PL_VK_VALIDATION": "1"}), ("cpu", {"PL_RINGS_CARDFAIL": "1", "PL_NO_LINES_CARD": "1"})):
        d = os.path.join(work, "flyby_" + tag); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            if f.endswith(".bmp"): os.remove(os.path.join(d, f))
        runs[tag] = run_app(binary, d, "flyby_" + tag, dict(BENCH, **extra), frames=100000, size="1280x800", timeout=900)
    card = runs["card"]
    errs = [l for l in card.err.splitlines() if l.startswith("PLVKVALIDATION error")]
    check("PLVKVALIDATION missing" not in card.err and not errs and "PLLINESCARD state=1" in card.out and "PLRINGCARD state=1" in card.out,
          f"flyby: the card drew the lines and the rings, validation on, no errors ({errs[:2]})")
    try:
        a, b = Bmp(os.path.join(work, "flyby_card", "orbit.flyby.bmp")), Bmp(os.path.join(work, "flyby_cpu", "orbit.flyby.bmp"))
        n = over = 0
        for y in range(a.h):
            for x in range(a.w):
                n += 1
                over += max(abs(p - q) for p, q in zip(a.px(x, y)[:3], b.px(x, y)[:3])) > 8
        print(f"  flyby: card against processor, {over} of {n} pixels over 8 levels ({100 * over / max(1, n):.3f} %)")
        check(n > 0 and over <= 0.01 * n, f"flyby: the card's picture and the processor's of the same moment: {over} of {n} pixels over 8 levels")
    except Exception as e:
        check(False, f"flyby: both pictures saved ({e})")

def part_cardlines(binary, work):
    # 0.68.0: the Orbit Lab's trails, orbits and distance rings on the card (Vulkan): its start-up check passes here (three cases against the processor's
    # LinePen, orbit/lines_check.h), and the pictures of three views (the benchmark's orbit.heaviest, orbit.real_fast, and orbit.earth's close-up), card
    # against processor (PL_NO_LINES_CARD), multisampling off, differ in at most 1 % of the pixels by more than 8 levels (the Vulkan picture limit). Made to
    # fail on purpose (PL_LINES_CARDFAIL) the processor's lines are drawn: that picture is the processor's.
    def over(x, y):
        px, py = x.pixels(), y.pixels()
        st = x.bpp
        return sum(1 for i in range(0, min(len(px), len(py)) - st + 1, st) if max(abs(px[i] - py[i]), abs(px[i + 1] - py[i + 1]), abs(px[i + 2] - py[i + 2])) > 8)
    for sc in ("orbit.heaviest", "orbit.real_fast", "orbit.earth", "orbit.alphacen"):   # (0.73.0: and the Alpha Centauri scene's ellipses, validation on)
        env = {"PL_NO_MSAA": "1", "PL_BENCH_SETUP": sc}
        card = run_app(binary, work, "lines_card_" + sc, dict(env, PL_VK_VALIDATION="1") if sc == "orbit.alphacen" else env, frames=60)
        if sc == "orbit.alphacen":
            errs = [l for l in card.err.splitlines() if l.startswith("PLVKVALIDATION error")]
            check("PLVKVALIDATION missing" not in card.err and not errs and "PLLINESCARD state=1" in card.out, f"lines, {sc}: the card drew it, validation on, no errors ({errs[:2]})")
        cpu = run_app(binary, work, "lines_cpu_" + sc, dict(env, PL_NO_LINES_CARD="1"), frames=60)
        if sc == "orbit.heaviest":
            cases = [ln for ln in card.out.splitlines() if ln.startswith("PLLINESCASE ")]
            check("PLLINESCARD state=1" in card.out and len(cases) == 6, f"lines: the start-up check passes, six cases ({cases})")   # (0.73.0, on purpose: a fourth case, the Alpha Centauri scene's ellipses; 0.75.0: a fifth and sixth, the camera by a line that passes it and goes behind it, orbit/lines_check.h)
            bad = run_app(binary, work, "lines_fail", dict(env, PL_LINES_CARDFAIL="1"), frames=60)
            check("PLLINESCARD state=-1" in bad.out, "lines: a failed check leaves the processor's lines")
            o2 = over(Bmp(bad.bmp), Bmp(cpu.bmp))   # (two runs of the processor's own picture already differ in a pixel or two here: at most 0.01 %)
            check(o2 <= 100, f"lines: the failed check's picture is the processor's ({o2} pixels over 8 levels)")
        a, b = Bmp(card.bmp), Bmp(cpu.bmp)
        o = over(a, b)
        print(f"  {sc}: card against processor, {o} pixels over 8 levels ({100 * o / max(1, a.w * a.h):.3f} %)")
        check((a.w, a.h) == (b.w, b.h) and a.w > 0 and o <= 0.01 * a.w * a.h, f"lines, {sc}: the card's picture is the processor's within the limit ({o} pixels over 8 levels)")

def main():
    binary, work = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None
    want = lambda n: only is None or n in only
    os.makedirs(work, exist_ok=True)
    for name, fn in (("airedge", part_airedge), ("nightside", part_nightside), ("planets", part_planets), ("rings", part_rings), ("ringslant", part_ringslant), ("flyby", part_flyby), ("cardlines", part_cardlines), ("review085", part_review085), ("sun094", part_sun094)):
        if want(name):
            fn(binary, work)
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
