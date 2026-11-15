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
#   disc:      0.55.1, the gas disc: off for the two real holes, on for the feeding preset; switched on, the picture changes where the module puts the disc
#              (nearly face-on and steep), Sagittarius A*'s label; the readout's hottest ring = the papers' (written here); feeding rate and outer edge move
#              the picture and the readout; a restart keeps the settings (1280 x 800, Dark); the taller card at 880 x 560
#   precision: the card's bent sky against the double-precision reference (PL_BH_COMPARE): the shadow's edge, the directions, the plain sky = the Orbit Lab's;
#              0.55.1: the card's gas disc against the double lookup and the reference (two distances, two inclinations)
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
        check(any(n.startswith("ctlsections") and "preset mass camera light disc honest" in n for n in r.notes), f"{size}: the Controls panel has its six sections (0.55.1: the gas disc)")

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
    # 0.55.1, the disc: two distances (27 M and 4 M, inside the inner edge), two inclinations (10 and 60 degrees above it), the feeding preset's disc of 6 to 30 M.
    # The card against the double-precision lookup of the same table at every pixel: the same image (order) at every pixel centre, the edges moved by under
    # 0.01 pixel, r within 1e-4 and g within 1e-5 (float rounding of the table's coordinates: s ~ 30, the column ~ 255); the table against the reference
    # (Binet by Dormand-Prince) at every 16th pixel: r and g within 1e-6 (blackhole_physics's limit for the table), the same image everywhere.
    dd = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHDISC ")]
    check(len(dd) == 4 and {(x["dist"], x["incl"]) for x in dd} == {("27", "10"), ("27", "60"), ("4", "10"), ("4", "60")}, f"the disc at two distances and two inclinations ({len(dd)})")
    for x in dd:
        w = f"disc dist {x['dist']} incl {x['incl']}"
        check(int(x["pixels"]) > 50000 and int(x["edgepixels"]) > 500, f"{w}: the disc covers part of the picture ({x['pixels']} pixels, {x['edgepixels']} at an edge)")
        check(int(x["wrong"]) == 0 and float(x["edgeshift"]) < 0.01, f"{w}: the card's disc where the reference's is: {x['wrong']} pixels differ, edges moved by {x['edgeshift']} px")
        check(float(x["dr"]) < 1e-4 and float(x["dg"]) < 1e-5, f"{w}: r and g within float rounding of the double lookup ({x['dr']}, {x['dg']})")
        check(int(x["refwrong"]) == 0 and float(x["refdr"]) < 1e-6 and float(x["refdg"]) < 1e-6, f"{w}: the table against the reference ({x['refwrong']}, {x['refdr']}, {x['refdg']})")
    p = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHPLAIN ")]
    check(bool(p) and int(p[0]["maxdiff"]) <= 1, f"bending off = the Orbit Lab's sky projection, within one step of 255 ({p})")

def disc_note(r):
    return bh(r, "bhdisc ")
def disc_points(r):
    s = [ln for ln in r.out.splitlines() if ln.startswith("PLBHDISCPT ")]
    if not s: return None
    d = dict(kv.split("=", 1) for kv in s[-1].split()[1:])
    pts = lambda v: [tuple(float(a) for a in q.split(",")) for q in v.split(";") if q]
    return pts(d["on"]), pts(d["off"]), int(d["grid"])
def hottest_k(mass, feed):
    # the hottest ring of the standard thin disc at a = 0, written here from the papers (independent of the app's code): Page & Thorne 1974 (11b), (15n) in Luminet 1979's
    # (15) form with 3^(1/2)/2 before the log; L = feed L_Edd = (1 - E(6M)) Mdot c^2, E(6M) = (8/9)^(1/2) (Bardeen, Press & Teukolsky 1972 (2.12)); L_Edd = 4 pi G M c / 0.04 m^2/kg
    # (Yuan & Narayan 2014 fn. 1); sigma T^4 = F with sigma from the exact h, k, c; F in W/m^2 = Mdot c^2 / r_g^2 times the M = Mdot = 1 value
    GM, c, h, k = 1.32712440041279419e20 * mass, 299792458.0, 6.62607015e-34, 1.380649e-23
    sigma = 2 * math.pi ** 5 * k ** 4 / (15 * h ** 3 * c ** 2)
    def F(r):
        s, s3, s6 = math.sqrt(r), math.sqrt(3), math.sqrt(6)
        return 3 / (8 * math.pi) / ((r - 3) * r ** 2.5) * (s - s6 + s3 / 2 * math.log((s + s3) * (s6 - s3) / ((s - s3) * (s6 + s3))))
    fmax = max(F(6 + i * 1e-4) for i in range(1, 200000))
    mdot = feed * 4 * math.pi * GM * c / 0.04 / ((1 - math.sqrt(8 / 9)) * c * c)
    rg = GM / c ** 2
    return (mdot * c * c / rg ** 2 * fmax / sigma) ** 0.25

