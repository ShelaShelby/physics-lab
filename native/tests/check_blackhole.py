#!/usr/bin/env python3
# 0.55.0: the Black Hole Lab in the REAL app (Linux, Mesa's software Vulkan; not a real graphics card; counts and pictures, no timing). tests/TESTS.md lists the parts kept and why.
#   open:      the top bar's Black Hole tab opens the lab: its picture (a black shadow in the middle, the bent sky round it), its card, its bar
#   presets:   M87* and Sagittarius A* set the card's mass and measured ring to presets.h's numbers (read from src/blackhole/presets.cpp)
#   distance:  three distances (one typed in the Controls panel): the card's shadow size is Synge's (Perlick & Tsupko 2022, eq. (7), written here in Python) and grows as
#              the camera comes closer; the black disc on the picture has the radius the lab says (to a pixel)
#   light:     Gravity bends light Off and Sky: Black change the picture (off: the middle is sky, not shadow; black: the whole picture is black)
#   realest:   Most realistic sets bending on and our sky
#   orbitcard: the Orbit Lab's Sagittarius A* card has "Open in the Black Hole Lab"; it opens the lab on Sagittarius A*
#   settings:  the lab's settings survive a restart; a 0.54 settings file (no Black Hole lines) still opens its lab
#   disc:      0.55.1, the gas disc: off for the two real holes, on for the feeding preset; switched on, the picture changes where the module puts the disc
#              (nearly face-on and steep), Sagittarius A*'s label; the readout's hottest ring = the papers' (written here); feeding rate and outer edge move
#              the picture and the readout; a restart keeps the settings (1280 x 800, Dark); the taller card at 880 x 560
#   precision: the card's bent sky against the double-precision reference (PL_BH_COMPARE): the shadow's edge, the directions, the plain sky = the Orbit Lab's;
#              0.55.1: the card's gas disc against the double lookup and the reference (two distances, two inclinations); 0.55.2: the same round a spinning hole; 0.56.2: the Film at 2560 x 1369, every pixel
#   spin:      0.55.2, spin: the control exists; spin 0 is the picture of 0.55.1 to the pixel; a typed 0.99 (the top) is taken, 0.999 is not; the card's horizon,
#              inner edge, efficiency and dragging = the papers' (written here); at 0.99 the drawn shadow's edge is the module's at 24 points from the equator
#              (not round there) and from near the axis (round); the presets start at 0 with their sentence; a restart keeps the spin (1280 x 800, Dark)
#              0.55.3: the looks: Real's saved case to the pixel (its hash made by 0.55.2's code; 0.55.8: drawn with 0.55.7's rays, and the new fine table within one level); Film at the paper's view (James et al. 2015, Fig. 13) against the
#              double lookup: the disc's image and its edges where the module puts the paper's edges, one colour all over the disc (0.86.0: J15's opaque,
#              hard-edged disc, no glow of the lab's own: PLAN_REVIEW.md R6)
#   whiteout:  0.70.0, no white-outs: through preset, look, glare, disc and distance changes no frame is over 30 % pure white where the processor's settled
#              picture is under 5 % (the async path, the owner's M87* view)
#   sky, precisioncard: the bent sky's and the card's halves of precision, parts of their own since 0.76.0 (the 5-minute part limit)
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
# 0.72.0: the moving gas (on by default) makes a new picture every frame; the checks below compare still pictures (against references, between runs of different
# lengths, at rest), so they run with it off (PL_BH_GAS=off: a changed reference, not a loosened one; REPORT 0.72.0). Part "gas" checks the moving gas itself.
BASE = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1", "PL_FACTS": "off", "PL_BH_GAS": "off"}
# 0.71.0: the controls that moved into More settings (DECISIONS 21 Nov 2026; mass, the typed boxes, field of view's twin texts, bend, sky, light, sky beside, glare,
# exposure, outer edge, the paper's view, every long caption): a check that clicks or reads one opens the section first (changed reference, not loosened)
MORE = "blackhole.ctl.more=o"
GL = dict(BASE)   # (the precision checks and the guard records on Vulkan)

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
        check(int(st["images"]) > 150 and int(st["extra"]) > 50, f"the stars are drawn, many doubled ({st})")   # (0.70.0: the starting view 45 degrees across, not 90: a quarter of the sky's area; 300 until then)

    # dragging turns the camera round the hole (the yaw of the view), the hole stays in the middle. 0.82.0 (a reference changed on purpose, REPORT 0.82.0): as the
    # Orbit Lab's drag, 0.01 rad a point, a right drag takes the yaw down (until 0.81.0 the sky followed the pointer: up), then it glides at most 2.5 / 7.5 rad
    d = run_app(binary, work, "bh_drag", dict(BASE, PL_BLACKHOLE="1", PL_SCRIPT="6:drag=300,400,400,400"), frames=20)
    s0, s1 = bh(r), bh(d)
    turned = -math.remainder(float(s1["yaw"]) - float(s0["yaw"]), 2 * math.pi) if s0 is not None and s1 is not None else 0
    check(s0 is not None and s1 is not None and 1.0 - 1e-6 <= turned <= 1.0 + 2.5 / 7.5 + 1e-6 and s1["pitch"] == s0["pitch"] and s1["dist"] == s0["dist"],
          f"a drag of 100 points to the right circles the camera 1.0 rad round the hole and a short glide ({turned:.4f} rad; {s0} -> {s1})")

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
        e = dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o," + MORE, **env)
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
    on = run_app(binary, work, "bh_light_on", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="atom.card=o," + MORE), frames=12)
    off = run_app(binary, work, "bh_light_off", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="atom.card=o," + MORE, PL_SCRIPT="6:click=blackhole.bend.off"), frames=14)
    blk = run_app(binary, work, "bh_light_black", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="atom.card=o," + MORE, PL_SCRIPT="6:click=blackhole.sky.black"), frames=14)
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
    # 0.77.0 (outside review item 5): the moving gas's clump and patches are models: Most realistic turns them off
    check(s is not None and s.get("gasclump") == "0" and s.get("gaspattern") == "0", f"Most realistic: the clump and the patches (models) off ({s})")

