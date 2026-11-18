# Plan: the app moves from OpenGL to Vulkan (owner decision 18 Nov 2026, DECISIONS.md "Graphics and speed")
All of the app, in five steps. OpenGL is the backup (0.63.0: Vulkan the default) until every part works on Vulkan on the owner's PC; then it is removed. Every rule
of 16 Nov stays: the processor's version is the exact reference the tests check the card against and the fallback through the guard; single precision on
the card; the target is the screen's rate (his PC: 255 Hz).

1. **The foundation** (0.59.0, done): window, device (a discrete card first), swapchain, Dear ImGui on Vulkan, screenshots, the switch (Settings,
   Performance, Graphics; PL_GRAPHICS), the guard (app/gpu_guard.h gpuVulkanGuard, log lines "[vulkan] ..."), the build on Linux and Windows (Vulkan-Headers,
   volk, glslang by FetchContent; SPIR-V made at build time). Code: render/vk_backend.*, render/vk/*.vert|frag.
2. **renderer2d on Vulkan** (0.60.0, done): the Orbit, Atom and Molecule Labs' drawing with OpenGL's own shader text as SPIR-V (render/vk_draw2d.*,
   render/renderer2d_vk.cpp), 4x multisampling, Save image; the card programs take the processor's path on Vulkan until step 3.
3. **The card programs** (0.61.0, done): the groups, the outline, the rocks (double precision: shaderFloat64), the Glow dots, the Slice, the jump and the
   Density cloud on Vulkan from shaders.h's text (render/vk_card.*), each under its own Vulkan guard; their batches on the window's queue, fenced.
4. **The Black Hole Lab's programs** (0.62.0, done): the lens, the table pass, the sky per pixel (kKerrSkyFS from its parts), the exposure, the lower
   resolution while moving, the probe, the Film glow, the eye's glare and the light paths (kKerrTableCS) on Vulkan (render/vk_blackhole.*), Vulkan 1.3's
   dynamic rendering and synchronization2, the big pipelines on a worker thread with a pipeline cache; Vulkan 1.3 required, 1.4 used where present.
5. **The benchmark, the picture tests and the UI suite on Vulkan; Vulkan the default** (0.63.0, done but for OpenGL's removal): OpenGL the backup
   (Settings; a card below 1.3 or a failed Vulkan start); the benchmark's card times from timestamp queries; one guarded step open at a time in the
   whole process; steps 1-3's code (vk_draw2d, vk_card, the backend) on 1.3's dynamic rendering and synchronization2. Left: OpenGL removed once the
   owner's PC shows every scene works (the owner decides: ROADMAP); the copy into the window (present.frag) goes with it (ROADMAP "Whole app").

Rules for every step: zero validation errors in this box's test runs (lavapipe, PL_VK_VALIDATION=1); every VkResult checked; only features that are
queried; no implicit int/float conversions in shaders, every variable initialised (AMD's compiler is stricter than Mesa's); the OpenGL path bit for bit
as it was until step 5; a new Vulkan stage of the start is a guarded step.
