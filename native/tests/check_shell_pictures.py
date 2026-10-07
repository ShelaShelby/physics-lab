#!/usr/bin/env python3
# 0.15 stage 3: pictures of the new UI in the REAL app (Linux, headless on Mesa llvmpipe; not Windows, not a GPU):
#   check_shell_pictures.py <PhysicsLab binary> <work folder> [<tests/reference folder>]      (add --write to make the guard file instead of checking it)
#   glass   in the 9 plain themes (10 before 0.15 stage 5m), a floating panel's pixels are the theme's card colour at the glass opacity of DESIGN.md section 3 over what is behind it: the picture
#           with the panel closed gives "behind", the picture with it open must equal  card * a + behind * (1 - a)  within 2 levels (8-bit rounding), in the strip at the
#           bottom right of the Time speed panel (no text, no shine). The card colours and the opacities are written HERE from the DESIGN.md table (computed by
#           docs/ui-0.15/tools/glass_alpha.py, a separate program): so this fails if the drawing ever uses another colour or opacity.
#   guard   the pictures of the new shell, Orbit Lab, Dark / Light, 1280 x 800 and 1000 x 640, Earth selected: closed (the card, the legend, the fact line), with the
#           Time panel open, with the Show panel open, with the card's All values (the data sheet), with search open and with About in the reading view: their SHA-256 must equal
#           tests/reference/shell_pictures_0.15s4.sha256. THESE ARE REGRESSION GUARDS, NOT AN INDEPENDENT REFERENCE: they were made by the stage 4 code on Mesa 25.2.8 llvmpipe
#           (LLVM 20.1.2) AFTER the pictures had been looked at once (the stage 3 file, shell_pictures_0.15s3.sha256, is gone: every one of those pictures changed on purpose, with the
#           new chip, toolbar, card and fact line). A different Mesa may round differently.
import hashlib, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, prefetch

# (card colour, opacity) of DESIGN.md section 3. 0.15 stage 5m (on purpose): Dark is drawn with the look M1 now (always on in the New UI): its card #17171a, SOLID (opacity 1.0, app/look_options.cpp, the owner's choice) replaces DESIGN.md's #1e1e21 / 0.96
GLASS = {"light": ("#ffffff", 0.97), "dark": ("#17171a", 1.0), "black": ("#0b0b0c", 0.93), "midnight": ("#111c38", 0.95), "solar": ("#271d15", 0.97),
         "paper": ("#fbf6ea", 0.97), "sage": ("#16211e", 0.95), "contrast": ("#000000", 0.70), "monochrome": ("#ffffff", 0.87)}

def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

SCREENS = {"closed": {"PL_SELECT": "Earth"}, "time": {"PL_SELECT": "Earth", "PL_SCRIPT": "10:click=top.chip"}, "nocontrols": {"PL_SELECT": "Earth", "PL_SCRIPT": "10:click=ctl.close"},
           "sheet": {"PL_SELECT": "Earth", "PL_SCRIPT": "10:click=card.all"}, "search": {"PL_SELECT": "Earth", "PL_SCRIPT": "10:key=ctrl+k;16:text=moo"}, "about": {"PL_OPEN": "about"}}
SIZES = ("1280x800", "1000x640")
THEMES = ("dark", "light")   # 0.15 stage 5m: "aero" and "y2knight" were archived with the themes

def main():
    args = [a for a in sys.argv[1:] if a != "--write"]
    write = "--write" in sys.argv
    binary, work = args[0], args[1]
    ref = os.path.join(args[2] if len(args) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "reference"), "shell_pictures_0.15s4.sha256")
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    prefetch([((binary, work, f"glass_{t}_closed", {"PL_NEWUI": "on"}), dict(frames=40, size="1000x640", theme=t)) for t in GLASS] +
             [((binary, work, f"glass_{t}_open", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.chip"}), dict(frames=40, size="1000x640", theme=t)) for t in GLASS] +
             [((binary, work, f"shell_{scr}_{t}_{sz}", dict({"PL_NEWUI": "on"}, **extra)), dict(frames=40, size=sz, theme=t)) for t in THEMES for sz in SIZES for scr, extra in SCREENS.items()])
    # ---- glass ----
    for theme, (card, a) in GLASS.items():
        base = run_app(binary, work, f"glass_{theme}_closed", {"PL_NEWUI": "on"}, frames=40, size="1000x640", theme=theme)
        open_ = run_app(binary, work, f"glass_{theme}_open", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.chip"}, frames=40, size="1000x640", theme=theme)
        r = open_.rects.get("panel.chip")
        check(r is not None, f"{theme}: the Time panel is open")
        if r is None:
            continue
        b0, b1 = Bmp(base.bmp), Bmp(open_.bmp)
        c = rgb(card)
        worst = 0
        n = 0
        for y in range(int(r[1] + r[3] - 12), int(r[1] + r[3] - 5)):
            for x in range(int(r[0] + r[2] * 0.6), int(r[0] + r[2] - 24)):
                behind, got = b0.px(x, y), b1.px(x, y)
                want = tuple(c[k] * a + behind[k] * (1 - a) for k in range(3))
                worst = max(worst, max(abs(got[k] - want[k]) for k in range(3)))
                n += 1
        check(n > 500, f"{theme}: enough pixels sampled ({n})")
        check(worst <= 2.5, f"{theme}: the panel is the card colour {card} at opacity {a} over what is behind it (worst difference {worst:.2f} levels)")
        print(f"glass {theme}: worst difference {worst:.2f} of 255 over {n} pixels")
    # ---- guards ----
    got = {}
    for theme in THEMES:
        for size in SIZES:
            for scr, extra in SCREENS.items():
                name = f"shell_{scr}_{theme}_{size}"
                res = run_app(binary, work, name, dict({"PL_NEWUI": "on"}, **extra), frames=40, size=size, theme=theme)
                d = open(res.bmp, "rb").read()
                got[name] = hashlib.sha256(d[54:]).hexdigest()
    if write:
        with open(ref, "w") as f:
            for k in sorted(got):
                f.write(f"{got[k]}  {k}\n")
        print("wrote", ref, len(got), "pictures")
        return 0
    want = {}
    for ln in open(ref):
        if ln.strip():
            h, k = ln.split()
            want[k] = h
    check(set(want) == set(got), "the guard file lists the same pictures")
    for k in sorted(got):
        check(want.get(k) == got[k], f"DIFFERENT PICTURE (a regression guard, see the note at the top): {k}")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
