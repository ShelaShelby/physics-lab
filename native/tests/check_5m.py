#!/usr/bin/env python3
# 0.15 stage 5m (look, parts 1 and 2). Checked in the REAL app (Linux, software OpenGL, scripted events; NOT a real mouse, NOT Windows, NOT a GPU).
#   check_5m.py <PhysicsLab binary> <work folder> <part>      parts: themes
# themes (part 1):
#   * the picker's theme list has none of the three archived themes (the app prints the list it really uses: PLSHELL look themes=...);
#   * a settings file that names an archived theme (each of the three, and the Y2K Ice of stage 5g) opens as Dark, once, with no message: the same theme and the SAME PICTURE as a file that says Dark;
#   * nothing archived is found by search (themes, the Theme effects row, the facts) and the fact list lost the Synthwave surprise (26 facts, not 27);
#   * the look M1 is on with New UI On and the Dark theme without any environment variable (PL_LOOK is not read any more), Inter is the font; with New UI Off it is not.
# The expected values are written here by hand on purpose: the app builds its lists from its own tables.
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Result

fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

ARCHIVED = ("aero", "y2knight", "synthwave")
REMOVED_EARLIER = ("y2kice",)                      # stage 5g
SHOWN = "auto,light,dark,black,midnight,solar,paper,sage,contrast,monochrome"   # the picker's order, written by hand

def look_note(r):
    n = r.note("look ")
    return dict(p.split("=", 1) for p in n[-1].split()[1:]) if n else {}

def searches(r):
    out = []
    for ln in r.out.splitlines():
        m = re.match(r"PLSEARCH query=(.*?) hits=(.*)$", ln)
        if m:
            out.append((m.group(1), [h for h in m.group(2).split("|") if h]))
    return out

def run_with_settings(binary, work, name, settings_text, extra=None, frames=40):
    """Like shell_lib.run_app, but the app finds a SETTINGS FILE (PL_SETTINGS) and PL_THEME is not set: the theme comes from the file, the way a real start does."""
    binary, work = os.path.abspath(binary), os.path.abspath(work)
    home = os.path.join(work, "home_" + name)
    os.makedirs(home, exist_ok=True)
    path = os.path.join(work, name + "_settings.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write(settings_text)
    env = {k: v for k, v in os.environ.items() if not k.startswith("PL_") and k != "XDG_DATA_HOME"}
    env.update(HOME=home, SDL_VIDEODRIVER="offscreen", PL_WINDOW="1280x800", PL_MOTION="on", PL_FIXED_DT="0.016", PL_DYK_FIRST="3", PL_DATE="2026-10-05",
               PL_EXIT_AFTER_FRAMES=str(frames), PL_LOG_SHELL="1", PL_SETTINGS=path, PL_NEWUI="on")
    env.update(extra or {})
    bmp = os.path.join(work, name + ".bmp")
    env["PL_SCREENSHOT"] = bmp
    p = subprocess.run([binary], env=env, cwd=work, capture_output=True, text=True, errors="replace", timeout=900)
    r = Result(p.stdout, p.stderr, bmp)
    r.settings_path = path
    return r

def part_themes(binary, work):
    # 1. the list the app really uses, and the look, with no environment variable for it
    on = run_app(binary, work, "m_on", {"PL_NEWUI": "on"})
    n = look_note(on)
    check(n.get("themes") == SHOWN, f"the picker's theme list is exactly {SHOWN}: {n.get('themes')}")
    for t in ARCHIVED + REMOVED_EARLIER:
        check(t not in (n.get("themes") or "").split(","), f"the archived theme {t} is not in the picker's list")
    check(n.get("theme") == "dark" and n.get("m1") == "1" and n.get("font") == "m_inter", f"New UI On + Dark: the look M1 is on (m1=1) and the font is Inter (m_inter) with no PL_LOOK: {n}")
    check(n.get("facts") == "26", f"the fact list has 26 facts (27 in the web: its 'surprise' fact only unlocked Synthwave): {n.get('facts')}")
    check(on.scripted_missing() == [], "no scripted event was missed")
    off = run_app(binary, work, "m_off", {"PL_NEWUI": "off"})
    no = look_note(off)
    check(no.get("m1") == "0" and no.get("font") != "m_inter", f"New UI Off: the look M1 and Inter are not used: {no}")
    black = run_app(binary, work, "m_black", {"PL_NEWUI": "on"}, theme="black")
    nb = look_note(black)
    check(nb.get("theme") == "black" and nb.get("m1") == "0", f"the Black theme keeps its own colours for now (M1 is applied to Dark only; stage 7 does the other themes): {nb}")
    # the environment variable of stages 5i/5j is not read: PL_LOOK=m2 changes nothing
    envr = run_app(binary, work, "m_env", {"PL_NEWUI": "on", "PL_LOOK": "m2"})
    check(open(envr.bmp, "rb").read() == open(on.bmp, "rb").read(), "PL_LOOK=m2 changes nothing (the variable is not read any more): the picture is byte for byte the same")
    # 2. a settings file that names an archived theme opens as Dark: same theme, same picture as a file that says Dark
    ref = run_with_settings(binary, work, "m_file_dark", "theme=dark\n")
    check(look_note(ref).get("theme") == "dark", f"a settings file with theme=dark opens Dark: {look_note(ref)}")
    for t in ARCHIVED + REMOVED_EARLIER:
        r = run_with_settings(binary, work, f"m_file_{t}", f"theme={t}\nunlock=synthwave,monochrome\nach.synthwave=1789000000000\n")
        nn = look_note(r)
        check(nn.get("theme") == "dark", f"a settings file that names {t} opens as Dark: {nn.get('theme')}")
        check(open(r.bmp, "rb").read() == open(ref.bmp, "rb").read(), f"{t}: the picture is the same as for a file that says Dark (nothing of the old theme is left on screen)")
        check("error" not in r.err.lower() and "unknown" not in r.out.lower(), f"{t}: no error or message on the console")
        again = run_with_settings(binary, work, f"m_file2_{t}", open(r.settings_path, encoding="utf-8").read())
        check(look_note(again).get("theme") == "dark", f"{t}: a second start from the same file is still Dark")
    # 3. search finds none of the archived words (a control word finds something, so the box works)
    words = ["synthwave", "frutiger", "y2k", "theme effects", "neon nights"]
    items, f = [], 10
    for w in ["moo"] + words:
        items += [(f, "key=ctrl+k"), (f + 3, "text=" + w), (f + 8, "key=escape")]
        f += 14
    sr = run_app(binary, work, "m_search", {"PL_NEWUI": "on", "PL_SCRIPT": ";".join(f"{a}:{b}" for a, b in items)}, frames=f + 6)
    ss = searches(sr)
    check(any(q == "moo" and h for q, h in ss), "the control word 'moo' finds results (the search box works in this run)")
    for w in words:
        hits = [h for q, h in ss if q == w]
        check(hits != [], f"the search for {w!r} really ran (the app logged the query)")
        check(all(not h for h in hits), f"search for {w!r} finds nothing: {hits[:1]}")

PARTS = {"themes": part_themes}

if __name__ == "__main__":
    PARTS[sys.argv[3]](sys.argv[1], sys.argv[2])
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
