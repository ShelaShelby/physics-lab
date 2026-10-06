#!/usr/bin/env python3
# 0.15 stage 3: the Orbit Lab's top-10 controls through the NEW UI give exactly the same lab state as through the OLD side panel (compare state, not pixels).
#   check_shell_state.py <PhysicsLab binary> <work folder>
# Both runs press the same things through the real input path (PL_SCRIPT = mouse clicks on the controls, found by their names), starting from the same paused lab:
#   Play and Pause, Today, a jump to the date of Halley's 1986 closest approach, the Time speed slider, Moons / Famous comets / Belts and Oort cloud, Planet physics Real and
#   General relativity, Sizes Easy to see, Trails Full orbit trails, the 3D view, pick Earth (last: Today and a date jump would let it go), and Save picture.
# The state printed (PLSTATE: date, simulated time, pause, Time speed, the switches, the selection, follow) must be identical, except two things that cannot be:
#   zoom   Pick Earth zooms "so the planet fills the picture", and the picture is a different size in the two layouts (the full window against the window minus the side column)
#   tspeed a click lands on a whole pixel of the slider, and the two sliders are not the same width: the two values must agree within one pixel of travel (0.04 of the scale,
#          about 0.03 per pixel; log10 of seconds per second); the mouse is a whole number of pixels, so exact equality is not possible for a slider
# It also checks that Save picture made one PNG in each. No hash is compared; nothing here is an earlier output of ours: both sides are the real app.
import os, sys, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

OLD = ("10:click=▶ Play;14:click=❚❚ Pause;18:click=☉ Reset to today;22:click=Halley at its closest, 1986;26:click=##tspeed@0.3;30:click=Moons;34:click=Famous comets;38:click=Belts and Oort cloud;"
       "42:click=Real;46:click=General relativity (1PN);50:click=##sizes;54:click=Easy to see;58:click=##trails;62:click=Full orbit trails;66:click=##view;70:click=3D (rotatable);"
       "74:click=Earth;78:click=shot;100:state")
NEW = ("10:click=tool.play;14:click=tool.play;18:click=top.today;22:click=top.chip;26:click=jump.1986-02-09;30:key=escape;34:click=tool.speed;38:click=speed.slider@0.3;42:key=escape;"
       "46:click=tool.show;50:click=show.moons;52:click=show.comets;54:click=show.belts;56:click=show.physics@0.8;60:click=show.gr;64:key=escape;68:click=tool.scale;72:click=scale.sizes@0.8;"
       "74:click=scale.trails@0.8;78:key=escape;82:click=tool.view.3d;86:click=tool.pick;90:click=pick.Earth;94:click=tool.save;110:state")

def parse(line):
    d = {}
    for tok in line.replace("|", " ").split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d

def main():
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    shots = {}
    runs = {}
    for name, newui, script in (("old", "off", OLD), ("new", "on", NEW)):
        shots[name] = os.path.abspath(os.path.join(work, "shots_" + name))
        os.makedirs(shots[name], exist_ok=True)
        for f in glob.glob(os.path.join(shots[name], "*")):
            os.remove(f)
        runs[name] = run_app(binary, work, "state_" + name, {"PL_NEWUI": newui, "PL_SCRIPT": script, "PL_SHOT_DIR": shots[name]}, frames=112, size="1000x2800")
        check(not runs[name].scripted_missing(), f"{name}: every scripted click found its control: {runs[name].scripted_missing()}")
        check(len(runs[name].states) == 1, f"{name}: one state line was printed")
        check(len(glob.glob(os.path.join(shots[name], "*.png"))) == 1, f"{name}: Save picture made one PNG ({glob.glob(os.path.join(shots[name], '*'))})")
    if all(runs[n].states for n in runs):
        o, n = parse(runs["old"].states[0]), parse(runs["new"].states[0])
        print("old:", runs["old"].states[0]); print("new:", runs["new"].states[0])
        check(set(o) == set(n), "the same fields")
        for k in sorted(o):
            if k == "zoom":
                continue
            if k == "tspeed":
                check(abs(float(o[k]) - float(n[k])) <= 0.04, f"tspeed within one pixel of slider travel (0.03): old {o[k]} new {n[k]}")
                continue
            check(o[k] == n[k], f"{k}: old {o[k]} new {n[k]}")
        # the state really changed from the start, so the comparison is not of two untouched labs
        start = run_app(binary, work, "state_start", {"PL_NEWUI": "off", "PL_SCRIPT": "20:state"}, frames=24)
        s = parse(start.states[0])
        for k in ("moons", "comets", "belts", "real", "3D", "trueScale", "fullTrails"):
            check(s[k] != o[k], f"{k} was changed by the script (start {s[k]}, end {o[k]})")
        check(s["epochMs"] != o["epochMs"], "the date was changed by the script")
        check(o["sel"] != s["sel"], "a body was picked")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
