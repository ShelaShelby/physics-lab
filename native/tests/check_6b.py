#!/usr/bin/env python3
# 0.16 stage 2 and 0.56.1: the Benchmark window, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real GPU). tests/TESTS.md lists the parts kept and why.
#   bench:        the Benchmark window with both extra-scene boxes ticked stays inside 92 % of the window height, Start in view
#   benchrest:    a lab made busy on purpose (PL_BENCH_BUSY) is said to be busy at rest
#   benchporkchop: 0.94.1, the owner's 0.93.0 run: the two Orbit Lab waits after orbit.porkchop finish (they waited for a grid the closed Data panel never took)
#   benchrestore: after the run every lab, page and panel is as it was (0.83.0: and the window ui.resize_blackhole resized)
#   check_6b.py <PhysicsLab binary> <work folder> bench|benchrest|benchrestore
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def part_bench(binary, work):
    for sz in ("880x560", "1280x700"):
        r = run_app(binary, work, f"b_bn_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "settings", "PL_BENCH": "ask", "PL_BENCH_RISK": "1"}, frames=40, size=sz)
        h = r.window[1]
        bw, st = r.rects.get("win.benchmark"), r.rects.get("bench.start")
        check(bw is not None and st is not None, f"{sz}: window and Start are drawn")
        if bw and st:
            check(bw[3] <= 0.92 * h + 1 and bw[1] + bw[3] <= h, f"{sz}: window {bw} is at most 92 % of the height {h} and inside")
            check(st[1] >= bw[1] and st[1] + st[3] <= bw[1] + bw[3] and st[1] + st[3] <= h, f"{sz}: Start {st} is in view")

BENCH = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0", "PL_BENCH_FINISH": "1"}   # (FINISH: the software renderer here would queue frames for seconds)

def report(r):
    out = r.out
    a, b = out.find("PLBENCH-RESULT-BEGIN"), out.find("PLBENCH-RESULT-END")
    return out[a:b] if a >= 0 and b > a else ""

# what each new scene's report must say it set up (its own line), and whether its picture moves (0.72.0: blackhole.extreme starts from the lab's starting view,
# 50 GM/c^2 since 0.70.0: the reference said 20, the start until 0.69; a changed reference, failing since 0.70.0, REPORT 0.72.0)
SCENES = {
    "atom.slice_all_drag": "Og-294, Slice, every electron, Whole atom, Shells enlarged, Classic, 240000 dots", "atom.slice_dirac_drag": "U-238, Slice, one electron, 4f, Classic, 48000 dots",
    "atom.side_all_glow": "Og-294, Cloud, every electron, Whole atom, Shells enlarged, Glow, 240000 dots", "atom.side_all_classic": "Og-294, Cloud, every electron, Whole atom, Shells enlarged, Classic, 240000 dots",
    "atom.slice_glow_ultra": "U-238, Slice, one electron, 4f, Glow, 48000 dots", "atom.slice_classic_ultra": "U-238, Slice, one electron, 4f, Classic, 48000 dots",
    "orbit.saturn": "Orbit Lab: zoom 4.10", "orbit.earth": "Orbit Lab: zoom 5.14", "orbit.earth_clouds": "Orbit Lab: zoom 5.14",   # (0.75.0, on purpose: the closest zoom keeps the camera 1.5 Earth radii from its middle: 5.5 asked, 5.14 drawn) "orbit.real_fast": "the belts' rocks", "orbit.sky": "Orbit Lab: zoom 0.00",
    "orbit.journey": "Journey", "atom.all_enlarged": "Og-294, Cloud 3D, every electron, Whole atom, Shells enlarged, Glow, 240000 dots",
    "atom.all_true_zoomed": "Og-294, Cloud 3D, every electron, Whole atom, True scale, Glow, 240000 dots",
    "atom.all_one_orbital": "Og-294, Cloud 3D, every electron, One orbital 5f", "atom.all_classic": "Og-294, Cloud 3D, every electron, Whole atom, Shells enlarged, Classic, 240000 dots",
    "atom.jump_all": "Ra-226, Cloud 3D, every electron, Whole atom, Shells enlarged, Glow, 240000 dots", "atom.nucleus_density": "Og-294, Orbit, Density",
    "molecule.density": "Molecule Lab: C6H6 (12 atoms)", "molecule.vib_density": "Molecule Lab: H2O (3 atoms), zoom x1.60, vibration mode 1",
    "molecule.spacefill_vib": "Molecule Lab: C6H6 (12 atoms), zoom x4.00, vibration mode 1", "blackhole.film_tilt": "distance 74.1 GM/c², field of view 30", "blackhole.extreme": "Black Hole Lab: distance 50.0", "blackhole.all_light": "Black Hole Lab: distance 20.0",
    "blackhole.moving_gas": "Black Hole Lab: distance 10.0",   # 0.72.0
    "blackhole.disc_streaks": "Black Hole Lab: distance 2.2",  # 0.79.0
    "blackhole.circle_spin": "field of view 45",              # 0.82.0 (the distance depends on the picture's size: the closest where the shadow does not fill it)
    "orbit.alphacen": "Orbit Lab, Alpha Centauri: zoom",       # 0.73.0
    "orbit.flyby": "Orbit Lab: zoom 4.1",                      # 0.75.0
    "orbit.true_light_stars": "Orbit Lab: zoom",               # 0.86.0 (the closest zoom depends on the picture's size)
    "orbit.comet_tails": "Orbit Lab: zoom 1.00",               # 0.78.0: the zoom that puts Hale-Bopp's farthest grain at 0.3 of the picture's smaller side
    "ui.guide_scroll": "Learn guide 0:3", "ui.data_plot": "Data panel over the", "ui.settings_open": "the belts' rocks",
}

def part_benchrest(binary, work):
    r = run_app(binary, work, "b_rest", dict(BENCH, PL_BENCH_ONLY="rest.molecule", PL_BENCH_BUSY="1"), frames=100000, size="880x560", timeout=600)
    ln = [l for l in r.out.splitlines() if l.startswith("PLBENCH-REST ")]
    check(len(ln) == 1 and ln[0].split()[2] == "busy", f"a lab made busy on purpose is said to be busy ({ln})")
    t = report(r)
    check("AT REST" in t and "rest.molecule  busy" in t, "the report's rest table says busy")

def part_benchporkchop(binary, work):
    ids = "orbit.porkchop,response.lab_switch.blackhole_orbit,response.sunspot_table"
    r = run_app(binary, work, "b_porkchop", dict(BENCH, PL_BENCH_ONLY=ids), frames=100000, size="880x560", timeout=900)
    w = [l.split() for l in r.out.splitlines() if l.startswith("PLBENCH-WAIT ")]
    for name in ("response.lab_switch.blackhole_orbit", "response.sunspot_table"):
        ms = [float(x[2]) for x in w if x[1] == name]
        # the cap is 15,000 ms; 0.93.0 hit it every time, 0.94.1 takes 2-110 ms here. 5,000 ms leaves room for a slow build machine.
        check(len(ms) == 3 and max(ms) < 5000 and not any("capped" in " ".join(x) for x in w if x[1] == name), f"{name} finishes after orbit.porkchop ({ms} ms)")

def part_benchrestore(binary, work):
    ids = "ui.settings_open,ui.data_plot,ui.guide_scroll,molecule.vib_density,atom.jump_all,orbit.journey,orbit.earth,blackhole.extreme,ui.resize_blackhole,response.lab_switch.orbit_atom,rest.blackhole_glare"
    r = run_app(binary, work, "b_restore", dict(BENCH, PL_BENCH_ONLY=ids, PL_BENCH_WARMUP="0.2", PL_BENCH_MEASURE="0.4", PL_OPEN="settings"), frames=100000, size="880x560", timeout=1200)
    st = {l.split()[1]: l.split(" ", 2)[2] for l in r.out.splitlines() if l.startswith("PLBENCH-STATE ")}
    check("before" in st and "after" in st, "the state was printed before and after")
    if "before" in st and "after" in st:
        check(st["before"] == st["after"], f"every lab, page and panel is as it was:\n    before {st['before']}\n    after  {st['after']}")
        check("settings=1" in st["after"], "the Settings page that was open is open again")
    rs = [l for l in r.out.splitlines() if l.startswith("PLBENCH-RESIZE ")]   # 0.83.0: ui.resize_blackhole puts the window back exactly
    check(len(rs) == 1 and "exactly as at the start: 880 x 560" in rs[0], f"the window resized by ui.resize_blackhole is put back exactly ({rs})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"bench": part_bench, "benchrest": part_benchrest, "benchporkchop": part_benchporkchop, "benchrestore": part_benchrestore}[part](binary, work)
    print(f"check_6b {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
