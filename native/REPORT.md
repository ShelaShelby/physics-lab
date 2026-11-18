# 0.60.0: Vulkan, step 2 of 5: the labs' normal drawing (renderer2d) on Vulkan, behind the switch (based on 0.59.0; box: 4 cores, Mesa 25.2.8, lavapipe)
## Part 0: coverage (renderer2d's calls that are not card programs -> a view; found with the new PL_LOG_DRAWKINDS counts, 1280 x 800, dark theme)
- orbit_2d (Earth selected): rect, line, triangle, disc, ring, sphere, sunDisc, fadeDisc, squares (belts), skyMap. orbit_earth (3D, zoom 5.5, 12 Aug 2026
  17:46, paused): planet with map, eclipse (the Moon's shadow), Earth's air. orbit_saturn (3D, zoom 4.1): planet rings. orbit_mono (Monochrome): the grey pass.
- atom_nucleon (U-238, Simple quarks, + Sea, the nucleon): soft layer, cloud layer, blob kinds 0 1 3 4 6 7 8. atom_classic (Cloud 3D, Classic): kind 5.
  atom_density (nucleus, Density): kind 2. atom_chart: meshCreate/drawMesh. mol_fill (C6H6): spheres (kind 9). mol_real (CH3OH): triangle, kinds 3 6.
  mol_ball (H2O): kinds 0 3 8. maskedDisc is the Black Hole Lab's only (not drawn on Vulkan, below).
- Frame time, (1100 - 100 frames) / 1000, idle box, software renderers (the record only, no gate): Orbit (Earth, 3D, zoom 3) OpenGL 0.59.0 12.2 ms,
  Vulkan 17.9 ms; Atom (U-238 Cloud 3D 2p) 225.2 / 240.4 ms (OpenGL draws its cloud with its card program, Vulkan on the processor); Molecule (C6H6
  Space-filling) 12.0 / 19.1 ms. Vulkan: 4 samples and the copy into the window here; lavapipe, not the owner's card.
## Part 1: renderer2d on Vulkan (render/vk_draw2d.*, render/renderer2d_vk.cpp, render/vk/present.*, cmake/extract_glsl.cmake)
- One recording: Renderer2D's lists and segments as for OpenGL; only end() differs (endVulkan, segment by segment in OpenGL's order). Shaders: shaders.h's
  own text, taken out at build time and compiled by glslang with its relaxed Vulkan rules (-R: loose uniforms into one block per stage); the app reads the
  blocks' offsets and the stages' locations from the SPIR-V and checks them (no copies). SPIR-V bytes (step 1's fill: 1,136 + 500): tri 1,768 + 7,556;
  planet frag 21,172; square 1,624 + 536; blob 8,276 + 13,484; chart vert 6,320; composite 1,092 + 552; grey 1,016; present (new, Vulkan only) 1,136 + 1,416.
- Drawn with OpenGL's rows (first row at the bottom), so gl_FragCoord and the layers' coordinates are OpenGL's; present.frag turns the rows over into the
  picture area. Blends factor for factor; maps R8G8B8A8_SRGB/UNORM with blitted mip levels, GL's filters and wraps; the cloud layer R32G32B32A32_SFLOAT
  (queried; else R16G16B16A16, logged); soft layer at its scale; grey pass (kGrayFS); meshes uploaded once (device-local). 4x MSAA with resolve (queried);
  1 sample in the grey pass and for Save image, as OpenGL's offscreen pictures. Per frame in flight: one persistently mapped buffer and descriptor pool; no
  wait in a normal frame (replaced images and buffers kept until their frames are done).
- Guarded steps "renderer2d pipelines", "renderer2d textures", "renderer2d check": a 32 x 16 picture (flat rect, added blob, soft-layer quad) read back
  against the blend formulas (2 levels): here (160,90,146) (115,204,217) (16,223,144), within 1 level of the computed values. A failure: this start uses OpenGL.
## Part 2: main.cpp draws it before Dear ImGui; resize, VSync, minimise as in step 1; Save image (ui/shot.cpp) and PL_SCREENSHOT include it. Fixed on
  the way: step 1's Save image pass was not compatible with Dear ImGui's pipeline (its dependency differed; validation error), and its copy had a
  read-after-write hazard (sync validation): the same dependency as the window's pass, the copy behind a barrier.
## Part 3: on Vulkan no OpenGL object is made: compute(), outlineCompute(), rockCompute(), kerrCompute() nullptr; rocksGpu(), cloudGpuOk(), molCloud() false;
  the card paths go off ("PLATOMCARD slice/jump state=-1", "PLMOLCARD state=-1 why=on Vulkan the card programs come in step 3"); step 3's list: ROADMAP.
  Black Hole Lab: its picture is the lens pass (a card program), no processor path through the normal calls: a note on Vulkan (step 4). Direct OpenGL
  outside render/: main.cpp (in !useVulkan branches), shot.cpp takeShot, benchmark.cpp/gpu_timer (a benchmark cannot start on Vulkan), blackhole_view.cpp
  (its draw and compare hooks: not run on Vulkan).
## Part 4: tests (check_shell.py:vulkan2d in ui_shell; 0.59.0's build: 38 of 60 failed)
- Per view, one sample, both on the processor's paths, Vulkan with validation, pixels over 8 levels (limit 1 %): orbit_2d, _earth, _saturn, atom_nucleon,
  _classic, _density, _chart 0.002 %, orbit_mono 0.054 %, mol_fill, _real, _ball 0.001 %; none skipped; zero validation errors; each view's calls made.
  4 samples: every view zero errors, the log says 4 samples. Sync validation (layers, Save image): 0 errors. Save image 0.000 % against OpenGL's file.
  PL_VK_CHECK=break: OpenGL, Settings names the step. Step 1's part (vulkan): 17 checks pass (PL_NO_PICTURE on both sides now).
- OpenGL: the four labs' screenshots byte-identical to 0.59.0's. check.sh quick 18/18 (also after the link fix for gl_compare_tests). Full UI suite 32 of 34 (ui_shell, with vulkan2d, passed); ui_popups
  (check_6b benchwaits; benchscenes blackhole.all_light "-1.0000") and ui_molecule (check_molecule card) as in 0.59.0 (ROADMAP).
## For the owner's PC
- Settings, Performance, Graphics: Vulkan (test), restart. Each lab (planets close up, Saturn; Atom's clouds and nucleus; each Molecule style) should look as
  on OpenGL; Black Hole: a note. Try Save image, VSync off/on, resize; send gpu-paths-log.txt (Settings, Performance: the log's folder).
