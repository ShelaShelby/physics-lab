#!/usr/bin/env python3
# The app's start and its GPU guards, in the REAL app (Linux, headless on Mesa's software Vulkan, lavapipe; not Windows, not a GPU). tests/TESTS.md lists every part.
#   check_shell.py <PhysicsLab binary> <work folder> <part>      parts: smoke, vulkan, novulkan, guardturn, gpuhelper
# 0.76.0: the parts on text sizes, window layout, keys, the reading view and the Vulkan coverage views (vulkan2d, vulkancards, vulkanbh) were deleted (git keeps them).
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, Bmp

fails = 0
checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

# 0.59.0 (Vulkan step 1 of 5): the app with the validation layers on: it starts on Vulkan (lavapipe here), the log has every [vulkan] step, zero validation
# errors. Then the swapchain made again.
VK_STEPS = ["instance", "surface", "device", "swapchain", "pipelines", "renderer2d pipelines", "renderer2d textures", "renderer2d check", "first frame"]   # (0.60.0: + renderer2d)

def _guarded_programs():
    """The program names in vulkanCardGuards() (native/src/app/gpu_guard.cpp), in its order."""
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "app", "gpu_guard.cpp")
    text = open(src, encoding="utf-8").read()
    body = text[text.index("vulkanCardGuards() {"):]
    body = body[:body.index("return v;")]
    return re.findall(r'\{"(\w+)", &', body)

def _settings_files(home):
    log, cfg = "", ""
    for root, _dirs, files in os.walk(home):
        if "gpu-paths-log.txt" in files:
            log = open(os.path.join(root, "gpu-paths-log.txt"), errors="replace").read()
        for f in files:
            if f.endswith(".txt") and f.startswith("settings"):
                cfg = open(os.path.join(root, f), errors="replace").read()
    return log, cfg

def part_vulkan(binary, work):
    vk_env = {"PL_VK_VALIDATION": "1"}
    for name, opened in (("settings", "settings"), ("page", "about")):         # (Settings at its top: the Graphics row itself says which is in use)
        v = run_app(binary, work, "vk_" + name, dict(vk_env, PL_OPEN=opened, PL_NO_PICTURE="1"), frames=40)
        check("Vulkan: " in v.out and v.code == 0, f"{name}: the app started on Vulkan and ended normally (exit {v.code}; {[l for l in v.out.splitlines() if l.startswith('Vulkan')][:1]})")
        check("PLVKVALIDATION missing" not in v.err, f"{name}: the validation layers are installed and on")
        errs = [l for l in v.err.splitlines() if l.startswith("PLVKVALIDATION error")]
        warns = [l for l in v.err.splitlines() if l.startswith("PLVKVALIDATION warning")]
        check("PLVK validation errors=0" in v.out and not errs, f"{name}: zero validation errors ({len(errs)}; first: {errs[:1]}; warnings {len(warns)})")
        log, _cfg = _settings_files(os.path.join(os.path.abspath(work), "home_vk_" + name))
        missing = [st for st in VK_STEPS if f"[vulkan] " not in log or f"begin {st}" not in log or f"end {st}" not in log]
        check(not missing and "Vulkan: the whole path succeeded" in log, f"{name}: the log has every [vulkan] step (missing: {missing})")
        check(os.path.exists(v.bmp), f"{name}: the window's picture was read back")
        if name == "settings":
            gl_note = [n for n in v.notes if n.startswith("graphics ")]
            check(gl_note and "Graphics: Vulkan, on " in gl_note[0], f"Settings says Vulkan is in use: {gl_note[:1]}")
    # the swapchain made again: the window resized twice and VSync switched off and on (other present modes), validation still silent
    m = run_app(binary, work, "vk_resize", dict(vk_env, PL_OPEN="settings:vsync", PL_SCRIPT="8:size=1000x700;14:click=vsync;20:size=1280x800;26:click=vsync"), frames=34)
    errs = [l for l in m.err.splitlines() if l.startswith("PLVKVALIDATION error")]
    check("Vulkan: " in m.out and "PLVK validation errors=0" in m.out and not errs and not m.scripted_missing(), f"resized and VSync switched: zero validation errors ({errs[:1]}, {m.scripted_missing()})")
    check(os.path.exists(m.bmp) and Bmp(m.bmp).w == 1280, "after the resize the swapchain has the window's new size (the last picture 1280 wide)")

