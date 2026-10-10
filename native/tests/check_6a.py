#!/usr/bin/env python3
# 0.16 stage 1a: the Atom Lab's old-panel features that had no New UI home now have one. Checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU).
# Atom Lab, New UI on, 3 themes, ONE size. References: the old panel's own words (atom_panel.cpp: the Dirac level rows only with Dirac On; "Picture: N draw calls" line).
#   check_6a.py <PhysicsLab binary> <work folder> gaps
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

fails = checks = 0
THEMES = ("dark", "sage", "light")
SIZE = "1280x1800"

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def lab_card(r):
    last = None
    for ln in r.out.splitlines():
        if ln.startswith("PLATOMCARD card.labspectrum"):
            last = ln
    return last

def part_gaps(binary, work):
    base = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_LOG_CARD": "1"}
    for t in THEMES:
        r = run_app(binary, work, f"g_ctl_{t}", dict(base, PL_SCRIPT="10:click=atom.dirac=On;14:click=atom.restart;20:astate"), frames=30, size=SIZE, theme=t)
        miss = r.scripted_missing()
        check(not any("atom.dirac=On" in m for m in miss), f"{t}: the Dirac switch is in the Controls panel (a real click on it found it)")
        check(not any("atom.restart" in m for m in miss), f"{t}: the Restart atom action is in the Controls panel (a real click on it found it)")
        st = [ln for ln in r.out.splitlines() if ln.startswith("PLASTATE ")]
        check(bool(st) and "dirac=On" in st[-1], f"{t}: pressing Dirac On sets the setting, and Restart atom keeps the settings")
        spec = {}
        for dirac in ("Off", "On"):
            r = run_app(binary, work, f"g_spec_{dirac}_{t}", dict(base, PL_ATOM_SET=f"Z=1;N=0;dirac={dirac}", PL_SCRIPT="10:click=card.all;16:click=data.tab.spectrum"), frames=40, size=SIZE, theme=t)
            spec[dirac] = lab_card(r)
            check(spec[dirac] is not None, f"{t}: Dirac {dirac}: the Spectrum tab shows the lab's own spectrum card")
        if spec["Off"] and spec["On"]:
            check("The Dirac levels of this atom" not in spec["Off"], f"{t}: Dirac Off: no Dirac level rows")
            check("The Dirac levels of this atom" in spec["On"], f"{t}: Dirac On: the Dirac level rows are there")
        r = run_app(binary, work, f"g_set_{t}", dict(base, PL_OPEN="settings"), frames=30, size=SIZE, theme=t)
        check(any(n.startswith("drawcalls shown") for n in r.notes), f"{t}: the Settings performance card shows the draw-call line")
    print(f"check_6a gaps: {checks} checks, {fails} failed")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"gaps": part_gaps}[part](binary, work)
    sys.exit(1 if fails else 0)
