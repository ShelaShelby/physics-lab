#!/usr/bin/env python3
# 0.16 stage 2: four fixes, checked in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real GPU).
#   scrollbar: About closed and opened again still shows its scrollbar thumb (pixels, not the app's own rects) - was hidden behind the reopened page
#   bench:     Benchmark window with both extra-scene boxes ticked stays inside 92 % of the window height, Start in view
#   rail:      the About Contents rail is wide enough for its labels (>= 20 em)
#   nucleus:   Quark style / Nucleon detail / Nucleon edge can be clicked at the default zoom
#   0.56.1, the benchmark (its only-scene switch PL_BENCH_ONLY):
#   benchscenes:  every new scene sets up what its text says, and its picture changes between two moments of its counted time
#   benchwaits:   a wait is timed to the finished picture, not the first frame (the cloud filling, the spinning hole's sharp picture); after the glare's
#                 scene the switch to the Black Hole Lab is not capped (0.56.2)
#   benchrest:    a lab made busy on purpose (PL_BENCH_BUSY) is said to be busy at rest
#   benchrestore: after the run every lab, page and panel is as it was
#   check_6b.py <PhysicsLab binary> <work folder> scrollbar|bench|rail|nucleus|benchscenes|benchwaits|benchrest|benchrestore
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

# what each new scene's report must say it set up (its own line), and whether its picture moves (0.72.0: blackhole.extreme starts from the lab's starting view,
# 50 GM/c^2 since 0.70.0: the reference said 20, the start until 0.69; a changed reference, failing since 0.70.0, REPORT 0.72.0)
SCENES = {
    "atom.slice_all_drag": "Og-294, Slice, every electron, Whole atom, Shells enlarged, Classic, 240000 dots", "atom.slice_dirac_drag": "U-238, Slice, one electron, 4f, Classic, 48000 dots",
    "atom.side_all_glow": "Og-294, Cloud, every electron, Whole atom, Shells enlarged, Glow, 240000 dots", "atom.side_all_classic": "Og-294, Cloud, every electron, Whole atom, Shells enlarged, Classic, 240000 dots",
    "atom.slice_glow_ultra": "U-238, Slice, one electron, 4f, Glow, 48000 dots", "atom.slice_classic_ultra": "U-238, Slice, one electron, 4f, Classic, 48000 dots",
    "orbit.saturn": "Orbit Lab: zoom 4.10", "orbit.earth": "Orbit Lab: zoom 5.14",   # (0.75.0, on purpose: the closest zoom keeps the camera 1.5 Earth radii from its middle: 5.5 asked, 5.14 drawn) "orbit.real_fast": "the belts' rocks", "orbit.sky": "Orbit Lab: zoom 0.00",
    "orbit.journey": "Journey", "atom.all_enlarged": "Og-294, Cloud 3D, every electron, Whole atom, Shells enlarged, Glow, 240000 dots",
    "atom.all_true_zoomed": "Og-294, Cloud 3D, every electron, Whole atom, True scale, Glow, 240000 dots",
    "atom.all_one_orbital": "Og-294, Cloud 3D, every electron, One orbital 5f", "atom.all_classic": "Og-294, Cloud 3D, every electron, Whole atom, Shells enlarged, Classic, 240000 dots",
    "atom.jump_all": "Ra-226, Cloud 3D, every electron, Whole atom, Shells enlarged, Glow, 240000 dots", "atom.nucleus_density": "Og-294, Orbit, Density",
    "molecule.density": "Molecule Lab: C6H6 (12 atoms)", "molecule.vib_density": "Molecule Lab: H2O (3 atoms), zoom x1.60, vibration mode 1",
    "molecule.spacefill_vib": "Molecule Lab: C6H6 (12 atoms), zoom x4.00, vibration mode 1", "blackhole.film_tilt": "distance 74.1 GM/c², field of view 30", "blackhole.extreme": "Black Hole Lab: distance 50.0", "blackhole.all_light": "Black Hole Lab: distance 20.0",
    "blackhole.moving_gas": "Black Hole Lab: distance 10.0",   # 0.72.0
    "orbit.alphacen": "Orbit Lab, Alpha Centauri: zoom",       # 0.73.0
    "orbit.flyby": "Orbit Lab: zoom 4.1",                      # 0.75.0
    "ui.guide_scroll": "Learn guide 0:3", "ui.data_plot": "Data panel over the", "ui.settings_open": "the belts' rocks",
}

