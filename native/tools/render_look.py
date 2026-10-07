#!/usr/bin/env python3
# 0.15 stage 5i: renders the five look-option screens (PL_LOOK a|b|c) at 1280x800, Dark, with the real app (Linux, software OpenGL) into native/docs/look/.
#   render_look.py <PhysicsLab binary> <work folder> <out folder>
# (1) Orbit, Controls panel at "Pick a body"  (2) Orbit, subject card open + toolbar  (3) Atom Cloud 3D U-238, card open, panel closed  (4) Atom data sheet, Spectrum tab, U-238
# (5) the top bar + breadcrumbs + chip, cropped from (2), twice the size.  Then look_compare.png: screens 2 and 3 side by side, columns A | B | C.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests"))
from shell_lib import run_app, prefetch
from PIL import Image

ATOM = {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;view=Cloud 3D;orb=2p"}
SCREENS = {
    1: ({"PL_SCRIPT": "10:click=ctl.sec.pick"}, 40),
    2: ({"PL_SELECT": "Earth"}, 60),
    3: (dict(ATOM, PL_SCRIPT="8:click=ctl.handle"), 60),
    4: (dict(ATOM, PL_SCRIPT="10:click=card.all;16:click=data.tab.spectrum"), 50),
}

def png(im, path, colours=96):
    im.convert("RGB").quantize(colours, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(path, optimize=True)

def main(binary, work, out):
    os.makedirs(out, exist_ok=True)
    calls = [((binary, work, f"lk_{o}_{n}", dict(env, PL_NEWUI="on", PL_LOOK=o)), {"frames": fr}) for o in "abc" for n, (env, fr) in SCREENS.items()]
    prefetch(calls)
    ims = {}
    for o in "abc":
        for n, (env, fr) in SCREENS.items():
            r = run_app(binary, work, f"lk_{o}_{n}", dict(env, PL_NEWUI="on", PL_LOOK=o), frames=fr)
            im = Image.open(r.bmp).convert("RGB")
            ims[(o, n)] = im
            png(im, os.path.join(out, f"{o}_{n}.png"))
        crop = ims[(o, 2)].crop((0, 0, 760, 130)).resize((1520, 260), Image.LANCZOS)
        png(crop, os.path.join(out, f"{o}_5.png"))
    w, h = 1280 // 2, 800 // 2                       # half size: three columns of 640 x 400, two rows
    sheet = Image.new("RGB", (3 * w + 4 * 8, 2 * h + 3 * 8), (24, 24, 24))
    for i, o in enumerate("abc"):
        for j, n in enumerate((2, 3)):
            sheet.paste(ims[(o, n)].resize((w, h), Image.LANCZOS), (8 + i * (w + 8), 8 + j * (h + 8)))
    png(sheet, os.path.join(out, "look_compare.png"), 160)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
