#!/usr/bin/env python3
"""0.96.0 (outside review of 28 Nov 2026, part E): fit a precessing Kepler ellipse to the Moon of ELP2000-82B, for dates outside 1800 to 1 Jan 2100.

The ellipse: semi-major axis a, eccentricity e and inclination i fixed; mean longitude L, longitude of perigee varpi and longitude of the node Omega
linear in time; geocentric, in the J2000 ecliptic and equinox (the frame of orbit::moonGeocentricElp). Fitted by Gauss-Newton least squares to
ELP2000-82B positions every 0.25 day from 1 Jan 1800 12:00 TDB to 1 Jan 2100 12:00 TDB (the span the app checks ELP over).
The residual is the position difference over 384,400 km, so 1 rad of angle weighs like 384,400 km of distance.

Input: a binary file of (jd, x, y, z) doubles, x y z in metres, made by a small C++ program linked to orbit_core:
    Vec3 p = orbit::moonGeocentricElp(jd); fwrite(jd, p.x, p.y, p.z)   (jd = 2378497.0 .. 2488070.0, step 0.25)
Output: the fitted constants for src/orbit/moon_ellipse.cpp and the worst angle and distance error over the fit's samples, and over a second file.
Usage: python3 native/tools/fit_moon_ellipse.py fit.bin [wide.bin]
"""
import sys
import numpy as np

J2000 = 2451545.0
R0 = 384400e3


def load(path):
    v = np.fromfile(path, dtype=np.float64).reshape(-1, 4)
    return v[:, 0], v[:, 1:]


def model(p, jd):
    a, e, inc, L0, Ld, w0, wd, O0, Od = p
    t = jd - J2000
    L = L0 + Ld * t; w = w0 + wd * t; O = O0 + Od * t
    M = L - w
    E = M + e * np.sin(M)
    for _ in range(6):
        E = E - (E - e * np.sin(E) - M) / (1 - e * np.cos(E))
    xo = a * (np.cos(E) - e); yo = a * np.sqrt(1 - e * e) * np.sin(E)
    om = w - O
    co, so, cO, sO, ci, si = np.cos(om), np.sin(om), np.cos(O), np.sin(O), np.cos(inc), np.sin(inc)
    x = (cO * co - sO * so * ci) * xo + (-cO * so - sO * co * ci) * yo
    y = (sO * co + cO * so * ci) * xo + (-sO * so + cO * co * ci) * yo
    z = (so * si) * xo + (co * si) * yo
    return np.stack([x, y, z], axis=1)


def errors(p, jd, r):
    m = model(p, jd)
    cosang = np.sum(m * r, axis=1) / np.linalg.norm(m, axis=1) / np.linalg.norm(r, axis=1)
    ang = np.degrees(np.arccos(np.clip(cosang, -1, 1)))
    dist = np.linalg.norm(m - r, axis=1)
    return ang, dist


def main():
    jd, r = load(sys.argv[1])
    d2r = np.pi / 180
    # starting guesses only (the fit moves them): round numbers near the Moon's mean orbit
    p = np.array([384400e3, 0.055, 5.1 * d2r, 218.3 * d2r, 13.176 * d2r, 83.4 * d2r, 0.1114 * d2r, 125.0 * d2r, -0.05295 * d2r])
    scale = np.array([1e6, 1e-3, 1e-3, 1e-3, 1e-9, 1e-3, 1e-9, 1e-3, 1e-9])
    for it in range(30):
        f0 = ((model(p, jd) - r) / R0).ravel()
        J = np.empty((f0.size, p.size))
        for k in range(p.size):
            q = p.copy(); q[k] += scale[k]
            J[:, k] = (((model(q, jd) - r) / R0).ravel() - f0) / scale[k]
        du, *_ = np.linalg.lstsq(J * scale, -f0, rcond=None)   # in units of each parameter's step, so the columns are of like size
        dp = du * scale
        p = p + dp
        if np.max(np.abs(dp) / scale) < 1e-6:
            break
    print("iterations", it + 1)
    names = ["a_m", "e", "i_rad", "L0_rad", "Ldot_rad_per_day", "varpi0_rad", "varpidot_rad_per_day", "Omega0_rad", "Omegadot_rad_per_day"]
    for n, v in zip(names, p):
        print(f"{n} = {v:.17g}")
    ang, dist = errors(p, jd, r)
    print(f"fit span: {jd[0]} .. {jd[-1]}, {jd.size} samples: worst angle {ang.max():.6f} deg at JD {jd[ang.argmax()]}, rms {np.sqrt(np.mean(ang**2)):.6f} deg;"
          f" worst distance {dist.max()/1e3:.3f} km at JD {jd[dist.argmax()]}")
    if len(sys.argv) > 2:
        jw, rw = load(sys.argv[2])
        aw, dw = errors(p, jw, rw)
        for lo, hi in [(2232000, 2305000), (2305000, 2378497), (2378497, 2488070), (2488070, 2561000), (2561000, 2634000)]:
            s = (jw >= lo) & (jw < hi)
            if s.any():
                print(f"  against ELP over JD {lo} .. {hi}: worst angle {aw[s].max():.4f} deg, worst distance {dw[s].max()/1e3:.1f} km")


main()
