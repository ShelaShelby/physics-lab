#!/usr/bin/env python3
# 0.15 stage 2, C2 test (needs the app built on Linux, headless on Mesa llvmpipe, like tests/check_effects_pictures.py):
#   check_page_cache.py <PhysicsLab binary> <work folder>
# The page effects cache (src/ui/style_draw.cpp pageCacheDraw: the page and its textures drawn ONCE into a texture, then a quad per docked panel) against the old drawing
# (PL_PAGE_CACHE=off) for Frutiger Aero, Y2K Ice and Y2K Night, on three screens (Orbit Lab, Atom Lab nucleus, chart of nuclides), 1000 x 640:
#   tolerance  the two pictures may differ only by rounding and by the edge pixels of the textures' circles (the old way is drawn into the window's multisampled
#              framebuffer, the texture is not): the share of pixels that differ at all, the share that differ by more than 1 level, and the biggest difference in any channel
#              are each at most the REGRESSION GUARDS below (the largest value measured on this Mesa, 0.15 stage 2, plus about 1 %): these are guards, not an independent reference.
#   rebuilds   with the cache on, the texture is built ONCE in 40 frames (the log line PLPAGECACHE rebuild), never again; with PL_PAGE_CACHE=off, with Theme effects off, and for
#              Dark and Light it is never built.
#   same       Theme effects off, Dark and Light: the cached and the old pictures are IDENTICAL (0 differing pixels): the cache is not used there.
import os, re, struct, subprocess, sys

# REGRESSION GUARDS (not an independent reference): measured worst case of the 9 pictures on Mesa 25.2.8 llvmpipe (LLVM 20.1.2), 5 Oct 2026: 12.18 % differ at all,
# 0.770 % differ by more than 1 level, biggest difference 19 levels; the guards are those values plus about 1 % (the integer 19 stays 19).
GUARD_ANY, GUARD_OVER1, GUARD_MAX = 0.1230, 0.00778, 19

def run(binary, work, name, theme, extra):
    home = os.path.join(work, "home_" + name); os.makedirs(home, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if k != "XDG_DATA_HOME"}
    env.update(HOME=home, SDL_VIDEODRIVER="offscreen", PL_WINDOW="1000x640", PL_MOTION="on", PL_FIXED_DT="0.016", PL_DYK_FIRST="3", PL_DATE="2026-10-05",
               PL_EXIT_AFTER_FRAMES="40", PL_LOG_WINDOWS="1", PL_THEME=theme)
    env.update(extra)
    shot = os.path.join(work, name + ".bmp"); env["PL_SCREENSHOT"] = shot
    r = subprocess.run([binary], env=env, cwd=work, capture_output=True, text=True, timeout=300)
    return shot, len(re.findall(r"^PLPAGECACHE rebuild", r.stderr, re.M))

def load(path):
    d = open(path, "rb").read()
    off = struct.unpack("<I", d[10:14])[0]; w, h = struct.unpack("<ii", d[18:26]); bp = struct.unpack("<H", d[28:30])[0] // 8
    return w, abs(h), bp, (w * bp + 3) // 4 * 4, d[off:]

def compare(a_path, b_path):
    w, h, bp, st, a = load(a_path); _, _, _, _, b = load(b_path)
    any_, over1, mx = 0, 0, 0
    for y in range(h):
        ra, rb = a[y * st:y * st + w * bp], b[y * st:y * st + w * bp]
        if ra == rb: continue
        for x in range(w):
            o = x * bp
            d = max(abs(ra[o + k] - rb[o + k]) for k in range(3))
            if d: any_ += 1
            if d > 1: over1 += 1
            if d > mx: mx = d
    return any_ / (w * h), over1 / (w * h), mx

SCREENS = {"orbit": {}, "nucleus": {"PL_ATOM": "1", "PL_ATOM_DO": "nucleus"}, "chart": {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;view=Chart of nuclides"}}

def main():
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    fail = 0
    worst = {"any": 0.0, "over1": 0.0, "max": 0}
    for theme in ("aero", "y2kice", "y2knight"):
        for scr, extra in SCREENS.items():
            on, n_on = run(binary, work, f"{theme}_{scr}_on", theme, extra)
            off, n_off = run(binary, work, f"{theme}_{scr}_off", theme, dict(extra, PL_PAGE_CACHE="off"))
            any_, over1, mx = compare(on, off)
            for k, v in (("any", any_), ("over1", over1), ("max", mx)): worst[k] = max(worst[k], v)
            print(f"{theme} {scr}: differing {any_*100:.2f} %, over 1 level {over1*100:.3f} %, max {mx}; rebuilds on={n_on} off={n_off}")
            if n_on != 1: print(f"FAIL {theme} {scr}: the texture was built {n_on} times in 40 frames (want exactly 1)"); fail += 1
            if n_off != 0: print(f"FAIL {theme} {scr}: PL_PAGE_CACHE=off still built the texture"); fail += 1
            if any_ > GUARD_ANY: print(f"FAIL {theme} {scr}: {any_*100:.2f} % of the pixels differ (guard {GUARD_ANY*100:.2f} %)"); fail += 1
            if over1 > GUARD_OVER1: print(f"FAIL {theme} {scr}: {over1*100:.3f} % differ by more than 1 level (guard {GUARD_OVER1*100:.3f} %)"); fail += 1
            if mx > GUARD_MAX: print(f"FAIL {theme} {scr}: biggest difference {mx} (guard {GUARD_MAX})"); fail += 1
    print(f"worst measured: any {worst['any']*100:.2f} %, over 1 level {worst['over1']*100:.3f} %, max {worst['max']}")
    # effects off (Aero), Dark and Light: the cache is not used, the pictures are identical, nothing is built
    for theme, extra in (("aero", {"PL_EFFECTS": "off"}), ("dark", {}), ("light", {})):
        a, n_a = run(binary, work, f"{theme}_same_on", theme, extra)
        b, n_b = run(binary, work, f"{theme}_same_off", theme, dict(extra, PL_PAGE_CACHE="off"))
        any_, over1, mx = compare(a, b)
        print(f"{theme} {'effects off' if extra else ''}: differing {any_*100:.4f} %, rebuilds {n_a}/{n_b}")
        if any_ != 0 or n_a != 0 or n_b != 0: print(f"FAIL {theme}: cache must not be used here (differing {any_}, rebuilds {n_a} {n_b})"); fail += 1
    print(f"{fail} problems")
    sys.exit(1 if fail else 0)

main()
