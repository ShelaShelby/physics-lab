#!/usr/bin/env python3
# 0.15 stage 5k and later: pages and panels of the top bar, in the REAL app (Linux, software Vulkan, scripted events; NOT a real mouse, NOT Windows). tests/TESTS.md lists the parts kept and why.
#   log    the changelog page opens; calc   the calculator works (0.49); data   the Data panel works (0.51); about   the About page's last line (0.74.0)
#   check_5k.py <PhysicsLab binary> <work folder> <part>
import os, re, sys
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
    {"log": part_log, "calc": part_calc, "data": part_data, "about": part_about}[part](binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
