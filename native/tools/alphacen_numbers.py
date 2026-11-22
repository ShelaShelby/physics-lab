#!/usr/bin/env python3
"""0.72.1 research (native/PLAN_ALPHACEN.md): every number the plan marks "(computed here)", from the published inputs typed below.

  python3 native/tools/alphacen_numbers.py                 print the numbers (the plan copies them)
  python3 native/tools/alphacen_numbers.py --picture DIR   also draw DIR/alphacen_overview.png (needs Pillow)

Every input carries its source and place (the papers are saved in the private sources repo under data/alphacen/, native/data/alphacen/README.txt).
Constants: GM_sun (JPL DE440) and the au are the app's own (src/orbit/units.h); the Julian year (365.25 d) as there.
No input is a guess; where the papers disagree the script uses each paper's own numbers side by side.
"""
import math
import sys

GM_SUN = 1.32712440041279419e20       # m^3/s^2, src/orbit/units.h kGMSun (JPL DE440)
AU = 149597870700.0                   # m, src/orbit/units.h kAU
YEAR = 365.25 * 86400.0               # s, Julian year (src/orbit/units.h kYear)
DAY = 86400.0
GM_EARTH = 398600.435507e9            # m^3/s^2, JPL DE440 (src/orbit/nbody.h line 63)
PC_AU = 648000.0 / math.pi            # au per parsec (definition: 1 pc = 648000/pi au, IAU 2015 B2)
LY_PC = 3.26156                       # light years per parsec, as sky_data.inc uses (1 pc = 3.26156 ly)
L_SUN = 3.828e26                      # W, the nominal solar luminosity Akeson et al. 2021 state (sect. 3.1.4) and Kervella et al. 2017a use
SIGMA = 5.670374419e-8                # W m^-2 K^-4, Stefan-Boltzmann constant (CODATA 2018, exact from h, c, k)

# --- the A-B orbit: three published solutions (a in arcsec, angles deg, P yr, T0 yr, parallax mas, masses M_sun) --------------------------
ORBITS = {
    # Pourbaix & Boffin 2016 (arXiv:1601.01636), Table 1, column "HARPS + ESO Coude Echelle" (their own fit)
    "PB16": dict(a=17.66, sa=0.026, i=79.32, w=232.3, W=204.75, P=79.91, sP=0.013, T0=1955.66, e=0.524, plx=743.0, splx=1.3,
                 mA=1.133, smA=0.0050, mB=0.972, smB=0.0045),
    # Kervella et al. 2016 (arXiv:1610.06079), Table 1, column K16
    "K16": dict(a=17.592, sa=0.013, i=79.320, w=232.006, W=205.064, P=79.929, sP=0.013, T0=1955.604, e=0.5208, plx=747.17, splx=0.61,
                mA=1.1055, smA=0.0039, mB=0.9373, smB=0.0033),
    # Akeson et al. 2021 (arXiv:2104.10086), Table 8, column "Present work" (epoch 2019.5)
    "A21": dict(a=17.4930, sa=0.0096, i=79.2430, w=231.519, W=205.073, P=79.762, sP=0.019, T0=1955.564, e=0.51947, plx=750.81, splx=0.38,
                mA=1.0788, smA=0.0029, mB=0.9092, smB=0.0025),
}
# ALMA relative positions of B about A, Akeson et al. 2021 Table 6 (position angle deg, separation arcsec): the test reference for the orbit.
ALMA = [(2018.7846, 336.2542, 4.80434), (2019.5358, 342.8634, 5.21125), (2019.6505, 343.7729, 5.27869)]

# --- stars ------------------------------------------------------------------------------------------------------------------------------
L_A, L_B = 1.5059, 0.4981          # L_sun, Akeson et al. 2021 Table 8
TEFF_A, TEFF_B = 5795.0, 5231.0    # K, Kervella et al. 2017a (arXiv:1610.06185) eqs. (16), (17)
L_P, TEFF_P = 0.00151, 2900.0      # L_sun, Ribas et al. 2017 (arXiv:1704.08449) sect. 4; K, Pavlenko et al. 2017 as NIRPS 2025 Table 2 quotes
R_P = 0.141                        # R_sun, Boyajian et al. 2012 / Demory et al. 2009 as NIRPS 2025 Table 2 (Ribas 2017) quotes it
R_SUN = 695700e3                   # m, IAU 2015 nominal solar radius (src/orbit/units.h kSunRadius)
M_P = 0.1221                       # M_sun, Mann et al. 2015 relation as used by Kervella 2017, Faria 2022, NIRPS 2025 (Table 2)
PLX_P = 768.067                    # mas, Gaia DR3 as Libralato et al. 2025 (arXiv:2512.08533) Table 2 and NIRPS 2025 Table 2 give it
V_A, V_B, V_P = -0.01, 1.35, 11.13 # V mag: HYG v4.1 rows (native/data/stars/hyg_v41_far_stars_rows.csv) for A and B; Proxima 11.13, NIRPS Table 2 (Jao 2014)
MV_SUN = 4.81                      # Willmer 2018 (ApJS 236, 47) Table 3, Johnson V, Vega system (native/data/photometry)
BARY_RA, BARY_DEC = 219.85892215, -60.83163195   # deg, A-B barycentre ICRS J2019.5, Akeson et al. 2021 Table 9

