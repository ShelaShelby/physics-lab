#!/usr/bin/env python3
# 0.67.0: the nucleus' Density look on the card against the processor, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real GPU). tests/TESTS.md lists the parts kept and why.
#   nucdens, nucdenscard: see the parts.
#   check_6c.py <PhysicsLab binary> <work folder> nucdens|nucdenscard
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

# 0.67.0: the nucleus' Density look on the card (Vulkan; atom/nucdens_check.h): its start-up check passes, the card's picture is the processor's within the
# Vulkan picture limit (at most 1 % of the pixels over 8 levels, multisampling off), its steps run once while the view stands still and again for each
# new view while it turns (PL_ATOM_SPIN), and a
# failed check leaves the processor's picture (PL_NUCDENS_CARDFAIL)
def part_nucdens(binary, work):
    from shell_lib import Bmp
    env = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_NO_MSAA": "1", "PL_ATOM_BENCHSCENE": "38", "PL_ATOM_BENCHSTOP": "45", "PL_DRAWSTAT": "1"}
    card = run_app(binary, work, "c_nd_card", env, frames=60)
    cpu = run_app(binary, work, "c_nd_cpu", dict(env, PL_VK_CARDFAIL="nucdensity"), frames=60)
    bad = run_app(binary, work, "c_nd_fail", dict(env, PL_NUCDENS_CARDFAIL="1"), frames=60)
    check("PLNUCDENSCARD state=1" in card.out, "the card's start-up check passed")
    check("PLNUCDENSCARD state=-1" in cpu.out and "PLNUCDENSCARD state=-1" in bad.out, "a failed pipeline and a failed check leave the processor's squares")
    a, b, c = Bmp(card.bmp), Bmp(cpu.bmp), Bmp(bad.bmp)
    def over(x, y):
        px, py = x.pixels(), y.pixels()
        st = 3 if x.bpp == 3 else 4
        return sum(1 for i in range(0, min(len(px), len(py)) - st + 1, st) if max(abs(px[i] - py[i]), abs(px[i + 1] - py[i + 1]), abs(px[i + 2] - py[i + 2])) > 8)
    o, o2 = over(a, b), over(c, b)
    print(f"  card against processor: {o} pixels over 8 levels ({100 * o / max(1, a.w * a.h):.3f} %); the failed check's picture: {o2}")
    check((a.w, a.h) == (b.w, b.h) and a.w > 0 and o <= 0.01 * a.w * a.h, f"the card's picture is the processor's within the limit ({o} pixels over 8 levels)")
    check(o2 == 0, f"the failed check's picture is the processor's ({o2} pixels over 8 levels)")
    def runs(r):
        return [int(m.group(1)) for m in (re.search(r"nucruns=(\d+)", ln) for ln in r.out.splitlines() if ln.startswith("PLDRAWSTAT ")) if m]
    still = runs(card)
    turn = runs(run_app(binary, work, "c_nd_turn", dict(env, PL_ATOM_SPIN="0.02"), frames=60))
    print(f"  the card's runs: still {still[0] if still else '-'} .. {still[-1] if still else '-'}; turning {turn[0] if turn else '-'} .. {turn[-1] if turn else '-'}")
    check(len(still) > 30 and still[-1] == 1, "still: the card's steps ran once (not again while nothing changed)")
    check(len(turn) > 30 and turn[-1] - turn[-30] >= 25, "turning: the card's steps run again for nearly every new view")

# 0.68.1: the owner's 0.68.0 run showed the Density look's card path never started and "the density grid made 0 times" while turning. Cause: the benchmark scene's zoom
# (0.3 closer than the whole nucleus) left the Density look in a window as big as his (2560 x 1369): nucleons drawn, the look's code never reached. Now the scene's zoom comes
# from the nucleon's size in points. The check runs the scene at his window size with the helper on, from a fresh settings folder (as his first start): the log says the whole path
# succeeded and the scene's turning half made new grids (a still half: one).
def part_nucdenscard(binary, work):
    import shutil
    home = os.path.join(os.path.abspath(work), "home_ndc"); shutil.rmtree(home, ignore_errors=True); os.makedirs(home, exist_ok=True)
    env = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": "atom.nucleus_density", "PL_BENCH_WARMUP": "0.5", "PL_BENCH_MEASURE": "6.0",
           "PL_NO_GPU_HELPER": "0", "HOME": home}
    r = run_app(binary, work, "c_ndcard", env, frames=100000, size="2560x1369", timeout=1800)
    log = ""
    for root, _d, files in os.walk(home):
        if "gpu-paths-log.txt" in files: log = open(os.path.join(root, "gpu-paths-log.txt"), errors="replace").read()
    check("[vulkan] [nucdensity]" in log and "The nucleus' Density look on Vulkan: the whole path succeeded" in log, "nucleus density card path: the log says the whole path succeeded (with the helper, at 2560 x 1369)")
    check("the Density look reached the card path" in log, "nucleus density card path: the log says the Density look reached the card path")
    out = r.out
    still = re.search(r"Standing still: (\d+) frames.*the density grid made (\d+) times", out)
    turn = re.search(r"Turning: (\d+) frames.*the density grid made (\d+) times", out)
    print(f"  still {still.groups() if still else None}; turning {turn.groups() if turn else None}")
    check(still and turn and int(turn.group(2)) >= 3 and int(still.group(2)) <= 1, f"nucleus density card path: the density grid is made while turning ({turn.group(2) if turn else None}) and once standing still ({still.group(2) if still else None})")
    check("not the Density look" not in out, "nucleus density card path: the scene draws the Density look")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"nucdens": part_nucdens, "nucdenscard": part_nucdenscard}[part](binary, work)
    print(f"check_6c {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
