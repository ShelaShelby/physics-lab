#!/usr/bin/env python3
# 0.15 stage 3: pictures of the new UI in the REAL app (Linux, headless on Mesa llvmpipe; not Windows, not a GPU):
#   check_shell_pictures.py <PhysicsLab binary> <work folder>
#   glass   in the 9 plain themes, a floating panel's pixels are the theme's card colour at the glass opacity of DESIGN.md section 3 over what is behind it: the picture
#           with the panel closed gives "behind", the picture with it open must equal  card * a + behind * (1 - a)  within 2 levels (8-bit rounding), in the strip at the
#           bottom right of the Time speed panel (no text, no shine). The card colours and the opacities are written HERE from the DESIGN.md table (computed in 0.15 by a
#           separate program, glass_alpha.py, now in git history): so this fails if the drawing ever uses another colour or opacity.
#   (0.22: the 24 SHA-256 picture guards are gone: any visual change broke all of them and they were then rewritten unseen, so they guarded nothing.)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, prefetch

# (card colour, opacity) of DESIGN.md section 3. 0.15 stage 5m (on purpose): Dark is drawn with the look M1 now (always on in the New UI): its card #17171a, SOLID (opacity 1.0, app/look_options.cpp, the owner's choice) replaces DESIGN.md's #1e1e21 / 0.96
GLASS = {"light": ("#ffffff", 0.97), "dark": ("#17171a", 1.0), "black": ("#0b0b0c", 0.93), "midnight": ("#111c38", 0.95), "solar": ("#271d15", 0.97),
         "paper": ("#fbf6ea", 0.97), "sage": ("#16211e", 0.95), "contrast": ("#000000", 0.70), "monochrome": ("#ffffff", 0.87)}

def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

def main():
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    prefetch([((binary, work, f"glass_{t}_closed", {"PL_NEWUI": "on"}), dict(frames=40, size="1000x640", theme=t)) for t in GLASS] +
             [((binary, work, f"glass_{t}_open", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.chip"}), dict(frames=40, size="1000x640", theme=t)) for t in GLASS])
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
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