def part_benchscenes(binary, work):
    # (0.68.1: the three Black Hole scenes in a run of their own, 40 s counted and the second picture at 24 s (9 s and 5.5 s passed by hand but failed in ctest, which runs with LP_NUM_THREADS=1: a frame takes several seconds there): they draw a frame in 1.4 s or more on this box and sometimes
    # several seconds under load, so a second picture at 2.4 s fell after the counted time or showed the same view (film_tilt 0.0000, extreme and all_light no picture: failed
    # here since 0.58.0). The other scenes keep 4 s and 2.4 s: at 5.5 s the two side views of the Atom Lab show the same picture. The second picture stays in the second half,
    # where nucleus_density turns.)
    bh = [i for i in SCENES if i.startswith("blackhole.")]
    rb = run_app(binary, work, "b_scenes_bh", dict(BENCH, PL_BENCH_ONLY=",".join(bh), PL_BENCH_WARMUP="0.3", PL_BENCH_MEASURE="40.0", PL_BENCH_SHOT="0.1,24"), frames=100000, size="880x560", timeout=2400)
    ids = ",".join(i for i in SCENES if i not in bh) + ",rest.orbit,rest.molecule"
    r = run_app(binary, work, "b_scenes", dict(BENCH, PL_BENCH_ONLY=ids, PL_BENCH_WARMUP="0.3", PL_BENCH_MEASURE="4.0", PL_BENCH_SHOT="0.1,2.4"), frames=100000, size="880x560", timeout=2400)
    t = report(r)
    tb = report(rb)
    check(bool(t) and bool(tb), "the benchmark ran the new scenes and gave its report")
    w, h = 880, 560
    for sid, want in SCENES.items():
        d = t[t.find("DETAILS"):] + "\n" + tb[tb.find("DETAILS"):]
        sec = d[d.find("\n" + sid + "  "):] if ("\n" + sid + "  ") in d else ""
        sec = sec[:sec.find("  GPU time")] if "  GPU time" in sec else sec
        check(want in sec, f"{sid}: the report says it set up {want!r}")
        frac = changed(os.path.join(work, sid + ".bmp"), os.path.join(work, sid + "_2.bmp"), w, h)
        check(frac > 0.0005, f"{sid}: the picture changes from one moment to the next ({frac:.4f} of the pixels)")
        print(f"  {sid}: {frac:.4f} of the pixels changed")
    check("SCENES, slowest first" in t and t.find("SCENES, slowest first") < t.find("DETAILS"), "the summary table comes first")
    # 0.63.1: a GPU time for every scene and rest line, below the frame's own time (the owner's 0.63.0 run on Vulkan: about 1e12 ms in most); each scene's
    # three worst frames with where their time went; the CPU work without the waits for the card; the benchmark's own start, part by part
    check("The benchmark's own start: " in t and "the GPU timer's check" in t, "the header has the benchmark's own start, part by part")
    d = t[t.find("DETAILS"):] + "\n" + tb[tb.find("DETAILS"):]
    for sid in SCENES:
        sec = d[d.find("\n" + sid + "  "):] if ("\n" + sid + "  ") in d else ""
        nxt = [sec.find("\n" + o + "  ", 1) for o in SCENES if o != sid and sec.find("\n" + o + "  ", 1) > 0]
        sec = sec[:min(nxt)] if nxt else sec
        g = re.search(r"GPU time per frame: ([\d.]+) ms", sec)
        a_ = re.search(r"Average: ([\d.]+) ms per frame", sec)
        # (25 % room: a frame's card time comes in a frame or more late, and here the software renderer makes the slow scenes card-bound with a handful of frames)
        check(bool(g) and bool(a_) and 0 < float(g.group(1)) < 1.25 * float(a_.group(1)), f"{sid}: a GPU time below the frame's own time ({g.group(0) if g else 'none'}; {a_.group(0) if a_ else ''})")
        check(sec.count("  Worst frame ") >= 2 and "Worst frame 1: " in sec and "frame thread" in sec, f"{sid}: its worst frames with where their time went")
        check("Waited for the card per frame: " in sec and "(the waits for the card taken out)" in sec, f"{sid}: the CPU work without the waits for the card, the waits on a line of their own")
    # 0.68.1: the benchmark's own work is not in a counted frame: film_tilt's picture comparison (19.6 ms of a counted frame in the owner's 0.68.0 run) leaves its frame out
    # and says so; none of the scene's worst frames holds more than 1 ms of the benchmark's numbers
    fsec = d[d.find("\nblackhole.film_tilt  "):]
    nxt = [fsec.find("\n" + o + "  ", 1) for o in SCENES if o != "blackhole.film_tilt" and fsec.find("\n" + o + "  ", 1) > 0]
    fsec = fsec[:min(nxt)] if nxt else fsec
    nums = [float(x) for x in re.findall(r"the benchmark's numbers ([\d.]+) ms", fsec)]
    check(all(x <= 1.0 for x in nums), f"blackhole.film_tilt: no worst frame holds the benchmark's numbers above 1 ms ({nums})")
    check(not nums or "left out of the counted frames: the benchmark's own numbers" in fsec or all(x <= 1.0 for x in nums), "blackhole.film_tilt: a frame holding the benchmark's own numbers is left out and the report says so")
    for rid in ("rest.orbit", "rest.molecule"):
        row = [l for l in t.splitlines() if l.startswith(rid + " ")]
        m = re.search(r"([\d.]+) ms", row[0]) if row else None
        check(bool(m) and float(m.group(1)) < 1000, f"{rid}: a GPU time a frame at rest ({row[0][:120] if row else 'no row'})")

