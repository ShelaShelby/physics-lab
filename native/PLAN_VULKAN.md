# Plan: the app moves from OpenGL to Vulkan (owner decision 18 Nov 2026, DECISIONS.md "Graphics and speed")
All of the app, in five steps. OpenGL stays the default and the backup until every part works on Vulkan on the owner's PC; then it is removed. Every rule
of 16 Nov stays: the processor's version is the exact reference the tests check the card against and the fallback through the guard; single precision on
the card; the target is the screen's rate (his PC: 255 Hz).

1. **The foundation** (0.59.0, done): window, device (a discrete card first), swapchain, Dear ImGui on Vulkan, screenshots, the switch (Settings,
   Performance, Graphics; PL_GRAPHICS), the guard (app/gpu_guard.h gpuVulkanGuard, log lines "[vulkan] ..."), the build on Linux and Windows (Vulkan-Headers,
   volk, glslang by FetchContent; SPIR-V made at build time). Code: render/vk_backend.*, render/vk/*.vert|frag.
2. **renderer2d on Vulkan**: every lab's normal drawing (lines, dots, discs, textures, text in the picture, the sky, the maps).
3. **The card programs** of the Atom and Molecule Labs and the Orbit Lab's (atom_card, molecule_card, group/outline/rock compute).
4. **The Black Hole Lab's programs** (kerr_sky, kerr_compute, the lens and Kerr shaders); retry the compute path parked since 0.55.7.
5. **The benchmark, the picture tests and the UI suite on Vulkan**; Vulkan the default; then OpenGL removed once the owner's PC shows every scene works.

Rules for every step: zero validation errors in this box's test runs (lavapipe, PL_VK_VALIDATION=1); every VkResult checked; only features that are
queried; no implicit int/float conversions in shaders, every variable initialised (AMD's compiler is stricter than Mesa's); the OpenGL path bit for bit
as it was until step 5; a new Vulkan stage of the start is a guarded step.
