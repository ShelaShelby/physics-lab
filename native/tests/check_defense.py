#!/usr/bin/env python3
# 0.17 stage 1 (stage 2: save version 2, the core arrow): the Defense Lab in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU, NO timing: counts only).
#   Renders the small sample base (seed 20261011, 2,400 steps = 40 s of play) in two themes and saves the pictures to look at (defense_dark.bmp, defense_light.bmp);
#   checks the draw counts (terrain one mesh a chunk, belts and buildings batched), the build bar, the core where it should be;
#   then: the game saves when the app closes and the next start continues from it; a damaged save file gives a message (no crash) and a new game can be started.
#   check_defense.py <PhysicsLab binary> <work folder>
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, inside

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def stats(r):
    return [dict((k, int(x)) for k, x in re.findall(r"(\w+)=(\d+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLDEFSTAT ")]

def main(binary, work):
    base = {"PL_NEWUI": "on", "PL_DEFENSE": "1", "PL_DEFENSE_SEED": "20261011", "PL_DEFENSE_SAMPLE": "small", "PL_DEFENSE_TICKS": "2400", "PL_DRAWSTAT": "1"}
    for theme in ("dark", "light"):
        save = os.path.join(work, f"save_{theme}.txt")
        if os.path.exists(save):
            os.remove(save)
        r = run_app(binary, work, f"defense_{theme}", dict(base, PL_DEFENSE_SAVE=save), frames=30, theme=theme)
        st = stats(r)
        check(len(st) >= 20, f"{theme}: the draw counts were printed")
        if not st:
            continue
        v = st[-1]
        print(f"{theme}: {v}")
        check(v["entities"] == 32, f"{theme}: the sample base has 32 buildings and belts with the core ({v['entities']})")
        check(v["chunks"] >= 1 and v["calls"] <= v["chunks"] + 20, f"{theme}: terrain one draw call a chunk, everything else batched ({v['calls']} calls, {v['chunks']} chunks)")
        check(v["items"] > 0, f"{theme}: items are on the belts")
        bar = r.rects.get("defensebar")
        check(bar is not None and inside(bar, 1280, 800), f"{theme}: the build bar is on screen")
        check(any(n.startswith("defense tool=none") for n in r.notes), f"{theme}: no building chosen at the start")
        b = Bmp(r.bmp)
        px = b.px(652, 406)                                          # the core's lit centre (camera on tile 0, 24 points a tile)
        check(px[2] > 200 and px[1] > 180, f"{theme}: the core is drawn in the middle {px}")
        check(os.path.exists(save) and open(save).read().startswith("PhysicsLabDefense 2"), f"{theme}: the planet was saved when the app closed")
    # stage 2: looking far from the core (tile 120, 80): the arrow to the core is shown; on the core it is not
    save = os.path.join(work, "save_arrow.txt")
    if os.path.exists(save):
        os.remove(save)
    r = run_app(binary, work, "defense_arrow", dict(base, PL_DEFENSE_SAVE=save, PL_DEFENSE_CAM="120,80"), frames=12)
    st = stats(r)
    check(bool(st) and st[-1].get("arrow") == 1, "far from the core: the arrow to the core is shown")
    check(any(s.get("arrow") == 0 for s in stats(run_app(binary, work, "defense_noarrow", dict(base, PL_DEFENSE_SAVE=save + "x"), frames=6))), "on the core: no arrow")
    # the next start continues from the save (nothing ran while it was closed)
    save = os.path.join(work, "save_dark.txt")
    tick0 = int(re.search(r"^tick (\d+)$", open(save).read(), re.M).group(1))
    r = run_app(binary, work, "defense_again", dict(base, PL_DEFENSE_SAVE=save, PL_DEFENSE_SAMPLE="", PL_DEFENSE_TICKS="0"), frames=10)
    st = stats(r)
    check(bool(st) and st[-1]["entities"] == 32, "the second start has the saved base")
    tick1 = int(re.search(r"^tick (\d+)$", open(save).read(), re.M).group(1))
    check(tick0 >= 2400 and tick0 <= tick1 <= tick0 + 30, f"the second start went on from the saved moment (tick {tick0} -> {tick1})")
    # a damaged save: a message, no crash, the damaged file kept
    bad = os.path.join(work, "save_bad.txt")
    open(bad, "w").write("PhysicsLabDefense 2\nseed 5\ntick 1\ncamera 0 0 24\nnextid 2\ne 1 1 -1 -1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\nend 0000000000000000\n")
    r = run_app(binary, work, "defense_bad", dict(base, PL_DEFENSE_SAVE=bad, PL_DEFENSE_SAMPLE=""), frames=10)
    check(r.bmp and os.path.exists(r.bmp), "a damaged save: the app ran and drew (no crash)")
    check(any(n.startswith("defense loaderror") for n in r.notes), "a damaged save: the message is shown")
    check(os.path.exists(bad + ".damaged"), "a damaged save: the file is kept as .damaged")
    r = run_app(binary, work, "defense_bad_new", dict(base, PL_DEFENSE_SAVE=bad, PL_DEFENSE_SAMPLE="", PL_SCRIPT="5:click=defense.newgame.error"), frames=12)
    check(open(bad).read().startswith("PhysicsLabDefense 2") and "checksum" not in open(bad).read() and stats(r) and stats(r)[-1]["entities"] == 1,
          "a damaged save: Start a new game gives a fresh planet with only the core")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    main(binary, work)
    print(f"check_defense: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
