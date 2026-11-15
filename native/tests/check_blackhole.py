#!/usr/bin/env python3
# 0.55.0: the Black Hole Lab in the REAL app (Linux, Mesa's software OpenGL; not a real graphics card; counts and pictures, no timing).
#   open:      the top bar's Black Hole tab opens the lab: its picture (a black shadow in the middle, the bent sky round it), its card, its bar
#   presets:   M87* and Sagittarius A* set the card's mass and measured ring to presets.h's numbers (read from src/blackhole/presets.cpp)
#   distance:  three distances (one typed in the Controls panel): the card's shadow size is Synge's (Perlick & Tsupko 2022, eq. (7), written here in Python) and grows as
#              the camera comes closer; the black disc on the picture has the radius the lab says (to a pixel)
#   light:     Gravity bends light Off and Sky: Black change the picture (off: the middle is sky, not shadow; black: the whole picture is black)
#   realest:   Most realistic sets bending on and our sky
#   orbitcard: the Orbit Lab's Sagittarius A* card has "Open in the Black Hole Lab"; it opens the lab on Sagittarius A*
#   settings:  the lab's settings survive a restart; a 0.54 settings file (no Black Hole lines) still opens its lab
#   layout:    880 x 560 and 1280 x 800 (Dark): the bar, the card, the tab and the Controls panel inside the window, the bar clear of the panel and the card
#   look:      six themes at 1280 x 800: the card's and the Controls panel's text 4.5:1 over their background on this picture (WCAG 2.x, written here)
#   precision: the card's bent sky against the double-precision reference (PL_BH_COMPARE): the shadow's edge, the directions, the plain sky = the Orbit Lab's
#   check_blackhole.py <PhysicsLab binary> <work folder> [part]
import math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, inside, overlap, prefetch

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
BASE = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1", "PL_FACTS": "off"}

def bh(r, prefix="blackhole "):
    n = r.note(prefix)
    return dict(kv.split("=", 1) for kv in n[-1].split()[1:]) if n else None
def card(r):
    return bh(r, "bhcard ")
def stat(r):
    s = [ln for ln in r.out.splitlines() if ln.startswith("PLBHSTAT ")]
    return dict(kv.split("=") for kv in s[-1].split()[1:]) if s else None
def lum(c):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * ch(c[0]) + 0.7152 * ch(c[1]) + 0.0722 * ch(c[2])
def wcag(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)
def synge_deg(r):
    # the shadow's angular diameter for a hovering camera at r (G = c = M = 1): sin a = 3 sqrt(3) (1 - 2/r)^(1/2) / r, a > 90 degrees inside r = 3 (Perlick & Tsupko 2022, (7))
    s = 3 * math.sqrt(3) * math.sqrt(1 - 2 / r) / r
    a = math.asin(min(1.0, s))
    return 2 * math.degrees(a if r >= 3 else math.pi - a)
def presets():
    t = open(os.path.join(SRC, "blackhole", "presets.cpp"), encoding="utf-8").read().replace("\r", "")
    out = {}
    for m in re.finditer(r'\{"(\w+)", "([^"]+)", ([\d.e]+), ([\d.e]+), ([\d.e]+), ([\d.e]+), ([\d.]+), ([\d.]+),', t):
        out[m.group(1)] = dict(name=m.group(2), mass=float(m.group(3)), ring=float(m.group(7)), ringerr=float(m.group(8)))
    return out

