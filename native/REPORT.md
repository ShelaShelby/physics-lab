# 0.93.0: Atom Lab, atoms in fields

## What changed
- Part 0: check_shell.py gpuhelper expected 13 guarded programs. 0.92.0 added porkchop (14). The test now reads the list from vulkanCardGuards() in app/gpu_guard.cpp.
- A: hydrogen in an electric field (atom/stark.h). Parabolic states, the linear and second-order shifts (Anh-Tai et al. 2024, eq. 20-22 and 29-30), and the parabolic cloud. The cloud is sampled on the GPU (kStarkCS) with its CPU twin as the reference and fallback.
- B: the Breit-Rabi card on the Spectrum tab (atom/breit_rabi.h) for H-1, Na-23, Rb-85, Rb-87 and Cs-133. Steck eq. 23 and 26-29. Values from Steck's D line notes (rev. 2.3.4), Martin and Wiese, CODATA 2022.
- Benchmark: Hard scene atom.stark_drag (id 48). blackhole.edge_on moved to All scenes.
- Sources: 8 papers and notes in the private repo under data/fields/. Extracts, README and 16 SOURCES.md rows here. About lists them.

## Tests run
- atom_tests: 12396 checks, 0 failed. Stark is checked against a Coulomb-Sturmian diagonalisation and Jentschura 2001 eq. 29-30. Breit-Rabi is checked against a full Jacobi diagonalisation and Steck's values.
- app_tests: 202476 checks, 0 failed.
- check_atom_chip.py cardstark (new, GPU against CPU): 4 checks, 0 failed.
- check.sh job: passed (quick and ui_smoke, 0 failed).

## Not done
- Parts C (broadened spectrum, Boltzmann-plot temperature) and D (many-electron Slice sampler on the GPU): skipped by the owner on 27 Nov 2026. Both stay in ROADMAP.md.
- The cloud of one M sublevel in the magnetic field: not started (part of the skipped work).
- This job needs pc_tests.ps1 on the owner's PC (atom.stark_drag, the Stark program's start-up check).

## References changed on purpose
- Benchmark lists (test_app.cpp): 29 Hard, 72 All (was 29, 71). kBenchAtomScenes 49 (was 48).
- check_shell.py gpuhelper: the fixed count 13 is replaced by the guard list from gpu_guard.cpp (it was stale since 0.92.0).