def part_disc(binary, work):
    # the presets: the two real holes open with the disc off, the feeding one with it on
    for key, want in (("m87", "0"), ("feeding", "1"), ("sgra", "0")):
        r = run_app(binary, work, f"bh_disc_preset_{key}", dict(BASE, PL_BLACKHOLE="1", PL_BH_SET="preset=m87" if key == "sgra" else "", PL_SCRIPT=f"6:click=blackhole.preset.{key}"), frames=14)
        s = bh(r)
        check(not r.scripted_missing() and s is not None and s["preset"] == key and s["disc"] == want, f"preset {key}: the disc {'on' if want == '1' else 'off'} ({s}, {r.scripted_missing()})")
    # switching it on draws it where the module says the disc is (Sagittarius A*: also its label), nearly face-on and steep
    for name, pitch in (("faceon", 0.35), ("steep", 1.40)):
        e = dict(BASE, PL_BLACKHOLE="1", PL_BH_DISCPTS="1", PL_PANELS="blackhole.card=o", PL_BH_SET=f"pitch={pitch}")
        off = run_app(binary, work, f"bh_disc_{name}_off", e, frames=12)
        on = run_app(binary, work, f"bh_disc_{name}_on", dict(e, PL_SCRIPT="6:click=blackhole.disc.on"), frames=16)
        check(not on.scripted_missing(), f"{name}: the switch was there ({on.scripted_missing()})")
        pts = disc_points(on)
        if pts is None: check(False, f"{name}: the module's disc points were printed"); continue
        B0, B1 = Bmp(off.bmp), Bmp(on.bmp)
        occ = [v for k, v in on.rects.items() if re.match(r"occ\d+$", k) or k == "handle"]
        free = lambda x, y: not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ)
        onp = [(x, y) for x, y in pts[0] if free(x, y) and y > 60]
        offp = [(x, y) for x, y in pts[1] if free(x, y) and y > 60]
        changed = sum(1 for x, y in onp if max(abs(a - b) for a, b in zip(B0.px(int(x), int(y)), B1.px(int(x), int(y)))) > 20)
        same = sum(1 for x, y in offp if max(abs(a - b) for a, b in zip(B0.px(int(x), int(y)), B1.px(int(x), int(y)))) <= 2)
        check(len(onp) >= 10 and changed >= 0.9 * len(onp), f"{name}: the picture changes where the module puts the disc ({changed} of {len(onp)})")
        check(len(offp) >= 10 and same >= 0.97 * len(offp), f"{name}: and not elsewhere ({same} of {len(offp)} the same)")
        check(any("does not have one" in tx[2] for tx in on.texts), f"{name}: Sagittarius A* with the disc says it has none")
        d = disc_note(on)
        if d is None or d.get("on") != "1": check(False, f"{name}: the card's disc numbers ({d})"); continue
        tk = hottest_k(4.297e6, 0.1)
        check(abs(float(d["thot"]) / tk - 1) < 1e-5 and float(d["rin"]) == 6 and abs(float(d["rhot"]) - 9.551) < 0.001, f"{name}: the readout's hottest ring {d['thot']} K = {tk:.6g} K at {d['rhot']}, inner edge {d['rin']}")
        check(float(d["bright"]) > 1 and d["auto"] == "1" and float(d["white"]) > 0, f"{name}: brighter than the Sun's surface, automatic exposure ({d['bright']}, {d['white']})")
    # the feeding rate and the outer edge move the picture and the readout (a fixed exposure, so that brightness shows)
    runs = {}
    for name, extra in (("f02", "feed=0.02;exposure=3.5"), ("f20", "feed=0.2;exposure=3.5"), ("o15", "outer=15;exposure=1"), ("o60", "outer=60;exposure=1")):
        runs[name] = run_app(binary, work, f"bh_disc_{name}", dict(BASE, PL_BLACKHOLE="1", PL_BH_DISCPTS="1", PL_PANELS="blackhole.card=o", PL_BH_SET=f"preset=feeding;pitch=1.2;{extra}"), frames=12)
    a, b = disc_note(runs["f02"]), disc_note(runs["f20"])
    if a and b:
        check(abs(float(b["thot"]) / float(a["thot"]) - 10 ** 0.25) < 1e-6 and float(a["feed"]) == 0.02 and float(b["feed"]) == 0.2, f"feeding rate: T ~ rate^(1/4) ({a['thot']}, {b['thot']})")
        check(abs(float(a["thot"]) / hottest_k(1e8, 0.02) - 1) < 1e-5, f"the feeding preset's hottest ring = the papers' ({a['thot']} K)")
    else: check(False, "feeding rate runs printed the card")
    occ = [v for k, v in runs["f02"].rects.items() if re.match(r"occ\d+$", k) or k == "handle"]
    free = lambda x, y: y > 60 and not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ)
    pa = disc_points(runs["f02"])
    if pa:
        pa = ([q for q in pa[0] if free(*q)], pa[1], pa[2])
        Ba, Bb = Bmp(runs["f02"].bmp), Bmp(runs["f20"].bmp)
        la = sum(lum(Ba.px(int(x), int(y))) for x, y in pa[0])
        lb = sum(lum(Bb.px(int(x), int(y))) for x, y in pa[0])
        # ten times the feeding rate: T x 10^(1/4) = 1.78; far above the visible a black body's visible light grows about like T (the same exposure)
        check(lb > 1.5 * la, f"a higher feeding rate is brighter on the disc (linear light {la:.3g} -> {lb:.3g})")
    c15, c60 = disc_note(runs["o15"]), disc_note(runs["o60"])
    p15, p60 = disc_points(runs["o15"]), disc_points(runs["o60"])
    check(c15 is not None and c60 is not None and float(c15["outer"]) == 15 and float(c60["outer"]) == 60, f"outer edge in the readout ({c15}, {c60})")
    if p15 and p60:
        check(p60[2] > p15[2], f"a larger outer edge covers more of the picture ({p15[2]} -> {p60[2]} grid points)")
        B15, B60 = Bmp(runs["o15"].bmp), Bmp(runs["o60"].bmp)
        newp = [pt for pt in p60[0] if pt in p15[1] and free(*pt)]           # on the disc of 60 M, off the disc of 15 M
        diff = sum(1 for x, y in newp if max(abs(a - b) for a, b in zip(B15.px(int(x), int(y)), B60.px(int(x), int(y)))) > 10)
        check(len(newp) >= 5 and diff >= 0.9 * len(newp), f"the added ring is drawn ({diff} of {len(newp)})")
    # the card with the disc's rows at the small window (the card grows): inside the window, clear of the bar
    r = run_app(binary, work, "bh_disc_small", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o", PL_BH_SET="preset=feeding"), frames=14, size="880x560")
    cards = [v for k, v in r.rects.items() if k.startswith("card.blackhole.card")]
    bar = r.rects.get("blackholebar")
    check(bool(cards) and inside(cards[0], 880, 560) and bar is not None and not overlap(bar, cards[0]), f"880x560: the card with the disc inside the window, clear of the bar ({cards}, {bar})")
    # a restart keeps the settings
    sf = os.path.join(work, "bh_disc_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    k1 = run_app(binary, work, "bh_disc_keep1", dict(BASE, PL_BLACKHOLE="1", PL_SETTINGS=sf, PL_BH_SET="feed=0.05;outer=40;exposure=1.5", PL_SCRIPT="6:click=blackhole.disc.on"), frames=14)
    k2 = run_app(binary, work, "bh_disc_keep2", dict(BASE, PL_SETTINGS=sf), frames=10)
    s = bh(k2)
    check(not k1.scripted_missing() and s is not None and s["disc"] == "1" and float(s["feed"]) == 0.05 and float(s["outer"]) == 40 and s["exp"] == "1.5", f"a restart keeps the disc's settings ({s})")

PARTS = {"open": part_open, "presets": part_presets, "distance": part_distance, "light": part_light, "realest": part_realest, "orbitcard": part_orbitcard,
         "settings": part_settings, "layout": part_layout, "look": part_look, "precision": part_precision, "disc": part_disc}

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    part = sys.argv[3] if len(sys.argv) > 3 else None
    for name, fn in PARTS.items():
        if part is None or part == name:
            fn(binary, work)
    print(f"check_blackhole: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