def part_orbitcard(binary, work):
    r = run_app(binary, work, "bh_orbitcard", dict(BASE, PL_SELECT="far:sgra", PL_PANELS="orbit.card=o", PL_SCRIPT="10:click=card.blackhole"), frames=22)
    check(not r.scripted_missing(), f"the Orbit Lab's Sagittarius A* card has 'Open in the Black Hole Lab' ({r.scripted_missing()})")
    check(any("Open in the Black Hole Lab" in t[2] for t in r.texts), "its words")
    s = bh(r)
    check(s is not None and s["preset"] == "sgra" and any(n.startswith("top.tab.blackhole.on") for n in r.rects), f"it opens the Black Hole Lab on Sagittarius A* ({s})")

def part_settings(binary, work):
    sf = os.path.join(work, "bh_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    a = run_app(binary, work, "bh_keep1", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="atom.card=o," + MORE, PL_SETTINGS=sf, PL_SCRIPT="6:click=blackhole.preset.m87;10:click=##blackhole.dist.text;12:text=7.5;16:click=blackhole.sky.black"), frames=24)
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

def precision_card(binary, work):
    # 0.55.6, the spinning hole's table filled by the graphics card (blackhole/spin_card.h) against the processor's (PL_BH_COMPARE=card). Limits, measured
    # on llvmpipe (REPORT 0.55.6) with room: the shader's own double sine and cosine within 1e-15, exp, log, atan2, acos, cbrt and x^-0.2 within 4e-15 of
    # the C++ library's; the start-up check passes; for the benchmark's case, the Film case and spin 0.99 the whole fine table node by node: the same ends
    # (sky or horizon) everywhere, no ray lost, the far direction within 1e-8 rad from 1e-3 of the shadow's radius outward and 1e-4 rad nearer (the
    # table's own limits against the integrator are 2e-3 and 2e-2: blackhole_physics), b within 1e-7, u within 1e-3 (relative to 1 + |u|; 1e-4 nearer),
    # the edge samples within 1e-12 rad, the rows within 1e-8; on 300 of the card's rays the null condition and Carter's q within 1e-8 at the end (the
    # table's tolerance is 1e-10; blackhole_physics asks 1e-9 at 1e-11), no worse than 10 x the processor's own same rays.
    r = run_app(binary, work, "bh_precision_card", dict(GL, PL_BLACKHOLE="1", PL_BH_COMPARE="card"), frames=3, timeout=3000)
    for ln in r.out.splitlines():
        if ln.startswith("PLBHCARD") or ln.startswith("PLBHRET"): print(ln)
    m = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHCARDMATH ")]
    check(len(m) == 1, "the card's double functions were compared")
    if m:
        x = m[0]
        check(float(x["sin"]) < 1e-15 and float(x["cos"]) < 1e-15 and all(float(x[k]) < 4e-15 for k in ("exp", "log", "atan2", "acos", "cbrt", "pow")),
              f"the shader's double functions = the C++ library's ({' '.join(k + '=' + x[k] for k in ('sin', 'cos', 'exp', 'log', 'atan2', 'acos', 'cbrt', 'pow'))})")
    c = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHCARDCHECK ")]
    check(len(c) == 1 and c[0]["ok"] == "1", f"the start-up check passes on this renderer ({c[0] if c else None})")
    # 0.87.0 (PLAN_REVIEW R11): the returning light's table made on the card against the processor's, the same rays (blackhole_view.cpp): the flux sent,
    # F + F_o, within 1 % at every radius (test_blackhole.cpp R11 (c)'s limit: below 8 levels of 255), every ray answered, at spin 0 and the top spin
    rt = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHRET ")]
    check(len(rt) == 2 and all(float(x["rel"]) < 0.01 and x["unanswered"] == "0" for x in rt), f"the returning light on the card = the processor's ({[(x['a'], x['rel'], x['unanswered']) for x in rt]})")
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

def precision_sky(binary, work):
    # 0.56.0, the spinning hole drawn per pixel by the card from the closed forms in single precision (render/shaders.h kKerrSkyFS) against kerr_closed.h and
    # elliptic.cpp in double (PL_BH_COMPARE=sky; measured on llvmpipe, REPORT 0.56.0). The elliptic functions on 3576 argument sets: R_F, R_J, R_C, F, Pi
    # (n below 1 and past the pole), E, K within 1e-5 relative (measured 1.5e-6 at most), am within 1e-5 (1 + |u|). Per pixel, four pictures of 320 x 200
    # (the benchmark's camera, the Film's, spin 0.99 at 6 M, a camera in the disc's plane): no ray lost; no pixel ending elsewhere (sky or horizon) farther
    # than 1.5 pixels from the shadow's edge (the closed test's edge, single precision runs out only there); the far direction within 2e-3 rad (today's
    # limit for the fine table) 2 pixels or more from the edge at all but 1 in 10000 sky pixels (counted and placed); b within 1e-4; the disc's first image
    # the same at all but 1 in 10000 of its pixels, its radius within 5e-3 and g within 1e-3 (both measured 1.6e-3 / 2.4e-4 at spin 0.99, 3e-5 elsewhere),
    # over 1e-3 at no more than 1 in 10000 disc pixels. The start-up check passes on this renderer.
    r = run_app(binary, work, "bh_precision_sky", dict(GL, PL_BLACKHOLE="1", PL_BH_COMPARE="sky", PL_EXIT_AFTER_FRAMES="1"), frames=1, timeout=1800)
    seen = set()
    for ln in r.out.splitlines():
        if (ln.startswith("PLBHSKY") or ln.startswith("PLBHTONE")) and ln not in seen: print(ln); seen.add(ln)
    m = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSKYMATH ")]
    check(len(m) >= 1, "the sky shader's elliptic functions were compared")
    if m:
        x = m[0]
        check(all(float(x[k]) < 1e-5 for k in ("rf", "rj", "rc", "am", "f", "pi", "e", "k")),
              "the sky shader's elliptic functions = elliptic.cpp's within single precision (" + " ".join(k + "=" + x[k] for k in ("rf", "rj", "rc", "am", "f", "pi", "e", "k")) + ")")
    pics = {}
    for ln in r.out.splitlines():
        if ln.startswith("PLBHSKYPIC "):
            d = dict(kv.split("=") for kv in ln.split()[1:]); pics[d["case"]] = d
    check(set(pics) == {"benchmark", "film", "top", "plane"}, f"four pictures compared per pixel ({sorted(pics)})")
    for name, x in pics.items():
        w = f"sky per pixel, {name} (spin {x['a']}, {x['dist']} M)"
        sky, disc = int(x["sky"]), int(x["discpx"])
        check(int(x["lost"]) == 0 and int(x["reflost"]) == 0 and int(x["endsfar"]) == 0,
              f"{w}: every ray ends where traceClosed's does more than 1.5 px from the edge ({x['ends']} differ next to it, {x['lost']} lost)")
        check(float(x["dir"]) < 2e-3 or int(x["dirbad"]) * 10000 <= sky,
              f"{w}: far direction within 2e-3 rad (largest {x['dir']}; {x['dirbad']} of {sky} sky pixels over it, the farthest {x['dirbadfar']} px from the edge; within 2 px of it {x['dirnear']})")
        check(float(x["b"]) < 1e-4, f"{w}: b within 1e-4 ({x['b']})")
        check(int(x["discwrong"]) * 10000 <= disc and float(x["r"]) < 5e-3 and float(x["g"]) < 1e-3 and int(x["rbad"]) * 10000 <= disc,
              f"{w}: the disc's first image ({disc} px: {x['discwrong']} another image, r {x['r']}, g {x['g']}, {x['rbad']} over 1e-3)")
        # 0.72.0, the moving gas at 10^6 M of running (uMode 5; limits fixed before looking, sky_card.h): phi within 2e-3 rad and t within 1e-3 of itself of
        # traceClosed's (timed) at the first disc image 2 px or more from the edge, at all but 1 in 10000 of those pixels (the rule of the far direction
        # above); the factor and the clump within 1e-3 of gas.cpp's at every disc pixel
        tp, gp = int(x["timepx"]), int(x["gaspx"])
        check(tp > 0 and int(x["timebad"]) * 10000 <= tp and gp > 0 and x["gasbad"] == "0",
              f"{w}: the moving gas: phi {x['phi']}, t {x['t']} ({x['timebad']} of {tp} over), factor and clump {x['gas']} ({x['gasbad']} of {gp} over)")
    c = [ln for ln in r.out.splitlines() if ln.startswith("PLBHSKYCHECK ")]
    check(bool(c) and " ok=1 " in c[0], f"the sky's start-up check passes on this renderer ({c[:1]})")
    # 0.58.0: the start-up check's light, both Light modes (sky_card.h skyLightCompare: each pixel within its limit derived from the shader's operations)
    lt = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSKYCHECKLIGHT ")]
    check(bool(lt) and int(lt[0]["eyepx"]) == 2 * 48 * 40 and int(lt[0]["allpx"]) == 2 * 48 * 40 and lt[0]["eyebad"] == "0" and lt[0]["allbad"] == "0",
          f"the start-up check's light: every probe pixel within its limit, the eye's and All light ({lt[:1]})")
    # 0.70.0: the start-up check's camera curve (sky_card.h skyToneCompare: every probe pixel within 1e-5 of the screen's white of tone.h's double curve, a limit
    # fixed before looking)
    tn = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSKYCHECKTONE ")]
    check(bool(tn) and int(tn[0]["px"]) == 2 * 48 * 40 and tn[0]["bad"] == "0", f"the start-up check's camera curve: every probe pixel within its limit ({tn[:1]})")
    # 0.56.2: the owner's window (2560 x 1369), Film's hole (0.6, 74.1 M) at 30 deg across the height, the paper's view and 88, 90, 92 deg from the spin axis,
    # EVERY pixel against traceClosed in double, no allowance: no ray lost, none ending elsewhere more than 1.5 px from the edge, the far direction within
    # 2e-3 rad, the disc's first image the same (a radius within 1e-3 of rIn or rOut excepted: the radius's own limit), its r and g within 1e-3, and every
    # crossing in [rIn, rOut] within 1e-3, all 2 px or more from the edge. 0.56.1's shader lost 60-79 rays on one column here (lambda = 2 r+ / a, where
    # r3 = r+ and (B39)'s two terms cancelled in single precision): the owner's row of dark specks beside the shadow
    r = run_app(binary, work, "bh_precision_skyfull", dict(GL, PL_BLACKHOLE="1", PL_BH_COMPARE="sky", PL_BH_SKYFULL="2560,1369,30", PL_EXIT_AFTER_FRAMES="1"), frames=1, timeout=1800)
    full = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSKYFULL ")]
    for ln in r.out.splitlines():
        if ln.startswith("PLBHSKYFULL "): print(ln)
    check(len(full) == 4 and all(int(x["px"]) == 2560 * 1369 for x in full), f"Film at 2560 x 1369 compared at four tilts ({len(full)})")
    for x in full:
        bad = {k: x[k] for k in ("lost", "reflost", "endsfar", "dirbad", "discwrongout", "rbad", "ubad") if int(x[k]) != 0}
        check(not bad, f"every pixel of {x['case']} (2560 x 1369) within the limits ({bad or 'none over'}; dir {x['dir']}, r {x['r']}, u {x['u']})")
    # the exposure found on the card against the processor's scan of the same points (at rest, 640 x 400): within 1e-3 (measured 4e-5, 3e-8, 1.2e-4).
    # 0.70.0: Real's exposure is the samples' log-average (tone.h): the card's against the processor's at spin 0 and against every sample traced exactly
    # (traceClosed) at a spin (lrel; the spin table's own log-average is printed: its far disc is off by about 1 %, ROADMAP); Film: the brightest channel (rel)
    ex = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSKYEXP ")]
    check({x["case"] for x in ex} == {"benchmark", "film", "spin0", "all", "allspin0"}, f"the card's exposure compared at spin 0.9, Film, spin 0 and All light at 0.9 and 0 ({len(ex)})")
    for x in ex:
        e = float(x["rel"]) if x["case"] == "film" else float(x["lrel"])
        check(x["whitemode"] == "1" and e < 1e-3 and int(x["n"]) > 1000, f"the card's exposure ({x['case']}): {x['card']} against the processor's {x['cpu']} "
              f"(rel {x['rel']}; log-average {x['cardlogavg']} against {x['logavg']}, {x['lrel']}; {x['n']} points)")
    # 0.70.0: the camera curve, the card's Real picture against the processor's (toneCompare): at most 1 % of the disc's inner pixels over 8 levels of 255
    tc = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHTONE ")]
    # 0.79.0: case "streaks", the feeding preset's surface, streaks and clump at 10^6 M of running against the processor's (blackhole_view.cpp toneCompare)
    # 0.84.0: the held camera's own shift drawn: "close" (spin 0, 2.2 GM/c^2, the whole sky round: the shadow fills a narrower view), "closespin" (spin 0.99,
    # 2.2), "pole" (0.99, 2.2, 8 degrees from the axis) and "closetable" (as closespin on the table's pass kKerrFS, the processor's exposure): the same limit
    # 0.87.0 (the disc's real light, PLAN_REVIEW R9, R10): "hot" (10 solar masses: the colour correction well above 1), "edgesky" and "edgetable" (spin 0.99,
    # 85 degrees from the pole: the limb darkening's grazing rays, on the sky pass and the table pass): the same limit. Every case draws the real light.
    check({x["case"] for x in tc} == {"near", "far", "spin", "streaks", "close", "closespin", "pole", "closetable", "hot", "edgesky", "edgetable"}, f"the curve's picture compared at spin 0 near and far, at 0.9, with the streaks, at the closest distance with no spin, the top spin, over the pole, on the table's pass, a hot disc and nearly edge on ({len(tc)})")
    for x in tc:
        floor = 2000 if x["case"] == "close" else 50000                   # (0.84.0: from 2.2 GM/c^2 with no spin the disc covers about 5,100 inner pixels of the whole sky round)
        check(x["ok"] == "1" and int(x["pixels"]) > floor and float(x["share"]) <= 0.01, f"the curve's picture ({x['case']}): {x['over8']} of {x['pixels']} disc pixels over 8 levels (max {x['maxdiff']})")
    # the card's moving picture and its picture at rest at the same resolution: not one pixel differs (the same formulas), Film at the paper's view and
    # 3 degrees below the plane; at half the resolution the pixels more than 8 levels off are counted (printed)
    st = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHSKYSTOP ")]
    check(len(st) == 2 and all(x["samesize"] == "0" for x in st), f"the card's moving picture = its picture at rest at the same resolution ({[(x['tilt'], x['samesize'], x['halfsize']) for x in st]})")

def part_precision(binary, work):   # (0.76.0: the sky's half and the card's are parts of their own, "sky" and "precisioncard": together they took over the 5-minute part limit)
    r = run_app(binary, work, "bh_precision", dict(GL, PL_BLACKHOLE="1", PL_BH_COMPARE="1"), frames=3, timeout=1800)
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
    # 0.0018 here), every radius on the disc within the paper's edges 9.26..18.70 M; every pixel fully on the disc exactly the one colour the module gives (looks.h
    # filmDiscLinear over the screen's white, encoded; 0.86.0 again, 0.81.0 to 0.85 the laid colour of the see-through disc).
    rl = [ln for ln in r.out.splitlines() if ln.startswith("PLBHREAL ")]
    # 0.55.8: the fine table's rays followed Mino's form (src/blackhole/mino.h); 0.55.9: they come from the closed forms (src/blackhole/kerr_closed.h). The same
    # case drawn with 0.55.8's rays (spin_sky.h kSpinFineFillMino) is still 0.55.8's picture to the pixel (0.55.8 showed that picture within one level of
    # 0.55.2's); the new picture differs from it by at most one level of 255 (measured: 1053 of 1,024,000 pixels by one level); its hash saved on purpose
    # (REPORT 0.55.9)
    # the two saved hashes (b37c532bb4916030 with 0.55.8's rays, e31a10335ce4dab6 with 0.55.9's) came from a renderer the app no longer has,
    # so they are printed, not compared: the check that stays is the physics one, the closed forms' picture within one level of Mino's form's
    rv = dict(kv.split("=") for kv in rl[0].split()[1:]) if rl else {}
    print(f"  Real on Vulkan: hash {rv.get('hash')} (0.55.9's rays), {rv.get('old')} (0.55.8's rays)")
    check(int(rv.get("maxdiff", 9)) <= 1 and int(rv.get("pixels", 0)) == 1280 * 800, f"Real: the saved case with the fine table of 0.55.9, within one level of 255 of the same case drawn with 0.55.8's rays ({rl})")
    fl = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHFILM ")]
    check(len(fl) == 1, f"Film compared at the paper's view ({len(fl)})")
    for x in fl:
        check(float(x["spin"]) == 0.6 and float(x["dist"]) == 74.1 and abs(float(x["theta"]) - 86.56 * math.pi / 180) < 1e-6 and float(x["rin"]) == 9.26 and float(x["rout"]) == 18.7,
              f"Film: the paper's spin, view and disc edges ({x['spin']}, {x['dist']}, {x['theta']}, {x['rin']}, {x['rout']})")
        check(int(x["pixels"]) > 20000 and int(x["wrong"]) == 0 and float(x["edgeshift"]) < 0.1, f"Film: the disc where the module puts it ({x['pixels']} pixels, {x['wrong']} wrong, edges moved {x['edgeshift']} px)")
        check(float(x["rmin"]) >= 9.26 - 1e-4 and float(x["rmax"]) <= 18.7 + 1e-4, f"Film: every radius on the disc within the paper's edges ({x['rmin']}, {x['rmax']})")
        # 0.86.0 (PLAN_REVIEW.md R6): Film is J15's opaque, hard-edged disc again (0.81.0 to 0.85: the lab's own see-through layers and a glow): every pixel
        # fully on the disc is the film's one colour (looks.h filmDiscLinear over the screen's white, encoded) within one level of 255, as until 0.80
        check(int(x["offcolour"]) == 0, f"Film: the one colour all over the disc, no glow ({x['offcolour']} off by more than 1 level, largest {x['maxoff']}; {x['colours']} colours, want {x['want']})")
    # 0.55.4: the eye's glare on Real's saved case: the veil lays on the picture the light it spreads, less what leaves the picture (blackhole/light.h; light.cpp's
    # own tests check the rule against the CIE equation and conservation), its time (llvmpipe; reported, not limited)
    ga = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHGLARE ")]
    check(len(ga) == 2, f"the glare measured at two sizes ({len(ga)})")
    for x in ga:
        sp, ld = float(x["spread"]), float(x["laid"])
        check(sp > 0 and 0.8 < ld / sp <= 1.01 and abs(float(x["keep"]) - 0.72066) < 1e-4, f"{x['size']}: the veil lays {ld / sp if sp else 0:.4f} of the light it spreads (the rest leaves the picture), keep {x['keep']}")
        print(f"  glare {x['size']}: block {x['b']} px, grid {x['grid']}, read-back {x['readms']} ms, veil {x['veilms']} ms, picture {x['withoutms']} -> {x['withms']} ms")
    # 0.85.5: the veil made on the card (kGlareLevelFS, kGlareSumFS) against the processor's glareVeil, the reference, on two grids. The card works in
    # single precision. Measured on the RX 9060 XT: 2.3e-6 of the largest veil value, 7.0e-5 relative where the veil is over 1 % of it (0.85.5). Limits fixed
    # from that, with room: 1e-4 and 1e-3 (a float error, far below a level of 255)
    gc = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHGLARECARD ")]
    check(len(gc) == 2 and all(x["made"] == "1" and float(x["diff"]) <= 1e-4 and float(x["reldiff"]) <= 1e-3 for x in gc),
          f"the glare's veil on the card = the processor's ({[(x['grid'], x['diff'], x['reldiff'], x['cardms'], x['cpums']) for x in gc]})")
    p = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHPLAIN ")]
    check(bool(p) and int(p[0]["maxdiff"]) <= 1, f"bending off = the Orbit Lab's sky projection, within one step of 255 ({p})")
    # 0.72.0, the moving gas at spin 0 (kLensFS uDebug 3) after 10^6 M of running: every pixel of the first disc image has phi within 2e-3 rad of the geometry
    # in double and t within 1e-3 of the double lookup (DiscTable::lookupT), every 16th pixel t within 1e-3 of Dormand-Prince on Binet's equation (disc.h
    # rayTime), the streaks' factor (0.79.0; 0.72-0.78: the pattern's) and the clump within 1e-3 of gas.cpp's at the card's own r, phi, t (limits fixed before looking: sky_card.h kSky*Limit)
    g0 = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHGAS0 ")]
    check(len(g0) == 2 and all(int(x["px"]) > 1000 and int(x["refpx"]) > 100 and int(x["gaspx"]) > 1000 and x["bad"] == "0" for x in g0),
          f"the moving gas at spin 0: the card's phi, t, factor and clump = the processor's within their limits ({g0})")
    # 0.79.0: the disc's surface (Real, spin 0): the card's radius against the double lookup's, pixels over 3e-4 relative at most 0.1 % (limit fixed before looking)
    check(all(int(x["rbad"]) * 1000 <= int(x["px"]) for x in g0), f"the disc's surface at spin 0: the card's radius = the processor's ({[(x['dr'], x['rbad'], x['px']) for x in g0]})")

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

# 0.82.1, bugs the owner saw (a bug the owner has seen: the new checks are parts of "disc", the group ui_blackhole)
def part_gasrow(binary, work):
    # 1. the moving gas's row says Off or On, and the picture changes when it is clicked. (0.72.0 to 0.82.0 a button "Moving gas" that did not look like a switch:
    #    the owner thought his gas models were gone.) The picture of a click on Off is the picture of the setting gas=off, to the pixel; the On picture has the streaks.
    # 0.84.0: PL_BH_SPINWAIT: the fine light paths at once. Film spins (0.6), so until the fine table is in, the caption "Working out the light paths ..." stands
    # above this row. On a loaded machine (GitHub, 4 parts at once) it went away between the frame that read the row's place and the press: the click missed
    # (0.83.0's full run: Off read On; reproduced here with 4 runs at once). The check itself is unchanged.
    GAS = dict(BASE, PL_BH_GAS=None, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=c", PL_BH_GAST="3000", PL_BH_SETTLE="16:6", PL_BH_SPINWAIT="1")   # (the same clock, and the run ends once the picture has settled)
    st = "preset=feeding;glare=off;look=film;gasrun=off"   # (0.84.0: the gas held at t = 3000 in every run: with the picture made at once its clock, on the wall's time, moved between runs)
    on = run_app(binary, work, "bh_gas_on", dict(GAS, PL_BH_SET=st), frames=80)
    off = run_app(binary, work, "bh_gas_off", dict(GAS, PL_BH_SET=st, PL_SCRIPT="6:click=blackhole.gas.off"), frames=80)
    ref = run_app(binary, work, "bh_gas_ref", dict(GAS, PL_BH_SET=st + ";gas=off"), frames=80)
    back = run_app(binary, work, "bh_gas_back", dict(GAS, PL_BH_SET=st, PL_SCRIPT="6:click=blackhole.gas.off;12:click=blackhole.gas.on"), frames=80)
    for r, n in ((on, "on"), (off, "off"), (back, "back")):
        check(not r.scripted_missing(), f"gas row, {n}: the buttons were there ({r.scripted_missing()})")
    s = {n: bh(r) for n, r in (("on", on), ("off", off), ("ref", ref), ("back", back))}
    check(all(v is not None for v in s.values()) and s["on"]["gasshown"] == "1" and s["off"]["gasshown"] == "0" and s["ref"]["gasshown"] == "0" and s["back"]["gasshown"] == "1",
          f"the row reads On, Off after a click, Off for gas=off, On after On again ({ {n: v and v['gasshown'] for n, v in s.items()} })")
    check(texts_has(on, "Streaks and clump") and any(k.startswith("seg.blackhole.gas.") for k in on.rects), "the row is named and drawn")
    check(any(t_[2] == "Play" for t_ in on.texts) and not any(t_[2] in ("Pause", "Play") for t_ in ref.texts) and "seg.blackhole.gas.sel1" in on.rects and "seg.blackhole.gas.sel0" in ref.rects, "Pause / Play and the rate are drawn only while it is On")
    def diff(a, b):                                                    # (the picture: left of the Controls panel, below the top bar, above the bar)
        A, B = Bmp(a.bmp), Bmp(b.bmp)
        return sum(1 for y in range(48, 740) for x in range(0, 930) if sum(abs(p - q) for p, q in zip(A.px(x, y), B.px(x, y))) > 6)
    if all(os.path.exists(r.bmp) for r in (on, off, ref, back)):
        check(diff(on, off) > 2000, f"the picture changes when the row is set to Off ({diff(on, off)} pixels)")
        check(diff(off, ref) == 0, f"a click on Off gives the picture of gas=off ({diff(off, ref)} pixels differ)")
        check(diff(back, on) == 0, f"Off, then On gives the streaks back ({diff(back, on)} pixels differ)")
    # Most realistic turns the clump and the streaks off: the row reads Off (it read On before), and On sets them back
    mr = run_app(binary, work, "bh_gas_real", dict(GAS, PL_BH_SET="preset=feeding;gasclump=off;gaspattern=off", PL_SCRIPT="6:click=blackhole.gas.on"), frames=24)
    m0 = run_app(binary, work, "bh_gas_real0", dict(GAS, PL_BH_SET="preset=feeding;gasclump=off;gaspattern=off"), frames=14)
    a, b = bh(m0), bh(mr)
    check(a is not None and b is not None and a["gasshown"] == "0" and b["gasshown"] == "1" and b["gasclump"] == "1" and b["gaspattern"] == "1",
          f"with the clump and the streaks both off the row reads Off, and On sets them on ({a and a['gasshown']}, {b and (b['gasshown'], b['gasclump'], b['gaspattern'])})")
    # 3. Film: a control that does nothing in Film is not drawn; one plain line stands in its place (0.82.0 drew them greyed, with no reason)
    f = run_app(binary, work, "bh_film_ctl", dict(BASE, PL_BLACKHOLE="1", PL_BH_SET="preset=feeding;look=film", PL_PANELS="blackhole.card=c"), frames=14)
    check(not any(k.startswith("slider.blackhole.spin") for k in f.rects) and not any(k.startswith("seg.blackhole.disc.") for k in f.rects),
          f"Film: no Spin slider and no Gas disc Off / On ({[k for k in f.rects if 'blackhole.spin' in k or 'blackhole.disc.' in k]})")
    lines = [t_[2] for t_ in f.texts if t_[2].startswith("Film always draws")]
    check(len(lines) >= 2, f"Film: one plain line for the disc and one for the spin ({lines})")
    g = run_app(binary, work, "bh_real_ctl", dict(BASE, PL_BLACKHOLE="1", PL_BH_SET="preset=feeding", PL_PANELS="blackhole.card=c"), frames=14)
    check(any(k.startswith("slider.blackhole.spin") for k in g.rects) and any(k.startswith("seg.blackhole.disc.") for k in g.rects), "Real: the Spin slider and the Gas disc row are drawn")

# 0.82.1: the disc seen nearly edge on, static (REPORT 0.82.1): the Real look's disc surface (spin 0). Hook 2e of the comparison (blackhole_view.cpp, PL_BH_COMPARE_VIEWS):
# the card against the double lookup at every pixel (the order and the radius within 3 %), and in the picture as the app draws it no pixel of the smooth disc may
# differ from the mean of its four neighbours by over 8 levels of 255 (at most 1 % of them: the Vulkan limit). Sagittarius A* with the disc, feeding 0.11, the
# two views the owner saw (57.5 at pitch 1.575; 1000), the plane (pitch pi/2) and a sweep of 0.2 degrees either side of it at 57.5 and 1000 GM/c^2.
def part_edgeon(binary, work):
    views = [(57.5, 1.575), (1000, 1.575), (1000, math.pi / 2), (57.5, math.pi / 2)]
    for d in (57.5, 1000):
        for o in (-0.0035, -0.0018, -0.0007, 0.0007, 0.0018, 0.0035):
            views.append((d, math.pi / 2 + o))
    spec = ";".join(f"{d},{p:.7f},0.11" for d, p in views)
    r = run_app(binary, work, "bh_edgeon", dict(GL, PL_BLACKHOLE="1", PL_BH_COMPARE="1", PL_BH_COMPARE_VIEWS=spec), frames=3, timeout=3000)
    rows = [dict(kv.split("=") for kv in ln.split()[1:]) for ln in r.out.splitlines() if ln.startswith("PLBHEDGEON ")]
    for x in rows: print("PLBHEDGEON", x)
    check(len(rows) == len(views), f"the edge-on views were compared ({len(rows)} of {len(views)})")
    for x in rows:
        w = f"edge on, {x['dist']} GM/c^2 at pitch {x['pitch']}"
        n, sm = int(x["pixels"]), int(x["smooth"])
        check(int(x["bad"]) <= 0.001 * n and int(x["badorder"]) <= 0.001 * n, f"{w}: the card = the double lookup ({x['bad']} of {n} pixels differ, {x['badorder']} in the order)")
        check(sm > 100000 and int(x["over8"]) <= 0.01 * sm, f"{w}: the near disc is smooth in the picture ({x['over8']} of {sm} pixels over 8 levels; the lookup's own isolated pixels {x['isolated']})")

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
    for name, extra in (("f02", "feed=0.02"), ("f20", "feed=0.2"), ("o15", "outer=15"), ("o60", "outer=60")):
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
        # ten times the feeding rate: T x 10^(1/4) = 1.78; far above the visible a black body's visible light grows about like T. 0.70.0: the exposure is
        # always tied to automatic (stops from it), so the picture is drawn at the disc's own key: the readout's brightest part carries the physics, the picture still changes
        ba, bb = float(disc_note(runs["f02"])["bright"]), float(disc_note(runs["f20"])["bright"])
        check(bb > 1.5 * ba and abs(lb - la) > 0.005 * la, f"a higher feeding rate is brighter on the disc (the readout's brightest part {ba:.4g} -> {bb:.4g}; the picture's light {la:.3g} -> {lb:.3g})")
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
    k1 = run_app(binary, work, "bh_disc_keep1", dict(BASE, PL_BLACKHOLE="1", PL_SETTINGS=sf, PL_BH_SET="disc=on;feed=0.05;outer=40;exposure=stops;exposurestops=1.5"), frames=14)   # (0.70.0: the disc switched on sets the stops back to 0)
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
    base = dict(BASE, PL_BLACKHOLE="1", PL_PANELS="blackhole.card=o," + MORE)   # (0.71.0: the typed spin is in More settings)
    r0 = run_app(binary, work, "bh_spin_none", base, frames=12)
    r1 = run_app(binary, work, "bh_spin_zero", dict(base, PL_BH_SET="spin=0"), frames=12)
    check(r0.bmp and r1.bmp and Bmp(r0.bmp).d == Bmp(r1.bmp).d, "spin 0 draws exactly the picture of 0.55.1 (the same pixels)")
    st = stat(r1)
    check(st is not None and int(st["tablefills"]) >= 1, f"spin 0 uses the non-spinning hole's closed-form table ({st})")
    # typed: 0.99 (the top) through the text field; the card's numbers are the module's (written here from the papers)
    rt = run_app(binary, work, "bh_spin_typed", dict(base, PL_BH_SPINWAIT="1", PL_SCRIPT="6:click=##blackhole.spin.text;8:text=0.99"), frames=20)
    s, n = bh(rt), spin_note(rt)
    check(any(k.startswith("slider.blackhole.spin") for k in r1.rects), "the Spin slider is in the Controls panel (0.71.0: the typed value in More settings)")
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
        r = run_app(binary, work, f"bh_spin_edge_{name}", dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_SPINEDGE="1", PL_BH_SET=f"spin=0.99;pitch={pitch};dist=20;fov=90"), frames=12)   # (0.70.0: the view until then; the start is 45 degrees)
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
    k1 = run_app(binary, work, "bh_spin_keep1", dict(BASE, PL_BLACKHOLE="1", PL_PANELS="atom.card=o," + MORE, PL_SETTINGS=sf, PL_SCRIPT="6:click=##blackhole.spin.text;8:text=0.6"), frames=14)
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
def part_alllight(binary, work):
    # 0.58.0: Light: What an eye sees (the default, the dot) / All light, Real look with the disc on (1280 x 800, Dark): the control there and not for Film; a click
    # sets All light (the state line), which leaves out the eye's glare and the true sky; the disc's picture changes (its centre line brighter on the approaching
    # side against the other far more than in the eye's picture: the 4th power of the shift); the panel's sentence; a restart keeps it.
    base = dict(BASE, PL_BLACKHOLE="1", PL_BH_DISCPTS="1", PL_BH_SPINWAIT="1", PL_PANELS="blackhole.card=c," + MORE)   # (0.71.0: More settings open)
    feed = "preset=feeding;pitch=1.2;glare=off;exposure=stops;exposurestops=-4"   # (0.70.0: 4 stops darker: the camera curve nearly linear there, undone below)
    sf = os.path.join(work, "bh_al_settings.txt")
    if os.path.exists(sf): os.remove(sf)
    e = run_app(binary, work, "bh_al_eye", dict(base, PL_BH_SET=feed, PL_SCRIPT="4:click=blackhole.light.eye"), frames=12)
    a = run_app(binary, work, "bh_al_all", dict(base, PL_BH_SET=feed, PL_SCRIPT="4:click=blackhole.light.all", PL_SETTINGS=sf), frames=12)
    f = run_app(binary, work, "bh_al_film", dict(base, PL_BH_SET=f"{feed};look=film", PL_SCRIPT="4:click=blackhole.light.all"), frames=10)
    se, sa = bh(e), bh(a)
    check(not e.scripted_missing() and not a.scripted_missing() and se is not None and sa is not None and se.get("light") == "eye" and sa.get("light") == "all"
          and sa.get("glareon") == "0" and sa.get("trueon") == "0", f"Light: the control, a click sets All light, no glare, no true sky ({se and se.get('light')}, {sa and {k: sa.get(k) for k in ('light', 'glareon', 'trueon')}})")
    check(marks(e).get("blackhole.light") == "on" and marks(a).get("blackhole.light") == "off", f"the closest-to-reality dot on What an eye sees ({marks(e)}, {marks(a)})")
    check(f.scripted_missing() and not any("blackhole.light" in k for k in f.rects), "not for Film")
    check(texts_has(a, "most of it ultraviolet") and texts_has(a, "4th power"), "All light's sentence")
    # the light far more gathered in the hot, approaching part (the 4th power of T and of the shift): the picture's mean light over its brightest, sampled over the
    # middle of the picture (Part 0: from the hottest ring to 30 GM/c^2 the eye's light falls about 3 times, all light about 40 times)
    E, A = Bmp(e.bmp), Bmp(a.bmp)
    pts = disc_points(e)
    occ = [v for k, v in e.rects.items() if re.match(r"occ\d+$", k) or k in ("handle", "chip", "blackholebar") or k.startswith("card.")]
    onp = [(int(x), int(y)) for x, y in (pts[0] if pts else []) if y > 60 and not any(o[0] - 2 <= x <= o[0] + o[2] + 2 and o[1] - 2 <= y <= o[1] + o[3] + 2 for o in occ)]
    def lw_of(r):                                                        # 0.70.0: Real's burn-out white (tone.h) of the run, from its PLBHDISCPT line
        ln = [x for x in r.out.splitlines() if x.startswith("PLBHDISCPT ")]
        return float(dict(kv.split("=", 1) for kv in ln[-1].split()[1:])["lw"]) if ln else 0.0
    def unc(ld, w):                                                      # the curve undone (Reinhard et al. 2002 eq. (4) solved for L): L^2 / w^2 + L (1 - Ld) - Ld = 0
        ld = min(ld, 0.999999)
        return ld / (1 - ld) if w <= 0 else (-(1 - ld) + math.sqrt((1 - ld) ** 2 + 4 * ld / w ** 2)) * w ** 2 / 2
    def spread(B, w):                                                    # (over the disc's own sampled points, the panels left out; 0.70.0: in the light before the
        v = [unc(lum(B.px(x, y)), w) for x, y in onp] or [0.0]           #  camera curve, proportional to the light itself: the curve is a display choice)
        return sum(v) / len(v) / max(1e-9, max(v))
    se_, sa_ = spread(E, lw_of(e)), spread(A, lw_of(a))
    check(len(onp) >= 50 and sa_ < 0.5 * se_, f"All light: the light gathered in the hot, approaching part (disc points: mean over brightest {sa_:.3g} against the eye's {se_:.3g}; {len(onp)} points)")
    k = run_app(binary, work, "bh_al_restart", dict(base, PL_SETTINGS=sf), frames=6)
    sk = bh(k)
    check(sk is not None and sk.get("light") == "all", f"a restart keeps All light ({sk and sk.get('light')})")

def whites(r):
    return [float(m) for m in re.findall(r"PLBHWHITE f=\d+ share=([\d.]+)", r.out)]

def part_whiteout(binary, work):
    # 0.70.0: no white-outs (the owner's 0.69.0 screenshots: the whole picture white with a few stars on top). The limit, fixed before looking: no frame may
    # have more than 30 % pure-white pixels (all three channels 255; the middle 60 % of the picture, PL_BH_WHITECOUNT) where the settled picture of the same
    # view by the processor's exposure (PL_BH_GPUSKY=0, waited for) has under 5 %. The app's own asynchronous path (PL_BH_ASYNC), Real, Glare: As an eye sees it,
    # Sky beside the disc: Shown, 1280 x 800. Fails on 0.69.0's build: a chosen exposure was an absolute screen's white kept across presets (M87* with a bright
    # choice, then the feeding hole: 88 % white); since 0.70.0 the choice is stops from automatic, back to 0 on a new preset, look or disc.
    base = dict(BASE, PL_BLACKHOLE="1", PL_BH_ASYNC="1", PL_BH_WHITECOUNT="1", PL_PANELS="blackhole.card=c," + MORE, PL_BH_SETTLE="64:6")   # (0.71.0: the glare's control in More settings)   # (to the end: the glare at rest)
    view = "dist=1000;fov=48;skybeside=shown;glare=eye"
    m87 = f"preset=m87;disc=on;feed=0.11;outer=1800;{view}"
    # (exposure=-2: 0.69.0's absolute white, 10^-2 of the Sun's surface; read since 0.70.0 as an old file's choice, at 0 stops; exposurestops: 0.70.0's +12 stops)
    runs = [("bh_wo_m87feed", dict(base, PL_BH_SET=f"{m87};exposure=-2;exposurestops=12", PL_SCRIPT="20:click=blackhole.preset.feeding"), 600),
            ("bh_wo_m87auto", dict(base, PL_BH_SET=f"{m87};exposure=auto", PL_SCRIPT="20:click=blackhole.look.film;32:click=blackhole.look.real;44:click=blackhole.glare.off;"
                                                                                  "52:click=blackhole.glare.eye;60:wheel=3"), 600),
            ("bh_wo_feedsw", dict(base, PL_BH_SET="preset=feeding;exposure=auto;skybeside=shown;glare=eye", PL_SCRIPT="16:click=blackhole.disc.off;24:click=blackhole.disc.on;"
                                                                                  "34:click=blackhole.preset.m87;44:click=blackhole.disc.on;56:click=blackhole.preset.feeding"), 600)]
    refs = [("bh_wo_ref_feed", f"preset=feeding;{view}"), ("bh_wo_ref_m87", m87), ("bh_wo_ref_feed20", "preset=feeding;skybeside=shown;glare=eye")]
    calls = [((binary, work, n, e), dict(frames=f)) for n, e, f in runs]
    calls += [((binary, work, n, dict(BASE, PL_BLACKHOLE="1", PL_BH_SPINWAIT="1", PL_BH_GLARENOW="1", PL_BH_GPUSKY="0", PL_BH_WHITECOUNT="1", PL_PANELS="blackhole.card=c",
                                      PL_BH_SET=st)), dict(frames=16)) for n, st in refs]
    prefetch(calls)
    ref = {}
    for (args, kw) in calls[len(runs):]:
        w = whites(run_app(*args, **kw))
        ref[args[2]] = w[-1] if w else 1.0
    check(all(v < 0.05 for v in ref.values()), f"the processor's settled pictures of these views are not white ({ref})")
    for (args, kw) in calls[:len(runs)]:
        r = run_app(*args, **kw)
        w = whites(r)
        bad = [(i, v) for i, v in enumerate(w) if v > 0.30]
        check(len(w) >= 64 and not r.scripted_missing() and not bad, f"{args[2]}: no frame over 30 % pure white ({len(w)} frames, worst {max(w or [1]):.3f}, "
              f"over: {bad[:5]}, missing clicks: {r.scripted_missing()})")

PARTS = {"sky": precision_sky, "open": part_open, "presets": part_presets, "distance": part_distance, "light": part_light, "realest": part_realest, "orbitcard": part_orbitcard,
         "settings": part_settings, "precision": part_precision, "precisioncard": precision_card, "disc": part_disc, "gasrow": part_gasrow, "edgeon": part_edgeon, "spin": part_spin, "alllight": part_alllight, "whiteout": part_whiteout}

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    part = sys.argv[3] if len(sys.argv) > 3 else None
    for name, fn in PARTS.items():
        if part is None or part == name:
            fn(binary, work)
    print(f"check_blackhole: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
