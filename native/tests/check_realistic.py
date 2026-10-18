#!/usr/bin/env python3
# 0.21 stage 1: the Molecule Lab's Realistic style in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU, NO timing claims: counts and the CPU draw-list time only).
#   Four pictures to look at (1280x800): water's cloud (Dark), water's surface coloured by the potential (Dark), benzene's surface + potential (Light), CO2's potential (Dark).
#   Physics read from the pixels: water's surface is red (negative) beyond the O and blue (positive) beyond the H atoms; CO2 is red beyond both O atoms; benzene red over the ring's
#   face, blue at its rim. The draw counts (the cloud one instanced call, the nuclei one, the surface one run of triangles), the triangles sorted again only when the view moves,
#   a click on a nucleus picks it, the Realistic fields saved and read back, a damaged Realistic field falls back.
#   check_realistic.py <PhysicsLab binary> <work folder>
import os, sys, re, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def stats(r):
    return [dict((k, v) for k, v in re.findall(r"(\w+)=(\S+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLMOLSTAT ")]
def atoms(r):
    last = {}
    for ln in r.out.splitlines():
        if ln.startswith("PLMOLATOM "):
            d = dict(re.findall(r"(\w+)=(\S+)", ln))
            last[int(d["i"])] = (float(d["x"]), float(d["y"]))
    return last
def notes(r, prefix):
    return [n for n in r.notes if n.startswith(prefix)]
def patch(b, x, y, n=3):            # the mean colour of a (2n+1)^2 patch
    s = [0, 0, 0]; k = 0
    for dx in range(-n, n + 1):
        for dy in range(-n, n + 1):
            p = b.px(int(x) + dx, int(y) + dy); s = [s[i] + p[i] for i in range(3)]; k += 1
    return tuple(v / k for v in s)
def red(c): return c[0] > c[2] + 25 and c[0] > c[1] + 15
def blue(c): return c[2] > c[0] + 25

def main(binary, work):
    base = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1", "PL_MOLECULE_STYLE": "real"}
    # 1. water, the cloud only, Dark
    r = run_app(binary, work, "real_h2o_dots", dict(base, PL_MOLECULE="H2O", PL_MOLECULE_REAL="rshow=cloud", PL_SETTINGS=os.path.join(work, "s1.txt")), frames=20, theme="dark")
    st = stats(r)
    check(len(st) >= 10, "dots: the draw counts were printed")
    if st:
        v = st[-1]
        print("h2o dots:", v)
        check(v["mode"] == "realistic" and v["mol"] == "H2O" and v["atoms"] == "3" and v["sticks"] == "0", f"dots: water, Realistic, 3 nuclei, no bonds ({v})")
        check(55000 <= int(v["dots"]) <= 60000 and v["tris"] == "0", f"dots: about 60,000 dots on screen, no surface ({v['dots']}, {v['tris']})")
        check(int(v["calls"]) <= 3, f"dots: the cloud is one instanced draw call and the nuclei one ({v['calls']} calls in the frame)")
    a = atoms(r)
    if a and os.path.exists(r.bmp):
        b = Bmp(r.bmp)
        ox, oy = a[0]
        core, far = patch(b, ox + 4, oy + 4), patch(b, 60, 400)
        check(sum(core) > sum(far) + 120, f"dots: the cloud is bright round the oxygen ({core}) against the empty page ({far})")
    check(bool(notes(r, "molecule H2O style=real")), "dots: the bar's state says Realistic")
    # 2. water, the surface coloured by the potential, Dark
    r = run_app(binary, work, "real_h2o_surface", dict(base, PL_MOLECULE="H2O", PL_MOLECULE_REAL="rshow=surface rcol=pot riso=0.002"), frames=20, theme="dark")
    st = stats(r)
    if st:
        v = st[-1]
        print("h2o surface:", v)
        check(1000 < int(v["tris"]) < 30000 and v["dots"] == "0", f"surface: a triangle surface, no dots ({v['tris']})")
        check(all(s["resorted"] == "0" for s in st[3:]), "surface: the triangles are not sorted again while the view stands still")
    a = atoms(r)
    if len(a) == 3 and os.path.exists(r.bmp):
        b = Bmp(r.bmp)
        (ox, oy), (h1x, h1y), (h2x, h2y) = a[0], a[1], a[2]
        mx, my = (h1x + h2x) / 2, (h1y + h2y) / 2
        dx, dy = ox - mx, oy - my
        L = math.hypot(dx, dy) or 1
        po = (ox + dx / L * 95, oy + dy / L * 95)                   # beyond the O, away from the H atoms
        ph = (h1x + (h1x - ox) * 0.45, h1y + (h1y - oy) * 0.45)       # beyond H1
        co, ch = patch(b, *po), patch(b, *ph)
        print("h2o O side", co, "H side", ch)
        check(red(co), f"surface: red (negative) beyond the oxygen {co}")
        check(blue(ch), f"surface: blue (positive) beyond the hydrogen {ch}")
    # 3. benzene, surface + potential, Light
    r = run_app(binary, work, "real_c6h6_light", dict(base, PL_MOLECULE="C6H6", PL_MOLECULE_REAL="rshow=surface rcol=pot"), frames=20, theme="light")
    st = stats(r)
    if st:
        v = st[-1]
        print("c6h6:", v)
        check(v["atoms"] == "12" and 1000 < int(v["tris"]) < 30000, f"benzene: 12 nuclei and a surface ({v})")
    a = atoms(r)
    if len(a) == 12 and os.path.exists(r.bmp):
        b = Bmp(r.bmp)
        cx = sum(a[i][0] for i in range(6)) / 6; cy = sum(a[i][1] for i in range(6)) / 6
        spread = max(math.hypot(a[i][0] - cx, a[i][1] - cy) for i in range(6))
        hx, hy = a[6]
        rim = (hx + (hx - cx) * 0.35, hy + (hy - cy) * 0.35)        # just inside the surface's in-plane edge beyond H1
        cc, cr_ = patch(b, cx, cy), patch(b, *rim)
        print("c6h6 centre", cc, "rim", cr_, "ring radius on screen", spread)
        if spread > 60:                                              # seen roughly face-on: the ring's face is negative (the pi electrons), the H rim positive
            check(red(cc), f"benzene: red over the ring's face {cc}")
            check(blue(cr_), f"benzene: blue at the H rim {cr_}")
        else:
            check(False, f"benzene: expected the default view to show the ring face-on (ring radius {spread:.0f} px)")
    # 4. CO2, the potential, Dark
    r = run_app(binary, work, "real_co2", dict(base, PL_MOLECULE="CO2", PL_MOLECULE_REAL="rshow=surface rcol=pot"), frames=20, theme="dark")
    a = atoms(r)
    if len(a) == 3 and os.path.exists(r.bmp):
        b = Bmp(r.bmp)
        (cx, cy), (o1x, o1y), (o2x, o2y) = a[0], a[1], a[2]
        e1 = (o1x + (o1x - cx) * 1.1, o1y + (o1y - cy) * 1.1)       # near the tips of the surface on the axis (the 0.002 surface ends 2.75 A from C; the O atoms are at 1.162 A)
        e2 = (o2x + (o2x - cx) * 1.1, o2y + (o2y - cy) * 1.1)
        ax, ay = o2x - o1x, o2y - o1y
        L = math.hypot(ax, ay) or 1
        belt = (cx - ay / L * 0.6 * L, cy + ax / L * 0.6 * L)           # beside the carbon, 0.6 O-O distances off the axis (inside the surface's waist)
        c1, c2, cb = patch(b, *e1), patch(b, *e2), patch(b, *belt)
        print("co2 ends", c1, c2, "belt", cb, "atoms", a)
        # the O ends are only about -0.020 au against the +0.058 au at the carbon's waist (the symmetric scale), so they are a paler red than water's oxygen
        check(c1[0] > c1[2] + 12 and c2[0] > c2[2] + 12, f"CO2: red (negative) beyond both oxygens {c1} {c2}")
        check(blue(cb), f"CO2: blue (positive) round the carbon's waist {cb}")
    # 5. a click on a nucleus picks it (water's H1), as in the other styles
    r0 = run_app(binary, work, "real_h2o_surface", dict(base, PL_MOLECULE="H2O", PL_MOLECULE_REAL="rshow=surface rcol=pot riso=0.002"), frames=20, theme="dark")
    a = atoms(r0)
    if len(a) == 3:
        hx, hy = a[1]
        r6 = run_app(binary, work, "real_click", dict(base, PL_MOLECULE="H2O", PL_SCRIPT=f"6:clickxy={hx:.0f},{hy:.0f}"), frames=12)
        s6 = [n.split("sel=")[1] for n in notes(r6, "molecule H2O")]
        check(bool(s6) and s6[-1] == "1,-1", f"a click on the H1 nucleus picks atom 1 ({s6[-1] if s6 else None})")
    # 6. settings: Realistic chosen in the panel, then the dots and the isovalue; there at the next start. A damaged Realistic field falls back to its default
    sf = os.path.join(work, "s_real.txt")
    if os.path.exists(sf):
        os.remove(sf)
    env = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1", "PL_MOLECULE": "H2O", "PL_SETTINGS": sf, "PL_SCRIPT": "5:click=molecule.style.real;8:click=molecule.real.dots20k;11:click=molecule.real.iso0016"}
    run_app(binary, work, "real_save", env, frames=16)
    txt = open(sf).read() if os.path.exists(sf) else ""
    check(re.search(r"^molecule=v1 mol=H2O style=real .* rdots=20000 riso=0\.0016 rcol=pot rshow=both$", txt, re.M) is not None, "the settings file has Realistic, 20,000 dots, 0.0016")
    r7 = run_app(binary, work, "real_load", {"PL_NEWUI": "on", "PL_DRAWSTAT": "1", "PL_SETTINGS": sf}, frames=8)
    st7 = stats(r7)
    check(bool(st7) and st7[-1]["mode"] == "realistic" and 18000 <= int(st7[-1]["dots"]) <= 20000, f"the next start opens the Realistic style with 20,000 dots ({st7[-1] if st7 else None})")
    bad = os.path.join(work, "s_real_bad.txt")
    open(bad, "w").write("lab=Molecule Lab\nmolecule=v1 mol=H2O style=real rdots=12 riso=x rcol=? rshow=none\n")
    r8 = run_app(binary, work, "real_bad", {"PL_NEWUI": "on", "PL_DRAWSTAT": "1", "PL_SETTINGS": bad}, frames=8)
    st8 = stats(r8)
    check(bool(st8) and st8[-1]["mode"] == "realistic" and 55000 <= int(st8[-1]["dots"]) <= 60000 and int(st8[-1]["tris"]) > 0, f"damaged Realistic fields: the defaults (60,000 dots, cloud and surface), no crash ({st8[-1] if st8 else None})")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    main(binary, work)
    print(f"check_realistic: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
