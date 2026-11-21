#!/usr/bin/env python3
# 0.15 stage 3: the new UI, checked in the REAL app (Linux build, headless on Mesa llvmpipe; not Windows, not a GPU):
#   check_shell.py <PhysicsLab binary> <work folder>
# WRITTEN BEFORE the code was run in the app (the logic it tests is in app/shell.* with its own unit tests). What it checks, from what the app prints with PL_LOG_SHELL
# (every text the new UI draws with its size in pixels, every rectangle) and from scripted input (PL_SCRIPT: real mouse and key events):
#   sizes     every text the new UI draws, at 6 pairs of Text size and Windows scale (0.38.1: was all 16; see KEEP) (a stand-in: PL_SCALE), is exactly
#             max(floor, round(u * scale * text factor)) for its kind, so never under the floors 12 / 14 / 15 / 22 px (the expected values are written HERE, from DESIGN.md)
#   window    no panel, bar or card leaves the window, at the smallest window (880 x 560 units) and at 1280 x 800; the toolbar and the readout do not overlap
#   collapse  a collapsed panel keeps its title (the Legend, and the subject card after it is collapsed)
#   keys      Esc closes an open panel, then search; Ctrl+K opens search; Space plays / pauses (and is typing inside search)
#   shift     with a card or panel over the middle, the selected body is moved outside every open panel; with the shift switched off (PL_SHIFT=off) it is not (so the test can fail)
#   reading   About opens in the reading view: a column of at most 34 em, headings, a contents list, body lines of at most 75 characters (DejaVu Sans here, wider than Segoe UI)
#   old       no Old panel button in either lab, and PL_NEWUI=off no longer switches the New UI off (0.16 stage 1b)
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, prefetch, Bmp

# From docs/ui-0.15/DESIGN.md section 1 (the owner's chosen design): kind -> (size in units, floor in px)
SCALE = {"caption": (13, 12), "body": (15, 14), "readout": (15, 14),   # (stage 5c: the readout is Body size)
          "title": (17, 15), "key": (28, 22), "h1": (28, 22), "h2": (20, 16), "h3": (17, 15)}
# Text the old pages draw inside the reading view (About, guides ...) has many sizes of its own (17 pt card titles, 13 pt notes ...): only the floors are claimed for it, not an exact size
PAGE_FLOORS = {"pbody": 14, "pcaption": 12, "plabel": 12, "ctl": 14}   # ctl (stage 5c.1): text inside the Controls panel, logged again under this kind: 14 px at least (the owner); its exact size is checked under its own kind
TF = {"small": 0.9, "normal": 1.0, "large": 1.15, "xl": 1.3}
# (0.38: the lab dropdown "top.lab" became the lab tabs, which fold into a Labs button when the bar is too narrow; the exercise opens the menu there instead, a button every size has)
EXERCISE = ("10:click=top.chip;14:key=escape;16:click=ctl.sec.pick;24:click=show.physics@0.8;32:click=ctl.sec.show;38:click=ctl.sec.show;"
            "44:key=escape;46:click=top.menu;50:key=escape;52:click=fact.line;56:key=escape;58:key=ctrl+k;62:text=ear;66:key=escape;68:click=top.menu;72:key=escape;"
            "74:click=card.legend;80:click=card.all;86:key=escape;88:click=top.chip;94:click=time.info;98:click=time.mode@0.9")
fails = 0
checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

# 0.38.1 (the owner: "cut what is not needed"): 5 of the 16 Windows scale x Text size pairs, chosen to cover each rule once: the floors (100 %, Small: Body 13.5 -> 14),
# the plain size (100 %, Normal), the factor 1.3 held as a double (100 %, Extra large: 15 x 1.3 = 19.5 -> 20, DESIGN.md), a fractional scale with a factor (125 %, Large:
# 21.56 -> 22), a fractional scale alone (150 %, Normal) and the largest (200 %, Extra large). 6 of 16.
KEEP = {(1.0, "small"), (1.0, "normal"), (1.0, "xl"), (1.25, "large"), (1.5, "normal"), (2.0, "xl")}

def expected(kind, dpi, tf):
    u, floor = SCALE[kind]
    return max(floor, int(u * dpi * tf + 0.5))

def part_sizes(binary, work, only_first_scale):
    dpis = (1.0,) if only_first_scale else (1.0, 1.25, 1.5, 2.0)
    prefetch([((binary, work, f"sizes_{d}_{n}", {"PL_NEWUI": "on", "PL_SCALE": str(d), "PL_TEXTSIZE": n, "PL_SELECT": "Earth", "PL_SCRIPT": EXERCISE}),
               dict(frames=112, size=f"{int(round(1000 * d))}x{int(round(640 * d))}")) for d in dpis for n in TF if (d, n) in KEEP])
    for dpi in dpis:
        size = f"{int(round(1000 * dpi))}x{int(round(640 * dpi))}"
        for tfn, tf in TF.items():
            if (dpi, tfn) not in KEEP:
                continue
            r = run_app(binary, work, f"sizes_{dpi}_{tfn}", {"PL_NEWUI": "on", "PL_SCALE": str(dpi), "PL_TEXTSIZE": tfn, "PL_SELECT": "Earth", "PL_SCRIPT": EXERCISE}, frames=112, size=size)
            tag = f" (scale {dpi}, text {tfn})"
            check(r.window is not None, "the app printed its PLSHELL report" + tag)
            if r.window is None:
                continue
            check(not r.scripted_missing(), "every scripted click found its control" + tag + " " + str(r.scripted_missing()[:2]))
            check(len(r.texts) >= 25, f"the new UI drew at least 25 different texts ({len(r.texts)})" + tag)
            kinds = {k for k, _, _ in r.texts}
            check({"caption", "body", "title"} <= kinds, "caption, body and title texts all appear" + tag)
            bad = [(k, px, t) for k, px, t in r.texts if k in SCALE and px != expected(k, dpi, tf)]
            check(not bad, "every text is exactly max(floor, round(u x scale x text size))" + tag + " " + str(bad[:3]))
            under = [(k, px, t) for k, px, t in r.texts if px < (SCALE[k][1] if k in SCALE else PAGE_FLOORS[k])]
            check(not under, "no text under its floor" + tag + " " + str(under[:3]))
            labels = [px for k, px, _ in r.texts if k == "plabel"]
            check(labels, "the picture drew labels" + tag)
            check(all(px >= max(12.0, 13 * dpi * tf - 0.01) for px in labels), "the picture's own labels are never under the Caption size (13 u, floor 12 px)" + tag + " " + str(sorted(set(labels))[:3]))
            w, h = r.window[0], r.window[1]
            for name, rect in r.rects.items():
                if name.startswith("occ") or name.startswith("ringlabel.") or name.startswith("ringskip."):   # (the picture's own ring labels are not panels)
                    continue
                check(inside(rect, w, h), f"{name} stays inside the window {rect}" + tag)
            if "toolbar" in r.rects and "readout" in r.rects:
                check(not overlap(r.rects["toolbar"], r.rects["readout"]), "toolbar and readout do not overlap" + tag)