# --- Proxima about A-B: Akeson et al. 2021 Table 10 (a kau, P kyr, T0 = time to periastron from "present", kyr) and Kervella et al. 2017 Table 3
PROX_ORBITS = {"A21": dict(a=8.2, e=0.497, P=511.0, T0=278.0, i=124.9, W=165.0, w=151.0),
               "K17": dict(a=8.7, e=0.50, P=547.0, T0=283.0, i=107.6, W=126.0, w=72.3)}

# --- Proxima's planets: NIRPS 2025 (arXiv:2507.21751) Table 3 (circular fits; T0 = inferior conjunction, BJD - 2450000)
PLANETS = {"b": dict(P=11.18465, K=1.226, msini=1.055, a=0.04848, T0=10548.59, S=0.641, Teq=218.0),
           "d": dict(P=5.12338, K=0.392, msini=0.260, a=0.02881, T0=10557.55, S=1.814, Teq=282.0)}

# --- Kopparapu et al. 2014 (arXiv:1404.5292) Table 1, 1 Earth mass: (Seff_sun, a, b, c, d) of Eq. (4), T* = Teff - 5780 K
KOPP = {"recent Venus": (1.776, 2.136e-4, 2.533e-8, -1.332e-11, -3.097e-15),
        "runaway greenhouse": (1.107, 1.332e-4, 1.58e-8, -8.308e-12, -1.931e-15),
        "maximum greenhouse": (0.356, 6.171e-5, 1.698e-9, -3.198e-12, -5.575e-16),
        "early Mars": (0.32, 5.547e-5, 1.526e-9, -2.874e-12, -5.011e-16)}


def kepler_E(M, e):
    E = M if e < 0.8 else math.pi
    for _ in range(60):
        E -= (E - e * math.sin(E) - M) / (1 - e * math.cos(E))
    return E


def total_mass(o):
    a_au = o["a"] / (o["plx"] / 1000.0)
    return 4 * math.pi ** 2 * (a_au * AU) ** 3 / (GM_SUN * (o["P"] * YEAR) ** 2), a_au


def rel_pos(o, t):
    """B about A on the sky at decimal year t: (position angle deg E of N, separation arcsec). Thiele-Innes, w as published."""
    M = 2 * math.pi * (t - o["T0"]) / o["P"]
    E = kepler_E(math.remainder(M, 2 * math.pi), o["e"])
    X, Y = math.cos(E) - o["e"], math.sqrt(1 - o["e"] ** 2) * math.sin(E)
    w, W, i, a = math.radians(o["w"]), math.radians(o["W"]), math.radians(o["i"]), o["a"]
    A = a * (math.cos(w) * math.cos(W) - math.sin(w) * math.sin(W) * math.cos(i))
    B = a * (math.cos(w) * math.sin(W) + math.sin(w) * math.cos(W) * math.cos(i))
    F = a * (-math.sin(w) * math.cos(W) - math.cos(w) * math.sin(W) * math.cos(i))
    G = a * (-math.sin(w) * math.sin(W) + math.cos(w) * math.cos(W) * math.cos(i))
    north, east = A * X + F * Y, B * X + G * Y
    return math.degrees(math.atan2(east, north)) % 360, math.hypot(north, east)


def hz(L, teff, key):
    s0, a, b, c, d = KOPP[key]
    T = teff - 5780.0
    seff = s0 + a * T + b * T ** 2 + c * T ** 3 + d * T ** 4
    return math.sqrt(L / seff)


