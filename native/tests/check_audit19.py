#!/usr/bin/env python3
# 0.19 audit fixes checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real mouse). One part per fix:
#   monokey    the Atom Lab guide's clickable word "monochrome" (Sharp Eyes): its click had no action since 0.16 stage 1b (the action was set by the old header's fact line,
#              removed with the old interface), so the Monochrome theme and the Sharp Eyes achievement could not be found. The click must unlock the theme and earn the
#              achievement, saved when the app closes.
#   molsearch  search (Ctrl+K) in the Molecule Lab offered "Orbit Lab changelog", "Put this lab's settings back…" and "Legend", which do nothing there (the Molecule Lab has
#              no changelog of its own, no Defaults and no legend; its menu already leaves them out): search must not list them.
#   check_audit19.py <PhysicsLab binary> <work folder> <part>
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def monokey(binary, work):
    sf = os.path.join(work, "settings.txt")
    if os.path.exists(sf):
        os.remove(sf)
    r = run_app(binary, work, "monokey", {"PL_ATOM": "1", "PL_ATOM_LEARN": "1", "PL_ATOM_JUMP": "scale", "PL_SETTINGS": sf, "PL_SCRIPT": "20:click=html.monoKey"}, frames=40)
    check(not r.scripted_missing(), f"the word is on screen ({r.scripted_missing()})")
    text = open(sf).read() if os.path.exists(sf) else ""
    unlock = [ln for ln in text.splitlines() if ln.startswith("unlock=")]
    check(bool(unlock) and "monochrome" in unlock[0][7:].split(","), f"after the click the Monochrome theme is unlocked ({unlock}; PL_THEME, a test switch, also puts the test's theme in the list)")
    check("ach.monochrome=" in text, "after the click the Sharp Eyes achievement is saved")

def molsearch(binary, work):
    for word in ("changelog", "settings back", "legend"):
        r = run_app(binary, work, "molsearch_" + word.replace(" ", "_"), {"PL_MOLECULE": "H2O", "PL_SCRIPT": f"10:key=ctrl+k;16:text={word}"}, frames=30)
        n = r.note("searchcount ")
        check(bool(n) and n[-1] == "searchcount 0", f"Molecule Lab, search \"{word}\": nothing that does nothing here is listed ({n[-1:] if n else 'no search shown'})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"monokey": monokey, "molsearch": molsearch}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