def part_reading_sizes(binary, work):
    prefetch([((binary, work, f"read_{d}_{n}", {"PL_NEWUI": "on", "PL_SCALE": str(d), "PL_TEXTSIZE": n, "PL_OPEN": "about"}), dict(frames=30, size=f"{int(round(1000 * d))}x{int(round(700 * d))}"))
              for d in (1.0, 1.25, 1.5, 2.0) for n in TF if (d, n) in KEEP])
    # the reading view at every Text size and Windows scale: its headings are exact, its page text never under the floors
    for dpi in (1.0, 1.25, 1.5, 2.0):
        for tfn, tf in TF.items():
            if (dpi, tfn) not in KEEP:
                continue
            r = run_app(binary, work, f"read_{dpi}_{tfn}", {"PL_NEWUI": "on", "PL_SCALE": str(dpi), "PL_TEXTSIZE": tfn, "PL_OPEN": "about"}, frames=30, size=f"{int(round(1000 * dpi))}x{int(round(700 * dpi))}")
            tag = f" (reading view, scale {dpi}, text {tfn})"
            check(any(k == "pbody" for k, _, _ in r.texts) and any(k == "h1" for k, _, _ in r.texts), "the reading view drew its title and its page text" + tag)
            bad = [(k, px, t) for k, px, t in r.texts if k in SCALE and px != expected(k, dpi, tf)]
            check(not bad, "its headings and card texts are exactly on the scale" + tag + " " + str(bad[:3]))
            under = [(k, px, t) for k, px, t in r.texts if px < (SCALE[k][1] if k in SCALE else PAGE_FLOORS[k])]
            check(not under, "no page text under its floor" + tag + " " + str(under[:3]))
            col = r.rects.get("reading.column")
            check(col is not None and inside(col, r.window[0], r.window[1]), "the reading column stays inside the window" + tag)

def part_small(binary, work):
    prefetch([((binary, work, f"small_{d}_{n}", {"PL_NEWUI": "on", "PL_SCALE": str(d), "PL_TEXTSIZE": n, "PL_SELECT": "Earth", "PL_SCRIPT": EXERCISE}), dict(frames=112, size=sz))
              for d, sz in ((1.0, "880x560"), (1.5, "1320x840")) for n in ("small", "xl")])
    # the smallest window: 880 x 560 units, at 100 % and at 150 %
    for dpi, size in ((1.0, "880x560"), (1.5, "1320x840")):
        for tfn in ("small", "xl"):
            r = run_app(binary, work, f"small_{dpi}_{tfn}", {"PL_NEWUI": "on", "PL_SCALE": str(dpi), "PL_TEXTSIZE": tfn, "PL_SELECT": "Earth", "PL_SCRIPT": EXERCISE}, frames=112, size=size)
            tag = f" (smallest window, scale {dpi}, text {tfn})"
            w, h = r.window[0], r.window[1]
            for name, rect in r.rects.items():
                if not (name.startswith("occ") or name.startswith("ringlabel.") or name.startswith("ringskip.")):
                    check(inside(rect, w, h), f"{name} stays inside the smallest window {rect}" + tag)
            check(not overlap(r.rects["toolbar"], r.rects["readout"]), "toolbar and readout do not overlap" + tag)

def part_collapse(binary, work):
    r = run_app(binary, work, "collapse", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "10:click=card.card.collapse"}, frames=40)
    check("card.orbit.legend.collapsed" in r.rects, "the Legend starts collapsed")
    check(any(k == "title" and t == "Legend" for k, _, t in r.texts), "the collapsed Legend still shows its title")
    check("card.orbit.card.collapsed" in r.rects, "the subject card collapses when its button is pressed: " + str([k for k in r.rects if k.startswith("card")]))
    check(any(k == "title" and t == "Earth" for k, _, t in r.texts), "the collapsed subject card still shows its title (Earth)")
    cr = r.rects.get("card.orbit.card.collapsed")
    if cr:
        check(cr[3] <= 40 and cr[2] <= 200, f"a collapsed card is small ({cr})")

