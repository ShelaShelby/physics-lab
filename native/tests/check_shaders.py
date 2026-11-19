#!/usr/bin/env python3
"""Compiles every GLSL shader of the renderer (src/render/shaders.h) with glslang: the vertex and fragment shaders as GLSL 330 core, alone and linked in the pairs the renderer links,
and (0.13 stage 2) the compute shaders (names ending in CS) as GLSL 430 core.

A shader that does not compile (0.55.7: or gives a warning; a compute shader also as SPIR-V for OpenGL, validated by spirv-val when installed), or a pair
whose outputs and inputs do not match, fails the test. glslang is the Khronos reference compiler: it needs no graphics card.
(It checks the language; what a GPU's own driver does with the shader is only tested on a real PC.)

Usage: check_shaders.py path/to/shaders.h
Exit codes: 0 all compile, 1 a shader failed, 77 glslang not installed (ctest shows "skipped"; set PL_REQUIRE_GLSLANG=1 to make that a failure instead).
"""
import os, re, shutil, subprocess, sys, tempfile

# the pairs renderer2d.cpp links, in init()
PAIRS = [("kVertexShader", "kFragmentShader"), ("kSquareVS", "kSquareFS"), ("kBlobVS", "kBlobFS"), ("kCompositeVS", "kCompositeFS"),
         ("kCompositeVS", "kGrayFS"), ("kVertexShader", "kPlanetFS"), ("kChartVS", "kFragmentShader"),
         ("kCloudVS", "kBlobFS"), ("kCloudGridVS", "kCloudGridFS"),   # 0.46: the GPU cloud and its brightness grid
         ("kCompositeVS", "kLensFS"),
         ("kCompositeVS", "kKerrFS"),   # 0.55.0: the Black Hole Lab's bent sky
         ("kCompositeVS", "kKerrSkyTinyFS"), ("kCompositeVS", "kKerrSkyMathFS"), ("kCompositeVS", "kKerrSkyFS"), ("kCompositeVS", "kMaxDownFS"),   # 0.56.0: the spinning hole per pixel
         ("kMolDotVS", "kBlobFS"), ("kMolGridVS", "kCloudGridFS"),   # 0.57.1: the Molecule Lab's Density cloud on the card
         ("kVertexShader", "kRingsFS")]   # 0.64.0: Saturn's rings per pixel   # 0.57.1: the Molecule Lab's Density cloud on the card

# 0.56.0: shaders the renderer puts together from pieces (render/kerr_sky.cpp, the same order); a piece's name ends in "Part"
ASSEMBLED = {"kKerrSkyMathFS": ["kKerrSkyHeadPart", "kEllipticPart", "kKerrSkyMathPart"],
             "kKerrSkyFS": ["kKerrSkyHeadPart", "kEllipticPart", "kKerrSkyTracePart", "kKerrSkyMainPart"],
             # 0.64.0: Saturn's rings per pixel (Vulkan) and their probe (the start-up check)
             "kRingsFS": ["kRingsHeadFSPart", "kRingsPart", "kRingsMainFSPart"], "kRingsProbeCS": ["kRingsHeadCSPart", "kRingsPart", "kRingsMainCSPart"]}
# 0.56.0, SIZE: every shader's SPIR-V for OpenGL is measured as glslang writes it and with its size optimiser (-Os: every function inlined, as a driver
# does; the 0.55.6 compute shader kKerrTableCS, which died at link on the owner's card, is 61 KB plain and 251 KB inlined). The per-pixel spinning hole
# (kKerrSkyFS) inlined may not exceed SPIRV_ROOM times the largest inlined shader known to run on the owner's card (every shader of the app but
# kKerrTableCS), nor a third of kKerrTableCS inlined. (Measured for 0.56.0: REPORT.md.)
SPIRV_ROOM = 3.5
# the shaders known to run on the owner's card as they are (his 0.55.9 benchmark); kLensFS gained the card's exposure in 0.56.0 and is left out until it has run
KNOWN_RUN = {"kKerrFS", "kPlanetFS", "kBlobFS", "kGroupsCS", "kRocksCS", "kOutlineDensityCS", "kOutlineLevelCS", "kCloudVS", "kBlobVS", "kGlareAddFS", "kGlowAddFS"}

