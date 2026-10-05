#!/usr/bin/env python3
"""0.15 stage 1: THROWAWAY mockup code (HTML + CSS written by this script, drawn by Chromium through Playwright). Not part of the app, not reachable from it.
usage: mockups.py <pictures dir (make_pictures.py)> <out dir>   -> <out>/html/*.html and the PNG list
The lab pictures inside the mockups are real pictures of the real app (make_pictures.py). Text strings and numbers come from the app's own cards and buttons
(Earth's card, the chart's legend counts, the Orbit Lab's buttons); nothing here is a new fact. Design rules: ../../DESIGN.md.
Layouts: A rail + picture + one right panel with tabs; B slim top bar + picture + bottom drawer; C picture first, a subject card and popovers.
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
from theme_contrast import cr                                          # WCAG 2.x contrast, the same function that made the DESIGN.md table

PICS, OUT = sys.argv[1], sys.argv[2]
ICONS = os.path.join(HERE, "..", "assets", "icons")

def hexrgb(h):
    h = h.lstrip("#")
    if len(h) == 3: h = "".join(c * 2 for c in h)
    return [int(h[i:i + 2], 16) for i in (0, 2, 4)]
def tohex(c): return "#" + "".join(f"{max(0, min(255, round(v))):02x}" for v in c)
def mix(a, b, t): return tohex([x + (y - x) * t for x, y in zip(hexrgb(a), hexrgb(b))])

# ---- the two themes (the flat colours are the app's own: app_text.inc kThemes and theme_style.cpp kNativeThemes) ----
DARK = dict(id="dark", bg="#151517", card="#1e1e21", fg="#eeeeee", muted="#9a9a94", line="#333338", accent="#6c9cff", onacc="#ffffff")
AERO = dict(id="aero", bg="#8fd3ee", card="#ffffff", fg="#031f2e", muted="#0a3348", line="#4c9fbd", accent="#19b5d6", onacc="#04260d")
def fill_for(accent, onacc):                                           # the accent as a FILL behind text: darken (or lighten) it until the text on it passes 4.5:1 (what the app's accentFill does)
    f = accent
    for _ in range(60):
        if cr(onacc, f) >= 4.5: return f
        f = mix(f, "#000000" if cr(onacc, "#000000") > cr(onacc, "#ffffff") else "#ffffff", 0.05)
    return f
for T in (DARK, AERO):
    T["inset"] = mix(T["card"], T["fg"], 0.06); T["face"] = mix(T["card"], T["fg"], 0.12)
    T["fill"] = fill_for(T["accent"], T["onacc"])
    if T["id"] == "aero": T["fill"] = "#6cc419"                          # Aero's chosen / primary buttons are lime jelly (0.14 stage 2), with dark green text
    checks = [(T["fg"], T["bg"]), (T["muted"], T["card"]), (T["fg"], T["card"]), (T["onacc"], T["fill"])]
    if T["id"] == "dark": checks += [(T["muted"], T["inset"]), (T["fg"], T["face"]), (T["accent"], T["card"])]
    for a, b in checks:
        assert cr(a, b) >= 4.5, (T["id"], a, b, cr(a, b))              # every text pair in the mockups passes 4.5:1 (Aero's glass is checked against white below)

def icon(name, size=20, stroke=1.75, cls=""):
    s = open(os.path.join(ICONS, name + ".svg"), encoding="utf-8").read()
    inner = re.search(r"<svg[^>]*>(.*)</svg>", s, re.S).group(1)
    return f'<svg class="ic {cls}" width="{size}" height="{size}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="{stroke}" stroke-linecap="round" stroke-linejoin="round">{inner}</svg>'

def btn(label="", kind="secondary", ic=None, extra=""):
    i = icon(ic, 18) if ic else ""
    return f'<button class="btn {kind} {"icononly" if not label else ""}" {extra}>{i}<span>{label}</span></button>'
def seg(opts, active, small=False):
    return '<div class="seg">' + "".join(f'<span class="{"on" if o == active else ""}">{o}</span>' for o in opts) + "</div>"
def switch(label, on=True):
    return f'<div class="row"><span>{label}</span><span class="sw {"on" if on else ""}"><i></i></span></div>'
def slider(label, pct, val):
    return f'<div class="slider"><div class="row"><span>{label}</span><span class="cap">{val}</span></div><div class="track"><b style="width:{pct}%"></b><i style="left:{pct}%"></i></div></div>'
def stat(k, v): return f'<div class="stat"><span class="k">{k}</span><span class="v">{v}</span></div>'
def group(title, body, first=False): return f'<section class="grp"><h3>{title}</h3>{body}</section>'
def tabs(active):
    t = [("Choose", "search"), ("View", "sliders-horizontal"), ("Info", "info")]
    return '<nav class="tabs">' + "".join(f'<span class="{"on" if n == active else ""}">{icon(i, 18)}{n}</span>' for n, i in t) + "</nav>"
def more(text, summary): return f'<p class="sum">{summary} <a>More</a></p>'

# ---- the content (strings from the app: Earth's card in the Orbit Lab, the chart's legend counts, the buttons) ----
EARTH_STATS = [("Size", "1.0 × Earth (radius 6,378 km)"), ("Mass", "1.0 Earths · gravity 1.0 g"), ("Distance", "1.00 AU (150 million km)"), ("Speed", "29.8 km/s"),
               ("One lap", "1.00 years"), ("Temperature", "15 °C (real, mean surface)"), ("Turns once in", "23 h 56 min"), ("Axis leans", "23.4° from upright")]
STABLE = "#f1f1f1"                                                      # Aero's chart draws stable nuclides black (its page is white); set per theme below
LEGEND = [("STABLE", "Stable", "253"), ("#3b82f6", "Beta minus", "1401"), ("#ef4444", "Beta plus / electron capture", "1110"), ("#eab308", "Alpha", "479"),
          ("#22c55e", "Fission", "59"), ("#f97316", "Proton emission", "104"), ("#a855f7", "Neutron emission", "30"), ("#9ca3af", "Several ways, odds not measured", "121")]
HISTORY = ["Halley at its closest, 1986", "Neptune found, 1846", "Mars closest, 2003", "Venus crosses the Sun, 2012", "Great conjunction, 2020", "Halley returns, 2061"]
URANIUM = "Uranium-238 (²³⁸U)"
HALF = "Half-life 4.47 billion years · decays by alpha, spontaneous fission 5.4×10⁻⁵ %"

def info_orbit(small):
    n = 6 if small else 8
    return (f'<div class="subject"><h1>Earth</h1><p class="cap">Rocky planet (home)</p></div>'
            f'<p class="sum">The only known world with life. Northern autumn, southern spring. <a>More</a></p>'
            f'<div class="stats">{"".join(stat(k, v) for k, v in EARTH_STATS[:n])}</div><p class="more"><a>All values {icon("chevron-down", 16)}</a></p>')
def choose_orbit(small):
    items = HISTORY[:4] if small else HISTORY
    return (group("Date", '<div class="row"><span class="title">5 Oct 2026 · 12:00 UT</span>' + btn("Today", "quiet") + '</div><div class="row two">' + '<span class="field">YYYY-MM-DD</span>' + btn("Jump", "secondary") + '</div>')
            + group("Journey", btn("Earth to the galactic centre", "primary", "sparkles", 'style="width:100%"'))
            + group("Moments in history", '<div class="list">' + "".join(f'<div class="li">{h}{icon("chevron-right", 16)}</div>' for h in items) + "</div>"))
def view_orbit(small):
    return (group("Show", switch("Moons") + switch("Dwarf planets") + switch("Comets") + switch("Belts and Oort cloud", False) + (switch("Trails", False) if not small else ""))
            + group("Planet physics", seg(["Simple", "Real"], "Simple"))
            + group("Time", slider("Time speed", 46, "1 s = 29 days") + slider("Zoom", 22, "×0.06")))
def choose_chart(small):
    return (group("Nuclide", f'<div class="pick"><div><h2>{URANIUM}</h2><p class="cap">Z = 92 protons · N = 146 neutrons</p></div>{btn("Change", "quiet")}</div>')
            + group("Look at", '<div class="list">' + "".join(f'<div class="li">{x}{icon("chevron-right", 16)}</div>' for x in ["Its decay path (14 steps)", "The atom: nucleus and electron", "The element: Uranium"][:3 if not small else 2]) + "</div>"))
def view_chart(small):
    rows = LEGEND if not small else LEGEND[:6]
    leg = '<div class="legend">' + "".join(f'<div class="lg"><i style="background:{STABLE if c == "STABLE" else c}"></i><span>{n}</span><span class="cap">{k}</span></div>' for c, n, k in rows) + "</div>"
    return (group("Height", seg(["Flat", "Landscape"], "Flat"))
            + group("Colour: how it decays", leg + ('<p class="cap">Empty square: no such nuclide seen.</p>'))
            + group("Marks", switch("Magic numbers", True) + switch("Valley of stability label", True)))
def info_chart(small):
    return (f'<div class="subject"><h1>{URANIUM}</h1><p class="cap">Radioactive: no stable isotope of uranium</p></div>'
            + f'<p class="sum">{HALF}. <a>More</a></p>'
            + f'<div class="stats">{stat("Likeliest path", "14 steps")}{stat("Chosen", "Z 92 · N 146")}</div>'
            + '<p class="note cap">The valley of stability is measured. The island of stability is a prediction and is not drawn. <a>Sources</a></p>')

TAB_BODY = {"orbit": dict(Choose=choose_orbit, View=view_orbit, Info=info_orbit), "chart": dict(Choose=choose_chart, View=view_chart, Info=info_chart)}
OPEN_TAB_A = {"orbit": "Info", "chart": "View"}                       # which tab is open in layout A (so the card / the legend design is seen)
TITLE = {"orbit": ("Orbit Lab", "Real orbits from NASA/JPL data"), "chart": ("Atom Lab", "Chart of nuclides")}

# ---- CSS ----
def css(T, W, H, small):
    aero = T["id"] == "aero"
    panel_bg = "rgba(255,255,255,.62)" if aero else T["card"]
    if aero:
        bg = "linear-gradient(180deg,#5fc0e8 0%,#bfe8f7 55%,#e9f8d8 100%)"
    else: bg = T["bg"]
    prim_bg = "linear-gradient(180deg,#9be04f 0%,#6cc419 52%,#52a80c 100%)" if aero else T["fill"]
    sec_bg = "linear-gradient(180deg,#e6fbff 0%,#8fe0f2 55%,#4fcbe6 100%)" if aero else T["face"]
    glass = "backdrop-filter: blur(10px) saturate(1.2); border:1px solid rgba(255,255,255,.85); box-shadow: 0 6px 24px rgba(5,60,90,.28), inset 0 1px 0 rgba(255,255,255,.9);" if aero else "box-shadow: 0 10px 30px rgba(0,0,0,.5);"
    prim_txt = T["onacc"]
    sec_txt = T["fg"]
    fs = "16px"
    return f"""
