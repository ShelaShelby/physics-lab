# 0.64.0: speed batch 1 (based on 0.63.1; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe; processor ms per frame = the benchmark's "CPU work per frame", same box, same scene)
(work in progress: checkpoint 1)
## A. The heavy views the benchmark missed (six new Hard scenes, app/bench_scenes.cpp, atom/atom_bench.*)
- Searched each lab's per-frame code. Measured here (each at the zooms -0.5, 0, 0.5, 1.0; the slowest kept): atom.slice_dirac_drag 85 ms (zoom 0), atom.slice_all_drag
  32 ms (-0.5), atom.side_all_glow 24 ms (-0.5, zoomed to 0 and back), atom.side_all_classic 21 ms (-0.5), atom.slice_glow_ultra 5.0 ms (-0.5), atom.slice_classic_ultra
  4.2 ms (-0.5): all above 3.9 ms, so all six are Hard scenes (ROADMAP, Atom Lab, slowest first). For comparison atom.slice_drag 3.5 ms here.
- Checked, no scene: Saturn with the camera still (the same loop as orbit.saturn); the side view with GPU colours Off and the Molecule Lab's Density with GPU Density
  cloud Off (switches for comparison, twins of existing scenes); the Black Hole Lab at rest, the chart, the molecule's surfaces (kept between frames).
- For the owner: the Hard run grows from 26 to 32 scenes, about 4 min 45 s (was about 4 min; the static_assert and app_tests' bound say so).
