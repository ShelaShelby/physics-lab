#!/usr/bin/env python3
# 0.15 stage 5k and later: pages and panels of the top bar, in the REAL app (Linux, software Vulkan, scripted events; NOT a real mouse, NOT Windows). tests/TESTS.md lists the parts kept and why.
#   log    the changelog page opens; calc   the calculator works (0.49); data   the Data panel works (0.51); about   the About page's last line (0.74.0)
#   check_5k.py <PhysicsLab binary> <work folder> <part>
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, Bmp

SIZES = ("880x560", "2000x1127")   # 0.38.1: the narrow window (Contents menu) and the wide one (Contents rail); 1109x882 dropped
THEMES = ("dark",)                  # 0.38.1: scrolling does not depend on the theme (the look tests check the colours); light dropped
PAGES = (("about", {"PL_OPEN": "about"}), ("learn1", {"PL_LEARN": "1"}), ("timeline", {"PL_LEARN": "3"}), ("history", {"PL_OPEN": "history"}))
FULL = ("about", "timeline")                                    # these get every focus case in both themes; the others the plain wheel and key cases
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def scroll(r):
    for n in r.note("pagescroll"):
        m = re.match(r"pagescroll y=(-?\d+) max=(-?\d+)", n)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None

def pscrolls(r):
    out = []                                                     # (frame, y, max, [the scroll offset of each Contents entry at that moment])
    for ln in r.out.splitlines():
        if ln.startswith("PLSCROLL "):
            a, b = ln.split("|")
            f = a.split()
            out.append((float(f[1]), float(f[2]), float(f[3]), [int(v) for v in b.split()]))
    return out

def anchors(r):
    n = r.note("tocanchors")
    return [int(v) for v in n[0].split()[1:]] if n else []

def mid(rc):
    return rc[0] + rc[2] / 2, rc[1] + rc[3] / 2

def run(binary, work, name, env, sz, theme, frames):
    return run_app(binary, work, name, env, frames=frames, size=sz, theme=theme)

def nm_sz(nm):
    for sz in SIZES:
        if f"_{sz}_" in nm:
            return sz
    return "1280x800"

def nm_theme(nm):
    return "light" if "_light" in nm else "dark"