def part_open(binary, work):
    r = run_app(binary, work, "bh_open", dict(BASE, PL_SCRIPT="5:click=top.tab.blackhole"), frames=14)
    check(not r.scripted_missing(), f"the Black Hole tab was there to click ({r.scripted_missing()})")
    s = bh(r)
    check(s is not None and s["preset"] == "sgra" and s["bend"] == "1" and s["sky"] == "ours", f"the tab opens the lab on Sagittarius A*, bending on, our sky ({s})")
    check(any(n.startswith("top.tab.blackhole.on") for n in r.rects), "the tab is lit")
    check(any(n.startswith("card.blackhole.card") for n in r.rects) and "blackholebar" in r.rects, "its card and its bar are on screen")
    st = stat(r)
    if r.bmp and os.path.exists(r.bmp) and st:
        b = Bmp(r.bmp)
        cx, cy = float(st["cx"]), float(st["cy"])
        # the hole: the black middle of the free part; the sky round it is lit
        dark = [b.px(int(x), int(y)) for x, y in ((cx, cy), (cx - 20, cy), (cx + 20, cy - 10))]
        check(all(max(p) < 8 for p in dark), f"the middle of the picture is the black shadow {dark}")
        ring = max(lum(b.px(int(cx + dx), int(cy))) for dx in range(-400, -100) if 0 <= cx + dx < 1280)
        check(ring > 0.05, f"the sky round the shadow is lit (brightest {ring:.3f})")
        check(int(st["images"]) > 300 and int(st["extra"]) > 50, f"the stars are drawn, many doubled ({st})")

    # dragging turns the camera round the hole (the yaw of the view), the hole stays in the middle
    d = run_app(binary, work, "bh_drag", dict(BASE, PL_BLACKHOLE="1", PL_SCRIPT="6:drag=300,400,400,400"), frames=20)
    s0, s1 = bh(r), bh(d)
    check(s0 is not None and s1 is not None and float(s1["yaw"]) - float(s0["yaw"]) > 0.1 and s1["pitch"] == s0["pitch"] and s1["dist"] == s0["dist"], f"a drag to the right turns the view ({s0} -> {s1})")

def part_presets(binary, work):
    P = presets()
    check(set(P) == {"sgra", "m87"}, f"presets.cpp read ({sorted(P)})")
    for key, other in (("m87", "sgra"), ("sgra", "m87")):
        r = run_app(binary, work, f"bh_preset_{key}", dict(BASE, PL_BLACKHOLE="1", PL_BH_SET=f"preset={other}", PL_SCRIPT=f"6:click=blackhole.preset.{key}"), frames=14)
        c = card(r)
        check(not r.scripted_missing() and c is not None, f"{key}: the preset row was there ({r.scripted_missing()})")
        if c and key in P:
            check(c["preset"] == key and abs(float(c["mass"]) / P[key]["mass"] - 1) < 1e-5 and abs(float(c["ring"]) - P[key]["ring"]) < 1e-9,
                  f"{key}: the card's mass and ring are presets.h's ({c}, {P[key]})")
            check(any(P[key]["name"] in t[2] for t in r.texts), f"{key}: the chip and card name it")

def part_distance(binary, work):
    runs = [("bh_d1000", {"PL_BH_SET": "dist=1000"}, ""), ("bh_d6", {}, "6:click=##blackhole.dist.text;8:text=6"), ("bh_d2_5", {"PL_BH_SET": "dist=2.5;fov=300"}, "")]
    last = None
    for name, env, script in runs:
        e = dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o", **env)
        if script: e["PL_SCRIPT"] = script
        r = run_app(binary, work, name, e, frames=16)
        c, st = card(r), stat(r)
        if c is None or st is None:
            check(False, f"{name}: the card and the picture's numbers were printed"); continue
        d = float(c["dist"])
        check(abs(float(c["shadow"]) - synge_deg(d)) < 1e-4 * synge_deg(d), f"{name}: the shadow's size {c['shadow']} deg = Synge's {synge_deg(d):.6g} deg at {d} GM/c^2")
        if last is not None: check(float(c["shadow"]) > last, f"{name}: closer, the shadow is larger ({c['shadow']} > {last})")
        last = float(c["shadow"])
        # the black disc on the picture: its radius along the row through the middle (the free part's middle: the bar is centred on it)
        b = Bmp(r.bmp)
        R, cx, cy = float(st["shadowpx"]), float(st["cx"]), float(st["cy"])
        if 3 < R < 300:                              # the black run through the hole's middle, up and down
            ys = [y for y in range(int(cy - R - 6), int(cy + R + 7)) if max(b.px(int(cx), y)) < 4]
            if ys:
                meas = (max(ys) - min(ys) + 1) / 2
                check(abs(meas - R) < 1.5, f"{name}: the black disc's radius on the picture {meas:.1f} px = the lab's {R:.2f} px")
    check(last is not None and last > 180, f"inside the photon orbit the shadow is more than half the sky ({last} deg)")

