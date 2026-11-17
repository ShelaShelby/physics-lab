# 0.57.1: the Molecule Lab's Density cloud on the graphics card (molecule.vib_density, molecule.density)
Based on "native 0.57.0". This box: Xeon 2.3 GHz, 4 threads, Mesa llvmpipe (software: its card times are NOT the owner's; the benchmark's 190 ms frames here are llvmpipe's rasterizer).
## Part 0 (measured first, each scene alone with PL_BENCH_ONLY, the view's parts timed with temporary timers; per frame, this box)
- vib_density (water, 100,000 dots): 17.8 ms of the lab's own work. The Density panel asked for the still molecule each frame, which undid the swing: the still
  surface made again (1.9 ms) and one swing moment's 100,000 dots sampled again (6.8-9.5 ms); the vibrating surface 2.3 ms (blend 0.43, ln field 0.15, marching
  1.36, normals and potential 0.37); the dots projected 0.65 and glowBlobs 3.4; nuclei 0.35, sort 0.25, colours 0.06, triangles 0.17; 8.0 MB of blobs sent.
- density (benzene, 100,000 dots, camera turning): 9.6 ms: dots projected 0.8, glowBlobs 4-8 (the nuclei cap: 12 nuclei x every dot x up to 4 rounds),
  project + sort of 14,128 triangles 0.73, colours 0.16, triangles 0.53; 8.0 MB of blobs and 2.0 MB of triangle vertices sent.
## Plain waste out (bit for bit; density_tests testPlainWaste0571, 17 checks)
- MoleculeLab::ensureReal (the panel and the picture ask the same), RealCache::ensure (meshVib: the still surface no longer made in every swing frame),
  dens::blendVib (no whole copy before writing; withFine false for the surface), dens::marchTets (a table in place of the hash map; hashEdges keeps the
  old way as reference). Vibrating surface 2.3 -> 1.0 ms; both surfaces equal bit for bit (water, benzene, methanol, strides 1 and 2, two isovalues).
## Part 1: the cloud on the card (render/molecule_card.*, molecule/molecule_card_paths.*, guard gpuMolCloudGuard: compile, link, first small run, check, first full use)
- kMolDotsCS: each dot's X, Y, size and depth weight (molecule/mol_cloud.h cloudDot, the processor's formulas unchanged, is the reference); kMolGridVS sums the
  exposure grid on the card (read back, as the Atom Lab's card cloud); kMolDotVS draws the soft dots with kBlobFS. The dots are sent once per set (18 sets kept:
  the still cloud and the swing's 17 moments). The nuclei cap: only each nucleus's own nearby dots on the processor (cloudNear, exactly glowCapExposure's sums).
- Gate: the processor's cloud work 4.4-9.3 ms -> 0.2-0.75 ms (cloudNear 0.19 ms for 100,000 dots); kMolDotsCS for 100,000 dots 0.3 ms on llvmpipe.
- SPIR-V plain / inlined: kMolDotsCS 4.7 / 3.7 KB, kMolDotVS 3.3 / 2.7, kMolGridVS 2.4 / 2.0 (beside kKerrFS 27.3 / 19.5, kKerrSkyFS 101.4 / 62.3, kSliceDotsCS 16.1 / 13.0). Float only.
- Limits (mol_cloud.h cloudLimit, derived from the operations before any card result: GLSL 4.30 4.7.1 rounding, R/pan/s/ext rounded to float, the
  processor's own float casts). Start-up check here: 6 cameras x (8,192 of the lab's dots, 4,096 in a box beyond them) = 73,728 dots, 0 beyond their limit,
  the largest 0.65 of its limit, 0 drawn by one side only.
- Pictures here (llvmpipe): card against processor pixel for pixel: water and benzene dark identical; water light 4 pixels 1 level off.
- Found and fixed on the way: gpuGuardDecide read a "molcloud ..." record as an older line (a changed shader would never retry; test_app check, failed
  before); the first-full-use fence made again every frame was never seen signalled while the card was a frame behind (now made once). The Atom Lab's jump has
  the same fence pattern: ROADMAP **fix**.
- Changed: density (blendVib, marchTets), molecule_view (ensure, drawRealistic), shell_molecule, renderer2d (molCloud, Kind::MolCloud), shaders.h, gpu_guard,
  prefs, settings (row gpumolcloud), orbit_panel (Performance line, Try again, switch "GPU Density cloud"), benchmark (Path lines), main (PL_MOL_CARD=0, guard start).
  Nothing checks for the benchmark.
## Estimate for the owner's PC (an estimate: llvmpipe is not his card)
- The lab's own processor work after: vib 2.3 ms, density 2.2 ms on this box (from 17.8 and 9.6), the 8 MB of blobs a frame gone. Scaled to his 14.45 / 7.44 ms:
  about 2.5-3.5 ms (vib) and 1.5-2.5 ms (density), under 3.9 ms. Card: as before (0.96 ms) plus the dots' compute and grid, and a short wait for the grid read-back.
- Left on the processor (ROADMAP): the surface's sort and colours each frame, the vibrating surface each new phase, the grid read-back.
## Tests
- check.sh quick 18/18. check_molecule.py card (added to ui_molecule) by hand: 28/28 (check, paths, log, killed in each of the 5 steps, changed shader, Try again, pictures).
- The full UI suite was not run (a small version).