@font-face {{ font-family:'Inter'; src:url('file://{os.path.abspath(os.path.join(HERE, '..', 'assets'))}/Inter-subset.woff') format('woff'); font-weight:400 700; }}
:root {{ --bg:{T['bg']}; --panel:{panel_bg}; --solid:{T['card']}; --inset:{('rgba(255,255,255,.55)' if aero else T['inset'])}; --face:{T['face']}; --text:{T['fg']}; --muted:{T['muted']};
  --line:{T['line']}; --accent:{T['accent']}; --fill:{T['fill']}; --onacc:{T['onacc']}; }}
* {{ box-sizing:border-box; margin:0; padding:0; }}
html,body {{ width:{W}px; height:{H}px; overflow:hidden; background:{bg}; color:var(--text); font-family:'Inter',sans-serif; font-size:{fs}; line-height:24px; font-feature-settings:'tnum','cv11'; }}
.ic {{ flex:none; }} h1 {{ font-size:28px; line-height:34px; font-weight:600; }} h2 {{ font-size:20px; line-height:26px; font-weight:600; }}
h3 {{ font-size:13px; line-height:18px; font-weight:600; letter-spacing:.06em; text-transform:uppercase; color:var(--muted); margin-bottom:8px; }}
.cap {{ font-size:13px; line-height:18px; color:var(--muted); }} a {{ color:{'#055a73' if aero else 'var(--accent)'}; font-weight:600; cursor:pointer; white-space:nowrap; }}
.btn {{ display:inline-flex; align-items:center; justify-content:center; gap:8px; height:36px; padding:0 16px; border:0; border-radius:8px; font:600 16px 'Inter'; color:var(--text); background:{sec_bg}; white-space:nowrap;
  {'border:1px solid rgba(255,255,255,.9); box-shadow: inset 0 1px 0 rgba(255,255,255,.95), 0 1px 3px rgba(5,60,90,.35); color:#031f2e;' if aero else ''} }}