def part_log(binary, work):
    r = run(binary, work, "klog", {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_SCRIPT": "6:move=640,400;10:wheel=-2;14:key=pagedown;18:key=ctrl+shift+f12"}, "1280x800", "dark", 40)
    path = os.path.join(os.path.abspath(work), "home_klog", ".local", "share", "PhysicsLab", "PhysicsLab", "physics-lab-input.txt")
    check(os.path.isfile(path), f"the hotkey wrote {path}")
    if not os.path.isfile(path):
        return
    t = open(path, encoding="utf-8").read()
    lines = t.splitlines()
    check(len(lines) < 200, f"the file is under 200 lines ({len(lines)})")
    for key in ("time:", "app window: 1280 x 800", "theme: dark", "motion: on", "hovered window:", "focused window:", "WantCaptureMouse=", "WantCaptureKeyboard=", "reading page: shown", "scroll y=", "max=",
                "flags=0x", "rect=", "dimmed layer: yes", "contents list:", "popups open:", "last wheel events", "wheel -2.00 at", "last key events", "PageDown down"):
        check(key in t, f"the file says \"{key}\"")
    check(any("Input log written to" in x[2] for x in r.texts), "the toast says where the log was written")

def part_calc(binary, work):
    # 0.49: the calculator. The top bar's button opens it; a typed line is worked out live; Enter keeps it in the history (and the settings file); the key 2 typed into it
    # does not switch labs; Esc closes it and Ctrl+= opens it again; a formula of the selected planet works out its numbers; a history line can be used again.
    seq = ("10:click=top.calc;20:text=100 km / 2 hr to km/hr;24:key=enter;30:key=2;34:key=escape;44:key=ctrl+plus;50:click=calc.clear;"
           "56:click=calc.recipe1;60:move=400,600;61:wheel=-20;66:click=calc.hist0")   # (the history is below the keypad: scrolled to first)
    env = {"PL_NEWUI": "on", "PL_SCRIPT": seq, "PL_SELECT": "Earth"}
    r = run_app(binary, work, "calc_40", env, frames=40, size="1280x800")
    check("panel.calc" not in r.rects, "Esc in the calculator's box closes it (frame 40)")
    r = run_app(binary, work, "calc_80", env, frames=80, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ")]
    def find(pred):
        return [ln for ln in lines if pred(ln)]
    check(find(lambda ln: ln.endswith("|100 km / 2 hr to km/hr") and " number=50 unit=km/hr " in ln), "100 km / 2 hr to km/hr is worked out as 50 km/hr while typing")
    check(find(lambda ln: " hist=1 " in ln and " ans=13.888888888888889 " in ln), "Enter keeps it: one history line, and ans = 13.89 m/s (50 km/hr in SI)")
    check(lines and all(" lab=0 " in ln for ln in lines), "typing (and the key 2) in the calculator never switches the lab")
    check("panel.calc" in r.rects, "Ctrl+= opens it again (frame 80)")
    rec = find(lambda ln: ln.endswith(" to km/s") and "|(" in ln)
    check(rec and " number=29." in rec[0] and " unit=km/s " in rec[0], f"Earth's formula: its speed round the Sun, 29.x km/s ({rec[:1]})")
    check(lines and lines[-1].endswith("|100 km / 2 hr to km/hr") and " number=50 " in lines[-1], f"a tap on the history puts the line back ({lines[-1:]})")
    saved = ""
    for root, _, files in os.walk(os.path.join(os.path.abspath(work), "home_calc_80")):
        for f in files:
            try:
                t = open(os.path.join(root, f), encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            if "calc.0=" in t:
                saved = t
    check("calc.0=100 km / 2 hr to km/hr" in saved, "the history is in the settings file (calc.0=...)")
    # the keypad: the same line built with taps only (no typing); ⌫ takes off a whole unit name ("hr", not "h")
    keys = ["1", "0", "0", "km", "÷", "2", "hr", "to", "km", "÷", "hr", "⌫", "hr"]
    seq = "10:click=top.calc;" + ";".join(f"{14 + 4 * i}:click=calc.key.{k}" for i, k in enumerate(keys))
    r = run_app(binary, work, "calc_keys", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=14 + 4 * len(keys) + 8, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ")]
    check(lines and lines[-1].endswith("|100 km / 2 hr to km / hr") and " number=50 " in lines[-1], f"the keypad alone builds 100 km / 2 hr to km / hr = 50 ({lines[-1:]})")
    check(any(ln.endswith("|100 km / 2 hr to km /") for ln in lines), "⌫ takes off the whole name hr")
    # 0.50.1: the Rabi line: ħ (typed hbar) makes the answer an angular frequency; the note says so and gives the cyclic f = ω/2π = 1279.5 MHz
    seq = "10:click=top.calc;20:text=e*a0*1e5 V/m/hbar to MHz"
    r = run_app(binary, work, "calc_rabi", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=30, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ") and ln.endswith("|e*a0*1e5 V/m/hbar to MHz")]
    check(lines and " number=8039.617101 " in lines[-1] and "angular frequency" in lines[-1] and "1279.5" in lines[-1],
          f"the Rabi line shows 8039.617101 MHz with the angular-frequency note and f = 1279.5 MHz ({lines[-1:]})")
    # 0.50.2: the Larmor line μp B / h (ħ divided by h) is a cyclic frequency, so NO ω note (0.50.1 offered 3.388 MHz)
    seq = "10:click=top.calc;20:text=gp*e*hbar/(2*mp)*1 T/(2*hP) to MHz"
    r = run_app(binary, work, "calc_larmor", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=30, size="1280x800")
    lines = [ln for ln in r.out.splitlines() if ln.startswith("PLCALC ") and ln.endswith("|gp*e*hbar/(2*mp)*1 T/(2*hP) to MHz")]
    check(lines and " number=21.28873923 " in lines[-1] and "angular" not in lines[-1] and "ω/2π" not in lines[-1],
          f"the Larmor line μp B/h shows 21.28873923 MHz with no ω note ({lines[-1:]})")

def part_data(binary, work):
    # 0.51: the Data panel. The top bar's button opens it on the lab's first table with its first worked analysis (Kepler's third law: the planets' periods against a,
    # a power law: n = 3/2 within the giants' mass ratio); the user's own data (the example: a pendulum with uncertainties) is fitted weighted; Send puts the parameters
    # into the calculator, which opens; Esc closes the calculator, then the panel. Then Menu > Copy setup / Paste setup brings back the Atom Lab and its element.
    def pldata(r):
        return [ln for ln in r.out.splitlines() if ln.startswith("PLDATA ")]
    seq = ("10:click=top.data;16:click=data.tables;20:click=data.table.user;24:move=400,500;25:wheel=-10;30:click=data.model;34:move=400,500;35:wheel=-10;"
           "40:click=data.model.power;46:click=data.send;52:key=escape;56:key=escape")
    r = run_app(binary, work, "data_fit", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=62, size="1280x800")
    lines = pldata(r)
    kep = [ln for ln in lines if " table=planets " in ln and " model=power " in ln and " ok=1 " in ln]
    p1 = float(re.search(r" p1=(\S+)", kep[0]).group(1)) if kep else 0
    check(kep and abs(p1 - 1.5) < 1e-3, f"the Data panel opens on the planets with Kepler's third law fitted: n = {p1} (3/2 within 1e-3)")
    usr = [ln for ln in lines if " table=user " in ln and " model=power " in ln and " ok=1 " in ln and " weighted=1 " in ln]
    check(usr, "the example data are fitted with a power law, weighted by their uncertainties")
    sent = [ln for ln in lines if "send=In the calculator now: fit_Apow" in ln]
    check(sent, "Send to calculator names fit_Apow")
    check(not r.scripted_missing(), f"every scripted control was there ({r.scripted_missing()[:2]})")
    check("panel.data" not in r.rects and "panel.calc" not in r.rects, "Esc closes the calculator, then the Data panel")
    seq = "10:click=top.menu;14:click=menu.copysetup;20:key=1;26:click=top.menu;30:click=menu.pastesetup;36:click=top.data"
    r = run_app(binary, work, "data_setup", {"PL_NEWUI": "on", "PL_SCRIPT": seq, "PL_ATOM": "1", "PL_ATOM_SET": "Z=26"}, frames=44, size="1280x800")
    lines = pldata(r)
    check(lines and " table=ionlines " in lines[-1] and " rows=91 " in lines[-1], f"Paste setup brings back the Atom Lab with iron (its one-electron ion's 91 NIST lines): {lines[-1:]}")
    check(not r.scripted_missing(), f"every scripted control was there ({r.scripted_missing()[:2]})")
    # 0.51.1: the moons' worked analysis holds n at 3/2 (A alone is fitted: Jupiter's GM within 0.2 % of the card's 1.2668e17 m^3/s^2); unticking the hold frees n again;
    # the log axes switch on (the drawing only: the fit stays the same)
    seq = "10:click=top.data;14:click=data.tables;18:click=data.table.moons;24:click=data.preset0;30:click=data.logx;34:click=data.logy;38:move=400,500;39:wheel=-10;44:click=data.hold.n"
    r = run_app(binary, work, "data_hold", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=52, size="1280x800")
    lines = pldata(r)
    held = [ln for ln in lines if " table=moons " in ln and " held1=1.5 " in ln and " ok=1 " in ln]
    gm = 0
    if held:
        a = float(re.search(r" p0=(\S+)", held[0]).group(1))
        gm = 4 * 3.141592653589793 ** 2 * 1e9 / (a * 86400) ** 2
    check(held and abs(gm / 1.2668e17 - 1) < 2e-3, f"Jupiter's moons with n held at 3/2: GM = {gm:.5g} m^3/s^2 (the card's within 0.2 %)")
    check(lines and " logx=1 logy=1 " in lines[-1] and " held1=" not in lines[-1] and " p1=1.5001" in lines[-1], f"log axes on, and n free again after Hold is unticked: {lines[-1:]}")
    check(not r.scripted_missing(), f"every scripted control was there ({r.scripted_missing()[:2]})")

def part_celestial(binary, work):
    # 0.92.0: the Orbit Lab's celestial mechanics tools in the Data panel. One run opens each in turn (Lagrange points of the Sun and Saturn, the Kirkwood histogram, an ephemeris table of Jupiter, the
    # drift check) and reads the line each tool prints (PLCELESTIAL); the numbers are checked here by their own mathematics, not by the app's functions: the collinear equation solved again by
    # bisection, the histogram counted again from asteroid_data.inc, the resonances by Kepler's third law, Jupiter's distance between its perihelion and aphelion, the drift under the limits of the
    # unit tests. A second run turns the five points on in the picture (amber and green rings: pixels of those colours outside the panel). A third makes the porkchop grid (Earth to Mars: its best
    # C3 near the Hohmann one) and says which program solved it.
    import math
    def lines(r, tool):
        return [ln[len("PLCELESTIAL "):] for ln in r.out.splitlines() if ln.startswith("PLCELESTIAL " + tool)]
    def kv(ln):
        d = {}
        for tok in re.findall(r"(\w+)=(\S+)", ln):
            d[tok[0]] = tok[1]
        return d
    seq = ("10:click=top.data;16:click=data.tables;20:click=data.table.cm:lagrange;26:click=cm.lg.pick;32:click=cm.lg.5;"
           "42:click=data.tables;46:click=data.table.cm:kirkwood;"
           "56:click=data.tables;60:click=data.table.cm:ephem;"
           "70:click=data.tables;74:click=data.table.cm:drift;80:click=cm.dr.run")
    r = run_app(binary, work, "celestial_a", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=330, size="1280x800")
    check(not r.scripted_missing(), f"celestial: every scripted control was there ({r.scripted_missing()[:3]})")
    # --- the Lagrange points of the Sun and Saturn
    lg = [kv(l) for l in lines(r, "lagrange")]
    d = next((d for d in lg if "L1km" in d and abs(float(d["mu"]) - 2.85e-4) < 1e-4), None)
    check(d is not None, f"celestial: the Lagrange line for the Sun and Saturn was printed ({lg[-1:]})")
    if d:
        mu, sep = float(d["mu"]), float(d["sepkm"])
        def force(x):
            d1, d2 = x + mu, x - 1 + mu
            return -(1 - mu) * d1 / abs(d1) ** 3 - mu * d2 / abs(d2) ** 3 + x
        def root(lo, hi):
            ln = force(lo) < 0
            for _ in range(200):
                m = (lo + hi) / 2
                if (force(m) < 0) == ln: lo = m
                else: hi = m
            return (lo + hi) / 2
        rh = (mu / 3) ** (1 / 3)
        x1, x2, x3 = root(-mu + 1e-9, 1 - mu - 1e-3 * rh), root(1 - mu + 1e-3 * rh, 3), root(-3, -mu - 1e-9)
        want = [(1 - mu - x1) * sep, (x2 - (1 - mu)) * sep, (1 - mu - x3) * sep, sep, sep]     # the distance of each point from the light body; L4 and L5 are one separation from both
        got = [float(d[k]) for k in ("L1km", "L2km", "L3km", "L4km", "L5km")]
        check(all(abs(g - w) <= 1e-6 * w for g, w in zip(got, want)), f"celestial: L1..L5 distances from the light body match the collinear equation solved again ({got} vs {want})")
        check(abs(got[0] / sep - (rh - rh * rh / 3)) < 0.01 * rh and abs(got[1] / sep - (rh + rh * rh / 3)) < 0.01 * rh, "celestial: L1 and L2 lie at the Hill radius (mu/3)^(1/3) -/+ a third of its square")
        check(d["stable"] == "1", "celestial: L4 and L5 of the Sun and Saturn are stable (27 mu (1 - mu) < 1)")
    # --- the Kirkwood histogram
    kw = [kv(l) for l in lines(r, "kirkwood")]
    check(bool(kw), "celestial: the Kirkwood line was printed")
    if kw:
        d = kw[-1]
        inc = open(os.path.join(HERE, "..", "src", "orbit", "asteroid_data.inc"), encoding="utf-8").read()
        rocks = [tuple(float(v) for v in m.split(",")[:3]) for m in re.findall(r"^\{(\d[^{}]*)\}", inc, re.M)]
        lo, w = float(d["lo"]), float(d["w"])
        counts = [0] * 80
        below = above = total = 0
        for g, a, e in rocks:
            if int(g) != 0: continue
            total += 1
            if a < lo: below += 1
            elif a >= lo + 80 * w: above += 1
            else: counts[int((a - lo) // w)] += 1
        got = [int(c) for c in d["counts"].strip(",").split(",")]
        check(got == counts and int(d["total"]) == total and int(d["below"]) == below and int(d["above"]) == above, f"celestial: the histogram is the data counted again ({total} rocks, {below} below, {above} above)")
        ms = {m.split("@")[0]: float(m.split("@")[1]) for m in d["markers"].strip(",").split(",")}
        sol = open(os.path.join(HERE, "..", "src", "orbit", "solar_data.cpp"), encoding="utf-8").read()
        aj = float(re.search(r'\{"Jupiter", "#[0-9a-f]+", "gas giant",\s*\{([0-9.]+),', sol).group(1))
        okm = all(abs(ms[f"{p}:{q}"] - aj * (q / p) ** (2 / 3)) < 1e-6 for p, q in ((4, 1), (3, 1), (5, 2), (7, 3), (2, 1)))
        check(okm, f"celestial: the five resonances are a_J (q/p)^(2/3) with a_J = {aj} AU from the lab's Jupiter ({ms})")
        # the gap at 3:1 is in the data: the bins within 0.03 AU of it hold fewer than half the rocks of the same number of bins on each side
        k31 = int((ms["3:1"] - lo) // w)
        inside, beside = sum(got[k31 - 1:k31 + 2]), (sum(got[k31 - 5:k31 - 2]) + sum(got[k31 + 3:k31 + 6])) / 2
        check(inside * 2 < beside, f"celestial: the 3:1 gap is in the counts ({inside} rocks in 3 bins, {beside} in each side's 3)")
    # --- the ephemeris table: Jupiter, Simple
    ep = [kv(l) for l in lines(r, "ephem")]
    check(bool(ep) and ep[-1]["planet"] == "Jupiter" and ep[-1]["rows"] == "8", f"celestial: the ephemeris table has 8 rows for Jupiter ({ep[-1:]})")
    if ep:
        d0, dn = float(ep[-1]["dist0"]), float(ep[-1]["distN"])
        check(4.9 < d0 < 5.5 and 4.9 < dn < 5.5 and 0 <= float(ep[-1]["lon0"]) < 360 and abs(float(ep[-1]["lat0"])) < 1.4, f"celestial: Jupiter is between its perihelion and aphelion distances ({d0}, {dn} AU)")
        check(abs(float(ep[-1]["first"]) - (2461319.0 + 69.184 / 86400)) < 1.5, f"celestial: the first row is 2026-10-05 12:00 UT turned into TDB ({ep[-1]['first']})")
    # --- the drift check
    dr = [kv(l) for l in lines(r, "drift")]
    check(bool(dr) and dr[-1]["done"] == "1", f"celestial: the drift check finished ({dr[-1:]})")
    if dr:
        check(float(dr[-1]["maxE"]) < 1e-11 and float(dr[-1]["maxL"]) < 1e-12 and dr[-1]["steps"] == "36525", f"celestial: energy and angular momentum drift under the unit tests' limits ({dr[-1]['maxE']}, {dr[-1]['maxL']})")
    # --- the five points on the picture
    def amber(rr):
        b = Bmp(rr.bmp)
        pan = rr.rects.get("panel.data") or (0, 0, 0, 0)
        n = 0
        for y in range(50, b.h - 50, 1):
            for x in range(0, b.w, 1):
                if pan[0] <= x <= pan[0] + pan[2] and pan[1] <= y <= pan[1] + pan[3]: continue
                if x > b.w - 345: continue                                         # (the Controls panel)
                R, G, B = b.px(x, y)
                if (abs(R - 255) < 12 and abs(G - 199) < 14 and abs(B - 89) < 18) or (abs(R - 115) < 12 and abs(G - 217) < 14 and abs(B - 140) < 18): n += 1
        return n
    seq = "10:click=top.data;16:click=data.tables;20:click=data.table.cm:lagrange;26:click=cm.lg.pick;30:click=cm.lg.4;36:click=cm.lg.show"
    rb = run_app(binary, work, "celestial_b", {"PL_NEWUI": "on", "PL_SCRIPT": seq, "PL_ZOOM": "-0.26", "PL_SELECT": "Jupiter"}, frames=70, size="2000x900")   # (wide: the picture's middle, with Jupiter in it, is right of the panel)
    on, off = amber(rb), amber(r)
    check(not rb.scripted_missing() and on > off + 8, f"celestial: the picture shows the Lagrange points of the Sun and Jupiter (amber and green ring pixels outside the panel: {on} with them, {off} without)")
    # --- the transfer planner
    seq = "10:click=top.data;16:click=data.tables;20:click=data.table.cm:transfer"
    rc = run_app(binary, work, "celestial_c", {"PL_NEWUI": "on", "PL_SCRIPT": seq}, frames=200, size="1280x800")
    tr = [kv(l) for l in lines(rc, "transfer")]
    check(bool(tr) and tr[-1]["dep"] == "Earth" and tr[-1]["arr"] == "Mars", f"celestial: the transfer planner made an Earth to Mars grid ({tr[-1:]})")
    if tr:
        mu = 1.32712440041279419e20; au = 149597870700.0
        r1, r2 = 1.00000261 * au, 1.52371034 * au          # JPL's approximate semi-major axes of Earth and Mars (the lab's elements)
        hoh = (math.sqrt(mu / r1) * (math.sqrt(2 * r2 / (r1 + r2)) - 1) / 1000) ** 2
        c3 = float(tr[-1]["best_c3"])
        check(0.85 * hoh <= c3 <= 1.4 * hoh, f"celestial: the best C3 of the grid ({c3:.3g} km2/s2) is near the Hohmann transfer's {hoh:.3g}")
        check(tr[-1]["n"] == "96" and "PLPORKCHOPCARD state=1" in rc.out and tr[-1]["cardstate"] == "1", "celestial: the card's porkchop program passed its start-up check against the processor")
    check(not rc.scripted_missing(), f"celestial: every scripted control of the planner was there ({rc.scripted_missing()[:3]})")

def part_about(binary, work):
    # 0.74.0 (owner, 22 Nov 2026): the developer's paragraph of About ends with "Free Palestine" in bold and a small DRAWN flag of Palestine (html_view.cpp lrnFlagPS: 1:2, the height of the
    # line's letters, right after the words). One run: the page as it opens (the paragraph is in the first card), the words drawn and the flag's box in view.
    r = run(binary, work, "k_about_flag", {"PL_OPEN": "about", "PL_NEWUI": "on"}, "1280x800", "dark", 30)
    words = [t for t in r.texts if t[2].startswith("Free Palestine")]
    flag = r.rects.get("page.flagps")
    check(bool(words), "About: the words 'Free Palestine' are drawn")
    check(flag is not None and abs(flag[2] - 2 * flag[3]) < 0.5 and 8 <= flag[3] <= 30 and 0 <= flag[1] <= 800 - flag[3], f"About: the drawn flag is there, 1:2, about a line's letters high ({flag})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"log": part_log, "calc": part_calc, "data": part_data, "about": part_about, "celestial": part_celestial}[part](binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