def part_light(binary, work):
    on = run_app(binary, work, "bh_light_on", dict(BASE, PL_BLACKHOLE="1"), frames=12)
    off = run_app(binary, work, "bh_light_off", dict(BASE, PL_BLACKHOLE="1", PL_SCRIPT="6:click=blackhole.bend.off"), frames=14)
    blk = run_app(binary, work, "bh_light_black", dict(BASE, PL_BLACKHOLE="1", PL_SCRIPT="6:click=blackhole.sky.black"), frames=14)
    check(not off.scripted_missing() and not blk.scripted_missing(), f"the two controls were there ({off.scripted_missing()} {blk.scripted_missing()})")
    s1, s2 = bh(off), bh(blk)
    check(s1 is not None and s1["bend"] == "0", f"Gravity bends light: Off ({s1})")
    check(s2 is not None and s2["sky"] == "black", f"Sky: Black ({s2})")
    B0, B1, B2 = Bmp(on.bmp), Bmp(off.bmp), Bmp(blk.bmp)
    st = stat(on)
    cx, cy = (int(float(st["cx"])), int(float(st["cy"]))) if st else (460, 420)
    mid = [(cx + dx, cy + dy) for dx in (-10, 0, 10) for dy in (-10, 0, 10)]
    check(all(max(B0.px(x, y)) < 8 for x, y in mid), "bending on: the middle is the shadow")
    check(sum(1 for x, y in mid if max(B1.px(x, y)) > 8) >= 5, "bending off: the middle is sky (the galactic centre's light)")
    differ = sum(1 for y in range(60, 740, 4) for x in range(0, 900, 4) if B0.px(x, y) != B1.px(x, y))
    check(differ > 2000, f"bending off changes the picture ({differ} samples differ)")
    occ = [v for k, v in blk.rects.items() if re.match(r"occ\d+$", k) or k == "handle"]   # the bars, the card, the Controls panel and its handle
    free = lambda x, y: not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ)
    lit = sum(1 for y in range(0, 800, 4) for x in range(0, 1280, 4) if free(x, y) and max(B2.px(x, y)) > 3)
    check(lit == 0, f"Sky: Black: the picture round the panels is black ({lit} lit samples)")

def part_realest(binary, work):
    r = run_app(binary, work, "bh_realest", dict(BASE, PL_BLACKHOLE="1", PL_BH_SET="bend=off;sky=black", PL_SCRIPT="6:click=blackhole.realest"), frames=14)
    s = bh(r)
    check(not r.scripted_missing() and s is not None and s["bend"] == "1" and s["sky"] == "ours", f"Most realistic: bending on, our sky ({s}, {r.scripted_missing()})")

def part_orbitcard(binary, work):
    r = run_app(binary, work, "bh_orbitcard", dict(BASE, PL_SELECT="far:sgra", PL_PANELS="orbit.card=o", PL_SCRIPT="10:click=card.blackhole"), frames=22)
    check(not r.scripted_missing(), f"the Orbit Lab's Sagittarius A* card has 'Open in the Black Hole Lab' ({r.scripted_missing()})")
    check(any("Open in the Black Hole Lab" in t[2] for t in r.texts), "its words")
    s = bh(r)
    check(s is not None and s["preset"] == "sgra" and any(n.startswith("top.tab.blackhole.on") for n in r.rects), f"it opens the Black Hole Lab on Sagittarius A* ({s})")

