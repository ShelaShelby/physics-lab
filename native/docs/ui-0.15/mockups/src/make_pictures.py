#!/usr/bin/env python3
"""0.15 stage 1, throwaway mockup code (not part of the app): the lab pictures of the mockups are made by the REAL app (the scratch probe build of
tools/README.md, headless) with Save image, at the exact size a layout gives the picture. usage: make_pictures.py <probe PhysicsLab> <out dir>
The status line and legend of the chart are left out (PL_MOCK_CLEAN, in the scratch copy only): the mockups draw those themselves."""
import os, subprocess, sys, glob, shutil, tempfile
BIN, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
SIZES = {"L_A": (832, 744), "L_BC": (1280, 744), "S_A": (616, 584), "S_BC": (1000, 584), "L_B": (1280, 496), "S_B": (1000, 360)}   # picture size per layout and window (large 1280x800, small 1000x640)
LABS = {"orbit": {}, "chart": {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;view=Chart of nuclides", "PL_MOCK_CLEAN": "1"}}
def probe(path, key):
    for line in open(path, encoding="utf-8", errors="replace"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "W" and f[1] == key: return float(f[5])
    return 0.0
def run(lab, theme, pw, ph, extra_h, tag):
    d = tempfile.mkdtemp(prefix="pic", dir=OUT); home = os.path.join(d, "home"); os.makedirs(home)
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="offscreen", PL_MOTION="on", PL_FIXED_DT="0.016", PL_DATE="2026-10-05", PL_THEME=theme,
               PL_WINDOW=f"{pw + 360}x{ph}", PL_SHOT_DIR=d, PL_SAVE_IMAGE="30", PL_EXIT_AFTER_FRAMES="40", PL_PROBE=os.path.join(d, "p.txt"), **LABS[lab])
    subprocess.run([BIN], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    pngs = glob.glob(os.path.join(d, "*.png"))
    return d, pngs[0] if pngs else None
def png_size(p):
    out = subprocess.check_output(["identify", "-format", "%w %h", p]).decode().split(); return int(out[0]), int(out[1])
for theme in ("dark", "aero"):
    for lab in LABS:
        for key, (w, h) in SIZES.items():
            H = h + 260                                               # first guess: header + the Atom Lab's bar
            for attempt in range(4):
                d, p = run(lab, theme, w, H, 0, key)
                pw_, ph_ = png_size(p)
                if (pw_, ph_) == (w, h): break
                H += h - ph_                                          # the picture is (window height - header - bar) high: move by the difference
            else:
                sys.exit(f"could not get {w}x{h} for {lab} {theme}: got {pw_}x{ph_}")
            dst = os.path.join(OUT, f"{lab}_{theme}_{key}.png"); shutil.copy(p, dst)
            print(dst, w, h, "window height", H)