def main():
    if len(sys.argv) != 2:
        print(__doc__); return 1
    tool = os.environ.get("PL_GLSLANG") or shutil.which("glslangValidator") or shutil.which("glslang")
    if not tool:
        msg = "glslangValidator not found (apt install glslang-tools)"
        if os.environ.get("PL_REQUIRE_GLSLANG") == "1":
            print("FAIL: " + msg); return 1
        print("SKIPPED: " + msg); return 77
    src = open(sys.argv[1], encoding="utf-8").read()
    shaders = dict((m.group(1), m.group(2)) for m in re.finditer(r'static const char\* const (k\w+) = R"\((.*?)\)";', src, re.S))
    for name, parts in ASSEMBLED.items():
        missing = [q for q in parts if q not in shaders]
        if missing:
            print("FAIL %s: pieces missing: %s" % (name, missing)); return 1
        shaders[name] = "".join(shaders[q] for q in parts)
    for q in [n for n in shaders if n.endswith("Part")]:
        del shaders[q]
    names = sorted(shaders)
    sizes, inlined = {}, {}
    failed = 0
    if len(shaders) < 15 or sum(n.endswith("CS") for n in shaders) < 4:
        print("FAIL: found only %d shaders in %s (expected at least 15, four of them compute shaders)" % (len(shaders), sys.argv[1])); return 1
    with tempfile.TemporaryDirectory() as d:
        paths = {}
        for n in names:
            kind = "comp" if n.endswith("CS") else "vert" if n.endswith("VS") or n == "kVertexShader" else "frag"
            want = "#version 430 core\n" if kind == "comp" else "#version 330 core\n"
            if not shaders[n].startswith(want):
                print("FAIL %s: does not start with '%s'" % (n, want.strip())); failed += 1
            p = os.path.join(d, n + "." + kind)
            open(p, "w", encoding="utf-8").write(shaders[n])
            paths[n] = p
            r = subprocess.run([tool, p], capture_output=True, text=True)
            if r.returncode != 0:
                print("FAIL %s:\n%s%s" % (n, r.stdout, r.stderr)); failed += 1
            elif "WARNING" in r.stdout + r.stderr:            # 0.55.7: a warning fails too (a stricter driver may reject what glslang only warns about)
                print("FAIL %s (a warning):\n%s%s" % (n, r.stdout, r.stderr)); failed += 1
            else:
                print("ok   %s (GLSL %s core)" % (n, "430" if kind == "comp" else "330"))
            for opt, into in (([], sizes), (["-Os"], inlined)):   # 0.56.0: the size of each as SPIR-V for OpenGL, plain and inlined
                spv = os.path.join(d, n + ".size.spv")
                r = subprocess.run([tool, "-G", "--aml", "--amb"] + opt + ["-o", spv, p], capture_output=True, text=True)
                if r.returncode == 0 and os.path.exists(spv):
                    into[n] = os.path.getsize(spv)
            if kind == "comp":
                # 0.55.7: the compute shaders also through glslang's whole compiler into SPIR-V for OpenGL (uniforms and buffers given places automatically)
                # and, if spirv-val is installed, that SPIR-V validated: a deeper check of the code than the front end's (types, control flow, doubles)
                spv = os.path.join(d, n + ".spv")
                r = subprocess.run([tool, "-G", "--aml", "--amb", "-o", spv, p], capture_output=True, text=True)
                val = shutil.which("spirv-val")
                v = subprocess.run([val, "--target-env", "opengl4.5", spv], capture_output=True, text=True) if r.returncode == 0 and val else None
                if r.returncode != 0 or "WARNING" in r.stdout + r.stderr or (v is not None and v.returncode != 0):
                    print("FAIL %s as SPIR-V:\n%s%s%s" % (n, r.stdout, r.stderr, (v.stdout + v.stderr) if v else "")); failed += 1
                else:
                    print("ok   %s as SPIR-V for OpenGL%s" % (n, ", validated by spirv-val" if v else " (spirv-val not installed)"))
        for v, f in PAIRS:
            r = subprocess.run([tool, "-l", paths[v], paths[f]], capture_output=True, text=True)
            if r.returncode != 0:
                print("FAIL link %s + %s:\n%s%s" % (v, f, r.stdout, r.stderr)); failed += 1
            else:
                print("ok   link %s + %s" % (v, f))
    known = {n: v for n, v in inlined.items() if n in KNOWN_RUN}
    if known and "kKerrSkyFS" in inlined and "kKerrTableCS" in inlined:
        big = max(known, key=known.get)
        print("SPIR-V sizes in bytes, plain / inlined: " + ", ".join("%s %d / %d" % (n, sizes.get(n, 0), inlined[n]) for n in sorted(inlined, key=inlined.get, reverse=True)))
        line = "kKerrSkyFS inlined %d bytes against the largest known to run, %s %d (limit x%.1f), and kKerrTableCS %d (limit a third)" % (
            inlined["kKerrSkyFS"], big, known[big], SPIRV_ROOM, inlined["kKerrTableCS"])
        if inlined["kKerrSkyFS"] > SPIRV_ROOM * known[big] or 3 * inlined["kKerrSkyFS"] > inlined["kKerrTableCS"]:
            print("FAIL size: " + line); failed += 1
        else:
            print("ok   size: " + line)
    print("%d shaders, %d failed" % (len(shaders), failed))
    return 1 if failed else 0

sys.exit(main())
