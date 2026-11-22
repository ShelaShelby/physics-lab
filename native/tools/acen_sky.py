#!/usr/bin/env python3
"""0.73.0: the distance of every star of the Orbit Lab's sky (src/orbit/sky_data.inc kSkyStars, 2,874 stars), so the Alpha Centauri scene can show the
sky as seen from there (src/orbit/alphacen.cpp acenSky: each star moved by the parallax of the 1.33 pc step, its V by the inverse square of its new distance).

  PL_SOURCES_DIR=<checkout of ShelaShelby/physics-lab-sources> python3 native/tools/acen_sky.py     writes native/src/orbit/sky_dist.inc

The source: the HYG Database v4.1 (D. Nash, astronexus; hygdata_v41.csv, CC BY-SA 4.0, the sky's own catalogue; saved in the sources repo as
data/stars/hygdata_v41.csv, native/data/SOURCES.md), column "dist" (parsecs; 100000 = no parallax). Each catalogue star is matched to the HYG row
nearest in direction (J2000 equator -> ecliptic by the lab's obliquity 84381.448") among the rows within 0.03 degrees whose V ("mag") cut to 0.1 is the
catalogue's (its V and places are cut, not rounded: checked on samples); the catalogue keeps its own direction and V (the match only gives the distance). Python 3, standard library only."""
import csv, math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "src", "orbit"))
sys.path.insert(0, HERE)
EPS = math.radians(84381.448 / 3600)


def catalogue():
    text = open(os.path.join(SRC, "sky_data.inc"), encoding="utf-8").read()
    body = text.split("kSkyStars =", 1)[1].split(";", 1)[0]
    s = "".join(re.findall(r'"([^"]*)"', body))
    out = []
    for i in range(len(s) // 10):
        t = s[10 * i:10 * i + 10]
        lon, lat, mag = int(t[0:3], 36) / 100.0, int(t[3:6], 36) / 100.0 - 90, int(t[6:8], 36) / 10.0 - 2
        out.append((lon, lat, mag))
    return out


def ecl(ra_h, dec_d):
    a, d = math.radians(ra_h * 15), math.radians(dec_d)
    x, y, z = math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d)
    y, z = math.cos(EPS) * y + math.sin(EPS) * z, -math.sin(EPS) * y + math.cos(EPS) * z
    return math.degrees(math.atan2(y, x)) % 360, math.degrees(math.asin(max(-1, min(1, z))))


def main():
    src = os.environ.get("PL_SOURCES_DIR")
    if not src:
        print("PL_SOURCES_DIR is not set: the HYG Database (data/stars/hygdata_v41.csv) is in the sources repo"); return 1
    rows = []
    with open(os.path.join(src, "data", "stars", "hygdata_v41.csv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            try:
                m = float(r["mag"])
            except ValueError:
                continue
            if m > 7.0 or r["proper"] == "Sol":
                continue
            lo, la = ecl(float(r["ra"]), float(r["dec"]))
            rows.append((lo, la, m, float(r["dist"]), r["id"]))
    cat = catalogue()
    dist, miss, nopar = [], 0, 0
    for (lon, lat, mag) in cat:
        best, bd = None, 1e9
        for (lo, la, m, d, i) in rows:
            if abs(la - lat) > 0.03 or not (-0.011 <= m - mag < 0.111):   # (the catalogue's V and places are cut to 0.1 and 0.01, not rounded)
                continue
            dl = (lo - lon + 180) % 360 - 180
            sep = math.hypot(dl * math.cos(math.radians(lat)), la - lat)
            if sep < 0.03 and sep < bd:
                best, bd = (d, i), sep
        if best is None:
            miss += 1; dist.append(0.0)
        elif best[0] >= 100000:
            nopar += 1; dist.append(0.0)
        else:
            dist.append(best[0])
    lines = ["// Made by native/tools/acen_sky.py (0.73.0) from the HYG Database v4.1 (hygdata_v41.csv, CC BY-SA 4.0; sources repo data/stars/hygdata_v41.csv): the",
             "// distance in parsecs of each star of kSkyStars, in its order (0: no distance, the star was not matched or has no parallax; it stays where it is).",
             f"// {len(cat)} stars: {len(cat) - miss - nopar} with a distance, {miss} not matched, {nopar} without a parallax.",
             f"static const float kSkyStarPc[{len(cat)}] = {{"]
    vals = [("%.5g" % d) for d in dist]
    vals = [v if ("." in v or "e" in v) else v + ".0" for v in vals]   # (a float literal needs a point: 125f is not one)
    for k in range(0, len(vals), 16):
        lines.append("    " + ", ".join(v + "f" for v in vals[k:k + 16]) + ",")
    lines.append("};")
    open(os.path.join(SRC, "sky_dist.inc"), "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    print(f"{len(cat)} stars: {len(cat) - miss - nopar} with a distance, {miss} not matched, {nopar} without a parallax")
    return 0


if __name__ == "__main__":
    sys.exit(main())