def hw99(mu, e):  # Holman & Wiegert 1999 (astro-ph/9809315) Eq. (1): a_c / a_b
    return 0.464 - 0.380 * mu - 0.631 * e + 0.586 * mu * e + 0.150 * e ** 2 - 0.198 * mu * e ** 2


def mag_at(V, plx_mas, r_au):
    """apparent V of a star of apparent V seen from Earth at parallax plx, when seen from r_au away."""
    Mabs = V + 5 + 5 * math.log10(plx_mas / 1000.0)
    return Mabs + 5 * math.log10(r_au / PC_AU / 10.0)


def report():
    out = []
    p = out.append
    p("== A-B: Kepler's third law with each paper's own a, P and parallax (computed here) ==")
    for k, o in ORBITS.items():
        Mt, a_au = total_mass(o)
        rel = math.sqrt((3 * o["sa"] / o["a"]) ** 2 + (3 * o["splx"] / o["plx"]) ** 2 + (2 * o["sP"] / o["P"]) ** 2)
        pub = o["mA"] + o["mB"]
        p(f"{k}: a = {a_au:.3f} au, periastron {a_au*(1-o['e']):.2f} au, apastron {a_au*(1+o['e']):.2f} au; a^3/P^2 = {Mt:.4f} +- {Mt*rel:.4f} M_sun"
          f" (errors added in quadrature, correlations ignored); published mA+mB = {pub:.4f}; difference {Mt-pub:+.4f};"
          f" distance {1000/o['plx']:.5f} pc = {1000/o['plx']*LY_PC:.4f} ly")
    p("== A-B orbit against ALMA (Akeson 2021 Table 6), Thiele-Innes with each paper's elements (computed here) ==")
    for k, o in ORBITS.items():
        for t, pa, sep in ALMA:
            cpa, csep = rel_pos(o, t)
            dpa = (cpa - pa + 180) % 360 - 180
            p(f"{k} {t}: PA {cpa:.4f} (ALMA {pa}) dPA {dpa:+.4f} deg = {math.radians(dpa)*sep*1000:+.1f} mas across; sep {csep:.5f} (ALMA {sep}) d {1000*(csep-sep):+.1f} mas")
    o = ORBITS["A21"]
    for t in (2026.9, 2035.0, 2035.6):
        pa, sep = rel_pos(o, t)
        p(f"A21 predicted {t}: PA {pa:.2f} deg, separation {sep:.3f} arcsec = {sep/(o['plx']/1000):.2f} au projected")
    nxt = o["T0"] + o["P"]
    p(f"A21 next periastron: {nxt:.3f} (T0 + P); previous {o['T0']:.3f}")
    p("== Stability (Holman & Wiegert 1999 Eq. 1 with A21 masses, e, a; computed here) ==")
    Mt, a_au = total_mass(o)
    muA = o["mB"] / (o["mA"] + o["mB"]); muB = o["mA"] / (o["mA"] + o["mB"])
    p(f"around A (mu = mB/(mA+mB) = {muA:.4f}): a_c = {hw99(muA, o['e']):.4f} a_b = {hw99(muA, o['e'])*a_au:.2f} au")
    p(f"around B (mu = {muB:.4f}): a_c = {hw99(muB, o['e']):.4f} a_b = {hw99(muB, o['e'])*a_au:.2f} au")
    p(f"Quarles & Lissauer 2016 line fit for B: a_o = b/|m| = 0.890/0.367 = {0.890/0.367:.3f} au (their sect. 3.2 numbers)")
    p("== Habitable zones, Kopparapu et al. 2014 Eq. (4)-(5), 1 Earth mass (computed here) ==")
    for name, L, T in (("A", L_A, TEFF_A), ("B", L_B, TEFF_B), ("Proxima", L_P, TEFF_P)):
        d = {k: hz(L, T, k) for k in KOPP}
        p(f"{name} (L {L}, Teff {T:.0f}): " + ", ".join(f"{k} {v:.4f} au" for k, v in d.items()))
    p("check against NIRPS 2025 Table 2 (Proxima, runaway greenhouse to early Mars): 0.03731 and 0.088 au")
    p("== Proxima about A-B (Kepler's equation with each published orbit; computed here) ==")
    for k, q in PROX_ORBITS.items():
        M = 2 * math.pi * (-q["T0"] / q["P"])
        E = kepler_E(math.remainder(M, 2 * math.pi), q["e"])
        r = q["a"] * (1 - q["e"] * math.cos(E))
        nu = 2 * math.atan2(math.sqrt(1 + q["e"]) * math.sin(E / 2), math.sqrt(1 - q["e"]) * math.cos(E / 2))
        apo = q["T0"] - q["P"] / 2
        p(f"{k}: now r = {r:.2f} kau (true anomaly {math.degrees(nu)%360:.1f} deg); apastron {q['a']*(1+q['e']):.1f} kau, "
          f"{'in' if apo > 0 else 'was'} {abs(apo):.1f} kyr {'from now' if apo > 0 else 'ago'}; periastron {q['a']*(1-q['e']):.1f} kau in {q['T0']:.0f} kyr")
    p("measured separation (Hipparcos epoch, K16 parallax 747.17 and Benedict 1999's 768.77 for Proxima): 12 947 +- 260 au (Kervella et al. 2017, sect. 3.1)")
    # the same separation from today's numbers: A-B barycentre (Akeson Table 9, J2019.5, moved to 2016.0 with its proper motion) and Proxima (Gaia DR3 at 2016.0, Libralato Table 2)
    dt = 2016.0 - 2019.5
    ra_ab = BARY_RA + (-3639.95 * dt / 3.6e6) / math.cos(math.radians(BARY_DEC)); de_ab = BARY_DEC + 700.40 * dt / 3.6e6
    def vec(ra, de, plx):
        r = PC_AU / (plx / 1000.0); a, d = math.radians(ra), math.radians(de)
        return (r * math.cos(d) * math.cos(a), r * math.cos(d) * math.sin(a), r * math.sin(d))
    va, vp = vec(ra_ab, de_ab, ORBITS["A21"]["plx"]), vec(217.392321472, -62.676075117, PLX_P)
    dv = [x - y for x, y in zip(vp, va)]
    los = sum(x * y for x, y in zip(dv, va)) / math.sqrt(sum(x * x for x in va))
    sep = math.sqrt(sum(x * x for x in dv))
    ang = math.degrees(math.acos(sum(x * y for x, y in zip(va, vp)) / math.sqrt(sum(x * x for x in va) * sum(x * x for x in vp))))
    p(f"today's numbers (A21 parallax 750.81 +- 0.38, Gaia DR3 768.067 +- 0.050): angle on the sky {ang:.3f} deg, Proxima {-los:.0f} au nearer to us, separation {sep:.0f} au;"
      f" the parallax error of A-B alone moves it by about {PC_AU/0.75081*0.38/750.81*abs(los)/sep:.0f} au")
    p("== Proxima's planets (computed here; M_star = 0.1221, Mann 2015 as the papers use) ==")
    gm = GM_SUN * M_P
    for k, q in PLANETS.items():
        P = q["P"] * DAY
        m = q["msini"] * GM_EARTH / GM_SUN
        a = (GM_SUN * (M_P + m) * P ** 2 / (4 * math.pi ** 2)) ** (1 / 3) / AU
        K = (2 * math.pi * GM_SUN / P) ** (1 / 3) * m / (M_P + m) ** (2 / 3)
        S = L_P / a ** 2
        Teq = (L_P * L_SUN * (1 - 0.3) / (16 * math.pi * SIGMA * (a * AU) ** 2)) ** 0.25
        Teq_RT = TEFF_P * math.sqrt(R_P * R_SUN / (2 * q["a"] * AU)) * 0.7 ** 0.25
        p(f"{k}: a = {a:.5f} au (paper {q['a']}); K from m sin i = {K:.3f} m/s (paper {q['K']}); S = L/a^2 = {S:.3f} S_earth (paper {q['S']});"
          f" Teq (A = 0.3, full redistribution) from L = {Teq:.0f} K, from Teff and R (Teff sqrt(R/2a)(1-A)^1/4) = {Teq_RT:.0f} K (paper {q['Teq']});"
          f" max separation seen from Earth {a*PLX_P:.1f} mas")
        for incl in (90, 47, 20, 10):
            p(f"   true mass if i = {incl} deg: {q['msini']/math.sin(math.radians(incl)):.2f} M_earth")
    t_now = 2461002.5 - 2450000   # 2025-12-01 00:00 TT as BJD - 2450000 (example date)
    for k, q in PLANETS.items():
        n = (t_now - q["T0"]) / q["P"]
        p(f"{k}: orbits since its NIRPS reference conjunction to BJD {t_now+2450000:.1f}: {n:.3f} (phase {n%1:.3f} after inferior conjunction)")
    p("== The sky (computed here) ==")
    d = 1000 / ORBITS["A21"]["plx"]
    p(f"Sun seen from A-B: V = M_V + 5 log10(d/10 pc) = {MV_SUN + 5*math.log10(d/10):.2f} (d = {d:.5f} pc)")
    p(f"Sun seen from Proxima: V = {MV_SUN + 5*math.log10((1000/PLX_P)/10):.2f}")
    ra, dec = (BARY_RA + 180) % 360, -BARY_DEC
    h = ra / 15; p(f"Sun's direction from A-B: RA {ra:.5f} deg = {int(h)}h{int(h%1*60):02d}m{(h*60%1)*60:04.1f}s, Dec {dec:+.5f} deg (the antipode of Akeson Table 9)")
    r_now = sep   # today's separation computed above
    p(f"from Proxima (r = {r_now:.0f} au, today's separation above): A V = {mag_at(V_A, ORBITS['A21']['plx'], r_now):.2f}, B V = {mag_at(V_B, ORBITS['A21']['plx'], r_now):.2f},"
      f" A+B {-2.5*math.log10(10**(-0.4*mag_at(V_A, ORBITS['A21']['plx'], r_now))+10**(-0.4*mag_at(V_B, ORBITS['A21']['plx'], r_now))):.2f}")
    p(f"Proxima from A-B: V = {mag_at(V_P, PLX_P, r_now):.2f}")
    for name, V in (("A", V_A), ("B", V_B)):
        p(f"Proxima at periastron 4.1 kau / apastron 12.3 kau (A21): {name} V = {mag_at(V, ORBITS['A21']['plx'], 4100):.2f} / {mag_at(V, ORBITS['A21']['plx'], 12300):.2f}")
    return out