def part_benchwaits(binary, work):
    # (PL_FRAME_MS: frames paced as VSync paces them on a screen; the session box has none, and the frame loop would take the cores the workers need)
    # (0.68.1: with one thread the software renderer fills the Density cloud in over the app's 15 s cap; the test's own cap is 150 s (PL_BENCH_CAP), the app's stays 15 s)
    r = run_app(binary, work, "b_waits", dict(BENCH, PL_BENCH_ONLY="response.atom_density,response.blackhole_spin", PL_FRAME_MS="16", PL_BENCH_CAP="150"), frames=100000, size="880x560", timeout=1200)
    ln = [l.split() for l in r.out.splitlines() if l.startswith("PLBENCH-WAIT ")]
    for wid in ("response.atom_density", "response.blackhole_spin"):
        tries = [l for l in ln if l[1] == wid]
        check(len(tries) == 3, f"{wid}: three tries ({len(tries)})")
        for l in tries:
            frames = int(l[4].split("=")[1])
            check(frames >= 2 and "(capped)" not in " ".join(l), f"{wid}: timed to the finished picture over {frames} frames, not the first frame ({' '.join(l)})")
    t = report(r)
    check("WAITS (each done 3 times" in t and "response.atom_density" in t, "the report has the waits table")
    # 0.56.2: after a scene with the eye's glare (real_move) the glare half made stayed under way once the glare was off: every wait for the Black Hole Lab
    # then ran to the cap (the owner's 0.56.1 run: 15 s from the Molecule Lab). The card's light paths off, as on the owner's PC.
    r = run_app(binary, work, "b_waits_glare", dict(BENCH, PL_BENCH_ONLY="blackhole.real_move,response.lab_switch.molecule_blackhole", PL_BENCH_WARMUP="0.3",
                PL_BENCH_MEASURE="1.5", PL_FRAME_MS="16", PL_BH_GPUPATHS="0"), frames=100000, size="880x560", timeout=600)
    tries = [l for l in r.out.splitlines() if l.startswith("PLBENCH-WAIT response.lab_switch.molecule_blackhole ")]
    check(len(tries) == 3 and not any("(capped)" in l for l in tries), f"after the glare's scene the Black Hole Lab is finished after a switch ({tries})")

def part_benchrest(binary, work):
    r = run_app(binary, work, "b_rest", dict(BENCH, PL_BENCH_ONLY="rest.molecule", PL_BENCH_BUSY="1"), frames=100000, size="880x560", timeout=600)
    ln = [l for l in r.out.splitlines() if l.startswith("PLBENCH-REST ")]
    check(len(ln) == 1 and ln[0].split()[2] == "busy", f"a lab made busy on purpose is said to be busy ({ln})")
    t = report(r)
    check("AT REST" in t and "rest.molecule  busy" in t, "the report's rest table says busy")

