# 0.61.0: Vulkan, step 3 of 5: the Atom, Molecule and Orbit Labs' card programs on Vulkan, behind the switch (based on 0.60.0; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe)
## Part 0: frame times (ms, (260 - 60 frames) / 200, 1280 x 800, software renderers: the record only, no gate). Before = 0.60.0 (+ the new test hook PL_ATOM_BENCHSCENE only)
- OpenGL / Vulkan, 0.60.0 -> 0.61.0: Atom Cloud 3D Glow (U-238 2p) 208.2 / 207.6 -> 203.2 / 228.9; superposition (scene 12) 40.5 / 48.4 -> 42.4 / 46.0; Slice moving
  (scene 14) 34.0 / 55.8 -> 33.1 / 34.7; jump with every electron (scene 37) 130.0 / 60.2 -> 119.3 / 129.7 (on lavapipe the card's jump costs what it does on
  llvmpipe: both are the processor); Molecule Density (C6H6) 148.3 / 162.2 -> 141.6 / 153.3; Orbit with the belts (3D) 48.6 / 55.4 -> 47.0 / 52.1.
- The extra copy (present.frag): with it skipped (PL_VK_NOCOPY, measuring only) Orbit 48.1 ms (copy: about 4 ms here, in the noise: Density went 153 -> 168).
  Kept: straight into the window needs Vulkan's row order, but the soft layer, the cloud layer and the grey pass sample OpenGL's rows (shared shader text).
## Part 1: the programs (render/vk_card.*): shaders.h's own text as SPIR-V (CMakeLists PL_VK2D_SHADERS; compute blocks at binding 7), the same interfaces
- IGroupCompute, IOutlineCompute, IRockCompute, AtomCard (now a base: AtomCardGl / Vulkan's / AtomCardNone), MoleculeCard (the same), the Glow dots (cloudGpu,
  kCloudVS / kMolDotVS added to vk_draw2d as kCloud / kMolDot, the grids kCloudGridVS / kMolGridVS + kCloudGridFS into R32_SFLOAT). Labs' code: only the gate
  R.computeShaders() in place of R.outlineCompute() (its Vulkan text). Batches of their own on the window's queue with a fence; polled with zero timeout where
  OpenGL polls (outline, Slice, checks, first use); waited where OpenGL reads back at once (groups, the clouds' grids). Buffers on the card per set (rocks
  16 slots, Density 18 sets, the Glow ring sends only new dots); replaced buffers and pipelines go only when their batch and the frames after it are done.
- shaderFloat64 here: yes (enabled at device creation when present; PL_VK_NO_FLOAT64 pretends not: the rocks on the processor, logged, Graphics row says so).
- Guards: groups, outline, rocks, Glow dots: steps "pipeline", "first small run" (exact known answers: groups 0 6 12 18; 1s grid within 3e-7 of e^-2r;
  rocks within 3.1e-5 pt; one dot's cell 2.9e-8 rel.), "first full use"; Slice, jump, Density: the labs' own steps and checks with OpenGL's limits (Slice
  5 planes 0 beyond limit, worst 0.091; jump 80,014 groups 0 differ; Density 12 cases 0 beyond). Vulkan twins (markers gpu-vk<name>-step.txt, settings
  "gpuvk<name>=", log "[vulkan] [<name>] ", versions "vulkan <name> <hash>"); a failure switches only that program off on Vulkan; Try again in the Graphics row.
- SPIR-V bytes (step 2's: tri 1,768 + 7,556, planet 21,172, blob 8,276 + 13,484 ...): groups 12,356; outline 9,388 + 4,736; rocks 9,860; slice 16,168;
  jump 7,604; Density dots 4,712; cloud vert 10,504; Density dot vert 3,424; grids 5,356 + 2,460 + 420.
## Part 2/3: Renderer2D on Vulkan gives these objects (useVulkanCards); PL_NO_COMPUTE and PL_VK_CARDFAIL=<name> (each of the 7 tried: only it falls back,
  0 validation errors). Windows/AMD: includes checked, no generic lambdas nested, formats and limits queried (R8_USCALED vertex, R32_SFLOAT blend, compute
  queue, 1,024-thread groups), stage-exact barriers (transfer->compute, compute->vertex input, compute->host, colour->transfer->host, batch start after the
  frames before). Fixed on the way: a pipeline destroyed while a batch used it (validation caught it with PL_VK_CARDFAIL=slice): pipelines dropped late too.
## Part 4: tests
- gl_compare_tests (Vulkan twin, tests/gl_vulkan.inc, validation on: 0 errors), 638 checks, 0 failed: groups (scenes 13-15, jumps 11-12) against the
  processor within the existing tolerances and 0 of 2.64 M groups differ from OpenGL; outline 7 states: worst 1.2e-5 rel. against the processor, 0 against
  OpenGL, levels equal; rocks 1.3 M places: 0.0022 pt / 7.7e-6 rel. against the processor, 0 against OpenGL; Slice 0 flags, 0 points differ from OpenGL;
  jump 0; Density 0 flags, 0 values; Glow grid 12,012 cells within the float sum's rounding. (The twin cannot be built on 0.60.0: no Vulkan programs.)
- check_shell.py vulkan2d + new half vulkancards (in ui_shell): Glow, superposition, outline, Slice, jump (every electron), Density, belts, Vulkan (sync
  validation on, 0 errors, PL_VK_SYNC) against OpenGL, one sample: 0.002, 0.002, 0.002, 0.002, 0.002, 0.001, 0.002 % over 8 levels (limit 1 %); each log
  names its first full use. Broken program (cloud): 0.000 % from the processor's picture; no float64: rocks on the processor, Settings says so. On 0.60.0's
  build: 13 of 38 failed. PL_VK_SYNC (tests): without it a moving Slice differs run to run on lavapipe (1.6 %) by when its batches finished.
- OpenGL: orbit, atom Glow, atom Slice, molecule, black hole screenshots byte-identical to 0.60.0's. check.sh quick 18/18. Full UI suite 33 of 35 (ui_shell with vulkan2d +
  vulkancards passed; gl_phase_colours with the Vulkan twin passed); ui_popups (check_6b benchwaits, benchscenes blackhole.all_light) and ui_molecule
  (check_molecule card) as in 0.60.0 (ROADMAP).
## For the owner's PC
- Settings, Performance, Graphics: Vulkan (test), restart. Try: Atom Cloud 3D Glow, a superposition, the 90 % outline, the Slice dragged, a jump with every
  electron (Glow), Molecule Density, Orbit with the belts; they should look as on OpenGL. Send gpu-paths-log.txt (its "[vulkan] [<name>]" lines).
