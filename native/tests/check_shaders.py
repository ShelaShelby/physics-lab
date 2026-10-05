#!/usr/bin/env python3
"""Compiles every GLSL shader of the renderer (src/render/shaders.h) with glslang: the vertex and fragment shaders as GLSL 330 core, alone and linked in the pairs the renderer links,
and (0.13 stage 2) the compute shaders (names ending in CS) as GLSL 430 core.

A shader that does not compile, or a pair whose outputs and inputs do not match, fails the test. glslang is the Khronos reference compiler: it needs no graphics card.
(It checks the language; what a GPU's own driver does with the shader is only tested on a real PC.)

Usage: check_shaders.py path/to/shaders.h
Exit codes: 0 all compile, 1 a shader failed, 77 glslang not installed (ctest shows "skipped"; set PL_REQUIRE_GLSLANG=1 to make that a failure instead).
"""
import os, re, shutil, subprocess, sys, tempfile

# the pairs renderer2d.cpp links, in init()
PAIRS = [("kVertexShader", "kFragmentShader"), ("kSquareVS", "kSquareFS"), ("kBlobVS", "kBlobFS"), ("kCompositeVS", "kCompositeFS"),
         ("kCompositeVS", "kGrayFS"), ("kVertexShader", "kPlanetFS")]

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
    names = sorted(shaders)
    failed = 0
    if len(shaders) < 13 or sum(n.endswith("CS") for n in shaders) < 3:
        print("FAIL: found only %d shaders in %s (expected at least 13, three of them compute shaders)" % (len(shaders), sys.argv[1])); return 1
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
            else:
                print("ok   %s (GLSL %s core)" % (n, "430" if kind == "comp" else "330"))
        for v, f in PAIRS:
            r = subprocess.run([tool, "-l", paths[v], paths[f]], capture_output=True, text=True)
            if r.returncode != 0:
                print("FAIL link %s + %s:\n%s%s" % (v, f, r.stdout, r.stderr)); failed += 1
            else:
                print("ok   link %s + %s" % (v, f))
    print("%d shaders, %d failed" % (len(shaders), failed))
    return 1 if failed else 0

sys.exit(main())
