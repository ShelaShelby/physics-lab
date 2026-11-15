#!/usr/bin/env python3
# 0.23: the Molecule Lab's vibrations, checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real mouse). The physics of the modes is in tests/test_vibration.cpp.
#   vib   a click on a mode moves the atoms (the picture changes, the panel lists the mode as chosen); Real size draws a smaller swing than 3 times bigger;
#         the swing runs at its slowed-down speed (its phase follows sin(2 pi t / T) with T = 2 s x 1000 / wavenumber), also when another lab was paused
#         (this lab has no clock; before, a pause in another lab stopped the Molecule Lab's motion); Density shows no vibration; another molecule stands still again
#   orbits  stage 2, the Orbit Lab: "Orbit paths" draws each planet's orbit (the picture changes when it is switched off) and is saved only when off; zoomed in on
#           Earth the picture is not tinted green (the habitable-zone band, far wider than the picture, fades out)
#   check_023.py <PhysicsLab binary> <work folder> <part>
import math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def vibnote(r):
    n = r.note("vib ")
    if not n: return None
    return dict(kv.split("=") for kv in n[-1].split()[1:])

def differ(a, b, x0, y0, x1, y1):
    A, B = Bmp(a.bmp), Bmp(b.bmp)
    return sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if A.px(x, y) != B.px(x, y))

def vib(binary, work):
    still = run_app(binary, work, "vib_still", {"PL_MOLECULE": "H2O"}, frames=20)
    bend = run_app(binary, work, "vib_bend", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "8:click=molecule.vib.0"}, frames=20)
    v = vibnote(bend)
    check(v is not None and v["mode"] == "0" and v["on"] == "1" and float(v["phase"]) == 1.0, f"Reduced motion: the bend is shown, held at its turning point ({v})")
    check(differ(still, bend, 0, 60, 900, 740) > 50, "the bend moves the atoms in the picture")
    real = run_app(binary, work, "vib_real", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "8:click=molecule.vib.0;18:click=molecule.vib.real"}, frames=26)
    check(vibnote(real) and vibnote(real)["size"] == "0", "Real size can be chosen")
    check(0 < differ(still, real, 0, 60, 900, 740) < differ(still, bend, 0, 60, 900, 740), "Real size moves the atoms less than 3 times bigger")
    # the swing runs (Motion Full), at sin(2 pi t / T); T for the bend from the panel's own wavenumber: 2 s x 1000 / nu
    rows = [t[2] for t in bend.texts if re.match(r"\d+ cm⁻¹  ·  ", t[2])]
    nu = float(rows[0].split()[0]) if rows else None
    check(nu is not None and 1400 < nu < 1800, f"the first row is the bend's wavenumber ({rows[:1]})")
    for frames in (30, 45):
        r = run_app(binary, work, f"vib_run_{frames}", {"PL_MOLECULE": "H2O", "PL_MOTION": "off", "PL_SCRIPT": "4:key=1;5:key=space;8:key=3;10:click=molecule.vib.0"}, frames=frames)
        w = vibnote(r)
        check(w is not None and w["on"] == "1", f"{frames} frames: the bend is on ({w})")
        if w and nu:
            # the click lands 2 frames after frame 10 (shell_test.cpp); the time runs from the frame after the click, 0.016 s a frame (PL_FIXED_DT)
            ph = float(w["phase"])
            ok = any(abs(ph - math.sin(2 * math.pi * k * 0.016 / (2.0 * 1000 / nu))) < 0.02 for k in range(frames - 16, frames - 9))
            check(ok and abs(ph) < 1.0, f"{frames} frames, the Orbit Lab paused before: the swing runs at the slowed-down speed (phase {ph})")
    # 0.29 (stage 3): live, every mode at once: the row turns it on, the atoms leave their places, its clock runs (Motion Full), and the room-temperature button sets 298.15 K
    lv = run_app(binary, work, "vib_live", {"PL_MOLECULE": "H2O", "PL_MOTION": "off", "PL_SCRIPT": "4:key=1;5:key=space;8:key=3;10:click=molecule.vib.live;14:click=molecule.live.room"}, frames=40)
    w = vibnote(lv)
    check(not lv.scripted_missing() and w is not None and w["live"] == "1" and w["on"] == "0" and abs(float(w["liveK"]) - 298.15) < 1e-9 and float(w["liveT"]) > 0.2,
          f"live: on, at room temperature, its clock running ({w} {lv.scripted_missing()})")
    check(differ(still, lv, 0, 60, 900, 740) > 50, "live: the atoms move in the picture")
    # (0.24: water's electron density vibrates too, so its Density style keeps the swing; CO2, with no vibrating density yet, stands still there)
    d = run_app(binary, work, "vib_density", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "8:click=molecule.vib.0;14:click=molecule.bar.real"}, frames=22)
    check(vibnote(d) is not None and vibnote(d)["on"] == "1", f"water, Density: the vibration goes on (0.24: its density vibrates) ({vibnote(d)})")
    c = run_app(binary, work, "vib_density_co2", {"PL_MOLECULE": "CO2", "PL_SCRIPT": "8:click=molecule.vib.0;14:click=molecule.bar.real"}, frames=22)
    check(vibnote(c) is not None and vibnote(c)["on"] == "0", f"CO2, Density: no vibration (no vibrating density for it yet) ({vibnote(c)})")
    o = run_app(binary, work, "vib_other", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "8:click=molecule.vib.0;12:click=molecule.pick.CO2"}, frames=20)
    check(vibnote(o) is not None and vibnote(o)["on"] == "0", f"another molecule stands still ({vibnote(o)})")

def orbits(binary, work):
    for f in ("s_on.txt", "s_off.txt"):                    # (fresh settings: a file left by the last run made the switch start off, and the test passed and failed in turn)
        if os.path.exists(os.path.join(work, f)): os.remove(os.path.join(work, f))
    on = run_app(binary, work, "orb_on", {"PL_SETTINGS": os.path.join(work, "s_on.txt")}, frames=20)
    off = run_app(binary, work, "orb_off", {"PL_SETTINGS": os.path.join(work, "s_off.txt"), "PL_SCRIPT": "8:click=show.orbits"}, frames=20)
    n = differ(on, off, 0, 60, 900, 740)
    check(n > 200, f"switching Orbit paths off changes the picture ({n} sampled pixels)")
    s_on = open(os.path.join(work, "s_on.txt")).read() if os.path.exists(os.path.join(work, "s_on.txt")) else ""
    s_off = open(os.path.join(work, "s_off.txt")).read() if os.path.exists(os.path.join(work, "s_off.txt")) else ""
    check("orbits" not in s_on and "orbits=0" in s_off.replace(" ", ""), "the setting is saved only when off")
    e = run_app(binary, work, "orb_earth", {"PL_SELECT": "Earth", "PL_ZOOM": "3"}, frames=20)
    b = Bmp(e.bmp)
    px = [b.px(x, y) for x in range(20, 560, 12) for y in range(80, 700, 12)]
    tint = sum(p[1] - (p[0] + p[2]) / 2 for p in px) / len(px)
    check(tint < 4, f"zoomed in on Earth the picture is not green (green excess {tint:.1f})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"vib": vib, "orbits": orbits}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
