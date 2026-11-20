# 0.15 stage 3: helpers for the tests of the new UI (check_shell.py, check_shell_state.py, check_shell_pictures.py): run the REAL app headless (SDL offscreen, Mesa llvmpipe)
# and read what it printed (PL_LOG_SHELL, PLSTATE) and the picture it saved (a BMP, bytes B G R A, rows bottom first).
import os, re, struct, subprocess

def _run_app(binary, work, name, extra, frames=60, size="1280x800", theme="dark", timeout=900):
    binary, work = os.path.abspath(binary), os.path.abspath(work)
    home = os.path.join(work, "home_" + name)
    os.makedirs(home, exist_ok=True)
    for sub in (".local", ".config"):                       # no saved settings from an earlier run
        p = os.path.join(home, sub)
        if os.path.isdir(p):
            subprocess.run(["rm", "-rf", p])
    env = {k: v for k, v in os.environ.items() if not k.startswith("PL_") and k != "XDG_DATA_HOME"}
    env.update(HOME=home, SDL_VIDEODRIVER="offscreen", PL_WINDOW=size, PL_MOTION="on", PL_FIXED_DT="0.016", PL_DYK_FIRST="3", PL_DATE="2026-10-05",
               PL_EXIT_AFTER_FRAMES=str(frames), PL_THEME=theme, PL_LOG_SHELL="1")
    env["PL_PANELS"] = "atom.card=o"   # 0.22 stage 6: the Atom Lab's card starts open in every check, as it did before it folded itself on narrow windows (a check can pass PL_PANELS="" for the app's own default)
    env["PL_NO_GPU_HELPER"] = "1"   # 0.68.0: no helper processes (every start with a fresh settings folder would run one for each card program); check_shell gpuhelper asks for them ("0")
    env.update(extra)
    env = {k: v for k, v in env.items() if v is not None}   # 0.68.1: a value of None removes the variable (a check that runs as the owner does: no PL_LOG_SHELL)
    bmp = os.path.join(work, name + ".bmp")
    env["PL_SCREENSHOT"] = bmp
    r = subprocess.run([binary], env=env, cwd=work, capture_output=True, text=True, errors="replace", timeout=timeout)
    return Result(r.stdout, r.stderr, bmp)

class Result:
    def __init__(self, out, err, bmp):
        self.out, self.err, self.bmp = out, err, bmp
        self.rects, self.notes, self.texts, self.window = {}, [], [], None
        for ln in out.splitlines():
            if not ln.startswith("PLSHELL "):
                continue
            body = ln[8:]
            if body.startswith("window "):
                kv = dict(p.split("=") for p in body.split()[1:])
                self.window = (int(kv["w"]), int(kv["h"]), float(kv["dpi"]), float(kv["textScale"]))
            elif body.startswith("rect "):
                kv = dict(p.split("=") for p in body.split()[1:])
                self.rects[kv["name"]] = tuple(float(kv[k]) for k in ("x", "y", "w", "h"))
            elif body.startswith("text "):
                m = re.match(r"text kind=(\S+) px=([\d.]+) t=(.*)$", body)
                if m:
                    self.texts.append((m.group(1), float(m.group(2)), m.group(3)))
            else:
                self.notes.append(body)
        self.states = [ln[8:] for ln in out.splitlines() if ln.startswith("PLSTATE ")]
    def note(self, prefix):
        return [n for n in self.notes if n.startswith(prefix)]
    def scripted_missing(self):
        return [ln for ln in self.out.splitlines() if ln.startswith("PLSCRIPT ")]

def inside(r, w, h, eps=0.6):
    return r[0] >= -eps and r[1] >= -eps and r[0] + r[2] <= w + eps and r[1] + r[3] <= h + eps

def overlap(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]