def part_settings(binary, work):
    sf = os.path.join(work, "bh_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    a = run_app(binary, work, "bh_keep1", dict(BASE, PL_BLACKHOLE="1", PL_SETTINGS=sf, PL_SCRIPT="6:click=blackhole.preset.m87;10:click=##blackhole.dist.text;12:text=7.5;16:click=blackhole.sky.black"), frames=24)
    check(not a.scripted_missing(), f"run 1: the controls were there ({a.scripted_missing()})")
    txt = open(sf, encoding="utf-8").read() if os.path.exists(sf) else ""
    check("lab=Black Hole Lab" in txt and "blackhole.preset=m87" in txt and "blackhole.dist=7.5" in txt and "blackhole.sky=black" in txt, "run 1: written to the settings file")
    b = run_app(binary, work, "bh_keep2", dict(BASE, PL_SETTINGS=sf), frames=10)
    s = bh(b)
    check(s is not None and s["preset"] == "m87" and float(s["dist"]) == 7.5 and s["sky"] == "black", f"run 2: the lab opens as it was left ({s})")
    old = os.path.join(work, "bh_old054.txt")
    with open(old, "w", encoding="utf-8", newline="\r\n") as f:          # a 0.54 file, with Windows line ends
        f.write("# Physics Lab settings. Written by the app; safe to delete.\ntheme=dark\ntextSize=normal\nfont=auto\nmotion=auto\nunits=metric\nlang=en\nlab=Molecule Lab\nmolecule=v1 mol=CO2\norbit.zoom=-2.5\natom.Z=92\n")
    c = run_app(binary, work, "bh_old054", dict(BASE, PL_SETTINGS=old), frames=10)
    check(any(n.startswith("molecule CO2") for n in c.notes) and bh(c) is None, "a 0.54 settings file opens its lab (the Molecule Lab on CO2), the Black Hole Lab untouched")

def part_layout(binary, work):
    for size in ("880x560", "1280x800"):
        W, H = (int(v) for v in size.split("x"))
        r = run_app(binary, work, f"bh_layout_{size}", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o"), frames=14, size=size)
        bar = r.rects.get("blackholebar")
        cards = [v for k, v in r.rects.items() if k.startswith("card.blackhole.card")]
        tab = [v for k, v in r.rects.items() if k.startswith("top.tab.blackhole")]
        check(bar is not None and inside(bar, W, H), f"{size}: the bar is inside the window ({bar})")
        check(bool(cards) and inside(cards[0], W, H), f"{size}: the card is inside the window ({cards})")
        if W >= 1000: check(bool(tab) and inside(tab[0], W, H), f"{size}: the Black Hole tab is inside the top bar ({tab})")
        else:                                        # a narrow window: the tabs fold into the Labs button; its menu opens the lab
            m = run_app(binary, work, f"bh_layout_{size}_menu", dict(BASE, PL_SCRIPT="5:click=top.lab;9:click=lab.blackhole"), frames=16, size=size)
            check(not m.scripted_missing() and bh(m) is not None, f"{size}: the tabs fold into the Labs button, whose menu opens the Black Hole Lab ({m.scripted_missing()})")
        dock = r.rects.get("occ1")
        if bar and cards: check(not overlap(bar, cards[0]), f"{size}: the bar is clear of the card")
        ctl = [v for k, v in r.rects.items() if k.startswith("ctlhead.")]
        if bar and ctl: check(all(not overlap(bar, c) for c in ctl), f"{size}: the bar is clear of the Controls panel")
        check(any(n.startswith("ctlsections") and "preset mass camera light honest" in n for n in r.notes), f"{size}: the Controls panel has its five sections")

def part_look(binary, work):
    themes = ("dark", "light", "black", "paper", "monochrome", "contrast")
    calls = []
    for t in themes:
        calls.append(((binary, work, f"bh_look_{t}", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o")), dict(frames=12, theme=t)))
        calls.append(((binary, work, f"bh_look_{t}_c", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=c")), dict(frames=12, theme=t)))
    prefetch(calls)
    for t in themes:
        o = run_app(binary, work, f"bh_look_{t}", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o"), frames=12, theme=t)
        c = run_app(binary, work, f"bh_look_{t}_c", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=c"), frames=12, theme=t)
        cr = [v for k, v in o.rects.items() if k == "card.blackhole.card"]
        if not cr:
            check(False, f"{t}: the card is open"); continue
        x0, y0, w, h = (int(v) for v in cr[0])
        B, A = Bmp(o.bmp), Bmp(c.bmp)
        px = [B.px(x, y) for y in range(y0 + 40, y0 + h - 4) for x in range(x0 + 8, x0 + w - 8) if B.px(x, y) != A.px(x, y)]
        if len(px) < 500:
            check(False, f"{t}: the card drew {len(px)} pixels"); continue
        px.sort(key=lum)
        bg = px[len(px) // 2]
        lo, hi = px[len(px) // 200], px[-(len(px) // 200) - 1]
        ratio = max(wcag(lo, bg), wcag(hi, bg))
        check(ratio >= 4.5, f"{t}: the card's text is {ratio:.1f}:1 over its glass on this picture (need 4.5) text {lo if wcag(lo, bg) > wcag(hi, bg) else hi} glass {bg}")
        # the Controls panel (solid): its section texts over its face
        hd = [v for k, v in o.rects.items() if k.startswith("ctlhead.")]
        if hd:
            x0, y0, w, h = (int(v) for v in hd[0])
            q = sorted((B.px(x, y) for y in range(y0, y0 + h) for x in range(x0, x0 + w)), key=lum)
            ratio = max(wcag(q[0], q[len(q) // 2]), wcag(q[-1], q[len(q) // 2]))
            check(ratio >= 4.5, f"{t}: the Controls panel's heading is {ratio:.1f}:1 over its face")

def part_precision(binary, work):
    r = run_app(binary, work, "bh_precision", dict(BASE, PL_BLACKHOLE="1", PL_BH_COMPARE="1"), frames=3, timeout=1800)
    rows = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHCMP ")]
    for ln in r.out.splitlines():
        if ln.startswith("PLBH"): print(ln)
    check(len(rows) == 3, f"three distances compared ({len(rows)})")
    for d in rows:
        # the edge: x in pixels from the GPU against the reference, within a thousandth of a pixel; no pixel on the wrong side
        check(float(d["edgeerr"]) < 1e-3 and int(d["wrong"]) == 0, f"dist {d['dist']}: the shadow's edge within 0.001 pixel ({d['edgeerr']}), {d['wrong']} pixels on the wrong side")
        # the bent map: within float rounding away from the edge (1e-5 rad: a 4K pixel at this scale is about 4e-4 rad)
        check(float(d["dir1"]) < 1e-5 and float(d["dir01"]) < 1e-5, f"dist {d['dist']}: the bent sky within 1e-5 rad of the reference at least 0.1 pixel from the edge ({d['dir1']}, {d['dir01']})")
    check(any(float(d["dist"]) < 3 for d in rows) and any(float(d["dist"]) >= 27 for d in rows), "one camera far away, one inside the photon orbit")
    p = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHPLAIN ")]
    check(bool(p) and int(p[0]["maxdiff"]) <= 1, f"bending off = the Orbit Lab's sky projection, within one step of 255 ({p})")

PARTS = {"open": part_open, "presets": part_presets, "distance": part_distance, "light": part_light, "realest": part_realest, "orbitcard": part_orbitcard,
         "settings": part_settings, "layout": part_layout, "look": part_look, "precision": part_precision}

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    part = sys.argv[3] if len(sys.argv) > 3 else None
    for name, fn in PARTS.items():
        if part is None or part == name:
            fn(binary, work)
    print(f"check_blackhole: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
