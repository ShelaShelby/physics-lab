#!/usr/bin/env python3
"""Write the lab's measured B(E2; 0+ -> 2+) table (src/atom/atom_data.inc, kBetaKnown) from ENSDF, as served by the IAEA's LiveChart API.

Python 3, standard library only. Run from anywhere:
    python3 -I native/tools/make_be2_data.py fetch   (network: nds.iaea.org; saves data/be2/iaea_<nuclide>_gammas.csv exactly as received, into the sources checkout: PL_SOURCES_DIR)
    python3 -I native/tools/make_be2_data.py build   (offline)
fetch: https://nds.iaea.org/relnsd/v1/data?fields=gammas&nuclides=<a><sym> (ENSDF, the evaluators and cut-off date are in each file).
build: the 2+(1) -> 0+ gamma's b_e2 (B(E2) down, in Weisskopf units) -> B(E2) up = (2*2+1)/(2*0+1) * B(E2) down = 5 B(E2) down, with the Weisskopf unit
       B_W(E2) = (1/4pi) (3/5)^2 (1.2 A^(1/3))^4 e^2 fm^4 = 0.05940 A^(4/3) e^2 fm^4 (the unit ENSDF uses), and 1 b = 100 fm^2.
       Sign of the deformation: from the measured spectroscopic quadrupole moment of the same 2+ state in the IAEA table of recommended moments
       (native/data/quadrupole/elec_mom_recomm.csv, N. J. Stone, INDC(NDS)-0833): Q(2+) < 0 means prolate (+), Q(2+) > 0 oblate (-).
       U-238: no 2+ moment in either file; kept prolate (+), the sign the lab had (no source in hand: REPORT.md of 0.28 says so).
"""
import csv, math, os, sys, urllib.request
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pl_sources   # the saved originals are in the private sources repo (PL_SOURCES_DIR)

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = pl_sources.data_dir("be2", create=True) if sys.argv[1:2] == ["fetch"] else os.path.dirname(pl_sources.path("be2/iaea_ground_states.csv"))   # in the sources checkout (PL_SOURCES_DIR), not in this repo
INC = os.path.join(HERE, "..", "src", "atom", "atom_data.inc")
QCSV = pl_sources.path("quadrupole/elec_mom_recomm.csv")
# 0.46.3 (outside review: only 11 nuclei had measured shapes): every even-even nucleus with A >= 12 that occurs in nature (an abundance in the IAEA LiveChart ground
# states, data/be2/iaea_ground_states.csv, https://nds.iaea.org/relnsd/v1/data?fields=ground_states&nuclides=all, downloaded 2026-11-08, saved as received): 168 nuclei.
GS = os.path.join(DIR, "iaea_ground_states.csv")
NUC = [(int(r["z"]), int(r["z"]) + int(r["n"]), r["symbol"].lower()) for r in csv.DictReader(open(GS, encoding="utf-8"))
       if r["abundance"] and int(r["z"]) % 2 == 0 and int(r["n"]) % 2 == 0 and int(r["z"]) + int(r["n"]) >= 12]
FRDM = os.path.join(HERE, "..", "src", "atom", "frdm_data.inc")


def fetch():
    import time
    for z, a, s in NUC:
        if os.path.exists(os.path.join(DIR, "iaea_%d%s_gammas.csv" % (a, s))):
            continue
        time.sleep(0.5)
        for f in ("gammas",):
            url = "https://nds.iaea.org/relnsd/v1/data?fields=%s&nuclides=%d%s" % (f, a, s)
            req = urllib.request.Request(url, headers={"User-Agent": "physics-lab-data-check/1.0"})
            open(os.path.join(DIR, "iaea_%d%s_%s.csv" % (a, s, f)), "wb").write(urllib.request.urlopen(req, timeout=60).read())


def build():
    import re
    q = {}
    zs = {str(n[0]) for n in NUC}
    for r in csv.DictReader(open(QCSV, encoding="utf-8")):
        if r["spin"].strip() == "2+" and r["z"] in zs:
            q[(int(r["z"]), int(r["n.n+n.z"]), float(r["energy [keV]"]))] = r["electric quadrupole [b]"]
    frdm = {(int(m.group(1)), int(m.group(2))): float(m.group(3)) for m in re.finditer(r"\{(\d+), (\d+), (-?[\d.]+)\}", open(FRDM).read())}
    out, skipped = [], []
    for z, a, s in NUC:
        rows = list(csv.DictReader(open(os.path.join(DIR, "iaea_%d%s_gammas.csv" % (a, s)))))
        # the lowest 2+ level's gamma straight to the ground state (0.46.3: not always level 1, e.g. Pb-208's first excited state is 3-)
        g = [r for r in rows if r.get("start_level_jp", "").strip() == "2+" and r.get("end_level_idx") == "0" and r.get("end_level_jp", "").strip() == "0+"]
        g.sort(key=lambda r: float(r["start_level_energy"]))
        lowest2 = min((float(r["start_level_energy"]) for r in rows if r.get("start_level_jp", "").strip() == "2+"), default=None)
        if not g or float(g[0]["start_level_energy"]) != lowest2 or not g[0]["b_e2"]:
            skipped.append("%d%s" % (a, s)); continue
        e = float(g[0]["start_level_energy"])
        wu = 1 / (4 * math.pi) * (3 / 5) ** 2 * (1.2 * a ** (1 / 3)) ** 4
        up = 5 * float(g[0]["b_e2"]) * wu / 1e4                      # e^2 b^2
        # 0.46.4 (outside review): the moment file rounds level energies (C-12: 4438 keV against ENSDF's 4439.82), so the 2+ row is matched within 5 keV (it was 1.5 keV, which missed
        # C-12's measured Q(2+) = +0.06(3) b, 1983Ve01, and drew it prolate). No other nucleus of the list has a 2+ row between 1.5 and 30 keV off (checked), so nothing else moves.
        qs = [v for (zz, aa, ee), v in q.items() if zz == z and aa == a and abs(ee - e) < 5]
        fb = frdm.get((z, a))
        if qs:
            sign, ssrc = (-1 if qs[0].lstrip().startswith("+") else 1), 1    # measured Q(2+)
        elif fb is not None and fb != 0:
            sign, ssrc = (1 if fb > 0 else -1), 2                           # 0.46.3: FRDM(2012)'s calculated sign (a model)
        else:
            sign, ssrc = 1, 3                                                # no measured or calculated sign: drawn prolate, the card says the sign is not known
        out.append("{%d, %d, %s, %d, %d}" % (z, a, "%.4g" % up, sign, ssrc))
        print("%3d %3d  E(2+) %8.3f keV  B(E2)down %s W.u. -> up %.4g e2b2  Q(2+) %s  FRDM %s  sign %+d (%d)" % (z, a, e, g[0]["b_e2"], up, qs[0] if qs else "none", fb, sign, ssrc))
    print("%d nuclei with a B(E2); no usable 2+ -> 0+ B(E2) in ENSDF for %d: %s" % (len(out), len(skipped), " ".join(skipped)))
    src = open(INC, encoding="utf-8").read()
    head = "static const BetaKnown kBetaKnown[] = {"
    i = src.index(head) + len(head)
    j = src.index("};", i)
    open(INC, "w", encoding="utf-8", newline="\n").write(src[:i] + ", ".join(out) + src[j:])


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("fetch", "build"):
        sys.exit("usage: make_be2_data.py fetch|build")
    fetch() if sys.argv[1] == "fetch" else build()