class Bmp:
    def __init__(self, path):
        d = open(path, "rb").read()
        self.off = struct.unpack("<I", d[10:14])[0]
        self.w, self.h = struct.unpack("<ii", d[18:26])
        self.bpp = struct.unpack("<H", d[28:30])[0] // 8
        self.stride = (self.w * self.bpp + 3) // 4 * 4
        self.d = d
        self.bottom_up = self.h > 0
        self.h = abs(self.h)
    def px(self, x, y):                                       # (R, G, B)
        row = (self.h - 1 - y) if self.bottom_up else y
        o = self.off + row * self.stride + x * self.bpp
        return (self.d[o + 2], self.d[o + 1], self.d[o])
    def pixels(self):
        return self.d[self.off:self.off + self.stride * self.h]


# 0.15 stage 4.1: independent app starts at the same time. prefetch([(args, kwargs), ...]) starts them in a pool (PL_JOBS workers, default one per core; `check.sh full` sets PL_JOBS=1 because
# ctest already runs one test per core); a later run_app(...) with the same arguments takes the finished result instead of starting the app again. Same start, same pixels, same checks:
# only the order in time differs (every start has its own HOME and work files, named by `name`).
import concurrent.futures, os as _os
_cache = {}

def _key(binary, work, name, extra, frames=60, size="1280x800", theme="dark", timeout=900):
    return (_os.path.abspath(binary), _os.path.abspath(work), name, tuple(sorted(extra.items())), frames, size, theme)

def run_app(binary, work, name, extra, frames=60, size="1280x800", theme="dark", timeout=900):
    k = _key(binary, work, name, extra, frames, size, theme, timeout)
    if k in _cache:
        return _cache.pop(k).result()
    return _run_app(binary, work, name, extra, frames, size, theme, timeout)

def prefetch(calls):
    jobs = int(_os.environ.get("PL_JOBS", _os.cpu_count() or 2))
    if jobs <= 1:
        return
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=jobs)
    for args, kwargs in calls:
        k = _key(*args, **kwargs)
        if k not in _cache:
            _cache[k] = pool.submit(_run_app, *args, **kwargs)
    pool.shutdown(wait=False)


# 0.16 stage 1b: the OLD interface (the side panels, the header; New UI Off) was removed from the app, so the checks that used it as their reference now read what it printed,
# saved from the last build that still had it (the commit before 0.16 stage 1b, "native 0.16 stage 1a"): tests/reference/old_ui_runs.json. A saved run keeps the lines the old
# app printed that start with PL (PLSTATE, PLASTATE, PLATOMCARD, PLSCRIPT, the PLSHELL notes ...; not the PLSHELL rects and texts). Nothing in it is produced by the new code.
# To capture again (needs a binary of that old commit): PL_CAPTURE_OLD=<old PhysicsLab binary> python3 check_xxx.py <new binary> <work>
import json
_OLD_FILE = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "reference", "old_ui_runs.json")
_old_runs = None

def old_run(test, name, extra, frames=60, size="1280x800", theme="dark", timeout=900):
    global _old_runs
    if _old_runs is None:
        try:
            _old_runs = json.load(open(_OLD_FILE))
        except OSError:
            _old_runs = {}
    key = f"{test}/{name}"
    cap = _os.environ.get("PL_CAPTURE_OLD")
    if cap:
        e = dict(extra); e["PL_NEWUI"] = "off"
        work = _os.path.join(_os.environ.get("TMPDIR", "/tmp"), "old_ui_capture"); _os.makedirs(work, exist_ok=True)
        r = _run_app(cap, work, "old_" + name, e, frames, size, theme, timeout)
        text = "\n".join(ln for ln in r.out.splitlines() if ln.startswith("PL") and not ln.startswith(("PLSHELL rect ", "PLSHELL text ")))
        import fcntl
        with open(_OLD_FILE + ".lock", "w") as lk:             # (several tests may capture at the same time)
            fcntl.flock(lk, fcntl.LOCK_EX)
            try:
                cur = json.load(open(_OLD_FILE))
            except (OSError, ValueError):
                cur = {}
            cur[key] = text
            json.dump(cur, open(_OLD_FILE, "w"), indent=0, sort_keys=True)
        _old_runs[key] = text
        return Result(text, "", "")
    if key not in _old_runs:
        raise KeyError("no saved run of the old interface: " + key)
    return Result(_old_runs[key], "", "")