def part_keys(binary, work):
    def panels(r): return [n for n in r.notes if n.startswith("panelopen") and n != "panelopen controls"]   # (the docked Controls panel is not a dropdown)
    r = run_app(binary, work, "k_open", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.menu"}, frames=30)
    check(panels(r) == ["panelopen menu"], "a click on the menu button opens its panel: " + str(panels(r)))
    r = run_app(binary, work, "k_esc", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.menu;20:key=escape"}, frames=30)
    check(panels(r) == [], "Esc closes the open panel: " + str(panels(r)))
    r = run_app(binary, work, "k_one", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.menu;20:click=top.chip"}, frames=34)
    check(panels(r) == ["panelopen chip"], "one dropdown at a time: " + str(panels(r)))
    r = run_app(binary, work, "k_ctrlk", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=ctrl+k"}, frames=30)
    check("panel.search" in r.rects, "Ctrl+K opens search")
    r = run_app(binary, work, "k_ctrlk_esc", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=ctrl+k;20:key=escape"}, frames=30)
    check("panel.search" not in r.rects, "Esc closes search")
    r = run_app(binary, work, "k_ctrlk_over_panel", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.menu;20:key=ctrl+k"}, frames=34)
    check("panel.search" in r.rects and panels(r) == [], "Ctrl+K closes an open dropdown and opens search")
    r = run_app(binary, work, "k_space_none", {"PL_NEWUI": "on", "PL_SCRIPT": "30:state"}, frames=34)
    r2 = run_app(binary, work, "k_space", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=space;30:state"}, frames=34)
    r3 = run_app(binary, work, "k_space_search", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=ctrl+k;14:key=space;30:state"}, frames=34)
    paused = lambda x: x.states[-1].split("paused=")[1].split()[0] if x.states else "?"
    check(paused(r) == "1", "the lab starts paused here (reduced motion): " + str(r.states))
    check(paused(r2) == "0", "Space plays the paused lab: " + str(r2.states))
    check(paused(r3) == "1", "Space inside search is typing, not play: " + str(r3.states))

def part_shift(binary, work):
    # 1000 x 640: the Time panel covers the middle where Earth is
    r = run_app(binary, work, "shift_on", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "20:click=top.chip"}, frames=100, size="1000x640")
    r0 = run_app(binary, work, "shift_off", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SHIFT": "off", "PL_SCRIPT": "20:click=top.chip"}, frames=100, size="1000x640")
    def body(res):
        for n in res.note("body"):
            _, x, y, rad = n.split()
            return float(x), float(y), float(rad)
    def hit(res, b):
        x, y, rad = b
        return [n for n, rc in res.rects.items() if n.startswith("occ") and rc[0] - rad < x < rc[0] + rc[2] + rad and rc[1] - rad < y < rc[1] + rc[3] + rad]
    b_on, b_off = body(r), body(r0)
    check(b_on is not None and b_off is not None, "the selected body's place is printed")
    if b_on and b_off:
        check(hit(r0, b_off), f"without the shift Earth {b_off} is under an open panel (so the test can fail): {hit(r0, b_off)}")
        check(not hit(r, b_on), f"with the shift Earth {b_on} is outside every open panel and card: {hit(r, b_on)}")
        check(abs(b_on[0] - 500) > 100, f"Earth really moved from the middle ({b_on})")

def part_reading(binary, work):
    r = run_app(binary, work, "reading", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=60)
    col = r.rects.get("reading.column")
    check(col is not None, "About opens in the reading view")
    if col:
        check(col[2] <= 34 * 15 + 0.6, f"the column is at most 34 em ({col[2]} px at body 15)")
        check(abs((col[0] + col[2] / 2) - 640) < 1.0, "the column is centred")
    check(any(k == "h1" and "About" in t for k, _, t in r.texts), "the page has its H1 title")
    check(any(k == "h3" for k, _, t in r.texts), "the page has headings")
    toc = r.note("toc")
    check(toc and int(toc[0].split()[1]) >= 4, "the contents list has the page's headings: " + str(toc))
    rm = r.note("readmax")
    check(rm and int(rm[0].split()[1]) <= 75, "no line of body text is longer than 75 characters: " + str(rm))
    r = run_app(binary, work, "reading_wide", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=60, size="2000x900")
    check("reading.column" in r.rects and r.note("toc") and "reading.toc" in r.rects, "a wide window shows the reading view with its contents rail")

def part_old(binary, work):
    # 0.16 stage 1b: the old interface is gone: neither lab has an Old panel button, and the app has no "off" layout
    r = run_app(binary, work, "old", {"PL_ATOM": "1", "PL_SCRIPT": "10:click=top.old"}, frames=40)
    check("panel.old" not in r.rects and len(r.scripted_missing()) == 1, "the Atom Lab has no Old panel button")
    r = run_app(binary, work, "old_orbit", {"PL_SCRIPT": "10:click=top.old"}, frames=40)
    check("panel.old" not in r.rects and len(r.scripted_missing()) == 1, "the Orbit Lab has no Old panel button")
    r = run_app(binary, work, "off", {"PL_NEWUI": "off"}, frames=30)
    check(r.window is not None and r.texts, "the New UI is always on (PL_NEWUI=off is ignored)")

# 0.59.0 (Vulkan step 1 of 5): the app with the validation layers on: it starts on Vulkan (lavapipe here), the log has every [vulkan] step, zero validation
# errors (0.69.0: OpenGL is gone, and with it the comparison of these pictures with OpenGL's). Then the swapchain made again. Only when asked (check_shell.py ... vulkan).
VK_STEPS = ["instance", "surface", "device", "swapchain", "pipelines", "renderer2d pipelines", "renderer2d textures", "renderer2d check", "first frame"]   # (0.60.0: + renderer2d)

def _settings_files(home):
    log, cfg = "", ""
    for root, _dirs, files in os.walk(home):
        if "gpu-paths-log.txt" in files:
            log = open(os.path.join(root, "gpu-paths-log.txt"), errors="replace").read()
        for f in files:
            if f.endswith(".txt") and f.startswith("settings"):
                cfg = open(os.path.join(root, f), errors="replace").read()
    return log, cfg

def _differ(a, b, limit=8):
    from shell_lib import Bmp
    A, B = Bmp(a), Bmp(b)
    if (A.w, A.h) != (B.w, B.h):
        return None
    big = 0
    for y in range(A.h):
        for x in range(A.w):
            p, q = A.px(x, y), B.px(x, y)
            if max(abs(p[0] - q[0]), abs(p[1] - q[1]), abs(p[2] - q[2])) > limit:
                big += 1
    return big / float(A.w * A.h)

def part_vulkan(binary, work):
    vk_env = {"PL_VK_VALIDATION": "1"}
    for name, opened in (("settings", "settings"), ("page", "about")):         # (Settings at its top: the Graphics row itself says which is in use)
        v = run_app(binary, work, "vk_" + name, dict(vk_env, PL_OPEN=opened, PL_NO_PICTURE="1"), frames=40)
        check("Vulkan: " in v.out and v.code == 0, f"{name}: the app started on Vulkan and ended normally (exit {v.code}; {[l for l in v.out.splitlines() if l.startswith('Vulkan')][:1]})")
        check("PLVKVALIDATION missing" not in v.err, f"{name}: the validation layers are installed and on")
        errs = [l for l in v.err.splitlines() if l.startswith("PLVKVALIDATION error")]
        warns = [l for l in v.err.splitlines() if l.startswith("PLVKVALIDATION warning")]
        check("PLVK validation errors=0" in v.out and not errs, f"{name}: zero validation errors ({len(errs)}; first: {errs[:1]}; warnings {len(warns)})")
        log, _cfg = _settings_files(os.path.join(os.path.abspath(work), "home_vk_" + name))
        missing = [st for st in VK_STEPS if f"[vulkan] " not in log or f"begin {st}" not in log or f"end {st}" not in log]
        check(not missing and "Vulkan: the whole path succeeded" in log, f"{name}: the log has every [vulkan] step (missing: {missing})")
        check(os.path.exists(v.bmp), f"{name}: the window's picture was read back")
        if name == "settings":
            gl_note = [n for n in v.notes if n.startswith("graphics ")]
            check(gl_note and "Graphics: Vulkan, on " in gl_note[0], f"Settings says Vulkan is in use: {gl_note[:1]}")
    # the swapchain made again: the window resized twice and VSync switched off and on (other present modes), validation still silent
    m = run_app(binary, work, "vk_resize", dict(vk_env, PL_OPEN="settings:vsync", PL_SCRIPT="8:size=1000x700;14:click=vsync;20:size=1280x800;26:click=vsync"), frames=34)
    errs = [l for l in m.err.splitlines() if l.startswith("PLVKVALIDATION error")]
    check("Vulkan: " in m.out and "PLVK validation errors=0" in m.out and not errs and not m.scripted_missing(), f"resized and VSync switched: zero validation errors ({errs[:1]}, {m.scripted_missing()})")
    check(os.path.exists(m.bmp) and Bmp(m.bmp).w == 1280, "after the resize the swapchain has the window's new size (the last picture 1280 wide)")

# 0.69.0 (OpenGL removed): a PC whose Vulkan cannot start. A step made to fail (PL_VK_FAIL=device, a test switch): the app shows its message (printed by the
# checks: PLNOVULKAN, PLNOVULKANTEXT; no window waits for a click) naming what is needed and what this PC has, draws no frame and ends with exit code 1, not
# a crash; the log and the settings keep the failed step. The next start (no switch): the guard keeps Vulkan off, the same message. Try again
# (PL_NOVULKAN_ANSWER=again) forgets it; the start after that runs on Vulkan. (On 0.68.1 the first start went on to OpenGL: no message.)
def part_novulkan(binary, work):
    home = os.path.join(os.path.abspath(work), "home_nv")
    subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    f = run_app(binary, work, "nv_fail", dict(HOME=home, PL_VK_FAIL="device"), frames=30)
    text = [l for l in f.out.splitlines() if l.startswith("PLNOVULKANTEXT ")]
    check(any(l.startswith("PLNOVULKAN ") and "made to fail on purpose" in l for l in f.out.splitlines()) and text and "Vulkan 1.3" in text[0] and "VK_KHR_dynamic_rendering" in text[0]
          and "VK_KHR_synchronization2" in text[0] and "This PC: " in text[0], f"no Vulkan: the message names what is needed and what this PC has ({text[:1]})")
    check(f.code == 1 and f.window is None and "PLNOVULKANANSWER close" in f.out, f"no Vulkan: no frame drawn, the app ends with exit code 1, no crash (exit {f.code})")
    log, cfg = _settings_files(home)
    check("[vulkan]" in log and "failed: made to fail on purpose" in log and "Vulkan could not start" in log, "no Vulkan: the log keeps the [vulkan] lines and says Vulkan could not start")
    check("gpuvulkan=fail | vulkan " in cfg, "no Vulkan: the settings file keeps the failed step")
    n = run_app(binary, work, "nv_next", dict(HOME=home, PL_NOVULKAN_ANSWER="again"), frames=30)
    check(n.code == 1 and any(l.startswith("PLNOVULKAN ") and "could not start last time" in l for l in n.out.splitlines()) and "PLNOVULKANANSWER again" in n.out,
          f"no Vulkan: the next start keeps it off and says so; Try again answered (exit {n.code})")
    _log, cfg2 = _settings_files(home)
    check("gpuvulkan=fail" not in cfg2, "no Vulkan: Try again forgets the failed step in the settings file")
    a = run_app(binary, work, "nv_again", dict(HOME=home), frames=30)
    check("Vulkan: " in a.out and a.code == 0 and a.window is not None and "PLNOVULKAN" not in a.out, f"no Vulkan: after Try again the app starts on Vulkan (exit {a.code})")

# 0.60.0 (Vulkan step 2 of 5, the Vulkan plan, done in 0.69.0 (git keeps it)): renderer2d on Vulkan. Each view of REPORT.md's coverage table (every drawing call of Renderer2D that is not a
# card program -> one view that makes it; PL_LOG_DRAWKINDS says which calls a run made, so a view that stops making its call fails here) drawn by Vulkan with
# the validation layers on, one sample (PL_NO_MSAA) and on the processor's paths (PL_NO_COMPUTE and the labs' card switches off), so the pictures show
# the drawing alone (0.69.0: OpenGL is gone, and with it their comparison with OpenGL's). Then: every view with 4 samples (zero validation errors, the log
# says 4 samples), one view with the synchronisation checks on and Save image (zero errors), Save image has the lab's picture, and the start-up check
# broken on purpose (PL_VK_CHECK=break: the message, exit code 1). Only when asked (check_shell.py ... vulkan2d).
VK2D_CPU = {"PL_NO_COMPUTE": "1", "PL_GPU_CLOUD": "off", "PL_MOL_CARD": "0", "PL_ATOM_CARD": "0", "PL_LOG_DRAWKINDS": "1"}
VK2D_VIEWS = {   # name: (settings, the drawing calls the view must make)
    "orbit_2d": ({"PL_SELECT": "Earth"}, "rect line triangle disc ring sphere sunDisc fadeDisc squares skyMap"),
    "orbit_earth": ({"PL_SELECT": "Earth", "PL_3D": "1", "PL_ZOOM": "5.5", "PL_DATE": "2026-08-12", "PL_TIME": "17:46", "PL_PAUSED": "1"}, "planet planetMap planetEclipse planetAir"),
    "orbit_saturn": ({"PL_SELECT": "Saturn", "PL_3D": "1", "PL_ZOOM": "4.1", "PL_PAUSED": "1"}, "planet planetMap planetRings"),
    "orbit_mono": ({"PL_SELECT": "Earth", "PL_THEME": "monochrome"}, "gray skyMap"),
    "atom_nucleon": ({"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;qstyle=Simple;detail=+ Sea", "PL_ATOM_DO": "nucleon"}, "softLayer cloudLayer blob0 blob3 blob4 blob6 blob7 blob8 blob10"),   # (0.66.0: the glows as kind 10, glow stacks; kind 1 is drawn no more)
    "atom_classic": ({"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;view=Cloud 3D;orb=2p;cstyle=Classic"}, "blob5"),
    "atom_density": ({"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;view=Orbit;style=Density", "PL_ATOM_DO": "nucleus"}, "blob2"),
    "atom_chart": ({"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;view=Chart of nuclides"}, "mesh"),
    "mol_fill": ({"PL_MOLECULE": "C6H6", "PL_MOLECULE_STYLE": "fill"}, "spheres"),
    "mol_real": ({"PL_MOLECULE": "CH3OH", "PL_MOLECULE_STYLE": "real"}, "triangle blob3 blob6"),
    "mol_ball": ({"PL_MOLECULE": "H2O"}, "blob0 blob3 blob8"),
}

# 0.61.0 (Vulkan step 3 of 5): the card views: each card program drawing on Vulkan (one sample, the validation layers with their synchronisation checks
# on; 0.69.0: their comparison with OpenGL's card programs went with OpenGL). The Vulkan log must name the program's first full use (so the view really
# drew from the card). Then: one program broken on purpose (PL_VK_CARDFAIL=cloud: only it falls back,
# the log says why, the picture is the processor's) and a card without double precision (PL_VK_NO_FLOAT64: the belts on the processor, Settings says so).
VK3_VIEWS = {   # name: (settings, frames, the Vulkan log line of its first full use, OpenGL's log line or "")
    "glow": ({"PL_ATOM": "1", "PL_ATOM_SET": "Z=1;view=Cloud 3D;orb=3d;cstyle=Glow"}, 60, "[vulkan] [cloud] |end the Glow dots' first full use", ""),
    "super": ({"PL_ATOM": "1", "PL_ATOM_BENCHSCENE": "12", "PL_MOTION": "off"}, 60, "[vulkan] [groups] |end the groups' first full use", ""),
    "outline": ({"PL_ATOM": "1", "PL_ATOM_SET": "Z=1;view=Cloud 3D;orb=3d;outline=On"}, 60, "[vulkan] [outline] |end the outline's first full use", ""),
    "slice": ({"PL_ATOM": "1", "PL_ATOM_BENCHSCENE": "14", "PL_ATOM_BENCHSTOP": "60", "PL_MOTION": "off"}, 120, "[vulkan] [slice] |end the Slice's first full use", "[slice] |end the Slice's first full use"),
    "jump": ({"PL_ATOM": "1", "PL_ATOM_BENCHSCENE": "37", "PL_MOTION": "off"}, 150, "[vulkan] [jump] |end the jump's first full use", "[jump] |end the jump's first full use"),
    "density": ({"PL_MOLECULE": "C6H6", "PL_MOLECULE_STYLE": "real"}, 90, "[vulkan] [molcloud] |end the Density cloud's first full use", "[molcloud] |end the Density cloud's first full use"),
    "belts": ({"PL_SELECT": "Earth"}, 60, "[vulkan] [rocks] |end the rocks' first full use", ""),
}
# (PL_VK_SYNC: a batch the app asks about is waited for, so the Slice's dots and the outline arrive at the frame that asks, every run, as OpenGL's software
# driver gives them here: without it Vulkan's pictures of a moving Slice differ from run to run by when the batches finished)
VK3_SYNC = {"PL_VK_VALIDATION": "1", "VK_KHRONOS_VALIDATION_VALIDATE_SYNC": "true", "PL_NO_MSAA": "1", "PL_VK_SYNC": "1"}

def _logged(log, prefix, text):   # a line of the guard's log: its prefix, its time, then the text
    return any(l.startswith(prefix) and re.search(r"\]\s*" + re.escape(text), l[len(prefix):]) for l in log.splitlines())

def part_vulkancards(binary, work):
    calls = []
    for name, (env, frames, _v, _g) in VK3_VIEWS.items():
        calls.append(((binary, work, "v3_vk_" + name, dict(env, **VK3_SYNC)), dict(frames=frames)))
    glow = VK3_VIEWS["glow"][0]
    calls.append(((binary, work, "v3_fail", dict(glow, PL_VK_CARDFAIL="cloud", **VK3_SYNC)), dict(frames=60)))
    calls.append(((binary, work, "v3_cpuglow", dict(glow, PL_GPU_CLOUD="off", **VK3_SYNC)), dict(frames=60)))
    calls.append(((binary, work, "v3_nof64", dict(PL_SELECT="Earth", PL_VK_NO_FLOAT64="1", PL_OPEN="settings:graphics", PL_LOG_DRAWKINDS="1", **VK3_SYNC)), dict(frames=40)))
    prefetch(calls)
    for name, (env, frames, vline, gline) in VK3_VIEWS.items():
        v = run_app(binary, work, "v3_vk_" + name, dict(env, **VK3_SYNC), frames=frames)
        vlog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v3_vk_" + name))
        check("Vulkan: " in v.out, f"card {name}: the app started on Vulkan")
        check(_logged(vlog, *vline.split("|")), f"card {name}: Vulkan drew it from the card program (the log: '{vline}')")
        errs = _vk_errors(v)
        check("PLVK validation errors=0" in v.out and not errs, f"card {name}: zero validation errors, synchronisation checks on ({len(errs)}; first: {errs[:1]})")
    # one program broken on purpose: only it falls back, the log says why, the picture is the processor's
    f = run_app(binary, work, "v3_fail", dict(glow, PL_VK_CARDFAIL="cloud", **VK3_SYNC), frames=60)
    c = run_app(binary, work, "v3_cpuglow", dict(glow, PL_GPU_CLOUD="off", **VK3_SYNC), frames=60)
    flog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v3_fail"))
    check("Vulkan card program on the processor: the Cloud 3D Glow dots: made to fail on purpose (PL_VK_CARDFAIL=cloud)" in f.out, "a broken program (PL_VK_CARDFAIL=cloud): the app says the Glow dots are on the processor, and why")
    check("[vulkan] [cloud]" in flog and "failed: made to fail on purpose" in flog, "a broken program: the log says why")
    check("end the groups' first small run" in flog and "end the outline's first small run" in flog and "end the rocks' first small run" in flog and "Vulkan: " in f.out,
          "a broken program: the other programs and Vulkan stay on")
    frac = _differ(f.bmp, c.bmp) if os.path.exists(f.bmp) and os.path.exists(c.bmp) else None
    print(f"vulkancards broken program: {('%.3f %%' % (100 * frac)) if frac is not None else 'no pair of pictures'} of the pixels differ from the processor's picture")
    check(frac is not None and frac <= 0.0001, f"a broken program: the picture is the processor's ({('%.4f %%' % (100 * frac)) if frac is not None else 'no pair'} of the pixels differ by more than 8 levels)")
    # a card without double precision in its shaders: the belts on the processor, Settings' Graphics row says so
    n = run_app(binary, work, "v3_nof64", dict(PL_SELECT="Earth", PL_VK_NO_FLOAT64="1", PL_OPEN="settings:graphics", PL_LOG_DRAWKINDS="1", **VK3_SYNC), frames=40)
    nlog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v3_nof64"))
    note = [x for x in n.notes if x.startswith("graphics ")]
    check("Orbit Lab rocks: Vulkan: CPU path (this card has no double precision in its shaders (shaderFloat64)" in n.out and "shaderFloat64): switched off" in nlog,
          "no double precision (PL_VK_NO_FLOAT64): the rocks are placed by the processor, the log says so")
    check("squares" in _kinds(n), "no double precision: the belts are still drawn (the processor's squares)")
    check(note and "The belts' rocks: the processor, because this card has no double precision in its shaders (shaderFloat64)" in note[0], f"no double precision: Settings' Graphics row says so: {note[:1]}")

# 0.62.0 (Vulkan step 4 of 5): the Black Hole Lab on Vulkan. Each view drawn by Vulkan (one sample, the validation layers with their synchronisation checks),
# running until the lab has settled (its tables, the sky's check, the exposure and the glare done: PL_BH_SETTLE; 0.69.0: their comparison with OpenGL's
# pictures went with OpenGL). The Vulkan log must name the first full picture of the lab's passes (and of the sky per pixel for the spinning hole). Then:
# 0.69.0: a card that offers Vulkan 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2 (PL_VK_PRETEND_API=1.2 on lavapipe) draws the lab with zero
# validation errors; without one of them (PL_VK_PRETEND_NOEXT) the message says what is missing. The pipeline cache (the
# second start makes the sky's pipeline faster than the first; a damaged file is ignored), one program broken on purpose (PL_VK_CARDFAIL=sky: only the
# sky per pixel falls back, to the processor's table, and the log says why).
VK4_FEED = "preset=feeding;spin=0.6"
VK4_VIEWS = {   # name: PL_BH_SET
    "lens": "preset=feeding;spin=0",
    "real": VK4_FEED + ";glare=off",
    "film": VK4_FEED + ";look=film",
    "glare": VK4_FEED + ";glare=eye",
    "alllight": VK4_FEED + ";light=all;glare=off",
    "skyblack": VK4_FEED + ";sky=black;glare=off",
    "truebright": VK4_FEED + ";skybeside=true;glare=off",
}
VK4_RUN = {"PL_BLACKHOLE": "1", "PL_BH_SETTLE": "20:30", "PL_BH_GLARENOW": "1"}   # (the glare at once, in its frame: OpenGL's thread of the veil, below normal
                                                                                  # priority, can wait for minutes here behind llvmpipe's own threads)

def _sky_ms(r):
    m = [l for l in r.out.splitlines() if l.startswith("PLVKBH pipeline the whole sky shader")]
    return float(m[-1].split("ms=")[1].split()[0]) if m else None

def part_vulkanbh(binary, work):
    calls = []
    for name, bh in VK4_VIEWS.items():
        calls.append(((binary, work, "v4_vk_" + name, dict(VK4_RUN, PL_BH_SET=bh, **VK3_SYNC)), dict(frames=1500, timeout=2400)))
    # (0.68.1: PL_BH_SETTLE 20:120, not 20:30. The run ends once the lab has been idle that many frames; the lens's first full picture waits for its turn behind the sky's last
    # step and begins a few frames after it, so 30 idle frames could end the run first: it passed only in the prefetch's slower parallel runs and failed alone, 0.68.0's build too)
    fail_env = dict(VK4_RUN, PL_BH_SET=VK4_VIEWS["real"], PL_VK_CARDFAIL="sky", **VK3_SYNC)
    fail_env["PL_BH_SETTLE"] = "20:120"
    async_env = dict(VK4_RUN, PL_BH_SET=VK4_VIEWS["glare"], **VK3_SYNC)
    del async_env["PL_BH_GLARENOW"]
    calls.append(((binary, work, "v4_async", async_env), dict(frames=1500, timeout=2400)))
    old_env = dict(VK4_RUN, PL_BH_SET=VK4_VIEWS["real"], PL_VK_PRETEND_API="1.2", **VK3_SYNC)
    calls.append(((binary, work, "v4_old", old_env), dict(frames=1500, timeout=2400)))
    calls.append(((binary, work, "v4_oldnoext", dict(PL_VK_PRETEND_API="1.2", PL_VK_PRETEND_NOEXT="synchronization2")), dict(frames=30)))
    calls.insert(0, ((binary, work, "v4_fail", fail_env), dict(frames=1500, timeout=2400)))
    prefetch(calls)
    for name, bh in VK4_VIEWS.items():
        v = run_app(binary, work, "v4_vk_" + name, dict(VK4_RUN, PL_BH_SET=bh, **VK3_SYNC), frames=1500, timeout=2400)
        vlog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v4_vk_" + name))
        check("Vulkan: " in v.out, f"black hole {name}: the app started on Vulkan")
        check(_logged(vlog, "[vulkan] [lens] ", "end the first full picture"), f"black hole {name}: Vulkan drew it with the lab's passes (the log: the lens guard's first full picture)")
        if name != "lens":
            check(_logged(vlog, "[vulkan] [sky] ", "end the first full picture") and "PLBHSKY state=1" in v.out,
                  f"black hole {name}: the spinning hole per pixel on Vulkan (its check passed, its first full picture)")
        errs = _vk_errors(v)
        check("PLVK validation errors=0" in v.out and not errs, f"black hole {name}: zero validation errors, synchronisation checks on ({len(errs)}; first: {errs[:1]})")
    # the glare made over the frames (the app's way: its blocks read back without waiting, the veil on a thread of its own): the same picture
    a = run_app(binary, work, "v4_async", async_env, frames=1500, timeout=2400)
    v = run_app(binary, work, "v4_vk_glare", dict(VK4_RUN, PL_BH_SET=VK4_VIEWS["glare"], **VK3_SYNC), frames=1500, timeout=2400)
    frac = _differ(a.bmp, v.bmp) if os.path.exists(a.bmp) and os.path.exists(v.bmp) else None
    print(f"vulkanbh glare over the frames: {('%.3f %%' % (100 * frac)) if frac is not None else 'no pair of pictures'} of the pixels differ from the glare made at once")
    check(frac is not None and frac <= 0.01 and not _vk_errors(a), "the glare made over the frames on Vulkan (read back without waiting, the veil on its thread) is the glare made at once")
    # the light paths on Vulkan: their pipeline and the check against the processor (spin_card.h's limits)
    vlog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v4_vk_real"))
    v = run_app(binary, work, "v4_vk_real", dict(VK4_RUN, PL_BH_SET=VK4_VIEWS["real"], **VK3_SYNC), frames=1500, timeout=2400)
    check(_logged(vlog, "[vulkan] [lightpaths] ", "end the light paths' pipeline: made") and "PLBHCARD state=1" in v.out,
          f"the light paths on Vulkan: their pipeline made, their check against the processor passed ({[l for l in v.out.splitlines() if l.startswith('PLBHCARD')][-1:]})")
    # one program broken on purpose: only the sky per pixel falls back (the processor's table), the log says why, the rest stays on Vulkan
    f = run_app(binary, work, "v4_fail", fail_env, frames=1500, timeout=2400)
    flog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v4_fail"))
    check("Vulkan: " in f.out and "PLBHSKY state=-1" in f.out and "made to fail on purpose (PL_VK_CARDFAIL=sky)" in flog and "[vulkan] [sky]" in flog,
          "a broken sky program (PL_VK_CARDFAIL=sky): only it falls back, the log says why")
    check(_logged(flog, "[vulkan] [lens] ", "end the first full picture") and "PLBHCARD state=1" in f.out and not _vk_errors(f),
          "a broken sky program: the lab's passes and the light paths stay on Vulkan, zero validation errors")
    # 0.69.0: Vulkan 1.2 with the two extensions is used (their KHR functions); without one, the message says which is missing
    o = run_app(binary, work, "v4_old", old_env, frames=1500, timeout=2400)
    olog, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v4_old"))
    errs = _vk_errors(o)
    check("Vulkan: " in o.out and "from VK_KHR_dynamic_rendering and VK_KHR_synchronization2" in olog and _logged(olog, "[vulkan] [lens] ", "end the first full picture")
          and "PLVK validation errors=0" in o.out and not errs, f"a card with Vulkan 1.2 and the two extensions: the Black Hole Lab drawn, zero validation errors ({len(errs)}; {errs[:1]})")
    x = run_app(binary, work, "v4_oldnoext", dict(PL_VK_PRETEND_API="1.2", PL_VK_PRETEND_NOEXT="synchronization2"), frames=30)
    nv = [l for l in x.out.splitlines() if l.startswith("PLNOVULKAN ")]
    check(x.code == 1 and nv and "offers Vulkan 1.2 without VK_KHR_synchronization2" in nv[0] and "needs Vulkan 1.3, or 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2" in nv[0],
          f"a card with Vulkan 1.2 but no VK_KHR_synchronization2: the message says what is missing and what is needed ({nv[:1]}, exit {x.code})")
    log, _c = _settings_files(os.path.join(os.path.abspath(work), "home_v4_vk_lens"))
    check(re.search(r"asked for 1\.[34]\.0", log) is not None and "the card's" in log, "the log keeps the version asked for and the card's own")
    # the pipeline cache: kept in the settings folder; the second start makes the sky's pipeline faster; a damaged file is ignored
    home = os.path.join(os.path.abspath(work), "home_v4_cache")
    subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    env = dict(VK4_RUN, PL_BH_SET=VK4_VIEWS["real"], HOME=home, PL_BH_SETTLE="20:5")
    c1 = run_app(binary, work, "v4_cache1", env, frames=600, timeout=2400)
    c2 = run_app(binary, work, "v4_cache2", env, frames=600, timeout=2400)
    c3 = run_app(binary, work, "v4_cache3", dict(env, PL_VK_CACHE_CORRUPT="1"), frames=600, timeout=2400)
    t1, t2 = _sky_ms(c1), _sky_ms(c2)
    print(f"vulkanbh pipeline cache: the whole sky shader's pipeline {t1} ms at the first start, {t2} ms at the second")
    held = re.search(r"PLVKCACHE read, (\d+) bytes", c2.out)
    check("PLVKCACHE none yet" in c1.out and held is not None and t1 is not None and t2 is not None, f"the pipeline cache: written at the first start, read at the second ({t1} -> {t2} ms)")
    if held and int(held.group(1)) > 32:                                   # (lavapipe keeps only the cache's 32-byte header, no pipelines: nothing to be faster with)
        check(t2 < t1, f"the pipeline cache holds pipelines ({held.group(1)} bytes): the sky's pipeline is made faster at the second start ({t1} -> {t2} ms)")
    else:
        print(f"vulkanbh pipeline cache: this driver keeps no pipelines in it ({held.group(1) if held else '?'} bytes): the second start cannot be faster here")
    check("PLVKCACHE ignored: the file is damaged" in c3.out and "Vulkan: " in c3.out and _sky_ms(c3) is not None, "a damaged pipeline cache is ignored (no crash, the pipelines made anew)")

# 0.63.0: the guards blame the right program. 0.62.0 on the owner's PC: the light paths' pipeline (made on a worker) crashed the driver while the outline's
# marker was still on disk (its first small run had ended, but a marker stayed until its whole path had passed), so the next start switched both off. Now at
# most one guarded step is open at a time in the whole process and a step's marker goes when it ends. The light paths' pipeline made to "crash" (PL_GPU_DIE)
# on a fresh settings folder; the next start: the light paths off, the outline still allowed and its steps run (its first full use drawn).
def part_guardturn(binary, work):
    home = os.path.join(os.path.abspath(work), "home_gt")
    subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    d = run_app(binary, work, "gt_die", dict(HOME=home, PL_BLACKHOLE="1", PL_BH_SET=VK4_FEED, PL_GPU_DIE="the light paths' pipeline", PL_NO_GPU_HELPER="1"), frames=600, timeout=1200)   # (0.67.0: the app's own step, not the helper's)
    log1, _c = _settings_files(home)
    check("(test switch PL_GPU_DIE" in log1 and _logged(log1, "[vulkan] [lightpaths] ", "begin the light paths' pipeline"), "guard turn: the first start died inside the light paths' pipeline (PL_GPU_DIE)")
    check(_logged(log1, "[vulkan] [outline] ", "end the outline's first small run"), "guard turn: the outline's start-up steps ran before (its first full use not yet: it has not passed)")
    a = run_app(binary, work, "gt_next", dict(VK3_VIEWS["outline"][0], HOME=home), frames=90)
    log2, cfg2 = _settings_files(home)
    check("Vulkan: " in a.out and re.search(r"gpuvklightpaths=off \| [^\n]*\| the light paths' pipeline", cfg2) is not None,
          f"guard turn: the next start: the light paths off, their step named ({[l for l in cfg2.splitlines() if l.startswith('gpuvklightpaths')]})")
    check("gpuvkoutline=off" not in cfg2 and "The 90 % outline on Vulkan: allowed" in log2 and _logged(log2, "[vulkan] [outline] ", "end the outline's first full use"),
          f"guard turn: the outline still allowed and its steps run ({[l for l in cfg2.splitlines() if l.startswith('gpuvkoutline')]})")

# 0.67.0: the crash-safe helper (app/gpu_helper.h). A fresh settings folder: the helper's step made to "crash" (PL_GPU_DIE in the helper process) -> the app runs
# on to its last frame, that program off and the log names the step ("the helper died making ..."), the settings keep it; the next start does not try it
# again (no helper for it, still off). The light paths killed the same way: the next start runs the five kKerrTableCS variants in helpers and logs each;
# the start after that does not run them again (once per driver).
def part_gpuhelper(binary, work):
    nd = {"PL_ATOM": "1", "PL_ATOM_BENCHSCENE": "38", "PL_ATOM_BENCHSTOP": "40", "PL_NO_GPU_HELPER": "0"}   # (0.68.0: the helpers on: shell_lib turns them off)
    home = os.path.join(os.path.abspath(work), "home_gh"); subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    a = run_app(binary, work, "gh_die", dict(nd, HOME=home, PL_GPU_DIE="the nucleus density's pipeline"), frames=150, timeout=1200)
    log1, cfg1 = _settings_files(home)
    check("Vulkan: " in a.out and os.path.exists(a.bmp), "helper: the app ran on to its last frame (its screenshot) while its helper died")
    check("PLGPUHELPER nucdensity died making the nucleus density's pipeline" in a.out and "PLNUCDENSCARD state=-1" in a.out and "PLNUCDENSCARD state=1" not in a.out,
          "helper: the nucleus density off, the processor's squares drawn")
    check(_logged(log1, "[vulkan] [nucdensity] ", "the helper died making the nucleus density's pipeline"), "helper: the log names the step the helper died in")
    check(re.search(r"gpuvknucdensity=off \| [^\n]*\| the helper died making the nucleus density's pipeline", cfg1) is not None, "helper: the settings keep it off, the step named")
    b = run_app(binary, work, "gh_next", dict(nd, HOME=home), frames=100, timeout=1200)
    log2, _c = _settings_files(home)
    check("the helper starts (nucdensity)" not in log2 and "PLGPUHELPER nucdensity" not in b.out and "PLNUCDENSCARD state=1" not in b.out,
          "helper: the next start does not try it again (no helper for it; still off)")
    home2 = os.path.join(os.path.abspath(work), "home_gh2"); subprocess.run(["rm", "-rf", home2]); os.makedirs(home2, exist_ok=True)
    c = run_app(binary, work, "gh_lp", dict(nd, HOME=home2, PL_GPU_DIE="the light paths' pipeline"), frames=60, timeout=1200)
    check("PLGPUHELPER lightpaths died making the light paths' pipeline" in c.out and os.path.exists(c.bmp), "helper: the light paths' helper died, the app ran on")
    d = run_app(binary, work, "gh_var", dict(nd, HOME=home2), frames=240, timeout=1200)
    log4, _c = _settings_files(home2)
    vs = re.findall(r"PLGPUHELPER variant (\w+): (.*)", d.out)
    print(f"  the variants: {vs}")
    check(len(vs) == 5 and all(v[1].startswith("made") for v in vs), f"helper: the five kKerrTableCS variants made by helpers on this driver ({len(vs)})")
    check(sum(1 for l in log4.splitlines() if "the helper's detective work, kKerrTableCS variant" in l) == 5, "helper: each variant's line in the log")
    e = run_app(binary, work, "gh_var2", dict(nd, HOME=home2), frames=60, timeout=1200)
    check("PLGPUHELPER variant" not in e.out, "helper: the variants are not run again with the same driver")
    # 0.68.1: the helper's time limit. (1) the app ends while a variant's helper sleeps (the limit left at 30 s): no helper process is left; (2) a variant that
    # never answers (PL_GPU_HANG, limit 3 s by PL_GPU_HELPER_LIMIT: the test's own value, the app's stays 30 s) is ended, logged "hung", the next variants are tried and
    # the variants file ends with five lines; (3) a file with the driver line and "math" alone (the owner's 0.68.0 file) tries the other four.
    vfile = None
    for root, _dirs, files in os.walk(home2):
        if "gpu-helper-variants.txt" in files: vfile = os.path.join(root, "gpu-helper-variants.txt")
    check(vfile is not None, "helper: the variants file exists")
    def vlines(): return [l.strip() for l in open(vfile, errors="replace").read().splitlines() if l.strip()]
    def helpers_left(): return subprocess.run(["pgrep", "-f", "--", "--gpu-helper"], capture_output=True, text=True).stdout.split()
    if vfile:
        head = vlines()[0]
        open(vfile, "w").write(head + "\n")
        f = run_app(binary, work, "gh_hang_exit", dict(nd, HOME=home2, PL_GPU_HANG="math"), frames=40, timeout=1200)
        check(os.path.exists(f.bmp) and not helpers_left(), f"helper: the app ended while a helper slept: no helper process left ({helpers_left()})")
        open(vfile, "w").write(head + "\n")
        g = run_app(binary, work, "gh_hang", dict(nd, HOME=home2, PL_GPU_HANG="step", PL_GPU_HELPER_LIMIT="3"), frames=900, timeout=1500)
        logh, _c = _settings_files(home2)
        vs2 = dict(re.findall(r"PLGPUHELPER variant (\w+): (.*)", g.out))
        print(f"  the variants with step hanging: {vs2}")
        check(vs2.get("step", "").startswith("hung making it (no answer after 3 s)"), f"helper: the hung variant is ended and named ({vs2.get('step')})")
        check(all(vs2.get(v, "").startswith("made") for v in ("math", "edge", "ray", "short")), "helper: the next variants were tried after the hung one")
        check(_logged(logh, "[vulkan] [lightpaths] ", "the helper hung making"), "helper: the log says the helper hung")
        vl = vlines()
        check(len(vl) == 6 and [l.split(":")[0] for l in vl[1:]].count("variant step") == 1 and "hung making it" in " ".join(vl), f"helper: the variants file holds the driver line and five variants ({len(vl)} lines)")
        check(not helpers_left(), "helper: no helper process left")
        open(vfile, "w").write("\n".join(vl[:2]) + "\n")                       # the driver line and the first variant (math) only: the owner's 0.68.0 file
        h = run_app(binary, work, "gh_rest", dict(nd, HOME=home2), frames=500, timeout=1500)
        vs3 = re.findall(r"PLGPUHELPER variant (\w+): ", h.out)
        check(sorted(vs3) == ["edge", "ray", "short", "step"] and len(vlines()) == 6, f"helper: a file with math alone tries the other four ({vs3})")
    # 0.68.1: a helper that ends without writing its result (exit code 7, PL_GPU_NORESULT): logged "ended without a result (exit code 7)", the app runs on. (On Windows a crash's
    # exit code, 0xC0000005, was a negative int and was read as "still running" for ever: the owner's 0.68.0 log never noticed the light paths' variant helper ending.)
    home4 = os.path.join(os.path.abspath(work), "home_gh4"); subprocess.run(["rm", "-rf", home4]); os.makedirs(home4, exist_ok=True)
    q = run_app(binary, work, "gh_nores", dict(nd, HOME=home4, PL_GPU_NORESULT="nucdensity"), frames=150, timeout=1200)
    logq, _c = _settings_files(home4)
    check("PLGPUHELPER nucdensity ended without a result (exit code 7)" in q.out and _logged(logq, "[vulkan] [nucdensity] ", "the helper ended without a result (exit code 7)") and os.path.exists(q.bmp),
          "helper: a helper that ends without a result is logged as such and the app runs on")
    # 0.68.0: every guarded program's pipelines and first small run made by a helper first; two programs whose helpers die (the lines' first small run, the
    # surface's pipelines): the app runs on, only those two off (the settings keep them, the log names their steps), the rest passed in their helpers
    home3 = os.path.join(os.path.abspath(work), "home_gh3"); subprocess.run(["rm", "-rf", home3]); os.makedirs(home3, exist_ok=True)
    t = run_app(binary, work, "gh_two", {"HOME": home3, "PL_NO_GPU_HELPER": "0", "PL_GPU_DIE": "the lines' first small run|the surface's pipelines"}, frames=300, timeout=1500)
    log5, cfg5 = _settings_files(home3)
    died = sorted(re.findall(r"PLGPUHELPER (\w+) died making ([^(]*) \(", t.out))
    passed = re.findall(r"PLGPUHELPER (\w+) passed", t.out)
    print(f"  died: {died}; passed: {passed}")
    check(os.path.exists(t.bmp) and died == [("lines", "the lines' first small run"), ("surface", "the surface's pipelines")], f"helper: two helpers died, the app ran on to its last frame ({died})")
    check(len(passed) == 13 and "lines" not in passed and "surface" not in passed, f"helper: the other 13 programs' helpers made their first steps ({len(passed)})")
    check(_logged(log5, "[vulkan] [lines] ", "the helper died making the lines' first small run") and _logged(log5, "[vulkan] [surface] ", "the helper died making the surface's pipelines"),
          "helper: the log names both programs and their steps")
    offs = sorted(re.findall(r"gpuvk(\w+)=off ", cfg5))
    check(offs == ["lines", "surface"], f"helper: only those two kept off in the settings ({offs})")
    check("PLLINESCARD state=-1" in t.out, "helper: the Orbit Lab draws its lines on the processor")

def _kinds(r):
    line = [l for l in r.out.splitlines() if l.startswith("PLDRAWKINDS ")]
    return {k for k, v in (p.split("=") for p in line[-1].split()[1:]) if v != "0"} if line else set()

def _vk_errors(r):
    return [l for l in r.err.splitlines() if l.startswith("PLVKVALIDATION error")]

def _png_rgb(path):   # (8-bit RGB or RGBA PNG, as ui/shot.cpp writes it) -> (w, h, rows of bytes RGB)
    import zlib, struct
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n"
    i, idat, w, h, ct = 8, b"", 0, 0, 2
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8]); c = d[i + 8:i + 8 + n]; i += 12 + n
        if t == b"IHDR": w, h, _bd, ct = struct.unpack(">IIBB", c[:10])
        elif t == b"IDAT": idat += c
    bpp = 4 if ct == 6 else 3
    raw, rows, prev, o = zlib.decompress(idat), [], bytearray(w * bpp), 0
    for _y in range(h):
        f, line = raw[o], bytearray(raw[o + 1:o + 1 + w * bpp]); o += 1 + w * bpp
        for x in range(len(line)):
            a = line[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line[k] for x in range(w) for k in range(x * bpp, x * bpp + 3))); prev = line
    return w, h, rows

def part_vulkan2d(binary, work):
    vk = {"PL_VK_VALIDATION": "1"}
    one = {"PL_NO_MSAA": "1"}
    calls = []
    for name, (env, _want) in VK2D_VIEWS.items():
        calls.append(((binary, work, "v2_vk_" + name, dict(VK2D_CPU, **one, **vk, **env)), dict(frames=40)))
        calls.append(((binary, work, "v2_ms_" + name, dict(VK2D_CPU, **vk, **env)), dict(frames=40)))
    prefetch(calls)
    for name, (env, want) in VK2D_VIEWS.items():
        v = run_app(binary, work, "v2_vk_" + name, dict(VK2D_CPU, **one, **vk, **env), frames=40)
        check("Vulkan: " in v.out, f"{name}: the app started on Vulkan")
        missing = set(want.split()) - _kinds(v)
        check(not missing, f"{name}: the view makes its drawing calls on Vulkan (missing: {sorted(missing)})")
        errs = _vk_errors(v)
        check("PLVK validation errors=0" in v.out and not errs, f"{name}: zero validation errors, one sample ({len(errs)}; first: {errs[:1]})")
        m = run_app(binary, work, "v2_ms_" + name, dict(VK2D_CPU, **vk, **env), frames=40)
        errs = _vk_errors(m)
        check("Vulkan picture: 4 samples" in m.out and "PLVK validation errors=0" in m.out and not errs, f"{name}: 4 samples on Vulkan, zero validation errors ({len(errs)}; first: {errs[:1]})")
    log, _cfg = _settings_files(os.path.join(os.path.abspath(work), "home_v2_ms_orbit_2d"))
    check("end renderer2d pipelines" in log and "4 samples" in log, "the log says the picture has 4 samples")
    # the synchronisation checks (the layer's validate_sync) on a view with both layers, and Save image on it
    sv = run_app(binary, work, "v2_sync", dict(VK2D_CPU, **vk, **VK2D_VIEWS["atom_nucleon"][0], VK_KHRONOS_VALIDATION_VALIDATE_SYNC="true",
                                                PL_SAVE_IMAGE="20", PL_SHOT_DIR=os.path.join(os.path.abspath(work), "v2_sync_shots")), frames=40)
    errs = _vk_errors(sv)
    check("Vulkan: " in sv.out and "PLVK validation errors=0" in sv.out and not errs, f"synchronisation checks on (layers, Save image): zero errors ({len(errs)}; first: {errs[:1]})")
    # Save image: the saved file has the lab's picture (0.69.0: its comparison with OpenGL's saved file went with OpenGL)
    d = os.path.join(os.path.abspath(work), "v2_shots_vk")
    if os.path.isdir(d):
        for f in os.listdir(d): os.remove(os.path.join(d, f))
    run_app(binary, work, "v2_save_vk", dict(VK2D_CPU, **one, **VK2D_VIEWS["orbit_2d"][0], **vk, PL_SAVE_IMAGE="20", PL_SHOT_DIR=d), frames=40)
    pngs = [f for f in os.listdir(d) if f.endswith(".png")] if os.path.isdir(d) else []
    shot = _png_rgb(os.path.join(d, pngs[0])) if pngs else None
    if shot:
        w, h, A = shot
        lit = sum(1 for y in range(0, h, 4) for x in range(0, 3 * w, 12) if max(A[y][x:x + 3]) > 90)   # (the picture is there: stars, paths, labels)
        print(f"vulkan2d save image: {w} x {h}, {lit} bright samples")
        check(lit > 50, f"Save image on Vulkan has the lab's picture ({lit} bright samples)")
    else:
        check(False, "Save image wrote a PNG")
    # the start-up check broken on purpose: the message (0.69.0: no OpenGL to start on), exit code 1
    b = run_app(binary, work, "v2_break", dict(vk, PL_VK_CHECK="break"), frames=30)
    nv = [l for l in b.out.splitlines() if l.startswith("PLNOVULKAN ")]
    check(b.code == 1 and nv and "wrong" in nv[0] and b.window is None, f"the start-up check failed on purpose: the message says why, exit code 1 ({nv[:1]}, exit {b.code})")

def main():
    binary, work = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None       # a third argument runs only some parts (for the perturbation runs): sizes1 sizes reading_sizes small collapse keys shift reading old
    want = lambda name: only is None or name in only
    os.makedirs(work, exist_ok=True)
    if want("sizes") or want("sizes1"):
        part_sizes(binary, work, not want("sizes"))
    if want("reading_sizes"):
        part_reading_sizes(binary, work)
    if want("small"):
        part_small(binary, work)
    if want("collapse"):
        part_collapse(binary, work)
    if want("keys"):
        part_keys(binary, work)
    if want("shift"):
        part_shift(binary, work)
    if want("reading"):
        part_reading(binary, work)
    if want("old"):
        part_old(binary, work)
    if only is not None and "vulkan" in only:                              # (0.59.0: only when asked; ui_shell lists check_shell.py:vulkan)
        part_vulkan(binary, work)
    if only is not None and "novulkan" in only:                            # (0.69.0: ui_shell lists check_shell.py:novulkan)
        part_novulkan(binary, work)
    if only is not None and "guardturn" in only:                           # (0.63.0: ui_shell lists check_shell.py:guardturn)
        part_guardturn(binary, work)
    if only is not None and "gpuhelper" in only:                           # (0.67.0: ui_shell lists check_shell.py:gpuhelper)
        part_gpuhelper(binary, work)
    if only is not None and "vulkan2d" in only:                            # (0.60.0: only when asked; ui_shell lists check_shell.py:vulkan2d)
        part_vulkan2d(binary, work)
        part_vulkancards(binary, work)                                     # (0.61.0: the card programs on Vulkan, in the same part)
        part_vulkanbh(binary, work)                                        # (0.62.0: the Black Hole Lab on Vulkan, in the same part)
    elif only is not None and "vulkancards" in only:                       # (0.61.0: that half alone, by hand; 0.62.0: with the Black Hole Lab's)
        part_vulkancards(binary, work)
        part_vulkanbh(binary, work)
    elif only is not None and "vulkanbh" in only:                          # (0.62.0: the Black Hole Lab's alone, by hand)
        part_vulkanbh(binary, work)
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
