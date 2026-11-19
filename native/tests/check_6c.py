#!/usr/bin/env python3
# 0.16 stage 3: the nucleus view at full zoom, checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU, NO timing: counts only).
#   fullzoom: lawrencium-266 at the largest zoom (x10,000,000) with "+ Sea": the picture uploads only what can reach the screen.
#             Measured on this commit's parent (the same settings, 1280x800): Accurate 311,076 triangle vertices and 843 blobs a frame (the antiquark rings,
#             up to 720 line pieces each, and blobs wholly off the picture), Cinematic on a light theme the same; after the fix 36 vertices and 24-25 blobs.
#             The limits below sit far between the two. The picture itself is unchanged (compared pixel for pixel in REPORT.md).
#   ds281sea (0.16 stage 5): darmstadtium-281, Cloud view, x2,063, "+ Sea" (0.43: in Simple; Accurate draws no sea rings: its clouds are net densities), the camera turning every frame. Measured on this commit's parent (1280x800):
#             449 draw calls and 33,323 triangle vertices a frame (each antiquark ring was 24+ line pieces plus a break of the blob batch); after: 5 calls, 30 vertices.
#   nucglow (0.66.0): see part_nucglow.
#   check_6c.py <PhysicsLab binary> <work folder> fullzoom|ds281sea|nucglow
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
    base = "Z=103;N=163;view=Orbit;ostyle=Modern;style=Nucleons;detail=+ Sea;zoom=7"
    for q, theme in (("Accurate", "dark"), ("Simple", "dark"), ("Accurate", "paper")):
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
        # (0.46.1: the nucleus snapshot's axis now lies across the first view, so other nucleons sit in the zoomed window: Simple dark draws 101 blobs; the limit, chosen far between
        #  the 24-25 after the fix and the 843 before it, is now 150, still far from 843: the check is that off-picture blobs are not uploaded)
        check(v["instances"] <= 150, f"{q} {theme}: {v['instances']} blobs a frame (was 843 for Accurate, 922 for Cinematic)")

def part_ds281sea(binary, work):
    r = run_app(binary, work, "c_ds281", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "Z=110;N=171;elec=Hydrogen-like;view=Cloud;style=Nucleons;detail=+ Sea;qstyle=Simple;zoom=3.3145",   # (0.46: the zoom is relative to the one-electron scene; the default became All)
                                          "PL_DRAWSTAT": "1", "PL_ATOM_SPIN": "0.15"}, frames=60, theme="light")
    ds = [dict((k, int(x)) for k, x in re.findall(r"(\w+)=(\d+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLDRAWSTAT ")][-20:]
    check(len(ds) == 20, "Ds-281: the draw counts were printed")
    check(bool(ds) and min(v["nrings"] for v in ds) >= 100, "Ds-281: the sea antiquark rings are there (at least 100 a frame)")
    for v in ds[-1:]:
        print(f"Ds-281 + Sea, Simple, turning: {v}")
    check(bool(ds) and max(v["calls"] for v in ds) <= 20, "Ds-281: at most 20 draw calls a frame (was 449)")
    check(bool(ds) and max(v["vertices"] for v in ds) <= 1000, "Ds-281: at most 1,000 triangle vertices a frame (was 33,323)")

# 0.66.0: the nucleus views' glows as glow stacks (one quad a nucleon, kind 10; atom_view.cpp) against a blob for each glow (PL_NUC_OLDGLOW, the drawing until
# 0.66.0), Vulkan, multisampling off, the benchmark's scenes still from frame 50: atom.nucleus_zoom (30), atom.nucleon (2) and Og-294 with every nucleon large (3).
# The picture limit (set before looking): at most 1 % of the pixels more than 8 levels apart in a channel. The pixels the glows' quads cover (ncover): measured on
# 0.65.0's drawing at atom.nucleus_zoom 67,584,000 square points a frame (66 pictures), with the stacks 16,384,000; the limit: at most 30 % of the old drawing's.
def part_nucglow(binary, work):
    from shell_lib import Bmp
    for scene in (30, 2, 3):
        runs = {}
        for tag, extra in (("new", {}), ("old", {"PL_NUC_OLDGLOW": "1"})):
            env = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_GRAPHICS": "vulkan", "PL_NO_MSAA": "1", "PL_ATOM_BENCHSCENE": str(scene), "PL_ATOM_BENCHSTOP": "50", "PL_DRAWSTAT": "1"}
            env.update(extra)
            runs[tag] = run_app(binary, work, f"c_ng{scene}_{tag}", env, frames=60)
        cover = {}
        for tag, r in runs.items():
            ds = [ln for ln in r.out.splitlines() if ln.startswith("PLDRAWSTAT ")]
            cover[tag] = int(re.search(r"ncover=(\d+)", ds[-1]).group(1)) if ds else -1
        a, b = Bmp(runs["new"].bmp), Bmp(runs["old"].bmp)
        pa, pb = a.pixels(), b.pixels()
        n = min(len(pa), len(pb))
        over = sum(1 for i in range(0, n - 2, 3) if max(abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2])) > 8) if a.bpp == 3 else \
               sum(1 for i in range(0, n - 3, 4) if max(abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2])) > 8)
        frac = over / max(1, a.w * a.h)
        print(f"scene {scene}: pixels covered {cover['old']} -> {cover['new']} square points; {over} pixels ({100 * frac:.3f} %) over 8 levels")
        check((a.w, a.h) == (b.w, b.h) and a.w > 0, f"scene {scene}: both pictures taken")
        check(frac <= 0.01, f"scene {scene}: {100 * frac:.3f} % of the pixels over 8 levels (limit 1 %)")
        check(cover["new"] > 0 and cover["old"] > 0 and cover["new"] <= cover["old"], f"scene {scene}: the stacks cover no more than the old blobs")
        if scene == 30:
            check(cover["new"] > 0 and cover["new"] <= 0.30 * cover["old"], f"atom.nucleus_zoom: the glows cover {cover['new']} square points, at most 30 % of the old {cover['old']}")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"fullzoom": part_fullzoom, "ds281sea": part_ds281sea, "nucglow": part_nucglow}[part](binary, work)
    print(f"check_6c {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
