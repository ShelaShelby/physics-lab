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
    env.update(extra)
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
