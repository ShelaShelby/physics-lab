#!/usr/bin/env python3
# 0.15 stage 5i/5j: renders the five look-option screens (PL_LOOK m1|m2|m3) at 1280x800, Dark, with the real app (Linux, software OpenGL) into native/docs/look/.
#   render_look.py <PhysicsLab binary> <work folder> <out folder>
# (1) Orbit, Controls panel at "Pick a body"  (2) Orbit, subject card open + toolbar  (3) Atom Cloud 3D U-238, card open, panel closed  (4) Atom data sheet, Spectrum tab, U-238
# (5) the top bar + breadcrumbs + chip, cropped from (2), twice the size.  Then look_compare.png: screens 1 and 4, four columns TODAY | M1 | M2 | M3.
# (5) = the top bar (from 2) over the top of the Controls panel (from 1), both at 2x.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests"))
from shell_lib import run_app, prefetch
from PIL import Image
LOOKS = ("m1", "m2", "m3")

ATOM = {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;view=Cloud 3D;orb=2p"}
SCREENS = {
    1: ({"PL_SCRIPT": "10:click=ctl.sec.pick"}, 40),
    2: ({"PL_SELECT": "Earth"}, 60),
    3: (dict(ATOM, PL_SCRIPT="8:click=ctl.handle"), 60),
    4: (dict(ATOM, PL_SCRIPT="10:click=card.all;16:click=data.tab.spectrum"), 50),
}

def png(im, path, colours=None):
    im.convert("RGB").save(path, optimize=True)      # stage 5j.1: full colour (a palette washed out the small coloured dots); `colours` is kept only so old calls still work

def main(binary, work, out):
    os.makedirs(out, exist_ok=True)
    names = ("today",) + LOOKS                         # "today": PL_LOOK empty = the app as it is
    def args(o, n, env, fr):
        return ((binary, work, f"lk_{o}_{n}", dict(env, PL_NEWUI="on", PL_LOOK="" if o == "today" else o)), {"frames": fr})
    prefetch([args(o, n, env, fr) for o in names for n, (env, fr) in SCREENS.items() if o != "today" or n in (1, 4)])
    ims = {}
    for o in names:
        for n, (env, fr) in SCREENS.items():
            if o == "today" and n not in (1, 4):
                continue
            a = args(o, n, env, fr)
            r = run_app(*a[0], frames=fr)
            im = Image.open(r.bmp).convert("RGB")
            ims[(o, n)] = im
            if o != "today":
                png(im, os.path.join(out, f"{o}_{n}.png"))
        if o != "today":
            crop = ims[(o, 2)].crop((0, 0, 760, 130)).resize((1520, 260), Image.LANCZOS)   # the top bar at 2x (screen 5)
            png(crop, os.path.join(out, f"{o}_5.png"))
    # screen 5 of the brief: the top bar + the top part of the Controls panel at 2x
    for o in LOOKS:
        bar = ims[(o, 2)].crop((0, 0, 760, 130)).resize((1520, 260), Image.LANCZOS)
        atom = ims[(o, 4)].crop((930, 52, 1280, 380)).resize((700, 656), Image.LANCZOS)       # the Atom Controls panel: segmented pills, check boxes, section headers
        pick = ims[(o, 1)].crop((930, 52, 1280, 380)).resize((700, 656), Image.LANCZOS)       # the Orbit panel at "Pick a body": the coloured dots, the filter box
        im = Image.new("RGB", (1520, 260 + 656), (0, 0, 0))
        im.paste(bar, (0, 0)); im.paste(atom, (0, 260)); im.paste(pick, (720, 260))
        png(im, os.path.join(out, f"{o}_5.png"))
    w, h = 480, 300
    sheet = Image.new("RGB", (4 * w + 5 * 8, 2 * h + 3 * 8), (24, 24, 24))
    for i, o in enumerate(names):
        for j, n in enumerate((1, 4)):
            sheet.paste(ims[(o, n)].resize((w, h), Image.LANCZOS), (8 + i * (w + 8), 8 + j * (h + 8)))
    png(sheet, os.path.join(out, "look_compare.png"), 160)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