# A PC whose Vulkan cannot start. A step made to fail (PL_VK_FAIL=device, a test switch): the app shows its message (printed by the
# checks: PLNOVULKAN, PLNOVULKANTEXT; no window waits for a click) naming what is needed and what this PC has, draws no frame and ends with exit code 1, not
# a crash; the log and the settings keep the failed step. The next start (no switch): the guard keeps Vulkan off, the same message. Try again
# (PL_NOVULKAN_ANSWER=again) forgets it; the start after that runs on Vulkan.
def part_novulkan(binary, work):
    home = os.path.join(os.path.abspath(work), "home_nv")
    subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    f = run_app(binary, work, "nv_fail", dict(HOME=home, PL_VK_FAIL="device"), frames=30)
    text = [l for l in f.out.splitlines() if l.startswith("PLNOVULKANTEXT ")]
    check(any(l.startswith("PLNOVULKAN ") and "made to fail on purpose" in l for l in f.out.splitlines()) and text and "Vulkan 1.3" in text[0] and "VK_KHR_dynamic_rendering" in text[0]
          and "VK_KHR_synchronization2" in text[0] and "This PC: " in text[0], f"no Vulkan: the message names what is needed and what this PC has ({text[:1]})")
    check(f.code == 1 and f.window is None and "PLNOVULKANANSWER close" in f.out, f"no Vulkan: no frame drawn, the app ends with exit code 1, no crash (exit {f.code})")
    log, cfg = _settings_files(home)
    check("[vulkan]" in log and "failed: made to fail on purpose" in log and "Vulkan could not start" in log, "no Vulkan: the log keeps the [vulkan] lines and says Vulkan could not start")
    check("gpuvulkan=fail | vulkan " in cfg, "no Vulkan: the settings file keeps the failed step")
    n = run_app(binary, work, "nv_next", dict(HOME=home, PL_NOVULKAN_ANSWER="again"), frames=30)
    check(n.code == 1 and any(l.startswith("PLNOVULKAN ") and "could not start last time" in l for l in n.out.splitlines()) and "PLNOVULKANANSWER again" in n.out,
          f"no Vulkan: the next start keeps it off and says so; Try again answered (exit {n.code})")
    _log, cfg2 = _settings_files(home)
    check("gpuvulkan=fail" not in cfg2, "no Vulkan: Try again forgets the failed step in the settings file")
    a = run_app(binary, work, "nv_again", dict(HOME=home), frames=30)
    check("Vulkan: " in a.out and a.code == 0 and a.window is not None and "PLNOVULKAN" not in a.out, f"no Vulkan: after Try again the app starts on Vulkan (exit {a.code})")

def _logged(log, prefix, text):   # a line of the guard's log: its prefix, its time, then the text
    return any(l.startswith(prefix) and re.search(r"\]\s*" + re.escape(text), l[len(prefix):]) for l in log.splitlines())


VK3_OUTLINE = {"PL_ATOM": "1", "PL_ATOM_SET": "Z=1;view=Cloud 3D;orb=3d;outline=On"}   # (the 90 % outline on the card)
VK4_FEED = "preset=feeding;spin=0.6"

# 0.63.0: the guards blame the right program. 0.62.0 on the owner's PC: the light paths' pipeline (made on a worker) crashed the driver while the outline's
# marker was still on disk (its first small run had ended, but a marker stayed until its whole path had passed), so the next start switched both off. Now at
# most one guarded step is open at a time in the whole process and a step's marker goes when it ends. The light paths' pipeline made to "crash" (PL_GPU_DIE)
# on a fresh settings folder; the next start: the light paths off, the outline still allowed and its steps run (its first full use drawn).
def part_guardturn(binary, work):
    home = os.path.join(os.path.abspath(work), "home_gt")
    subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    d = run_app(binary, work, "gt_die", dict(HOME=home, PL_BLACKHOLE="1", PL_BH_SET=VK4_FEED, PL_GPU_DIE="the light paths' pipeline", PL_NO_GPU_HELPER="1"), frames=600, timeout=1200)   # (0.67.0: the app's own step, not the helper's)
    log1, _c = _settings_files(home)
    check("(test switch PL_GPU_DIE" in log1 and _logged(log1, "[vulkan] [lightpaths] ", "begin the light paths' pipeline"), "guard turn: the first start died inside the light paths' pipeline (PL_GPU_DIE)")
    check(_logged(log1, "[vulkan] [outline] ", "end the outline's first small run"), "guard turn: the outline's start-up steps ran before (its first full use not yet: it has not passed)")
    a = run_app(binary, work, "gt_next", dict(VK3_OUTLINE, HOME=home), frames=90)
    log2, cfg2 = _settings_files(home)
    check("Vulkan: " in a.out and re.search(r"gpuvklightpaths=off \| [^\n]*\| the light paths' pipeline", cfg2) is not None,
          f"guard turn: the next start: the light paths off, their step named ({[l for l in cfg2.splitlines() if l.startswith('gpuvklightpaths')]})")
    check("gpuvkoutline=off" not in cfg2 and "The 90 % outline on Vulkan: allowed" in log2 and _logged(log2, "[vulkan] [outline] ", "end the outline's first full use"),
          f"guard turn: the outline still allowed and its steps run ({[l for l in cfg2.splitlines() if l.startswith('gpuvkoutline')]})")

