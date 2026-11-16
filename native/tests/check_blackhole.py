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
#              0.55.1: the card's gas disc against the double lookup and the reference (two distances, two inclinations); 0.55.2: the same round a spinning hole
#   spin:      0.55.2, spin: the control exists; spin 0 is the picture of 0.55.1 to the pixel; a typed 0.99 (the top) is taken, 0.999 is not; the card's horizon,
#              inner edge, efficiency and dragging = the papers' (written here); at 0.99 the drawn shadow's edge is the module's at 24 points from the equator
#              (not round there) and from near the axis (round); the presets start at 0 with their sentence; a restart keeps the spin (1280 x 800, Dark)
#              0.55.3: the looks: Real's saved case to the pixel (its hash made by 0.55.2's code; 0.55.8: drawn with 0.55.7's rays, and the new fine table within one level); Film at the paper's view (James et al. 2015, Fig. 13) against the
#              double lookup: the disc's image and its edges where the module puts the paper's edges, one colour all over the disc before the glow; the glow = its
#              double reference (blackhole/looks.h glowReference) to one level of 255, no pixel darker
#   looks:     0.55.3, Controls > Look (1280 x 800, Dark): Real with the dot; Film locks the spin and disc controls (a typed spin and Off are refused), shows its labels
#              and changes the chip; "The paper's view" sets the camera; switching at the film's spin makes no new table; Most realistic returns to Real; a restart
#              keeps the look; a 0.55.2 settings file opens as Real; six themes at 1280 x 800: the chip, the card and the Controls panel's text 4.5:1 over the Film disc
#   truelight: 0.55.4, Sky beside the disc and Glare (Real look, disc on; 1280 x 800, Dark): the controls only there; True brightness blacks the sky, the readout's
#              factor = the module's rule written here (Willmer 2018's -26.76 for the Sun); a lower exposure brings stars back; the glare lights the sky round the disc
#              far more than far from it; Most realistic; a restart; a 0.55.3 file; Film's glow leaves the far sky alone; the chip and panel 4.5:1 in six themes
#   smooth:    (0.55.7: also the guard round the card's path, its marker, Try again, a fence that never signals, the log's steps in order)
#              0.55.5, the frame never waits for the processor (1280 x 800, Dark; the app's asynchronous path, PL_BH_ASYNC): during a tilt drag and a spin drag
#              the frame thread's worst time stays under SMOOTH_LIMIT_MS; every frame shows one view (sky, exposure and stars of the same settings), never an older
#              one; at rest the picture catches up with the settings (fine table, glare) and nothing is redone afterwards
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
        check(any(n.startswith("ctlsections") and "preset look mass spin camera light disc honest" in n for n in r.notes), f"{size}: the Controls panel has its eight sections (0.55.1: the gas disc; 0.55.2: spin; 0.55.3: look)")

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

def precision_card(binary, work):
    # 0.55.6, the spinning hole's table filled by the graphics card (blackhole/spin_card.h) against the processor's (PL_BH_COMPARE=card). Limits, measured
    # on llvmpipe (REPORT 0.55.6) with room: the shader's own double sine and cosine within 1e-15, exp, log, atan2, acos, cbrt and x^-0.2 within 4e-15 of
    # the C++ library's; the start-up check passes; for the benchmark's case, the Film case and spin 0.99 the whole fine table node by node: the same ends
    # (sky or horizon) everywhere, no ray lost, the far direction within 1e-8 rad from 1e-3 of the shadow's radius outward and 1e-4 rad nearer (the
    # table's own limits against the integrator are 2e-3 and 2e-2: blackhole_physics), b within 1e-7, u within 1e-3 (relative to 1 + |u|; 1e-4 nearer),
    # the edge samples within 1e-12 rad, the rows within 1e-8; on 300 of the card's rays the null condition and Carter's q within 1e-8 at the end (the
    # table's tolerance is 1e-10; blackhole_physics asks 1e-9 at 1e-11), no worse than 10 x the processor's own same rays.
    r = run_app(binary, work, "bh_precision_card", dict(BASE, PL_BLACKHOLE="1", PL_BH_COMPARE="card"), frames=3, timeout=3000)
    for ln in r.out.splitlines():
        if ln.startswith("PLBHCARD"): print(ln)
    m = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHCARDMATH ")]
    check(len(m) == 1, "the card's double functions were compared")
    if m:
        x = m[0]
        check(float(x["sin"]) < 1e-15 and float(x["cos"]) < 1e-15 and all(float(x[k]) < 4e-15 for k in ("exp", "log", "atan2", "acos", "cbrt", "pow")),
              f"the shader's double functions = the C++ library's ({' '.join(k + '=' + x[k] for k in ('sin', 'cos', 'exp', 'log', 'atan2', 'acos', 'cbrt', 'pow'))})")
    c = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHCARDCHECK ")]
    check(len(c) == 1 and c[0]["ok"] == "1", f"the start-up check passes on this renderer ({c[0] if c else None})")
    t = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHCARDT ")]
    check(len(t) == 3 and {x["case"] for x in t} == {"benchmark", "film", "top"}, f"three tables compared ({len(t)})")
    for x in t:
        w = f"card table {x['case']} (spin {x['a']}, {x['dist']} M)"
        check(int(x["nodes"]) == 2 * 192 * 384 and int(x["endsdiffer"]) == 0 and int(x["lost"]) == 0 and int(x["mism"]) == 0,
              f"{w}: every one of {x['nodes']} nodes ends where the processor's does ({x['endsdiffer']} differ, {x['lost']} lost, {x['mism']} off their side)")
        check(float(x["dirmid"]) < 1e-8 and float(x["dirfar"]) < 1e-8 and float(x["dirnear"]) < 1e-4,
              f"{w}: far directions ({x['nmid']} + {x['nfar']} nodes: {x['dirmid']}, {x['dirfar']} rad; {x['nnear']} nearer the edge: {x['dirnear']} rad)")
        check(float(x["b"]) < 1e-7 and float(x["u"]) < 1e-3 and float(x["unear"]) < 1e-4, f"{w}: b {x['b']}, u {x['u']} (near the edge {x['unear']})")
        check(float(x["edge"]) < 1e-12 and float(x["rowchi"]) < 1e-8 and float(x["rowedge"]) < 1e-8, f"{w}: the edge {x['edge']}, the rows {x['rowchi']}, their edges {x['rowedge']}")
        check(int(x["ended"]) == 300 and float(x["null"]) < 1e-8 and float(x["qdrift"]) < 1e-8 and float(x["null"]) <= 10 * float(x["cpunull"]) + 1e-15
              and float(x["qdrift"]) <= 10 * float(x["cpuqdrift"]) + 1e-15,
              f"{w}: the conservation laws on the card's rays: null {x['null']} (processor {x['cpunull']}), q {x['qdrift']} (processor {x['cpuqdrift']})")