# 0.28 (on purpose): the element temperatures and densities now come from the CRC Handbook (97th ed., via mendeleev 1.3.0), uranium-238's B(E2) from ENSDF
# (12.31 e^2 b^2 for 12.32) and a gas density says "at 25 °C". The saved old-panel runs (tests/reference/old_ui_runs.json) predate that; these are exactly the
# strings that changed, old -> new, so the old-vs-new card checks still compare everything else string for string.
OLD_TO_NEW_028 = {
    # (0.46.1, on purpose: the outside review: hydrogen gas is H2 molecules, so the card gives the molecules' speed, 1/sqrt(2) of the atom speed)
    "Typical atom speed here: ≈ <b>2.70 km/s</b> (≈ 9,735 km/h). Hotter = faster.":
        "Typical molecule speed here (H₂): ≈ <b>1.91 km/s</b> (≈ 6,884 km/h). Hotter = faster. (Hydrogen gas is made of H₂ pairs; very hot gas splits into single atoms, which move √2 times faster.)",
    "Shape drawn: prolate (stretched like a rugby ball), β₂ = +0.2890.": "Shape drawn: prolate (stretched like a rugby ball), β₂ = +0.2889.",
    "At room temperature (20 °C): ☁️ gas · density 0.08 g/L (as a gas)": "At room temperature (20 °C): ☁️ gas · density 0.08 g/L (as a gas at 25 °C)",
    "Boiling point: 4,200 K · 3,927 °C · 7,100 °F": "Boiling point: 4,404 K · 4,131 °C · 7,468 °F",
    "Melting point: 1,405 K · 1,132 °C · 2,069 °F": "Melting point: 1,408 K · 1,135 °C · 2,075 °F",
    # 0.42, on purpose: the proton is drawn at the CODATA 2022 radius itself (kNucleonR was a rounded 0.84), and the card names PDG 2024 and the radius puzzle
    "One proton. It is drawn at the lab's proton radius, 0.84 fm (CODATA 2022: 0.84075 ± 0.00064 fm).":
        "One proton. It is drawn at its charge radius, 0.84075 ± 0.00064 fm (CODATA 2022; PDG 2024: 0.8409 ± 0.0004). Some electron-scattering results still give about 0.88 fm (the proton radius puzzle, now mostly resolved).",
    # 0.46 (review A7), on purpose: the estimated shape's curve is named and said to have no published source
    "Shape source: estimate, from the number of valence protons and neutrons (Casten's P-factor): a rule of thumb, not a measurement.":
        "Shape source: estimate, from the number of valence protons and neutrons (Casten's P-factor). The lab's curve, β₂ = 0.6 (1 − e^(−P/1.5)) up to A = 40 and 0.35 (1 − e^(−P/3)) above, has no published source (0.46 review): a rule of thumb, not a measurement.",
}


def old_strings_028(lines):
    # (0.39.2, on purpose: the nuclear-size text writes the square roots as signs, √(3/5) and √(5/3), not sqrt(...); atom_tests' wording reference changed then too)
    # (0.46.1, on purpose: the outside review: the heavy-atom hint no longer says "this lab uses non-relativistic physics"; in the Orbit view it names the orbit and points to the Cloud and Slice views' Dirac energy)
    return [OLD_TO_NEW_028.get(x, x).replace("sqrt(3/5)", "√(3/5)").replace("sqrt(5/3)", "√(5/3)")
            .replace("Heavy atoms: this lab uses non-relativistic physics, but a 1s electron", "Heavy atoms: the orbit is non-relativistic, but a 1s electron")
            .replace(" The readout gives the Dirac-equation value for a point nucleus.", " The Cloud and Slice views give the Dirac-equation energy for a point nucleus.") for x in lines]
