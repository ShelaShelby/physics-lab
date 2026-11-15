#!/usr/bin/env python3
# 0.38.1 stage 3 (the owner: "merge them"): one ctest test per screen. It runs the check scripts of that screen one after the other, each exactly as it ran as its own test
# before (same script, same part, its own work folder), and fails if any of them fails. Every member's output is printed under a header, so a failure still names its check.
#   run_group.py <PhysicsLab binary> <work folder> script.py[:part] ...
import os, subprocess, sys, time

def main():
    binary, work, members = sys.argv[1], sys.argv[2], sys.argv[3:]
    here = os.path.dirname(os.path.abspath(__file__))
    failed = []
    for m in members:
        script, _, part = m.partition(":")
        sub = os.path.join(work, os.path.splitext(script)[0][len("check_"):] + ("_" + part if part else ""))
        cmd = [sys.executable, os.path.join(here, script), binary, sub] + ([part] if part else [])
        t0 = time.time()
        r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
        ok = r.returncode == 0
        print(f"==== {m}: {'passed' if ok else 'FAILED (exit %d)' % r.returncode} in {time.time() - t0:.0f} s")
        out = (r.stdout + r.stderr).strip().splitlines()
        for ln in (out if not ok else [x for x in out if x.startswith("FAIL")] + out[-1:]):
            print("    " + ln)
        if not ok:
            failed.append(m)
    print(f"{len(members) - len(failed)} of {len(members)} checks passed" + (": failed " + ", ".join(failed) if failed else ""))
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