def part_precision(binary, work):
    precision_card(binary, work)
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
    # 0.55.2, spin: two spins (0.5, the top 0.99) x two distances (20, 4 M) x two latitudes (near the disc's plane, near the axis), the feeding preset's disc. The card against
    # the double lookup of the same table at every pixel: the shadow's edge within 0.001 pixel of the closed test (J15 (v)) and no pixel on the wrong side; the far direction
    # within 2e-3 rad at least a pixel from the edge (float rounding of the table's coordinates where the rays wind); the disc: the same image at every pixel away from an
    # image's edge, those edges moved by under 0.1 pixel, r within 1e-3 and g within 5e-4. The table against the integrator at every 16th pixel: the far direction within
    # 2e-3 rad from 1e-3 of the shadow's radius outward, 2e-2 nearer (blackhole_physics's limits); the disc's r and g away from the edges and folds within 3 % and 0.5 % at
    # spin 0.5 and 12 % and 2.5 % at the top (rays grazing the disc's plane from near it; REPORT 0.55.2), the same image at all but 0.1 % of them.
    sp = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSPIN ")]
    check(len(sp) == 8, f"spin: eight cases compared ({len(sp)})")
    for x in sp:
        w = f"spin {x['a']} dist {x['dist']} pitch {x['pitch']}"
        check(float(x["edgeclosed"]) < 1e-3 and int(x["wrongside"]) == 0, f"{w}: the card's shadow edge within 0.001 pixel of the closed test ({x['edgeclosed']}), {x['wrongside']} pixels on the wrong side")
        check(float(x["dir1"]) < 2e-3, f"{w}: the card's far direction = the double lookup's within 2e-3 rad a pixel or more from the edge ({x['dir1']})")
        check(int(x["discwrong"]) == 0 and float(x["edgeshift"]) < 0.1 and float(x["dr"]) < 1e-3 and float(x["dg"]) < 5e-4, f"{w}: the card's disc = the double lookup's ({x['discwrong']}, {x['edgeshift']}, {x['dr']}, {x['dg']})")
        check(float(x["refnear"]) < 2e-2 and float(x["refmid"]) < 2e-3 and float(x["reffar"]) < 2e-3, f"{w}: the table's far direction against the integrator ({x['refnear']}, {x['refmid']}, {x['reffar']})")
        top = float(x["a"]) > 0.9
        check(float(x["refdr"]) < (0.12 if top else 0.03) and float(x["refdg"]) < (0.025 if top else 0.005) and int(x["refwrong"]) <= int(x["refpx"]) // 1000,
              f"{w}: the table's disc against the integrator ({x['refdr']}, {x['refdg']}, {x['refwrong']} of {x['refpx']} another image)")
    # 0.55.3, the looks. Real: the feeding preset at spin 0.6 from 74.1 M, 86.56 deg, field of view 30, drawn as the app draws it: the hash of its 8-bit picture made
    # by 0.55.2's code before this version changed anything (REPORT 0.55.3; Mesa llvmpipe). Film at the same place (the paper's view): the card's disc against the
    # double lookup of the same table: the same image at every pixel away from an image's edge, those edges moved by under 0.1 pixel (the spin cases' limit; measured
    # 0.0018 here), every radius on the disc within the paper's edges 9.26..18.70 M; every pixel fully on the disc exactly the one colour the module gives
    # (looks.h filmDiscLinear over the screen's white, encoded); the glow: the card's picture with it = the double reference applied to the card's picture without
    # it, to one level of 255, no channel of any pixel darker, the added light = the reference's to 1 %.
    rl = [ln for ln in r.out.splitlines() if ln.startswith("PLBHREAL ")]
    # 0.55.8: the fine table's rays follow Mino's form (src/blackhole/mino.h): the same case drawn with 0.55.7's rays is still 0.55.2's picture to the pixel; the
    # new picture differs from it by at most one level of 255 (measured: 695 of 1,024,000 pixels by one level); its hash saved on purpose (REPORT 0.55.8)
    rv = dict(kv.split("=") for kv in rl[0].split()[1:]) if rl else {}
    check(rv.get("old") == "7fff50741d132f22", f"Real: drawn with 0.55.7's rays, the saved case is 0.55.2's picture to the pixel ({rl})")
    check(rv.get("hash") == "b37c532bb4916030" and int(rv.get("maxdiff", 9)) <= 1, f"Real: the saved case with the fine table of 0.55.8, within one level of 255 of 0.55.2's picture ({rl})")
    fl = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHFILM ")]
    check(len(fl) == 1, f"Film compared at the paper's view ({len(fl)})")
    for x in fl:
        check(float(x["spin"]) == 0.6 and float(x["dist"]) == 74.1 and abs(float(x["theta"]) - 86.56 * math.pi / 180) < 1e-6 and float(x["rin"]) == 9.26 and float(x["rout"]) == 18.7,
              f"Film: the paper's spin, view and disc edges ({x['spin']}, {x['dist']}, {x['theta']}, {x['rin']}, {x['rout']})")
        check(int(x["pixels"]) > 20000 and int(x["wrong"]) == 0 and float(x["edgeshift"]) < 0.1, f"Film: the disc where the module puts it ({x['pixels']} pixels, {x['wrong']} wrong, edges moved {x['edgeshift']} px)")
        check(float(x["rmin"]) >= 9.26 - 1e-4 and float(x["rmax"]) <= 18.7 + 1e-4, f"Film: every radius on the disc within the paper's edges ({x['rmin']}, {x['rmax']})")
        check(int(x["colours"]) == 1 and int(x["offcolour"]) == 0, f"Film: one colour all over the disc before the glow ({x['colours']} colours, {x['offcolour']} off; {x['want']})")
        check(int(x["darker"]) == 0 and int(x["brighter"]) > 1000 and int(x["glowdiff"]) <= 1 and abs(float(x["added"]) / float(x["addedref"]) - 1) < 0.01,
              f"Film: the glow adds light only and is its reference ({x['darker']} darker, {x['glowdiff']} levels, added {x['added']} against {x['addedref']})")
    # 0.55.4: the Film glow is made from the disc's own light: every pixel its reference leaves alone is the plain picture (the sky untouched); and the eye's glare
    # on Real's saved case: the veil lays on the picture the light it spreads, less what leaves the picture (blackhole/light.h; light.cpp's own tests check the
    # rule against the CIE equation and conservation), its time (llvmpipe; reported, not limited)
    gs = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHGLOWSKY ")]
    check(len(gs) == 1 and int(gs[0]["free"]) > 100000 and int(gs[0]["touched"]) == 0, f"Film: the glow leaves the sky alone ({gs})")
    ga = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHGLARE ")]
    check(len(ga) == 2, f"the glare measured at two sizes ({len(ga)})")
    for x in ga:
        sp, ld = float(x["spread"]), float(x["laid"])
        check(sp > 0 and 0.8 < ld / sp <= 1.01 and abs(float(x["keep"]) - 0.72066) < 1e-4, f"{x['size']}: the veil lays {ld / sp if sp else 0:.4f} of the light it spreads (the rest leaves the picture), keep {x['keep']}")
        print(f"  glare {x['size']}: block {x['b']} px, grid {x['grid']}, read-back {x['readms']} ms, veil {x['veilms']} ms, picture {x['withoutms']} -> {x['withms']} ms")
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
        e = dict(BASE, PL_BLACKHOLE="1", PL_BH_DISCPTS="1", PL_PANELS="blackhole.card=o", PL_BH_SET=f"pitch={pitch};glare=off")   # (0.55.4: the eye's glare, on by default, veils the sky round the disc)
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

def spin_note(r):
    return bh(r, "bhspin ")
def spin_edge(r):
    s = [ln for ln in r.out.splitlines() if ln.startswith("PLBHSPINEDGE ")]
    if not s: return None
    d = dict(kv.split("=", 1) for kv in s[-1].split()[1:])
    return int(d["fine"]), [tuple(float(a) for a in q.split(",")) for q in d["pts"].split(";") if q]
def spin_numbers(a, r, th):
    # the module's numbers written here from the papers (independent of the app's code): the horizon r+ = 1 + (1 - a^2)^(1/2) (BPT (2.6)); the innermost stable orbit
    # (BPT (2.21)); the efficiency 1 - E(r_ms) with E from BPT (2.12); the frame dragging omega = 2 a r / Sigma^2, Sigma^2 = (r^2 + a^2)^2 - a^2 Delta sin^2 theta (J15 (A.2))
    z1 = 1 + (1 - a * a) ** (1 / 3) * ((1 + a) ** (1 / 3) + (1 - a) ** (1 / 3)); z2 = math.sqrt(3 * a * a + z1 * z1)
    rms = 3 + z2 - math.sqrt((3 - z1) * (3 + z1 + 2 * z2))
    s = math.sqrt(rms)
    E = (rms * s - 2 * s + a) / (rms ** 0.75 * math.sqrt(rms * s - 3 * s + 2 * a))
    D = r * r - 2 * r + a * a
    S2 = (r * r + a * a) ** 2 - a * a * D * math.sin(th) ** 2
    return 1 + math.sqrt(1 - a * a), rms, 1 - E, 2 * a * r / S2