.btn.primary {{ background:{prim_bg}; color:{prim_txt}; {'border:1px solid #3d8a05;' if aero else ''} }}
.btn.quiet {{ background:transparent; color:{'#055a73' if aero else 'var(--accent)'}; box-shadow:none; border:0; padding:0 8px; }}
.btn.icononly {{ width:36px; padding:0; }} .btn.quiet.icononly {{ color:var(--text); }}
.seg {{ display:flex; background:{'rgba(255,255,255,.55)' if aero else T['face']}; border-radius:8px; padding:2px; gap:2px; }}
.seg span {{ flex:1; text-align:center; height:32px; line-height:32px; border-radius:6px; font-weight:600; color:var(--text); }}
.seg span.on {{ background:{prim_bg}; color:{prim_txt}; }}
.row {{ display:flex; align-items:center; justify-content:space-between; gap:8px; min-height:36px; }} .row.two {{ margin-top:8px; }} .row .title {{ font-size:16px; font-weight:600; }}
.sw {{ width:36px; height:20px; border-radius:10px; background:{'rgba(5,60,90,.35)' if aero else T['face']}; position:relative; flex:none; }}
.sw i {{ position:absolute; top:2px; left:2px; width:16px; height:16px; border-radius:8px; background:{'#fff' if aero else T['muted']}; }}
.sw.on {{ background:{'#6cc419' if aero else T['fill']}; }} .sw.on i {{ left:18px; background:{'#fff' if aero else T['onacc']}; }}
.slider {{ margin:4px 0 8px; }} .track {{ position:relative; height:4px; border-radius:2px; background:{'rgba(5,60,90,.3)' if aero else T['face']}; margin:10px 8px 6px 0; }}
.track b {{ position:absolute; left:0; top:0; bottom:0; border-radius:2px; background:{'#19b5d6' if aero else T['accent']}; }}
.track i {{ position:absolute; top:-6px; width:16px; height:16px; margin-left:-8px; border-radius:8px; background:{'#fff' if aero else T['fg']}; {'border:1px solid #19b5d6; box-shadow:0 1px 3px rgba(0,0,0,.4);' if aero else ''} }}
.field {{ flex:1; height:36px; line-height:36px; padding:0 12px; border-radius:8px; color:var(--muted); background:{'rgba(255,255,255,.7)' if aero else T['face']}; box-shadow: inset 0 0 0 1px {'#4c9fbd' if aero else mix(T['bg'], T['fg'], 0.45)}; }}
.grp {{ margin-bottom:24px; }} .stats .stat {{ display:flex; justify-content:space-between; gap:12px; padding:6px 0; border-top:1px solid {'rgba(5,60,90,.18)' if aero else T['line']}; }}
.stat .k {{ color:var(--muted); flex:none; }} .stat .v {{ text-align:right; font-weight:600; }} .stats .stat:first-child {{ border-top:0; }}
.sum {{ margin:8px 0 16px; }} .more {{ margin-top:8px; }} .more a {{ display:inline-flex; align-items:center; gap:4px; }} .note {{ margin-top:16px; }}
.list .li {{ display:flex; justify-content:space-between; align-items:center; min-height:40px; padding:0 4px; border-top:1px solid {'rgba(5,60,90,.18)' if aero else T['line']}; }} .list .li:first-child {{ border-top:0; }} .list .li svg {{ color:var(--muted); }}
.pick {{ display:flex; justify-content:space-between; align-items:center; background:var(--inset); border-radius:12px; padding:12px 12px 12px 16px; }}
.legend .lg {{ display:grid; grid-template-columns:16px 1fr auto; gap:4px 12px; align-items:center; min-height:30px; }} .legend .lg i {{ width:14px; height:14px; border-radius:4px; display:block; }}
.tabs {{ display:flex; gap:4px; margin-bottom:24px; }} .tabs span {{ flex:1; display:flex; justify-content:center; align-items:center; gap:8px; height:40px; border-radius:8px; font-weight:600; color:var(--muted); }}
.tabs span.on {{ background:var(--inset); color:var(--text); {'box-shadow: inset 0 1px 0 #fff; background:rgba(255,255,255,.85);' if aero else ''} }}
.panel {{ background:var(--panel); {'backdrop-filter:blur(10px); border-left:1px solid rgba(255,255,255,.8);' if aero else ''} overflow:hidden; position:relative; }}
.panel .scrollfade {{ position:absolute; left:0; right:0; bottom:0; height:56px; background:linear-gradient(transparent,{'rgba(220,243,250,.95)' if aero else T['card']}); pointer-events:none; }}
.pad {{ padding:24px 24px 0 24px; }}
.rail {{ width:64px; display:flex; flex-direction:column; align-items:center; gap:8px; padding:16px 0; background:{'rgba(255,255,255,.5)' if aero else T['card']}; {'border-right:1px solid rgba(255,255,255,.8);' if aero else ''} }}
.rail .nav {{ width:44px; height:44px; border-radius:12px; display:flex; align-items:center; justify-content:center; color:var(--muted); }}
.rail .nav.on {{ background:{prim_bg}; color:{prim_txt}; }} .rail .sp {{ flex:1; }} .rail .logo {{ color:{'#05501f' if aero else T['accent']}; margin-bottom:16px; }}
.top {{ height:56px; display:flex; align-items:center; gap:16px; padding:0 16px; background:{'rgba(255,255,255,.6)' if aero else T['card']}; {'backdrop-filter:blur(10px); border-bottom:1px solid rgba(255,255,255,.85);' if aero else ''} }}
.top .brand {{ display:flex; align-items:center; gap:8px; font-weight:600; font-size:20px; }} .top .brand svg {{ color:{'#05501f' if aero else T['accent']}; }} .top .sp {{ flex:1; }}
.top .seg {{ width:{'250px' if small else '280px'}; }}
.timebar {{ height:56px; display:flex; align-items:center; gap:16px; padding:0 16px; background:{'rgba(255,255,255,.6)' if aero else T['card']}; }}
.timebar .grow {{ flex:1; }} .timebar .slider {{ margin:0; }} .timebar .track {{ margin:6px 8px 0 0; }}
.pic {{ display:block; }} .stagewrap {{ position:relative; }}
.float {{ position:absolute; background:{"rgba(240,250,255,.93)" if aero else "var(--panel)"}; border-radius:12px; padding:16px; {glass} }}
.chip {{ position:absolute; display:flex; align-items:center; gap:8px; padding:6px 12px; border-radius:18px; font-weight:600; background:{'rgba(255,255,255,.8)' if aero else 'rgba(30,30,33,.88)'}; {glass} }}
.drawer {{ background:var(--panel); border-top:1px solid {"rgba(255,255,255,.9)" if aero else T["line"]}; overflow:hidden; }}
.drawer .head {{ display:flex; align-items:center; gap:16px; padding:8px 8px 8px 16px; }} .drawer .head .tabs {{ margin:0; flex:none; width:{'330px' if small else '400px'}; }} .drawer .head .sp {{ flex:1; }}
.drawer .cols {{ display:grid; grid-template-columns:repeat({2 if small else 3},1fr); gap:32px; padding:8px 24px 16px; border-top:1px solid {'rgba(5,60,90,.18)' if aero else T['line']}; }}
.drawer .cols .grp {{ margin-bottom:0; }} .drawer .cols .stat {{ padding:3px 0; }} .drawer .cols .sum {{ margin:4px 0 8px; }}
.tool {{ position:absolute; right:16px; top:16px; display:flex; flex-direction:column; gap:4px; padding:4px; border-radius:12px; background:var(--panel); {glass} }}
.tool .btn {{ background:transparent; box-shadow:none; border:0; color:var(--text); }} .tool .btn.on {{ background:{prim_bg}; color:{prim_txt}; }}
.pop {{ position:absolute; background:{"rgba(240,250,255,.93)" if aero else "var(--panel)"}; border-radius:12px; padding:16px; {glass} }}
.scroll {{ position:absolute; right:3px; top:0; width:4px; border-radius:2px; background:{'rgba(5,60,90,.35)' if aero else T['line']}; }}
"""

TOP_ICONS = [("graduation-cap", "Learn"), ("trophy", "Achievements"), ("lightbulb", "Facts"), ("palette", "Theme"), ("settings", "Settings")]
def labswitch(lab): return seg(["Orbit Lab", "Atom Lab"], "Orbit Lab" if lab == "orbit" else "Atom Lab")
def topbar(lab, small):
    right = "".join(btn("", "quiet", i) for i, _ in (TOP_ICONS[:3] if small else TOP_ICONS)) + (btn("", "quiet", "menu") if small else "")
    return f'<div class="top"><div class="brand">{icon("atom", 24, 2)}{"" if small else "<span>Physics Lab</span>"}</div>{labswitch(lab)}<div class="sp"></div>{right}</div>'
def picimg(lab, theme, key): return f'<img class="pic" src="file://{os.path.abspath(PICS)}/{lab}_{theme}_{key}.png">'
def transport(lab, small, bar=True):
    if lab == "orbit":
        return (btn("", "quiet", "skip-back") + btn("Play", "primary", "play") + btn("", "quiet", "skip-forward")
                + '<div class="grow">' + slider("Time speed", 46, "1 s = 29 days") + '</div>' + ('' if small else '<span class="cap">5 Oct 2026 · 12:00 UT</span>') + btn("Today", "quiet"))
    return ('<span class="cap">Hover a square for details · click to choose · arrow keys walk one proton or neutron</span>' if not small else '<span class="cap">Hover for details · click to choose</span>')\
        + '<div class="grow"></div>' + btn("Reset view", "quiet", "rotate-ccw")

def page_A(T, lab, small):
    W, H = (1000, 640) if small else (1280, 800); pw = 320 if small else 384; picw = W - 64 - pw; key = "S_A" if small else "L_A"
    nav = "".join(f'<div class="nav {"on" if (n == "orbit" and lab == "orbit") or (n == "atom" and lab == "chart") else ""}">{icon(i, 22)}</div>' for n, i in [("orbit", "orbit"), ("atom", "atom"), ("learn", "graduation-cap"), ("ach", "trophy")])
    rail = f'<div class="rail"><div class="logo">{icon("sparkles", 24, 2)}</div>{nav}<div class="sp"></div><div class="nav">{icon("lightbulb", 22)}</div><div class="nav">{icon("palette", 22)}</div><div class="nav">{icon("settings", 22)}</div></div>'
    tab = OPEN_TAB_A[lab]; t, sub = TITLE[lab]
    head = f'<div class="row" style="margin-bottom:16px"><div><h2>{t}</h2><p class="cap">{sub}</p></div>{btn("", "quiet", "camera")}</div>'
    body = TAB_BODY[lab][tab](small)
    panel = f'<div class="panel" style="width:{pw}px;height:{H}px"><div class="pad">{head}{tabs(tab)}{body}</div><div class="scrollfade"></div><div class="scroll" style="top:{H*0.06:.0f}px;height:{H*0.55:.0f}px"></div></div>'
    chip = ""
    if lab == "chart": chip = f'<div class="chip" style="left:112px;top:10px">{icon("atom", 18)}Chosen: {URANIUM}</div>'
    stage = f'<div><div class="stagewrap">{picimg(lab, T["id"], key)}{chip}</div><div class="timebar" style="width:{picw}px">{transport(lab, small)}</div></div>'
    return f'<div style="display:flex">{rail}{stage}{panel}</div>'

def legrows(n, small=False):
    return "".join('<div style="display:flex;gap:8px;align-items:center;min-height:26px"><i style="width:12px;height:12px;border-radius:3px;background:' + (STABLE if c == 'STABLE' else c) + '"></i>' + nm
                   + '<span class="cap" style="margin-left:auto">' + k + '</span></div>' for c, nm, k in LEGEND[:n])
def drawer_cols(lab, small):
    """Three columns (two in a small window) that must fit the drawer body: 168 px (large) / 144 px (small)."""
    sw = lambda n: "".join(switch(x) for x in ["Moons", "Dwarf planets", "Comets"][:n])
    if lab == "orbit":
        if small:
            left = '<h2>Earth</h2><p class="cap">Rocky planet (home)</p><div class="stats">' + "".join(stat(k, v) for k, v in EARTH_STATS[2:5]) + '</div>'
            return '<div>' + left + '</div><div>' + group("Show", sw(3)) + '</div>'
        hist = "".join('<div class="li" style="display:flex;justify-content:space-between;min-height:34px;border-top:1px solid ' + T_LINE + '">' + h + icon("chevron-right", 16) + '</div>' for h in HISTORY[:2])
        journey = btn("Earth to the galactic centre", "primary", "sparkles", 'style="width:100%"')
        info = ('<h3>Info · Earth</h3><p class="sum" style="margin:0 0 4px">Rocky planet (home). The only known world with life. <a>More</a></p><div class="stats">'
                + "".join(stat(k, v) for k, v in EARTH_STATS[2:4]) + '</div>')
        return '<div>' + group("Choose", journey + '<div style="height:8px"></div>' + hist) + '</div><div>' + group("View", sw(3)) + '</div><div>' + info + '</div>'
    info = '<h3>Info</h3><p class="sum" style="margin:0 0 4px">' + HALF + '. <a>More</a></p><div class="stats">' + stat("Likeliest path", "14 steps") + '</div>'
    if small:
        return '<div>' + group("Height", seg(["Flat", "Landscape"], "Flat") + legrows(3)) + '</div><div>' + info + '</div>'
    pick = '<div class="pick"><div><h2 style="font-size:16px;line-height:22px">' + URANIUM + '</h2><p class="cap">Z = 92 · N = 146</p></div>' + btn("Change", "quiet") + '</div>'
    return '<div>' + group("Choose", pick) + '</div><div>' + group("View · colour: how it decays", legrows(4)) + '</div><div>' + info + '</div>'
T_LINE = "#333338"

def page_B(T, lab, small):
    global T_LINE
    T_LINE = "rgba(5,60,90,.18)" if T["id"] == "aero" else T["line"]
    key = "S_B" if small else "L_B"; dh = 224 if small else 248
    chip = '<div class="chip" style="left:112px;top:10px">' + icon("atom", 18) + 'Chosen: ' + URANIUM + '</div>' if lab == "chart" else ""
    head = '<div class="head">' + tabs("Info") + '<div class="sp"></div>' + (transport(lab, True) if lab == "orbit" else btn("Reset view", "quiet", "rotate-ccw")) + btn("", "quiet", "chevron-down") + '</div>'
    drawer = '<div class="drawer" style="height:' + str(dh) + 'px">' + head + '<div class="cols">' + drawer_cols(lab, small) + '</div></div>'
    return '<div>' + topbar(lab, small) + '<div class="stagewrap">' + picimg(lab, T["id"], key) + chip + '</div>' + drawer + '</div>'

def page_C(T, lab, small):
    W, H = (1000, 640) if small else (1280, 800); key = "S_BC" if small else "L_BC"
    cw = 300 if small else 340
    if lab == "orbit":
        subject = (f'<div class="float" style="left:16px;top:44px;width:{cw}px"><div class="row"><h1>Earth</h1>{btn("", "quiet", "x")}</div><p class="cap">Rocky planet (home)</p>'
                   f'<p class="sum">The only known world with life. <a>More</a></p><div class="stats">{"".join(stat(k, v) for k, v in EARTH_STATS[2:6 if not small else 5])}</div><p class="more"><a>All values {icon("chevron-down", 16)}</a></p></div>')
        pop = (f'<div class="pop" style="right:72px;top:16px;width:{250 if small else 270}px">{group("Show", switch("Moons") + switch("Dwarf planets") + switch("Comets") + switch("Belts and Oort cloud", False))}'
               f'{seg(["Simple", "Real"], "Simple")}</div>')
    else:
        subject = (f'<div class="float" style="left:16px;top:44px;width:{cw}px"><div class="row"><div><h2>{URANIUM}</h2><p class="cap">Z = 92 · N = 146</p></div>{btn("", "quiet", "x")}</div>'
                   f'<p class="sum">{HALF}. <a>More</a></p><div class="stats">{stat("Likeliest path", "14 steps")}</div></div>')
        n = 5 if small else 8
        leg = "".join(f'<div class="lg"><i style="background:{STABLE if c == "STABLE" else c}"></i><span>{nm}</span><span class="cap">{k}</span></div>' for c, nm, k in LEGEND[:n])
        pop = f'<div class="pop" style="right:72px;top:16px;width:{300 if small else 330}px">{group("Height", seg(["Flat", "Landscape"], "Flat"))}<h3>Colour: how it decays</h3><div class="legend">{leg}</div></div>'
    tool = '<div class="tool">' + btn("", "on", "layers") + btn("", "quiet", "sliders-horizontal") + btn("", "quiet", "search") + btn("", "quiet", "scale") + btn("", "quiet", "camera") + "</div>"
    bar = f'<div class="float" style="left:50%;transform:translateX(-50%);bottom:16px;padding:8px 12px;border-radius:28px;display:flex;align-items:center;gap:12px;width:{560 if small else 640}px">{transport(lab, True)}</div>'
    if lab == "chart": bar = f'<div class="float" style="right:16px;bottom:30px;padding:6px 8px;border-radius:24px;display:flex;align-items:center;gap:4px">{btn("", "quiet", "mouse-pointer-click")}{btn("Reset view", "quiet", "rotate-ccw")}</div>'
    return f'<div>{topbar(lab, small)}<div class="stagewrap">{picimg(lab, T["id"], key)}{subject}{pop}{tool}{bar}</div></div>'

PAGES = {"A": page_A, "B": page_B, "C": page_C}
if __name__ == "__main__":
    os.makedirs(os.path.join(OUT, "html"), exist_ok=True)
    names = []
    for lay, fn in PAGES.items():
        for lab in ("orbit", "chart"):
            for T in (DARK, AERO):
                for small in (False, True):
                    STABLE = "#111111" if T['id'] == 'aero' else "#f1f1f1"
                    W, H = (1000, 640) if small else (1280, 800)
                    name = f"{lay}_{lab}_{T['id']}_{'small' if small else 'large'}"
                    html = f"<!doctype html><meta charset=utf-8><title>{name}</title><style>{css(T, W, H, small)}</style>{fn(T, lab, small)}"
                    p = os.path.join(OUT, "html", name + ".html"); open(p, "w", encoding="utf-8").write(html); names.append(name)
    print("\n".join(names))
