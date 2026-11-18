# 0.59.0: Vulkan, step 1 of 5: the foundation (behind a switch; OpenGL stays the default) (based on 0.58.0; box: 4 cores, Mesa 25.2.8)
## Part 0: measured first
- This box runs Vulkan in software: mesa-vulkan-drivers 25.2.8, libvulkan1 / vulkan-validationlayers 1.3.275. vulkaninfo: one device, "llvmpipe (LLVM
  20.1.2, 256 bits)" (Mesa's lavapipe), type CPU, Vulkan 1.4.318, driver 25.2.8; VK_LAYER_KHRONOS_validation 1.3.275; SDL3's offscreen driver gives a
  headless surface (VK_EXT_headless_surface), so the UI checks run on Vulkan too.
- OpenGL baseline (0.58.0's build, the menus alone: the Learn page over the lab, Settings open, 1280 x 800; (1100-frame run - 100-frame run) / 1000):
  17.1 ms a frame (llvmpipe, every core). Vulkan, the same state, this build: 10.4 ms (lavapipe). Software renderers, not the owner's card.
## Part 1: the build
- FetchContent at one SDK tag, checked to exist (git ls-remote, 18 Nov): Vulkan-Headers, volk and glslang, all vulkan-sdk-1.4.350.1. volk loads the PC's
  Vulkan loader at run time (no SDK, no link to vulkan-1.dll: a PC without Vulkan still starts). glslang (no optimiser, no HLSL) is built and turns
  render/vk/fill.vert|frag into SPIR-V headers at build time; Dear ImGui's own imgui_impl_vulkan (v1.92.9b, through volk). Cost here: about 1 min more of
  the first build on 4 cores (glslang is most of it). Windows: 0.58.0's run took 1:01 to configure and 5:11 to build; this run: see the chat's last message.
- CLAUDE.md first-build apt line: + mesa-vulkan-drivers libvulkan1 vulkan-tools vulkan-validationlayers. The workflow's Linux job builds no app (nothing new
  to install); the Windows job copies the volk and Vulkan-Headers licences into the zip; About credits both (test in app_settings).
## Part 2: what works (render/vk_backend.*, main.cpp, app/gpu_guard.*, ui/orbit_panel.cpp, ui/shot.cpp)
- Settings, Performance: "Graphics: OpenGL / Vulkan (test)", "Takes effect at the next start. In use now: ..." (the card and driver, or why OpenGL), Try
  again. Settings line graphics= (OpenGL not written); PL_GRAPHICS=vulkan|opengl. Search finds the row ("Graphics (OpenGL or Vulkan)").
- Instance (1.1 where the loader has it), SDL3 surface, device (every card logged; discrete > integrated > virtual > software; needs graphics + present +
  VK_KHR_swapchain), swapchain B8G8R8A8/R8G8B8A8_UNORM (not sRGB, as OpenGL's), the VSync rule read from main.cpp: on = SwapInterval(1) -> FIFO; off =
  SwapInterval(0) -> IMMEDIATE, else MAILBOX, else FIFO. Resize, out-of-date / suboptimal and VSync changes make the swapchain again; a minimised window
  draws nothing (as before). Every VkResult checked (Dear ImGui's too, through CheckVkResultFn); a failure after the start is kept by the guard.
- The picture area: the theme's background (our own pipeline, a push-constant fill) and the one centred line; the panels and controls work; the benchmark
  says it runs on OpenGL until step 5 and does not start.
- The guard: gpuVulkanGuard (marker gpu-vulkan-step.txt, gpuvulkan=, version "vulkan <app version>"), log lines "[vulkan] begin/end" for instance, surface,
  device (name, driver), swapchain, pipelines, first frame. New state "fail" (a step returned an error): this start falls back to OpenGL at once, the next
  ones too until Try again; a crash inside a step (marker) the same, "stopped the app". The light paths' guard texts are unchanged.
- Screenshots: PL_SCREENSHOT reads back the swapchain image; Save image draws the lab's text into an image of its own.
- OpenGL bit for bit: 0.58.0's and this build's screenshots of the four labs (Orbit, Atom, Molecule, the spinning hole) are byte-identical.
## Tests
- Tests first, failing on 0.58.0's build (11 of 17 failed): check_shell.py:vulkan (in ui_shell): Settings and About at 1280 x 800, Vulkan with validation
  against OpenGL with PL_NO_PICTURE (a test switch: OpenGL leaves the lab's picture out too): 0.102 % and 0.076 % of pixels differ by more than 8 levels
  (limit 1 %); zero validation errors and warnings in every run; every [vulkan] step logged; resized twice and VSync off/on (FIFO -> MAILBOX -> FIFO here),
  still zero errors; PL_VK_FAIL=device: starts on OpenGL, Settings says why, gpuvulkan=fail kept. PL_GPU_DIE=swapchain by hand: the next start is OpenGL,
  "Vulkan stopped the app last time (in the step 'swapchain')". app_settings: the "fail" state, the vulkan id form, graphics= (+11 checks).
- check.sh quick 18/18. Full UI suite: 31 of 34. ui_search failed on the new row (check_5l's hand-written row list: Graphics added; ui_search then passed).
  ui_popups (check_6b benchwaits, ROADMAP; benchscenes blackhole.extreme "-1.0000 of the pixels": the same with 0.58.0's build, ROADMAP **fix**) and
  ui_molecule (check_molecule card, ROADMAP) as in 0.58.0: not from this job.
## For the owner's PC
- Settings, Performance, Graphics: Vulkan (test), restart: the menus, Settings, About and a guide should look exactly as on OpenGL, the picture area shows the
  line. Then VSync off and on, resize, minimise, maximise, move to the other monitor. The log's folder (Settings, Performance) has gpu-paths-log.txt: its
  [vulkan] lines name the card Vulkan chose (the discrete one is wanted). If anything fails, the next start is OpenGL: please send that log.