# 0.67.0: the crash-safe helper (app/gpu_helper.h). A fresh settings folder: the helper's step made to "crash" (PL_GPU_DIE in the helper process) -> the app runs
# on to its last frame, that program off and the log names the step ("the helper died making ..."), the settings keep it; the next start does not try it
# again (no helper for it, still off). The light paths killed the same way: the next start runs the five kKerrTableCS variants in helpers and logs each;
# the start after that does not run them again (once per driver).
def part_gpuhelper(binary, work):
    nd = {"PL_ATOM": "1", "PL_ATOM_BENCHSCENE": "38", "PL_ATOM_BENCHSTOP": "40", "PL_NO_GPU_HELPER": "0"}   # (0.68.0: the helpers on: shell_lib turns them off)
    home = os.path.join(os.path.abspath(work), "home_gh"); subprocess.run(["rm", "-rf", home]); os.makedirs(home, exist_ok=True)
    a = run_app(binary, work, "gh_die", dict(nd, HOME=home, PL_GPU_DIE="the nucleus density's pipeline"), frames=150, timeout=1200)
    log1, cfg1 = _settings_files(home)
    check("Vulkan: " in a.out and os.path.exists(a.bmp), "helper: the app ran on to its last frame (its screenshot) while its helper died")
    check("PLGPUHELPER nucdensity died making the nucleus density's pipeline" in a.out and "PLNUCDENSCARD state=-1" in a.out and "PLNUCDENSCARD state=1" not in a.out,
          "helper: the nucleus density off, the processor's squares drawn")
    check(_logged(log1, "[vulkan] [nucdensity] ", "the helper died making the nucleus density's pipeline"), "helper: the log names the step the helper died in")
    check(re.search(r"gpuvknucdensity=off \| [^\n]*\| the helper died making the nucleus density's pipeline", cfg1) is not None, "helper: the settings keep it off, the step named")
    b = run_app(binary, work, "gh_next", dict(nd, HOME=home), frames=100, timeout=1200)
    log2, _c = _settings_files(home)
    check("the helper starts (nucdensity)" not in log2 and "PLGPUHELPER nucdensity" not in b.out and "PLNUCDENSCARD state=1" not in b.out,
          "helper: the next start does not try it again (no helper for it; still off)")
    home2 = os.path.join(os.path.abspath(work), "home_gh2"); subprocess.run(["rm", "-rf", home2]); os.makedirs(home2, exist_ok=True)
    c = run_app(binary, work, "gh_lp", dict(nd, HOME=home2, PL_GPU_DIE="the light paths' pipeline"), frames=60, timeout=1200)
    check("PLGPUHELPER lightpaths died making the light paths' pipeline" in c.out and os.path.exists(c.bmp), "helper: the light paths' helper died, the app ran on")
    d = run_app(binary, work, "gh_var", dict(nd, HOME=home2), frames=240, timeout=1200)
    log4, _c = _settings_files(home2)
    vs = re.findall(r"PLGPUHELPER variant (\w+): (.*)", d.out)
    print(f"  the variants: {vs}")
    check(len(vs) == 5 and all(v[1].startswith("made") for v in vs), f"helper: the five kKerrTableCS variants made by helpers on this driver ({len(vs)})")
    check(sum(1 for l in log4.splitlines() if "the helper's detective work, kKerrTableCS variant" in l) == 5, "helper: each variant's line in the log")
    e = run_app(binary, work, "gh_var2", dict(nd, HOME=home2), frames=60, timeout=1200)
    check("PLGPUHELPER variant" not in e.out, "helper: the variants are not run again with the same driver")
    # (0.76.0: the helper's time limit (a hung or silent helper, 0.68.1's fixes) is no longer run here: git keeps it)
    # 0.68.0: every guarded program's pipelines and first small run made by a helper first; two programs whose helpers die (the lines' first small run, the
    # surface's pipelines): the app runs on, only those two off (the settings keep them, the log names their steps), the rest passed in their helpers
    home3 = os.path.join(os.path.abspath(work), "home_gh3"); subprocess.run(["rm", "-rf", home3]); os.makedirs(home3, exist_ok=True)
    t = run_app(binary, work, "gh_two", {"HOME": home3, "PL_NO_GPU_HELPER": "0", "PL_GPU_DIE": "the lines' first small run|the surface's pipelines"}, frames=300, timeout=1500)
    log5, cfg5 = _settings_files(home3)
    died = sorted(re.findall(r"PLGPUHELPER (\w+) died making ([^(]*) \(", t.out))
    passed = re.findall(r"PLGPUHELPER (\w+) passed", t.out)
    print(f"  died: {died}; passed: {passed}")
    check(os.path.exists(t.bmp) and died == [("lines", "the lines' first small run"), ("surface", "the surface's pipelines")], f"helper: two helpers died, the app ran on to its last frame ({died})")
    # 0.93.0: the expected programs are read from vulkanCardGuards() (app/gpu_guard.cpp). The fixed count 13 went stale when 0.92.0 added porkchop (14 passed).
    guarded = _guarded_programs()
    others = sorted(set(guarded) - {"lines", "surface"})
    check(sorted(passed) == others, f"helper: the other {len(others)} guarded programs' helpers made their first steps ({len(passed)}: {sorted(set(others) ^ set(passed))} differ)")
    check(_logged(log5, "[vulkan] [lines] ", "the helper died making the lines' first small run") and _logged(log5, "[vulkan] [surface] ", "the helper died making the surface's pipelines"),
          "helper: the log names both programs and their steps")
    offs = sorted(re.findall(r"gpuvk(\w+)=off ", cfg5))
    check(offs == ["lines", "surface"], f"helper: only those two kept off in the settings ({offs})")
    check("PLLINESCARD state=-1" in t.out, "helper: the Orbit Lab draws its lines on the processor")

