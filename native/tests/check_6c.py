#!/usr/bin/env python3
# 0.16 stage 3: the nucleus view at full zoom, checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU, NO timing: counts only).
#   fullzoom: lawrencium-266 at the largest zoom (x10,000,000) with "+ Sea": the picture uploads only what can reach the screen.
#             Measured on this commit's parent (the same settings, 1280x800): Accurate 311,076 triangle vertices and 843 blobs a frame (the antiquark rings,
#             up to 720 line pieces each, and blobs wholly off the picture), Cinematic on a light theme the same; after the fix 36 vertices and 24-25 blobs.
#             The limits below sit far between the two. The picture itself is unchanged (compared pixel for pixel in REPORT.md).
#   ds281sea (0.16 stage 5): darmstadtium-281, Cloud view, x2,063, Accurate, "+ Sea", the camera turning every frame. Measured on this commit's parent (1280x800):
#             449 draw calls and 33,323 triangle vertices a frame (each antiquark ring was 24+ line pieces plus a break of the blob batch); after: 5 calls, 30 vertices.
#   check_6c.py <PhysicsLab binary> <work folder> fullzoom|ds281sea
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def part_fullzoom(binary, work):
    base = "Z=103;N=163;view=Orbit;ostyle=Modern;style=Fuzzy;detail=+ Sea;zoom=7"
    for q, theme in (("Accurate", "dark"), ("Classic", "dark"), ("Cinematic", "paper"), ("Cinematic", "dark")):
        r = run_app(binary, work, f"c_fz_{q}_{theme}", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": f"{base};qstyle={q}", "PL_DRAWSTAT": "1", "PL_SCRIPT": "80:astate"}, frames=90, theme=theme)
        st = [ln for ln in r.out.splitlines() if ln.startswith("PLASTATE ")]
        check(bool(st) and f"qstyle={q}" in st[-1] and "zoom=7\t" in st[-1] and "Z=103\t" in st[-1], f"{q} {theme}: the settings took")
        ds = [ln for ln in r.out.splitlines() if ln.startswith("PLDRAWSTAT ")]
        check(bool(ds), f"{q} {theme}: the draw counts were printed")
        if not ds:
            continue
        v = dict((k, int(x)) for k, x in re.findall(r"(\w+)=(\d+)", ds[-1]))
        print(f"{q} {theme}: {ds[-1]}")
        check(v["vertices"] <= 1000, f"{q} {theme}: {v['vertices']} triangle vertices a frame (was 311,076 for Accurate)")
        check(v["instances"] <= 100, f"{q} {theme}: {v['instances']} blobs a frame (was 843 for Accurate, 922 for Cinematic)")

def part_ds281sea(binary, work):
    r = run_app(binary, work, "c_ds281", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "Z=110;N=171;view=Cloud;style=Fuzzy;detail=+ Sea;qstyle=Accurate;zoom=3.3145",
                                          "PL_DRAWSTAT": "1", "PL_ATOM_SPIN": "0.15"}, frames=60, theme="light")
    ds = [dict((k, int(x)) for k, x in re.findall(r"(\w+)=(\d+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLDRAWSTAT ")][-20:]
    check(len(ds) == 20, "Ds-281: the draw counts were printed")
    check(bool(ds) and min(v["nrings"] for v in ds) >= 100, "Ds-281: the sea antiquark rings are there (at least 100 a frame)")
    for v in ds[-1:]:
        print(f"Ds-281 + Sea, Accurate, turning: {v}")
    check(bool(ds) and max(v["calls"] for v in ds) <= 20, "Ds-281: at most 20 draw calls a frame (was 449)")
    check(bool(ds) and max(v["vertices"] for v in ds) <= 1000, "Ds-281: at most 1,000 triangle vertices a frame (was 33,323)")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"fullzoom": part_fullzoom, "ds281sea": part_ds281sea}[part](binary, work)
    print(f"check_6c {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