# 0.67.0: the Orbit Lab at rest keeps its picture: the kept one is the picture drawn afresh (PL_ORBIT_NOKEEP=1), pixel for pixel (0.69.0: on Vulkan, the only API); a change at
# rest (a Show switch clicked) is drawn; the benchmark's rest line says it was not drawn anew (this box's processor share is the software renderer's own)
def part_orbitkeep(binary, work):
    def keepline(r):
        m = re.search(r"PLORBITKEEP kept=(\d+) drawn=(\d+)", r.out)
        return (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    for api in ("vulkan",):
        for tag, extra in (("still", {}), ("click", {"PL_SCRIPT": "50:click=show.comets"})):
            k = run_app(binary, work, f"okeep_{api}_{tag}", dict(extra, PL_NO_MSAA="1", PL_LOG_SHELL=None), frames=90)
            n = run_app(binary, work, f"onokeep_{api}_{tag}", dict(extra, PL_NO_MSAA="1", PL_ORBIT_NOKEEP="1"), frames=90)
            kept, drawn = keepline(k)
            a, b = Bmp(os.path.join(work, f"okeep_{api}_{tag}.bmp")), Bmp(os.path.join(work, f"onokeep_{api}_{tag}.bmp"))
            diff = sum(1 for y in range(0, 800, 2) for x in range(0, 1280, 2) if a.px(x, y) != b.px(x, y))
            print(f"  {api} {tag}: kept {kept}, drawn {drawn}; {diff} sampled pixels differ from the fresh picture")
            check(diff == 0, f"{api} {tag}: the kept picture is the fresh one, pixel for pixel ({diff} differ)")
            check(kept >= 70, f"{api} {tag}: the picture is kept at rest ({kept} of 90 frames)")
            if tag == "click":
                check(drawn >= 2, f"{api}: the click at rest drew the picture anew ({drawn} drawings)")
    # 0.68.1: as the owner's PC runs it: no PL_ORBIT_KEEP and no PL_LOG_SHELL (the checks' shell log made the picture drawn anew each frame unless PL_ORBIT_KEEP=1 asked for
    # keeping, so this box's earlier test forced it and could not see the owner's 765 drawings in 3 s), his picture size (2560 x 1369) and the helpers on. The cause of his
    # run: the Orbit Lab's picture is kept only once the card's lines are decided (0.68.0) and their helper never started, queued behind the light paths' variant helper that
    # never ended; the second run makes that happen (the lines' helper sleeps: PL_GPU_HANG, limit 2 s) and the picture must be kept once the limit ended it
    for tag, extra in (("defaults", {}), ("lines_helper_hangs", {"PL_GPU_HANG": "lines", "PL_GPU_HELPER_LIMIT": "2"})):
        r = run_app(binary, work, "b_rest_orbit_" + tag, dict(BENCH, PL_BENCH_ONLY="rest.orbit", PL_LOG_SHELL=None, PL_NO_GPU_HELPER="0", **extra), frames=100000, size="2560x1369", timeout=900)
        ln = [l for l in r.out.splitlines() if l.startswith("PLBENCH-REST ")]
        m = re.search(r"redraws=(\d+) .*frames=(\d+)", ln[0]) if ln else None
        check(bool(m) and int(m.group(1)) <= 3 and int(m.group(2)) > 20, f"rest.orbit ({tag}, 2560 x 1369): the picture is not drawn anew at rest ({ln})")

    # 0.75.1: the owner's 0.75.0 run drew rest.orbit anew every frame (766 in 3 s) although his lines were decided; this box kept it (0 drawn anew) after the whole Hard
    # list (0.75.1 REPORT: not reproduced), so this case runs the Orbit Lab's scenes and the interface's before rest.orbit, as his run orders them (short counted times), and
    # the same limit; and the diagnostic: with the key made to change each frame on purpose (PL_ORBIT_KEYNOISE) the rest line says which part of it changed, by name
    ids = ",".join(("orbit.alphacen,orbit.earth,orbit.flyby,orbit.heaviest,orbit.journey,orbit.real_fast,orbit.rocks8,orbit.saturn,orbit.sky,ui.data_plot,ui.guide_scroll,ui.settings_open,rest.orbit",))
    r = run_app(binary, work, "b_rest_orbit_after", dict(BENCH, PL_BENCH_ONLY=ids, PL_BENCH_WARMUP="0.2", PL_BENCH_MEASURE="0.4", PL_LOG_SHELL=None, PL_NO_GPU_HELPER="0"), frames=100000, size="2560x1369", timeout=1500)
    ln = [l for l in r.out.splitlines() if l.startswith("PLBENCH-REST rest.orbit")]
    m = re.search(r"redraws=(\d+) .*frames=(\d+)", ln[0]) if ln else None
    check(bool(m) and int(m.group(1)) <= 3 and int(m.group(2)) > 20, f"rest.orbit after the Orbit Lab's and the interface's scenes (2560 x 1369): the picture is not drawn anew at rest ({ln})")
    r = run_app(binary, work, "b_rest_orbit_noise", dict(BENCH, PL_BENCH_ONLY="rest.orbit", PL_LOG_SHELL=None, PL_ORBIT_KEYNOISE="1"), frames=100000, size="880x560", timeout=600)
    ln = [l for l in r.out.splitlines() if l.startswith("PLBENCH-REST rest.orbit")]
    check(len(ln) == 1 and ln[0].split()[2] == "busy" and "why:" in ln[0] and "the system's time and state" in ln[0], f"rest.orbit, key changing every frame on purpose: the rest line names the part that changed ({ln})")
    t = r.out[r.out.find("AT REST"):]                       # (the report's rest table; its end marker may come after the output the check keeps)
    check("why drawn anew:" in t and "the system's time and state" in t, "the report's rest table says which part of the key changed")

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
    {"scrollbar": part_scrollbar, "bench": part_bench, "rail": part_rail, "nucleus": part_nucleus, "benchscenes": part_benchscenes, "benchwaits": part_benchwaits, "orbitkeep": part_orbitkeep,
     "benchrest": part_benchrest, "benchrestore": part_benchrestore}[part](binary, work)
    print(f"check_6b {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