# 0.76.0: ui_smoke, the short check every job runs: the app starts, each of the four labs opens and draws a frame that is not empty with zero Vulkan validation
# errors, the app ends with exit code 0.
SMOKE_LABS = {"orbit": {}, "atom": {"PL_ATOM": "1"}, "molecule": {"PL_MOLECULE": "H2O"}, "blackhole": {"PL_BLACKHOLE": "1"}}
SMOKE_LABS = {k: dict(v, PL_VK_VALIDATION="1") for k, v in SMOKE_LABS.items()}   # (the validation layers on: each lab's drawing with zero errors)

def part_smoke(binary, work):
    if os.environ.get("PL_SMOKE_EMPTY"):                                   # (by hand: the labs draw no picture, so the check must fail)
        for v in SMOKE_LABS.values(): v["PL_NO_PICTURE"] = "1"
    prefetch([((binary, work, "smoke_" + lab, env), dict(frames=30, timeout=240)) for lab, env in SMOKE_LABS.items()])
    for lab, env in SMOKE_LABS.items():
        r = run_app(binary, work, "smoke_" + lab, env, frames=30, timeout=240)
        check(r.code == 0 and "Vulkan: " in r.out, f"smoke {lab}: the app started on Vulkan and ended with exit code 0 (exit {r.code})")
        errs = [l for l in r.err.splitlines() if l.startswith("PLVKVALIDATION error")]
        check("PLVKVALIDATION missing" not in r.err and "PLVK validation errors=0" in r.out and not errs, f"smoke {lab}: zero Vulkan validation errors ({len(errs)}; first: {errs[:1]})")
        # 0.89.0: no swapchain image is used before it is acquired (0.88.0 and before: Dear ImGui's helper moved every new image to PRESENT_SRC first)
        na = [l for l in r.err.splitlines() if l.startswith("PLVKVALIDATION") and "non-acquired-swapchain-image-used" in l]
        check(not na, f"smoke {lab}: no swapchain image used before it is acquired ({len(na)}; first: {na[:1]})")
        if not os.path.exists(r.bmp):
            check(False, f"smoke {lab}: the frame was read back")
            continue
        b = Bmp(r.bmp)
        px = [b.px(x, y) for y in range(int(0.3 * b.h), int(0.7 * b.h), 2) for x in range(int(0.25 * b.w), int(0.6 * b.w), 2)]   # (the middle of the picture: no panel there)
        counts = {}
        for c in px:
            counts[c] = counts.get(c, 0) + 1
        mode = max(counts, key=counts.get)
        off = sum(1 for c in px if max(abs(c[i] - mode[i]) for i in range(3)) > 24)
        print(f"smoke {lab}: {len(counts)} colours, {100 * off / len(px):.1f} % of the samples away from the main colour")
        check(len(counts) >= 20 and off >= 0.08 * len(px), f"smoke {lab}: the frame is not empty ({len(counts)} colours, {off} of {len(px)} samples away from {mode})")

def main():
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"smoke": part_smoke, "vulkan": part_vulkan, "novulkan": part_novulkan, "guardturn": part_guardturn, "gpuhelper": part_gpuhelper}[part](binary, work)
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
