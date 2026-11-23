#!/usr/bin/env python3
# 0.19 audit, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). tests/TESTS.md lists the parts kept and why.
#   molsearch  search (Ctrl+K) in the Molecule Lab must not list "Orbit Lab changelog", "Put this lab's settings back…" or "Legend", which do nothing there.
#   check_audit19.py <PhysicsLab binary> <work folder> molsearch
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

def molsearch(binary, work):
    for word in ("changelog", "settings back", "legend"):
        r = run_app(binary, work, "molsearch_" + word.replace(" ", "_"), {"PL_MOLECULE": "H2O", "PL_SCRIPT": f"10:key=ctrl+k;16:text={word}"}, frames=30)
        n = r.note("searchcount ")
        check(bool(n) and n[-1] == "searchcount 0", f"Molecule Lab, search \"{word}\": nothing that does nothing here is listed ({n[-1:] if n else 'no search shown'})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"molsearch": molsearch}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