def part_spin(binary, work):
    # 0.55.2: the Spin control (one theme, 1280 x 800). It exists; spin 0 is exactly the picture of 0.55.1 (the closed-form path: the same pixels as a run with no spin
    # setting at all)
    base = dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o")
    r0 = run_app(binary, work, "bh_spin_none", base, frames=12)
    r1 = run_app(binary, work, "bh_spin_zero", dict(base, PL_BH_SET="spin=0"), frames=12)
    check(r0.bmp and r1.bmp and Bmp(r0.bmp).d == Bmp(r1.bmp).d, "spin 0 draws exactly the picture of 0.55.1 (the same pixels)")
    st = stat(r1)
    check(st is not None and int(st["tablefills"]) >= 1, f"spin 0 uses the non-spinning hole's closed-form table ({st})")
    # typed: 0.99 (the top) through the text field; the card's numbers are the module's (written here from the papers)
    rt = run_app(binary, work, "bh_spin_typed", dict(base, PL_BH_SPINWAIT="1", PL_SCRIPT="6:click=##blackhole.spin.text;8:text=0.99"), frames=20)
    s, n = bh(rt), spin_note(rt)
    check(any(k.startswith("slider.blackhole.spin") for k in rt.rects), "the Spin slider is in the Controls panel, beside the typed value")
    check(not rt.scripted_missing() and s is not None and float(s["spin"]) == 0.99, f"a typed spin of 0.99 is taken ({s}, {rt.scripted_missing()})")
    if n and s:
        hz, rms, eff, om = spin_numbers(0.99, float(s["dist"]), float(s["pitch"]))
        check(abs(float(n["horizon"]) - hz) < 1e-8 and abs(float(n["rin"]) - rms) < 1e-8 and abs(float(n["eff"]) - eff) < 1e-8 and abs(float(n["omega"]) / om - 1) < 1e-8,
              f"the readout's horizon, inner edge, efficiency and dragging = the papers' ({n}; {hz:.9g} {rms:.9g} {eff:.9g} {om:.9g})")
        check(any("Dragged round" in t[2] for t in rt.texts) and any("Spin" == t[2] or t[2].startswith("Spin") for t in rt.texts), "the card says the camera is dragged round, and shows the spin")
    else: check(False, f"the spin readout was printed ({n})")
    # a spin above the top typed: not taken
    rh = run_app(binary, work, "bh_spin_high", dict(base, PL_SCRIPT="6:click=##blackhole.spin.text;8:text=0.999"), frames=14)
    s = bh(rh)
    check(s is not None and float(s["spin"]) == 0, f"a typed spin above the tested top (0.999) is not taken ({s})")
    # at the top the drawn shadow's edge is the module's (the closed test through the table's edge samples), from the equator and from near the axis:
    # inside it black, just outside it lit (where the sky there is lit)
    for name, pitch in (("equator", 1.5707963), ("axis", 0.01)):
        r = run_app(binary, work, f"bh_spin_edge_{name}", dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SPINEDGE="1", PL_BH_SET=f"spin=0.99;pitch={pitch};dist=20"), frames=12)
        e = spin_edge(r)
        if e is None or not r.bmp: check(False, f"{name}: the module's edge was printed"); continue
        fine, pts = e
        b = Bmp(r.bmp)
        occ = [v for k, v in r.rects.items() if re.match(r"occ\d+$", k) or k == "handle"]
        free = lambda x, y: y > 60 and not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ)
        dark = lit = used = 0
        for x, y, ux, uy in pts:
            xi, yi, xo, yo = x - 3 * ux, y - 3 * uy, x + 3 * ux, y + 3 * uy
            if not (free(xi, yi) and free(xo, yo)): continue
            used += 1
            if max(b.px(int(xi), int(yi))) < 8: dark += 1
            if max(max(b.px(int(xo + k * ux), int(yo + k * uy))) for k in range(0, 3)) > 20: lit += 1
        check(fine == 1 and used >= 16 and dark == used and lit >= 0.75 * used, f"{name}: the drawn shadow's edge is the module's: {dark} of {used} points 3 px inside black, {lit} just outside lit (fine table {fine})")
        if name == "equator":
            # flattened on the side moving towards the camera: the edge's distance from the middle differs left and right (PeT Table II)
            st = stat(r)
            cx, cy = float(st["cx"]), float(st["cy"])
            left = [math.hypot(x - cx, y - cy) for x, y, ux, uy in pts if ux < -0.9]
            right = [math.hypot(x - cx, y - cy) for x, y, ux, uy in pts if ux > 0.9]
            check(left and right and abs(min(left) - min(right)) > 3, f"from the equator the shadow is not round: left {min(left):.1f} px, right {min(right):.1f} px")
        else:
            st = stat(r)
            cx, cy = float(st["cx"]), float(st["cy"])
            d = [math.hypot(x - cx, y - cy) for x, y, ux, uy in pts]
            check(max(d) - min(d) < 2, f"from near the axis the shadow is round (radii {min(d):.1f}..{max(d):.1f} px)")
    # the presets start at 0 with their sentence; the feeding preset too
    for key in ("sgra", "m87", "feeding"):
        r = run_app(binary, work, f"bh_spin_preset_{key}", dict(base, PL_BH_SET="spin=0.7;preset=" + ("m87" if key == "sgra" else "sgra"), PL_SCRIPT=f"6:click=blackhole.preset.{key}"), frames=14)
        s = bh(r)
        check(not r.scripted_missing() and s is not None and s["preset"] == key and float(s["spin"]) == 0, f"preset {key} starts at spin 0 ({s})")
        check(any("starts at spin 0" in t[2] for t in r.texts) and (key == "feeding" or any("Spin not measured" in t[2] for t in r.texts)), f"preset {key}: its sentence says so")
    # Most realistic does not touch the spin (it has no closest-to-reality value); a restart keeps it
    rr = run_app(binary, work, "bh_spin_realest", dict(BASE, PL_BLACKHOLE="1", PL_BH_SET="spin=0.6;bend=off", PL_SCRIPT="6:click=blackhole.realest"), frames=14)
    s = bh(rr)
    check(not rr.scripted_missing() and s is not None and float(s["spin"]) == 0.6 and s["bend"] == "1", f"Most realistic leaves the spin alone ({s})")
    sf = os.path.join(work, "bh_spin_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    k1 = run_app(binary, work, "bh_spin_keep1", dict(BASE, PL_BLACKHOLE="1", PL_SETTINGS=sf, PL_SCRIPT="6:click=##blackhole.spin.text;8:text=0.6"), frames=14)
    k2 = run_app(binary, work, "bh_spin_keep2", dict(BASE, PL_SETTINGS=sf), frames=10)
    s = bh(k2)
    check(not k1.scripted_missing() and s is not None and float(s["spin"]) == 0.6, f"a restart keeps the spin ({s})")


def marks(r):
    drawn = {k[5:] for k in r.rects if k.startswith("real.")}
    state = {}
    for n in r.note("realmark."):
        k, v = n[len("realmark."):].split()
        if k in drawn: state[k] = v
    return state
def texts_has(r, words):
    return any(words.lower() in t[2].lower() for t in r.texts)           # (one wrapped line of a caption)
def chip_sub(r):
    n = r.note("bhchip ")
    return n[-1][len("bhchip "):] if n else ""

def part_looks(binary, work):
    film = blackhole_film()
    sf = os.path.join(work, "bh_looks_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    base = dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_SETTINGS=sf, PL_BH_SET="preset=feeding;spin=0.3")
    a = run_app(binary, work, "bh_looks_real", base, frames=10)
    s, mk = bh(a), marks(a)
    check(s is not None and s["look"] == "real" and any(k.startswith("seg.blackhole.look") for k in a.rects), f"Look exists and opens on Real ({s and s['look']})")
    check(mk.get("blackhole.look") == "on", f"the closest-to-reality dot is on Real ({mk.get('blackhole.look')})")
    check("standard thin disc" in chip_sub(a) and texts_has(a, "real for quasars"), f"Real's chip and caption ({chip_sub(a)[:90]})")
    # Film: the film's values, the spin and disc locked (a typed spin and Off do nothing), its labels, the chip; then the paper's view
    b = run_app(binary, work, "bh_looks_film", dict(base, PL_SCRIPT="6:click=blackhole.look.film;10:click=##blackhole.spin.text;12:text=0.8;16:click=blackhole.disc.off;20:click=blackhole.paperview"), frames=26)
    s = bh(b)
    check(not b.scripted_missing() and s is not None and s["look"] == "film", f"Film chosen ({b.scripted_missing()}, {s and s['look']})")
    check(s is not None and float(s["shownspin"]) == film["spin"] and float(s["spin"]) == 0.3 and s["showndisc"] == "1", f"Film: the film's spin drawn, the disc on; the typed spin refused, yours kept ({s})")
    for w in ("our own glow, in place of the film's lens flare", "the spin the film drew", "K all over (one temperature)", "no colour shift and no brightness shift", "white balance: a",
              "drawn white (appendix A.6)", "Locked while the Film look is chosen", "not of its pictures"):
        check(texts_has(b, w), f"Film's label: {w}")
    check(texts_has(b, f"{film['discT']:,.0f} K all over") and texts_has(b, f"{film['whiteT']:,.0f} K black body") and texts_has(b, f"{film['rOut']:g} GM/c²") and texts_has(b, f"{film['spin']:g}, the spin"),
          "the labels carry the module's numbers")
    check("film's choices (James et al. 2015)" in chip_sub(b) and chip_sub(b) != chip_sub(a), f"the chip line changes with the look ({chip_sub(b)[:90]})")
    check(s is not None and abs(float(s["dist"]) - film["camR"]) < 1e-9 and abs(float(s["pitch"]) - film["camThetaDeg"] * math.pi / 180) < 1e-6, f"The paper's view: 74.1 GM/c², 86.56° from the axis ({s and (s['dist'], s['pitch'])})")
    # switching to Film at the film's own spin makes no new table
    c = run_app(binary, work, "bh_looks_notable", dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SET="preset=feeding;spin=0.6", PL_SCRIPT="6:click=blackhole.look.film"), frames=12)
    st = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in c.out.splitlines() if ln.startswith("PLBHSTAT ")]
    check(len(st) > 8 and st[3]["spinfills"] == st[-1]["spinfills"] and bh(c)["look"] == "film", f"switching looks at the same spin refills no table ({st[3].get('spinfills') if st else None} -> {st[-1].get('spinfills') if st else None})")
    check(marks(c).get("blackhole.look") == "off", f"the dot stays on Real while Film is chosen ({marks(c).get('blackhole.look')})")
    # a restart keeps the look (and your own spin); Most realistic returns to Real
    d = run_app(binary, work, "bh_looks_keep", dict(BASE, PL_SETTINGS=sf), frames=10)
    s = bh(d)
    check(s is not None and s["look"] == "film" and float(s["spin"]) == 0.3 and float(s["shownspin"]) == film["spin"], f"a restart keeps the Film look ({s})")
    e = run_app(binary, work, "bh_looks_realest", dict(BASE, PL_SETTINGS=sf, PL_SCRIPT="6:click=blackhole.realest"), frames=12)
    s = bh(e)
    check(not e.scripted_missing() and s is not None and s["look"] == "real" and float(s["shownspin"]) == 0.3, f"Most realistic picks Real, your spin back ({s})")
    old = os.path.join(work, "bh_old0552.txt")
    with open(old, "w", encoding="utf-8", newline="\r\n") as f:          # a 0.55.2 file: Black Hole lines, no look
        f.write("# Physics Lab settings. Written by the app; safe to delete.\ntheme=dark\nlab=Black Hole Lab\nblackhole.preset=feeding\nblackhole.dist=20\nblackhole.disc=on\nblackhole.spin=0.5\n")
    g = run_app(binary, work, "bh_looks_old", dict(BASE, PL_SETTINGS=old), frames=10)
    s = bh(g)
    check(s is not None and s["look"] == "real" and float(s["spin"]) == 0.5, f"a 0.55.2 settings file opens as Real ({s})")
    # the look check: six themes at 1280 x 800, Film from 20 M where the disc fills the picture: the card's text over its glass (card open against closed), the chip's
    # text over its own pixels, the Controls panel's Look section over its face, each 4.5:1 (WCAG 2.x)
    themes = ("dark", "light", "black", "paper", "monochrome", "contrast")
    fb = dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SET="preset=feeding;look=film;dist=20")
    calls = []
    for t in themes:
        calls.append(((binary, work, f"bh_looks_{t}", dict(fb, PL_PANELS="blackhole.card=o")), dict(frames=12, theme=t)))
        calls.append(((binary, work, f"bh_looks_{t}_c", dict(fb, PL_PANELS="blackhole.card=c")), dict(frames=12, theme=t)))
    prefetch(calls)
    for t in themes:
        o = run_app(binary, work, f"bh_looks_{t}", dict(fb, PL_PANELS="blackhole.card=o"), frames=12, theme=t)
        cl = run_app(binary, work, f"bh_looks_{t}_c", dict(fb, PL_PANELS="blackhole.card=c"), frames=12, theme=t)
        B, A = Bmp(o.bmp), Bmp(cl.bmp)
        cr = o.rects.get("card.blackhole.card")
        if cr:
            x0, y0, w, h = (int(v) for v in cr)
            px = sorted((B.px(x, y) for y in range(y0 + 40, y0 + h - 4) for x in range(x0 + 8, x0 + w - 8) if B.px(x, y) != A.px(x, y)), key=lum)
            if len(px) >= 500:
                bg = px[len(px) // 2]
                lo, hi = px[len(px) // 200], px[-(len(px) // 200) - 1]
                check(max(wcag(lo, bg), wcag(hi, bg)) >= 4.5, f"{t}: Film: the card's text {max(wcag(lo, bg), wcag(hi, bg)):.1f}:1 over its glass")
            else: check(False, f"{t}: Film: the card drew {len(px)} pixels")
        else: check(False, f"{t}: Film: the card is open")
        for name in ("chip", "ctlhead.look"):
            r0 = o.rects.get(name)
            if not r0: check(False, f"{t}: Film: {name} is drawn"); continue
            x0, y0, w, h = (int(v) for v in r0)
            q = sorted((B.px(x, y) for y in range(y0 + 2, y0 + h - 2) for x in range(x0 + 4, x0 + w - 4)), key=lum)
            bg = q[len(q) // 2]
            k = len(q) // 200 if name == "chip" else 0                    # the chip is glass over the picture: past its outliers; the panel is solid (part_look's rule)
            ratio = max(wcag(q[k], bg), wcag(q[-k - 1], bg))
            check(ratio >= 4.5, f"{t}: Film: {name}'s text {ratio:.1f}:1 over its own background (need 4.5)")

def light_module():
    # the true exposure's constants as the module states them (src/blackhole/light.h, orbit/sky.h, orbit/units.h), to compare the readout with
    lh = open(os.path.join(SRC, "blackhole", "light.h"), encoding="utf-8").read()
    sk = open(os.path.join(SRC, "orbit", "sky.h"), encoding="utf-8").read()
    un = open(os.path.join(SRC, "orbit", "units.h"), encoding="utf-8").read()
    sunv = float(re.search(r"kSunV = (-?[\d.]+)", lh).group(1))
    rmin = float(re.search(r"kStarMinRadius = ([\d.]+)", sk).group(1))
    rsun = float(re.search(r"kSunRadius = ([\d.]+) \* kKm", un).group(1)) * 1000
    au = float(re.search(r"kAU = ([\d.]+);", un).group(1))
    return sunv, rmin, rsun, au

def part_truelight(binary, work):
    # 0.55.4: Sky beside the disc (Shown / True brightness) and Glare (Off / As an eye sees it), Real look with the disc on (1280 x 800, Dark): the controls only
    # there; True blacks the sky and the readout carries the module's factor (light.h: the Sun's V magnitude -26.76, Willmer 2018 Table 3; the star's light over its
    # dot, written here); a lower exposure brings stars back; the glare changes the picture round the disc far more than far from it; Most realistic; a restart;
    # a 0.55.3 file; Film's glow leaves the sky alone (far sky pixels = Real's with Glare Off); the chip's and the panel's text 4.5:1 in six themes.
    sunv, rmin, rsun, au = light_module()
    base = dict(BASE, PL_BLACKHOLE="1", PL_BH_DISCPTS="1", PL_BH_SPINWAIT="1", PL_PANELS="blackhole.card=c")   # (the glare at once, not after the view rests)
    feed = "preset=feeding;pitch=1.2"
    runs = {}
    calls = [((binary, work, f"bh_tl_{n}", dict(base, PL_BH_SET=f"{feed};{x}", PL_SCRIPT=f"4:click=blackhole.{g}")), dict(frames=12)) for n, x, g in
             (("shown_off", "skybeside=shown;glare=off", "glare.off"), ("true_off", "skybeside=true;glare=off", "skybeside.true"),
              ("true_eye", "skybeside=true;glare=eye", "skybeside.true"), ("shown_eye", "skybeside=shown;glare=eye", "glare.eye"))]   # (a click on the chosen option: the panel scrolls the two controls into view)
    prefetch(calls)
    for (args, kw) in calls: runs[args[2][6:]] = run_app(*args, **kw)
    a = runs["shown_off"]
    check(not a.scripted_missing() and any(k.startswith("seg.blackhole.skybeside") for k in a.rects) and any(k.startswith("seg.blackhole.glare") for k in a.rects),
          f"Real with the disc: both controls ({a.scripted_missing()}, {sorted(k for k in a.rects if 'skybeside' in k or 'glare' in k)})")
    mk, mk2 = marks(runs["true_eye"]), marks(runs["shown_eye"])
    check(mk.get("blackhole.skybeside") == "on" and mk2.get("blackhole.glare") == "on", f"the closest-to-reality dots: True brightness and the eye's glare ({mk}, {mk2})")
    f = run_app(binary, work, "bh_tl_film", dict(base, PL_BH_SET=f"{feed};look=film", PL_SCRIPT="4:click=blackhole.glare.eye"), frames=10)
    n = run_app(binary, work, "bh_tl_nodisc", dict(base, PL_BH_SET="preset=sgra", PL_SCRIPT="4:click=blackhole.glare.eye"), frames=10)
    check(all(r.scripted_missing() and not any("skybeside" in k or "blackhole.glare" in k for k in r.rects) for r in (f, n)), "not for Film, not without the disc")
    # True brightness: every sampled sky pixel off the disc is black (Shown: many are not), with the glare off
    pts = disc_points(a)
    occ = [v for k, v in a.rects.items() if re.match(r"occ\d+$", k) or k in ("handle", "chip", "blackholebar") or k.startswith("card.")]
    free = lambda x, y: y > 60 and not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ)
    offp = [(int(x), int(y)) for x, y in (pts[1] if pts else []) if free(x, y)]
    S, T = Bmp(a.bmp), Bmp(runs["true_off"].bmp)
    offp = [(x, y) for x, y in offp if S.px(x, y) != T.px(x, y) or max(T.px(x, y)) == 0]   # (not the app's own panels, the same in both)
    lit = sum(1 for x, y in offp if max(S.px(x, y)) > 8)
    black = sum(1 for x, y in offp if max(T.px(x, y)) == 0)
    notb = [(x, y, T.px(x, y)) for x, y in offp if max(T.px(x, y)) != 0][:6]
    check(len(offp) >= 50 and lit >= 0.5 * len(offp) and black == len(offp), f"True brightness: the sky off the disc black ({black} of {len(offp)}; Shown: {lit} lit; not black: {notb})")
    # the readout's factor = the module's rule written here: the star's light Omega_Sun 10^(-0.4 (m - m_Sun)) over its dot pi r^2 square points of 1 / K^2 sr each
    cd = run_app(binary, work, "bh_tl_card", dict(base, PL_PANELS="blackhole.card=o", PL_SCRIPT="4:click=blackhole.skybeside.true", PL_BH_SET=f"{feed};skybeside=true;glare=eye"), frames=10)
    tr, d, s = bh(cd, "bhtrue "), disc_note(cd), bh(cd)   # (the card's notes: the card open)
    if tr and d and s:
        K = 400 / (float(s["fov"]) * math.pi / 360)
        star = math.pi * (rsun / au) ** 2 * 10 ** (-0.4 * (float(tr["brightmag"]) - sunv)) / (math.pi * rmin ** 2 / K ** 2)
        want = float(d["bright"]) / star
        check(abs(float(tr["fainter"]) / want - 1) < 1e-4 and float(tr["brightmag"]) < -1.4, f"the readout: the brightest star {tr['fainter']} x fainter than the disc (written here: {want:.6g}; V {tr['brightmag']})")
        step = 0.5 / (255 * 12.92)
        check(abs(float(tr["showsat"]) / (star / step) - 1) < 1e-4 and float(tr["below"]) > 1e6 and int(tr["shown"]) == 0, f"shows first at white {tr['showsat']} (written here {star / step:.6g}); now {tr['below']} x below the step, none drawn")
        miss = [w for r0, w in ((cd, "fainter than"), (cd, "faintest step"), (runs["true_off"], "there are no stars"), (runs["true_off"], "brightness is not")) if not texts_has(r0, w)]
        check(not miss, f"the readout's lines and the honest limits (missing: {miss})")
        # turned down far enough the stars come back (the disc burns out): a thousand times below the readout's exposure (7.5 magnitudes: most of the catalogue)
        e = math.log10(float(tr["showsat"])) - 3
        u = run_app(binary, work, "bh_tl_low", dict(base, PL_BH_SET=f"{feed};skybeside=true;glare=off;exposure={e:.4f}"), frames=12)
        st = stat(u)
        U = Bmp(u.bmp)
        lit2 = sum(1 for x, y in offp if max(U.px(x, y)) > 0)
        check(st is not None and int(st["images"]) >= 20, f"exposure 10^{e:.2f}: stars drawn again ({st and st['images']} images)")
        Ux = sum(1 for y in range(0, 800, 2) for x in range(0, 1280, 2) if max(U.px(x, y)) > 0)
        check(Ux > 200, f"and on the picture ({Ux} lit points sampled; {lit2} of the sky samples)")
    else: check(False, f"the true exposure's readout ({tr}, {d})")
    # the glare: more light round the disc than far from it; disc points dimmed or kept (light moved), the sky near the disc brighter
    G = Bmp(runs["true_eye"].bmp)
    onp = [(int(x), int(y)) for x, y in (pts[0] if pts else []) if free(x, y)]
    def near(x, y, r):
        return any(abs(x - a0) <= r and abs(y - b0) <= r for a0, b0 in onp)
    nearp = [(x, y) for x, y in offp if near(x, y, 40)]
    farp = [(x, y) for x, y in offp if not near(x, y, 120)]
    gain = lambda P: sum(lum(G.px(x, y)) - lum(T.px(x, y)) for x, y in P) / max(1, len(P))
    gn, gf = gain(nearp), gain(farp)
    check(len(nearp) >= 5 and len(farp) >= 5 and gn > 0 and gn > 2 * max(gf, 0), f"Glare: more light round the disc than far from it ({gn:.4g} near, {gf:.4g} far; {len(nearp)}, {len(farp)} points)")
    darker = sum(1 for x, y in onp if lum(G.px(x, y)) < lum(T.px(x, y)) - 1e-6)
    check(texts_has(runs["shown_eye"], "in the eye") and texts_has(runs["shown_eye"], "different veil") and darker > 0,
          f"Glare's text; the disc gives up light to the veil ({darker} disc points darker)")
    gl = bh(cd, "bhglare ")
    check(gl is not None and 0.25 < float(gl["share"]) < 0.3, f"the eye spreads s of each point's light ({gl})")
    # Most realistic picks True brightness and the eye's glare
    m = run_app(binary, work, "bh_tl_realest", dict(base, PL_BH_SET=f"{feed};skybeside=shown;glare=off", PL_SCRIPT="6:click=blackhole.realest"), frames=12)
    s = bh(m)
    check(not m.scripted_missing() and s is not None and s["skybeside"] == "true" and s["glare"] == "eye", f"Most realistic: True brightness, the eye's glare ({s and (s['skybeside'], s['glare'])})")
    # a restart keeps the settings; a 0.55.3 file opens as Sky Shown (and the eye's glare, the default)
    sf = os.path.join(work, "bh_tl_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    run_app(binary, work, "bh_tl_keep1", dict(base, PL_SETTINGS=sf, PL_BH_SET=f"{feed};skybeside=shown;glare=eye", PL_SCRIPT="6:click=blackhole.skybeside.true;14:click=blackhole.glare.off"), frames=22)
    k2 = run_app(binary, work, "bh_tl_keep2", dict(BASE, PL_SETTINGS=sf), frames=10)
    s = bh(k2)
    check(s is not None and s["skybeside"] == "true" and s["glare"] == "off", f"a restart keeps True brightness and Glare Off ({s and (s['skybeside'], s['glare'])})")
    old = os.path.join(work, "bh_old0553.txt")
    with open(old, "w", encoding="utf-8", newline="\r\n") as fo:
        fo.write("# Physics Lab settings. Written by the app; safe to delete.\ntheme=dark\nlab=Black Hole Lab\nblackhole.preset=feeding\nblackhole.disc=on\nblackhole.look=real\n")
    o3 = run_app(binary, work, "bh_tl_old", dict(BASE, PL_SETTINGS=old), frames=10)
    s = bh(o3)
    check(s is not None and s["skybeside"] == "shown" and s["glare"] == "eye", f"a 0.55.3 file opens as Sky Shown, the eye's glare ({s and (s['skybeside'], s['glare'])})")
    # Film's glow leaves the sky alone: at the paper's view, far sky pixels (the corners) are Real's with Glare Off, pixel for pixel
    pv = "preset=feeding;spin=0.6;dist=74.1;pitch=1.5107643;fov=30"
    fa = run_app(binary, work, "bh_tl_filmsky", dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SET=f"{pv};look=film"), frames=12)
    ra = run_app(binary, work, "bh_tl_realsky", dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SET=f"{pv};look=real;glare=off"), frames=12)
    F, R0 = Bmp(fa.bmp), Bmp(ra.bmp)
    occ2 = [v for k, v in fa.rects.items() if re.match(r"occ\d+$", k) or k == "handle"]
    corner = [(x, y) for y in range(80, 260, 3) for x in list(range(20, 260, 3)) + list(range(1020, 1260, 3))
              if not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ2)]
    same = sum(1 for x, y in corner if F.px(x, y) == R0.px(x, y))
    check(len(corner) > 500 and same == len(corner), f"Film: its glow no longer touches the far sky ({same} of {len(corner)} sky pixels = Real's)")
    # the chip's and the Controls panel's text over the new picture (True brightness with the glare), six themes at 1280 x 800
    themes = ("dark", "light", "black", "paper", "monochrome", "contrast")
    tb = dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SET=f"{feed};skybeside=true;glare=eye", PL_SCRIPT="4:click=blackhole.glare.eye")
    prefetch([((binary, work, f"bh_tl_{t}", tb), dict(frames=12, theme=t)) for t in themes])
    for t in themes:
        o = run_app(binary, work, f"bh_tl_{t}", tb, frames=12, theme=t)
        B = Bmp(o.bmp)
        hd = sorted(k for k in o.rects if k.startswith("ctlhead."))
        for name in ("chip", hd[0] if hd else "ctlhead"):
            r0 = o.rects.get(name)
            if not r0: check(False, f"{t}: {name} is drawn"); continue
            x0, y0, w, h = (int(v) for v in r0)
            q = sorted((B.px(x, y) for y in range(y0 + 2, y0 + h - 2) for x in range(x0 + 4, x0 + w - 4)), key=lum)
            bg = q[len(q) // 2]
            k = len(q) // 200 if name == "chip" else 0
            ratio = max(wcag(q[k], bg), wcag(q[-k - 1], bg))
            check(ratio >= 4.5, f"{t}: True brightness with the glare: {name}'s text {ratio:.1f}:1 over its own background (need 4.5)")

# 0.55.5: the frame never waits for the processor (BlackHoleView::async; PL_BH_FRAMETIME prints each frame's own-thread time and the shown picture)
SMOOTH_LIMIT_MS = 8.0          # the frame thread's worst time in the lab during the drags (measured on this software renderer: REPORT.md of 0.55.5)
def frames_of(r):
    out = []
    for ln in r.out.splitlines():
        if ln.startswith("PLBHFRAME "):
            out.append(dict(kv.split("=", 1) for kv in ln.split()[1:]))
    return out

SMOOTH_TILT_SHARE = 0.9   # (measured here, LP_NUM_THREADS=1: 16 of 16; with 0.55.7's coarse table 12 of 16; REPORT 0.55.8)

def part_smooth(binary, work):
    # Real look, disc on, the default glare, spin 0.6 (1280 x 800, Dark): a scripted tilt drag over the picture, then a spin drag on the slider, then rest
    # (PL_FRAME_MS=16: a frame takes at least 16 ms, as with VSync on a real screen; the rest is long enough for the fine table on this software renderer)
    # (0.55.6: this first run is the processor's path, GPU light paths off, as 0.55.5; the card's path follows below)
    base = dict(BASE, PL_BLACKHOLE="1", PL_BH_ASYNC="1", PL_BH_FRAMETIME="1", PL_FRAME_MS="16", PL_PANELS="blackhole.card=o", PL_BH_SET="preset=feeding;spin=0.6",
                PL_BH_GPUPATHS="0")
    reach = "4:click=##blackhole.spin.text;10:key=escape"                # (the Spin row scrolled into the Controls panel's view)
    r0 = run_app(binary, work, "bh_smooth_where", dict(base, PL_SCRIPT=reach), frames=16)
    sl = r0.rects.get("slider.blackhole.spin.fill")
    kn = r0.rects.get("slider.blackhole.spin.knob")
    check(sl is not None and kn is not None, "the Spin slider's place was printed")
    if sl is None or kn is None: return
    sx, sy = kn[0] + kn[2] / 2, kn[1] + kn[3] / 2                      # (from the knob: a press there keeps the value)
    steps = [reach]
    for k in range(4):                                                   # tilt: four drags straight down over the free sky left of the hole
        steps.append(f"{20 + 7 * k}:drag=300,{300 + 30 * k},300,{330 + 30 * k}")
    for k in range(4):                                                   # spin: four drags along the slider to the left, down to 0 (no fine table to wait for at the end:
        x0 = sx - 70 * k                                                 # on this software renderer its picture takes over a minute to draw)
        steps.append(f"{90 + 7 * k}:drag={x0:.1f},{sy:.1f},{x0 - 70:.1f},{sy:.1f}")
    # the run ends once the lab has had nothing to do for 90 frames after the drags (PL_BH_SETTLE), at the latest after 4000 frames (64 s)
    r = run_app(binary, work, "bh_smooth", dict(base, PL_SCRIPT=";".join(steps), PL_BH_SETTLE="140:90"), frames=4000)
    f = frames_of(r)
    s = bh(r)
    check(not r.scripted_missing() and 230 <= len(f) < 3999 and s is not None, f"the run printed its frames and the lab settled ({len(f)} frames, {r.scripted_missing()})")
    if len(f) < 230 or s is None: return
    # (while a drag is under way: 0.55.6, only the frames in which the settings moved, so a picture that caught up during a pause between two drag steps
    # may make its glare: on this software renderer the glare's drawing runs inside the frame's own calls, on a graphics card it is queued)
    moving = [x for a, x in zip(f, f[1:]) if (20 <= int(x["f"]) <= 50 or 90 <= int(x["f"]) <= 120) and x.get("want") != a.get("want")]
    worst = max(float(x["draw"]) + float(x["glare"]) for x in moving)
    check(worst < SMOOTH_LIMIT_MS, f"during the tilt and spin drags the frame thread's worst time in the lab is {worst:.2f} ms (limit {SMOOTH_LIMIT_MS} ms)")
    check(any(x.get("behind") == "1" for x in moving), "while the view moves the lab says the picture is behind (the worker's picture comes later)")
    # 0.55.8: the tilt drag at spin on the processor's path: a new picture follows nearly every frame in which the view moved (the moving table, spin_sky.h
    # kSpinMoving; 0.55.7's coarse table on this machine: see REPORT 0.55.8)
    tm = [i for i in range(1, len(f) - 1) if 20 <= int(f[i]["f"]) <= 50 and f[i].get("want") != f[i - 1].get("want")]
    tn = [i for i in tm if int(f[i + 1].get("pic", "0")) > int(f[i].get("pic", "0"))]
    print(f"  tilt at spin 0.6 (processor): a new picture after {len(tn)} of {len(tm)} moved frames; coarse ms {[f[i].get('coarsems') for i in tm]}")
    check(len(tm) >= 8 and len(tn) >= SMOOTH_TILT_SHARE * len(tm), f"processor: a new picture after {len(tn)} of {len(tm)} frames of the tilt drag at spin 0.6 in which the view moved (needs {SMOOTH_TILT_SHARE:.0%})")
    shown = [x for x in f if x.get("pic", "0") != "0"]                   # (frames with a picture)
    one = [x for x in shown if x["sky"] == x["made"]]
    check(len(shown) > 0.9 * len(f) and len(one) == len(shown), f"every frame shows one view: the sky's settings = those its exposure and stars were made for ({len(shown) - len(one)} of {len(shown)} frames differ)")
    pics = [int(x.get("pic", "0")) for x in f]
    check(all(b >= a for a, b in zip(pics, pics[1:])), "the shown picture is never an older one than the frame before")
    last = f[-1]
    if "sky" not in last: check(False, "the frames name the shown picture's settings"); return
    sky = [float(v) for v in last["sky"].split(",")]
    want = [float(s["yaw"]), float(s["pitch"]), float(s["dist"]), float(s["spin"])]
    s0 = bh(r0)
    check(s0 is not None and float(s["spin"]) == 0 and float(s["pitch"]) != float(s0["pitch"]),
          f"the drags moved the settings (spin {s['spin']}, pitch {s['pitch']})")
    check(last["behind"] == "0" and all(abs(a - b) < 1e-5 for a, b in zip(sky, want)),
          f"at rest the picture caught up with the settings ({last['sky']} vs {want}, fine {s['fine']}, behind {last['behind']})")
    tail = f[-60:]
    keys = ("redraws", "coarse", "fine", "stars", "pic")
    grew = [k for k in keys if tail[0][k] != tail[-1][k]]
    check(not grew, f"at rest nothing is redone in the last 60 frames (grew: {grew})")
    check(any(float(x.get("glarewait", "0")) > 0 for x in f if int(x["f"]) > 130), "after the drags the glare was made (on its own thread)")
    # 0.55.6, the card's path (GPU light paths on; on this software renderer the card's work runs inside the frame's calls, so its frame times are not
    # checked here): the same tilt drags, then rest at spin 0.6. Every picture is the card's table at the fine table's quality (no coarse table, the
    # processor's fine thread never starts), a new picture follows nearly every frame in which the view moved, at rest the picture is the settings' own.
    # (PL_BH_CARDGRID: a smaller grid, for this renderer's speed; blackhole_physics and the part precision test the full one)
    cb = dict(base, PL_BH_GPUPATHS="1", PL_BH_CARDGRID="32,64,256")
    rc = run_app(binary, work, "bh_smooth_card", dict(cb, PL_SCRIPT=";".join(steps[:5]), PL_BH_SETTLE="60:30"), frames=600, timeout=3000)
    fc = frames_of(rc)
    sc = bh(rc)
    check("PLBHCARD state=1" in rc.out and len(fc) > 60 and sc is not None, f"the card's path passed its start-up check and the run settled ({len(fc)} frames)")
    if len(fc) <= 60 or sc is None: return
    mv = [i for i in range(1, len(fc) - 1) if 20 <= int(fc[i]["f"]) <= 50 and fc[i].get("want") != fc[i - 1].get("want")]
    new = [i for i in mv if int(fc[i + 1]["pic"]) > int(fc[i]["pic"])]
    check(len(mv) >= 8 and len(new) >= 0.9 * len(mv), f"card: a new picture after {len(new)} of {len(mv)} frames in which the view moved")
    pics = [x for x in fc if int(x["f"]) >= 20 and x.get("pic", "0") != "0"]
    check(all(x["card"] == "1" for x in pics), f"card: every picture shown is the card's table ({sum(x['card'] != '1' for x in pics)} frames another)")
    check(fc[-1]["coarse"] == fc[0]["coarse"] == "0", f"card: no coarse table was made ({fc[-1]['coarse']})")
    check(all(x.get("finethread") == "0" for x in fc), "card: the processor's fine table never started, at rest either")
    last = fc[-1]
    check(last["behind"] == "0" and last["card"] == "1" and last["sky"] == last["want"], f"card: at rest the picture is the settings' own ({last['sky']} vs {last['want']})")
    # the start-up check made to fail (test switch): the processor's path, and Settings, Performance says why
    rf = run_app(binary, work, "bh_smooth_cardfail", dict(cb, PL_BH_CARDFAIL="1", PL_OPEN="settings"), frames=30, timeout=1200)
    ff = frames_of(rf)
    said = any("Light paths of the spinning hole: the processor, because the check was made to fail" in n for n in rf.note("bhpaths"))
    check("PLBHCARD state=-1" in rf.out and "test switch" in rf.out and len(ff) > 0 and ff[-1]["cardstate"] == "-1" and ff[-1]["card"] == "0" and ff[-1]["pic"] != "0" and said,
          f"a failed start-up check: the processor's path draws the picture and Settings, Performance says so (said {said})")
    smooth_guard(binary, work, cb)

def smooth_guard(binary, work, cb):
    # 0.55.7, the guard round the card's path (app/gpu_guard.h) and its log, beside the settings file (here PL_SETTINGS in a folder of its own per case).
    # The log of a normal first run: the start's lines, then every step in order, the check's numbers, the first fill piece by piece, the verdict.
    gd = os.path.join(work, "guard_ok"); os.makedirs(gd, exist_ok=True)
    for f in os.listdir(gd): os.remove(os.path.join(gd, f))
    sf = os.path.join(gd, "settings.txt")
    ok = run_app(binary, work, "bh_guard_ok", dict(cb, PL_SETTINGS=sf), frames=60, timeout=1200)
    log = open(os.path.join(gd, "gpu-paths-log.txt"), encoding="utf-8").read() if os.path.exists(os.path.join(gd, "gpu-paths-log.txt")) else ""
    order = ["GPU: ", "OpenGL: ", "Double precision", "begin compile", "end compile the light paths' shader: compiled", "begin link", "end link",
             "begin check: dispatch 1", "begin check: fence wait, dispatch 1", "end check: fence wait", "begin check: read back dispatch 1", "double functions",
             "case 1 (spin 0.90, r 20, theta 1.3): dispatch the shadow's edge", "begin check: case 1 (spin 0.90, r 20, theta 1.3): dispatch 16 rays", "case 2 (spin 0.99",
             "32 rays: edge", "PASSED", "begin fill 1: dispatch pass A", "begin fill 1: fence wait, pass A", "begin fill 1: dispatch the rows' edges",
             "begin dispatch fill 1: piece 1 (rays 0 to 63 of", "begin fence wait, fill 1: piece 1", "begin dispatch fill 1: piece 2 (rays 64 to 319",
             "begin fill 1: copy", "begin fill 1: map the nodes", "begin fill 1: the worker reads the nodes", "end fill 1: the worker reads", "the whole path succeeded"]
    at, missing = 0, []
    for w in order:
        k = log.find(w, at)
        if k < 0: missing.append(w)
        else: at = k
    check("PLBHCARD state=1" in ok.out and not missing and len(log.splitlines()) < 400, f"guard: the log names every step in order on the software renderer's run (missing or out of order: {missing})")
    txt = open(sf, encoding="utf-8").read() if os.path.exists(sf) else ""
    check("gpupaths=ok | " in txt and not os.path.exists(os.path.join(gd, "gpu-paths-step.txt")), "guard: the whole path succeeded: kept as ok, no marker left")
    # the process ended inside a named step (test switch PL_GPU_DIE): the marker names it; the next start compiles nothing, draws on the processor's path,
    # and Settings, Performance names the step; a restart with the same driver stays off, another driver string tries again; "Try again" clears it
    for i, (die, want) in enumerate([("compile", "compile the light paths' shader"), ("check: fence wait, case 1", "check: fence wait, case 1 (spin 0.90, r 20, theta 1.3) edges"),
                                     ("dispatch fill 1: piece 2", "dispatch fill 1: piece 2 (rays 64 to 319 of 4096)")]):
        d = os.path.join(work, f"guard_die{i}"); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d): os.remove(os.path.join(d, f))
        sf = os.path.join(d, "settings.txt")
        run_app(binary, work, f"bh_guard_die{i}", dict(cb, PL_SETTINGS=sf, PL_GPU_DIE=die), frames=60, timeout=1200)
        mk = os.path.join(d, "gpu-paths-step.txt")
        marker = open(mk, encoding="utf-8").read() if os.path.exists(mk) else ""
        check(f"step={want}\n" in marker and "driver=" in marker and "version=" in marker, f"guard: the process ended inside '{die}': the marker names the step ({marker!r})")
        r = run_app(binary, work, f"bh_guard_after{i}", dict(cb, PL_SETTINGS=sf, PL_OPEN="settings"), frames=30, timeout=1200)
        fr = frames_of(r)
        lg = open(os.path.join(d, "gpu-paths-log.txt"), encoding="utf-8").read() if os.path.exists(os.path.join(d, "gpu-paths-log.txt")) else ""
        said = [n for n in r.note("bhpaths") if "stopped the app last time" in n and f"'{want}'" in n]
        check("PLBHCARD state=-1" in r.out and "begin compile" not in lg and len(fr) > 0 and fr[-1]["cardstate"] == "-1" and fr[-1]["card"] == "0" and fr[-1]["pic"] != "0" and said,
              f"guard: after '{die}' the shader is not compiled, the processor's path draws, Settings names the step (said {bool(said)})")
        txt = open(sf, encoding="utf-8").read() if os.path.exists(sf) else ""
        check("gpupaths=off | " in txt and f"| {want}\n" in txt, "guard: the settings file keeps it off with the step")
        check(not os.path.exists(mk), "guard: the marker is gone once the settings file keeps it")
        if i != 0: continue
        r2 = run_app(binary, work, "bh_guard_same", dict(cb, PL_SETTINGS=sf), frames=20, timeout=1200)
        check("PLBHCARD state=-1" in r2.out and "stopped the app last time" in r2.out, "guard: a restart with the same driver stays off")
        sf2 = os.path.join(d, "settings_other.txt")
        with open(sf2, "w", encoding="utf-8") as f: f.write(open(sf, encoding="utf-8").read())
        r3 = run_app(binary, work, "bh_guard_other", dict(cb, PL_SETTINGS=sf2, PL_GPU_DRIVER="Another driver 1.2.3"), frames=60, timeout=1200)
        check("PLBHCARD state=1" in r3.out, "guard: another driver string tries again (and passes here)")
        r4 = run_app(binary, work, "bh_guard_again", dict(cb, PL_SETTINGS=sf, PL_OPEN="settings:gpupaths", PL_SCRIPT="14:click=gpupaths.again"), frames=70, timeout=1200)
        txt = open(sf, encoding="utf-8").read() if os.path.exists(sf) else ""
        check(not r4.scripted_missing() and "PLBHCARD state=1" in r4.out and "gpupaths=ok | " in txt and "stopped the app" not in (r4.note("bhpaths") or [""])[-1],
              f"guard: Try again clears it: the card's path passes in the same run and the settings file keeps ok ({r4.scripted_missing()})")
    # a fence that never signals (test switch PL_BH_CARDHANG; the step's limit 3 s here): the frames go on meanwhile, then the processor's path, kept as slow
    d = os.path.join(work, "guard_hang"); os.makedirs(d, exist_ok=True)
    for f in os.listdir(d): os.remove(os.path.join(d, f))
    sf = os.path.join(d, "settings.txt")
    rh = run_app(binary, work, "bh_guard_hang", dict(cb, PL_SETTINGS=sf, PL_BH_CARDHANG="1", PL_BH_CARDLIMIT="3"), frames=400, timeout=1200)
    fh = frames_of(rh)
    waiting = [x for x in fh if x["cardstate"] == "2"]
    txt = open(sf, encoding="utf-8").read() if os.path.exists(sf) else ""
    check("PLBHCARD state=-1" in rh.out and "did not finish in time (fence wait, dispatch 1)" in rh.out and len(waiting) >= 20 and fh and fh[-1]["card"] == "0" and fh[-1]["pic"] != "0"
          and "gpupaths=slow | " in txt, f"guard: a fence that never signals: {len(waiting)} frames drawn while the check waited, then the processor's path, kept as slow")

def blackhole_film():
    # the Film look's numbers as the module states them (src/blackhole/looks.h FilmChoices), to compare the labels with
    t = open(os.path.join(SRC, "blackhole", "looks.h"), encoding="utf-8").read()
    out = {}
    for k in ("spin", "discT", "whiteT", "rIn", "rOut", "camR", "camThetaDeg"):
        m = re.search(r"\b" + k + r" = ([\d.]+)", t)
        out[k] = float(m.group(1))
    return out

PARTS = {"open": part_open, "presets": part_presets, "distance": part_distance, "light": part_light, "realest": part_realest, "orbitcard": part_orbitcard,
         "settings": part_settings, "layout": part_layout, "look": part_look, "precision": part_precision, "disc": part_disc, "spin": part_spin, "looks": part_looks,
         "truelight": part_truelight, "smooth": part_smooth}

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    part = sys.argv[3] if len(sys.argv) > 3 else None
    for name, fn in PARTS.items():
        if part is None or part == name:
            fn(binary, work)
    print(f"check_blackhole: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