def picture(folder):
    from PIL import Image, ImageDraw, ImageFont
    import os
    W, H = 1000, 1000
    im = Image.new("RGB", (W, H), (12, 14, 22))
    dr = ImageDraw.Draw(im)
    try:
        f = ImageFont.truetype("DejaVuSans.ttf", 17); fs = ImageFont.truetype("DejaVuSans.ttf", 14)
    except OSError:
        f = fs = ImageFont.load_default()
    o = ORBITS["A21"]; Mt, a_au = total_mass(o)
    fA = o["mB"] / (o["mA"] + o["mB"])
    # panel 1: A and B about their centre of mass, to scale (face-on, the orbit's own plane)
    cx, cy, s = 270, 260, 6.2           # px per au
    dr.text((20, 15), "1. A and B, to scale (au), the orbit's own plane, 80 years", fill=(230, 230, 230), font=f)
    for body, frac, col in (("A", -fA, (255, 236, 190)), ("B", 1 - fA, (255, 190, 120))):
        pts = []
        for k in range(361):
            E = math.radians(k)
            x, y = a_au * (math.cos(E) - o["e"]), a_au * math.sqrt(1 - o["e"] ** 2) * math.sin(E)
            pts.append((cx + frac * x * s, cy - frac * y * s))
        dr.line(pts, fill=col, width=2)
        x0 = cx + frac * a_au * (1 - o["e"]) * s
        dr.ellipse((x0 - 5, cy - 5, x0 + 5, cy + 5), fill=col)
        dr.text((x0 + 8, cy - 22 if body == "A" else cy + 6), body + " at periastron", fill=col, font=fs)
    for name, L, T, frac, col in (("A", L_A, TEFF_A, -fA, (90, 200, 120)), ("B", L_B, TEFF_B, 1 - fA, (60, 160, 100))):
        x0 = cx + frac * a_au * (1 - o["e"]) * s
        for key in ("runaway greenhouse", "maximum greenhouse"):
            r = hz(L, T, key) * s
            dr.ellipse((x0 - r, cy - r, x0 + r, cy + r), outline=col, width=1)
    dr.text((20, 452), f"a = {a_au:.1f} au, e = {o['e']:.3f} (Akeson et al. 2021)", fill=(200, 200, 200), font=fs)
    dr.text((20, 470), f"closest {a_au*(1-o['e']):.1f} au, farthest {a_au*(1+o['e']):.1f} au", fill=(200, 200, 200), font=fs)
    dr.text((20, 488), "green: habitable zones (Kopparapu 2014, computed here)", fill=(120, 200, 140), font=fs)
    dr.line((60, 420, 60 + 10 * s, 420), fill=(200, 200, 200), width=2); dr.text((60, 426), "10 au", fill=(200, 200, 200), font=fs)
    # panel 2: Proxima about A-B, to scale in thousands of au
    cx2, cy2, s2 = 760, 260, 13.0         # px per kau
    q = PROX_ORBITS["A21"]
    dr.text((520, 15), "2. Proxima about A+B (thousand au)", fill=(230, 230, 230), font=f)
    pts = []
    for k in range(361):
        E = math.radians(k)
        x, y = q["a"] * (math.cos(E) - q["e"]), q["a"] * math.sqrt(1 - q["e"] ** 2) * math.sin(E)
        pts.append((cx2 + x * s2 - 60, cy2 - y * s2))
    dr.line(pts, fill=(255, 120, 100), width=2)
    dr.ellipse((cx2 - 60 - 4, cy2 - 4, cx2 - 60 + 4, cy2 + 4), fill=(255, 230, 170))
    dr.text((cx2 - 60 + 6, cy2 + 4), "A+B", fill=(255, 230, 170), font=fs)
    M = 2 * math.pi * (-q["T0"] / q["P"]); E = kepler_E(math.remainder(M, 2 * math.pi), q["e"])
    xn, yn = q["a"] * (math.cos(E) - q["e"]), q["a"] * math.sqrt(1 - q["e"] ** 2) * math.sin(E)
    X, Y = cx2 + xn * s2 - 60, cy2 - yn * s2
    dr.ellipse((X - 5, Y - 5, X + 5, Y + 5), fill=(255, 120, 100)); dr.text((X - 60, Y + 8), "Proxima now", fill=(255, 140, 120), font=fs)
    dr.text((540, 452), f"a = {q['a']} kau, e = {q['e']} (Akeson 2021)", fill=(200, 200, 200), font=fs)
    dr.text((540, 470), f"one turn: {q['P']:.0f} thousand years", fill=(200, 200, 200), font=fs)
    dr.text((540, 488), "A and B's whole orbit is a dot here", fill=(200, 200, 200), font=fs)
    # panel 3: Proxima's planets to scale (au)
    cx3, cy3, s3 = 500, 760, 2600.0
    dr.text((20, 525), "3. Proxima's planets, to scale (au), drawn face-on: their real tilt is not known", fill=(230, 230, 230), font=f)
    r1, r2 = hz(L_P, TEFF_P, "runaway greenhouse") * s3, hz(L_P, TEFF_P, "maximum greenhouse") * s3
    dr.ellipse((cx3 - r2, cy3 - r2, cx3 + r2, cy3 + r2), outline=(60, 160, 100), width=2)
    dr.ellipse((cx3 - r1, cy3 - r1, cx3 + r1, cy3 + r1), outline=(90, 200, 120), width=2)
    for k, col, ang, ty in (("b", (140, 200, 255), -40, 600), ("d", (220, 220, 140), 30, 870)):
        r = PLANETS[k]["a"] * s3
        dr.ellipse((cx3 - r, cy3 - r, cx3 + r, cy3 + r), outline=col, width=2)
        px, py = cx3 + r * math.cos(math.radians(ang)), cy3 + r * math.sin(math.radians(ang))
        dr.ellipse((px - 5, py - 5, px + 5, py + 5), fill=col)
        dr.text((740, ty), f"{k}: {PLANETS[k]['a']} au, {PLANETS[k]['P']:.2f} days,", fill=col, font=fs)
        dr.text((740, ty + 18), f"at least {PLANETS[k]['msini']} Earth masses", fill=col, font=fs)
    dr.ellipse((cx3 - 4, cy3 - 4, cx3 + 4, cy3 + 4), fill=(255, 120, 100))
    dr.text((20, 950), "green: Proxima's habitable zone (Kopparapu 2014, conservative, computed here). Planets: NIRPS 2025 (Suarez Mascareno et al.).", fill=(120, 200, 140), font=fs)
    dr.text((20, 968), "Not drawn: Proxima c (disputed, ~1.5 au) and the candidate around A (unconfirmed); see PLAN_ALPHACEN.md.", fill=(200, 200, 200), font=fs)
    dr.line((60, 900, 60 + 0.02 * s3, 900), fill=(200, 200, 200), width=2); dr.text((60, 906), "0.02 au", fill=(200, 200, 200), font=fs)
    os.makedirs(folder, exist_ok=True)
    im.save(os.path.join(folder, "alphacen_overview.png"), optimize=True)


if __name__ == "__main__":
    print("\n".join(report()))
    if len(sys.argv) > 2 and sys.argv[1] == "--picture":
        picture(sys.argv[2])
