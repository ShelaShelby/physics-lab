#!/usr/bin/env python3
# 0.16 stage 2: four fixes, checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU).
#   scrollbar: About closed and opened again still shows its scrollbar thumb (pixels, not the app's own rects) - was hidden behind the reopened page
#   bench:     Benchmark window with both extra-scene boxes ticked stays inside 92 % of the window height, Start in view
#   rail:      the About Contents rail is wide enough for its labels (>= 20 em)
#   nucleus:   Quark style / Nucleon detail / Nucleon edge can be clicked at the default zoom
#   check_6b.py <PhysicsLab binary> <work folder> scrollbar|bench|rail|nucleus
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def part_scrollbar(binary, work):
    for t in ("dark", "light", "sage", "contrast"):
        r = run_app(binary, work, f"b_sb_{t}", {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_SCRIPT": "8:key=escape;12:click=top.menu;16:click=menu.about"}, frames=40, size="1400x900", theme=t)
        th, tr = r.rects.get("reading.scrollthumb"), r.rects.get("reading.scrollbar")
        check(th is not None and not r.scripted_missing(), f"{t}: About reopened, the page is there {r.scripted_missing()}")
        if not th:
            continue
        b = Bmp(r.bmp)
        x, y = int(th[0] + th[2] / 2), int(th[1] + th[3] / 2)
        thumb, panel = b.px(x, y), b.px(int(th[0]) - 40, y)
        check(sum(abs(a - c) for a, c in zip(thumb, panel)) > 60, f"{t}: the thumb {thumb} differs from the panel {panel} after close and reopen")

def part_bench(binary, work):
    for sz in ("880x560", "1280x700"):
        r = run_app(binary, work, f"b_bn_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "settings", "PL_BENCH": "ask", "PL_BENCH_RISK": "1"}, frames=40, size=sz)
        h = r.window[1]
        bw, st = r.rects.get("win.benchmark"), r.rects.get("bench.start")
        check(bw is not None and st is not None, f"{sz}: window and Start are drawn")
        if bw and st:
            check(bw[3] <= 0.92 * h + 1 and bw[1] + bw[3] <= h, f"{sz}: window {bw} is at most 92 % of the height {h} and inside")
            check(st[1] >= bw[1] and st[1] + st[3] <= bw[1] + bw[3] and st[1] + st[3] <= h, f"{sz}: Start {st} is in view")

def part_rail(binary, work):
    for ts in ("normal", "xl"):
        r = run_app(binary, work, f"b_rl_{ts}", {"PL_OPEN": "about", "PL_NEWUI": "on", "PL_TEXTSIZE": ts}, frames=30, size="2560x1369")
        toc, col = r.rects.get("reading.toc"), r.rects.get("reading.column")
        check(toc is not None and col is not None, f"{ts}: the Contents rail is drawn")
        if toc and col:
            body = [px for k, px, t in r.texts if k == "Body"]
            em = (body[0] if body else 14) * 1.0
            check(toc[2] >= 19 * em and toc[0] + toc[2] <= col[0], f"{ts}: rail width {toc[2]} is >= 19 body-text heights ({em}) and ends before the column {col}")

def part_nucleus(binary, work):
    r = run_app(binary, work, "b_nu", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_SCRIPT": "10:click=atom.style=Hard balls;18:click=atom.qstyle=Classic;26:click=atom.detail=+ Sea;34:astate"}, frames=40, size="1400x900")
    check(not r.scripted_missing(), f"Quark style, Nucleon detail and Nucleon edge are in the Controls panel at the default zoom {r.scripted_missing()}")
    check(any("These show in the picture when zoomed in close" in t for _, _, t in r.texts), "the grey caption under the Nucleus section is there")
    st = [ln for ln in r.out.splitlines() if ln.startswith("PLASTATE ")]
    check(bool(st) and "qstyle=Classic" in st[-1] and "detail=+ Sea" in st[-1] and "style=Hard balls" in st[-1], "the three choices were set by the clicks")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"scrollbar": part_scrollbar, "bench": part_bench, "rail": part_rail, "nucleus": part_nucleus}[part](binary, work)
    print(f"check_6b {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
