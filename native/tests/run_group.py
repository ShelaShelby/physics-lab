#!/usr/bin/env python3
# One ctest test per screen (0.38.1). It runs the check scripts of that screen one after the other, each with its own work folder,
# and fails if any of them fails. Each member's output is printed under a header with its time, so a failure names its check.
# 0.76.0: a part is stopped after 5 minutes (PL_PART_TIMEOUT seconds) and reported as failed with its last lines. Nothing can hang for hours.
# The list of parts, their times and why each is kept is in tests/TESTS.md.
#   run_group.py <PhysicsLab binary> <work folder> script.py[:part] ...
import os, signal, subprocess, sys, time

def run_member(cmd, limit):
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", start_new_session=True)
    try:
        out, _ = p.communicate(timeout=limit)
        return p.returncode, out, False
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)      # the script and every app it started
        out, _ = p.communicate()
        return -1, out, True

def main():
    binary, work, members = sys.argv[1], sys.argv[2], sys.argv[3:]
    here = os.path.dirname(os.path.abspath(__file__))
    limit = float(os.environ.get("PL_PART_TIMEOUT", "300"))
    failed = []
    times = []
    for m in members:
        script, _, part = m.partition(":")
        sub = os.path.join(work, os.path.splitext(script)[0][len("check_"):] + ("_" + part if part else ""))
        cmd = [sys.executable, os.path.join(here, script), binary, sub] + ([part] if part else [])
        t0 = time.time()
        code, out, stopped = run_member(cmd, limit)
        dt = time.time() - t0
        times.append((dt, m))
        ok = code == 0
        state = "passed" if ok else (f"FAILED (stopped after {limit:.0f} s)" if stopped else f"FAILED (exit {code})")
        print(f"==== {m}: {state} in {dt:.0f} s", flush=True)
        lines = (out or "").strip().splitlines()
        shown = lines[-20:] if stopped else (lines if not ok else [x for x in lines if x.startswith("FAIL")] + lines[-1:])
        for ln in shown:
            print("    " + ln)
        if not ok:
            failed.append(m)
    for dt, m in sorted(times, reverse=True):
        print(f"TIME {dt:6.1f} s  {m}")
    print(f"{len(members) - len(failed)} of {len(members)} checks passed" + (": failed " + ", ".join(failed) if failed else ""))
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
