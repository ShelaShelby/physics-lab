# 0.85.6: the spinning hole's table shader is back in the build

Built and tested on the owner's PC (RX 9060 XT, AMD driver 26.11.2, VS 2026 Build Tools).

## What changed
- 0.85.5 added the glare shaders to the shader list in CMakeLists.txt in front of a comment. The entry "kerrtable_comp:kKerrTableCS" was then inside that comment.
- So r2d_kerrtable_comp.h was not made, and a fresh build failed (GitHub "Native full tests" run 45702093649, vk_blackhole.cpp and vk_helper.cpp).
- The local build passed only because an old copy of the header was still in the build folder.
- The entry is back in the list, before the comment. No code changed.

## Tests run
- Local build after deleting the old header: r2d_kerrtable_comp.h is made again, PhysicsLab and blackhole_tests build.
- The app with a spinning hole (preset m87, 60 frames): starts, draws and exits cleanly.
- 0.85.5's tests (still valid, the code is the same): blackhole_tests 1141 checks, app_tests 202,429 checks, 0 failed. vk_compare_tests 707 checks, the same 3 known failures. PLBHGLARECARD within its limits.
- Memory over long runs: Black Hole with gas 679 MB, Atom 611 MB, Orbit 608 MB, Molecule 591 MB. None grows.

## Not done
- check_blackhole.py was not run here. The Linux UI scripts are not run on the owner's PC. The GitHub full tests run it.
- The spinning sky shader's cost, tone.close and precision.27 on AMD, atom.emit_all (buildManyJump 63 ms a frame). All in ROADMAP.

## References changed on purpose
- None.
