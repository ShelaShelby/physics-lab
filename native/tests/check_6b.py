#!/usr/bin/env python3
# 0.16 stage 2: four fixes, checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU).
#   scrollbar: About closed and opened again still shows its scrollbar thumb (pixels, not the app's own rects) - was hidden behind the reopened page
#   bench:     Benchmark window with both extra-scene boxes ticked stays inside 92 % of the window height, Start in view
#   rail:      the About Contents rail is wide enough for its labels (>= 20 em)
#   nucleus:   Quark style / Nucleon detail / Nucleon edge can be clicked at the default zoom
#   0.56.1, the benchmark (its only-scene switch PL_BENCH_ONLY):
#   benchscenes:  every new scene sets up what its text says, and its picture changes between two moments of its counted time
#   benchwaits:   a wait is timed to the finished picture, not the first frame (the cloud filling, the spinning hole's sharp picture)
#   benchrest:    a lab made busy on purpose (PL_BENCH_BUSY) is said to be busy at rest
#   benchrestore: after the run every lab, page and panel is as it was
#   check_6b.py <PhysicsLab binary> <work folder> scrollbar|bench|rail|nucleus|benchscenes|benchwaits|benchrest|benchrestore
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
    r = run_app(binary, work, "b_nu", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_SCRIPT": "10:click=atom.style=Nucleons;18:click=atom.qstyle=Simple;26:click=atom.detail=+ Sea;34:astate"}, frames=40, size="1400x900")
    check(not r.scripted_missing(), f"Quark style, Nucleon detail and Nucleon edge are in the Controls panel at the default zoom {r.scripted_missing()}")
    check(any("These show in the picture when zoomed in close" in t for _, _, t in r.texts), "the grey caption under the Nucleus section is there")
    st = [ln for ln in r.out.splitlines() if ln.startswith("PLASTATE ")]
    check(bool(st) and "qstyle=Simple" in st[-1] and "detail=+ Sea" in st[-1] and "style=Nucleons" in st[-1], "the three choices were set by the clicks")


BENCH = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0", "PL_BENCH_FINISH": "1"}   # (FINISH: the software renderer here would queue frames for seconds)

def report(r):
    out = r.out
    a, b = out.find("PLBENCH-RESULT-BEGIN"), out.find("PLBENCH-RESULT-END")
    return out[a:b] if a >= 0 and b > a else ""

def changed(p1, p2, w, h):
    # the share of pixels that differ by more than 2 in a channel, outside the benchmark's running box (top left): a turning glow cloud changes softly
    try:
        a, b = Bmp(p1), Bmp(p2)
    except Exception:
        return -1
    n = m = 0
    for y in range(0, h, 4):
        for x in range(0, w, 4):
            if x < 0.45 * w and y < 0.22 * h:
                continue
            n += 1
            if max(abs(c - d) for c, d in zip(a.px(x, y), b.px(x, y))) > 2:
                m += 1
    return m / max(1, n)

# what each new scene's report must say it set up (its own line), and whether its picture moves
SCENES = {
    "orbit.saturn": "Orbit Lab: zoom 4.10", "orbit.earth": "Orbit Lab: zoom 5.50", "orbit.real_fast": "the belts' rocks", "orbit.sky": "Orbit Lab: zoom 0.00",
    "orbit.journey": "Journey", "atom.all_enlarged": "Og-294, Cloud 3D, every electron, Whole atom, Shells enlarged, Glow, 240000 dots",
    "atom.all_true_zoomed": "Og-294, Cloud 3D, every electron, Whole atom, True scale, Glow, 240000 dots",
    "atom.all_one_orbital": "Og-294, Cloud 3D, every electron, One orbital 5f", "atom.all_classic": "Og-294, Cloud 3D, every electron, Whole atom, Shells enlarged, Classic, 240000 dots",
    "atom.jump_all": "Ra-226, Cloud 3D, every electron, Whole atom, Shells enlarged, Glow, 240000 dots", "atom.nucleus_density": "Og-294, Orbit, Density",
    "molecule.density": "Molecule Lab: C6H6 (12 atoms)", "molecule.vib_density": "Molecule Lab: H2O (3 atoms), zoom x1.60, vibration mode 1",
    "molecule.spacefill_vib": "Molecule Lab: C6H6 (12 atoms), zoom x4.00, vibration mode 1", "blackhole.film_tilt": "distance 74.1 GM/c², field of view 30", "blackhole.extreme": "Black Hole Lab: distance 20.0",
    "ui.guide_scroll": "Learn guide 0:3", "ui.data_plot": "Data panel over the", "ui.settings_open": "the belts' rocks",
}

def part_benchscenes(binary, work):
    ids = ",".join(SCENES)
    r = run_app(binary, work, "b_scenes", dict(BENCH, PL_BENCH_ONLY=ids, PL_BENCH_WARMUP="0.3", PL_BENCH_MEASURE="1.6", PL_BENCH_SHOT="0.1,1.0"), frames=100000, size="880x560", timeout=2400)
    t = report(r)
    check(bool(t), "the benchmark ran the new scenes and gave its report")
    w, h = 880, 560
    for sid, want in SCENES.items():
        d = t[t.find("DETAILS"):]
        sec = d[d.find("\n" + sid + "  "):] if ("\n" + sid + "  ") in d else ""
        sec = sec[:sec.find("  GPU time")] if "  GPU time" in sec else sec
        check(want in sec, f"{sid}: the report says it set up {want!r}")
        frac = changed(os.path.join(work, sid + ".bmp"), os.path.join(work, sid + "_2.bmp"), w, h)
        check(frac > 0.0005, f"{sid}: the picture changes from one moment to the next ({frac:.4f} of the pixels)")
        print(f"  {sid}: {frac:.4f} of the pixels changed")
    check("SCENES, slowest first" in t and t.find("SCENES, slowest first") < t.find("DETAILS"), "the summary table comes first")

def part_benchwaits(binary, work):
    # (PL_FRAME_MS: frames paced as VSync paces them on a screen; the session box has none, and the frame loop would take the cores the workers need)
    r = run_app(binary, work, "b_waits", dict(BENCH, PL_BENCH_ONLY="response.atom_density,response.blackhole_spin", PL_FRAME_MS="16"), frames=100000, size="880x560", timeout=1200)
    ln = [l.split() for l in r.out.splitlines() if l.startswith("PLBENCH-WAIT ")]
    for wid in ("response.atom_density", "response.blackhole_spin"):
        tries = [l for l in ln if l[1] == wid]
        check(len(tries) == 3, f"{wid}: three tries ({len(tries)})")
        for l in tries:
            frames = int(l[4].split("=")[1])
            check(frames >= 2 and "(capped)" not in " ".join(l), f"{wid}: timed to the finished picture over {frames} frames, not the first frame ({' '.join(l)})")
    t = report(r)
    check("WAITS (each done 3 times" in t and "response.atom_density" in t, "the report has the waits table")

def part_benchrest(binary, work):
    r = run_app(binary, work, "b_rest", dict(BENCH, PL_BENCH_ONLY="rest.molecule", PL_BENCH_BUSY="1"), frames=100000, size="880x560", timeout=600)
    ln = [l for l in r.out.splitlines() if l.startswith("PLBENCH-REST ")]
    check(len(ln) == 1 and ln[0].split()[2] == "busy", f"a lab made busy on purpose is said to be busy ({ln})")
    t = report(r)
    check("AT REST" in t and "rest.molecule  busy" in t, "the report's rest table says busy")

def part_benchrestore(binary, work):
    ids = "ui.settings_open,ui.data_plot,ui.guide_scroll,molecule.vib_density,atom.jump_all,orbit.journey,orbit.earth,blackhole.extreme,response.lab_switch.orbit_atom,rest.blackhole_glare"
    r = run_app(binary, work, "b_restore", dict(BENCH, PL_BENCH_ONLY=ids, PL_BENCH_WARMUP="0.2", PL_BENCH_MEASURE="0.4", PL_OPEN="settings"), frames=100000, size="880x560", timeout=1200)
    st = {l.split()[1]: l.split(" ", 2)[2] for l in r.out.splitlines() if l.startswith("PLBENCH-STATE ")}
    check("before" in st and "after" in st, "the state was printed before and after")
    if "before" in st and "after" in st:
        check(st["before"] == st["after"], f"every lab, page and panel is as it was:\n    before {st['before']}\n    after  {st['after']}")
        check("settings=1" in st["after"], "the Settings page that was open is open again")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"scrollbar": part_scrollbar, "bench": part_bench, "rail": part_rail, "nucleus": part_nucleus, "benchscenes": part_benchscenes, "benchwaits": part_benchwaits,
     "benchrest": part_benchrest, "benchrestore": part_benchrestore}[part](binary, work)
    print(f"check_6b {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
