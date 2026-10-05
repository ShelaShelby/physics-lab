# UI audit probe (0.15 stage 1) — throwaway tools, not part of the app

The audit numbers in `../AUDIT.md` come from the real app, drawn headless (SDL offscreen + Mesa llvmpipe), with a **scratch copy** of Dear ImGui that writes
down, for the last frame, every text run (pixel size, wrap width, lines), interactive item, closed outline and window (scroll range).
Nothing here is built into the app or the Windows exe.

1. Copy `native/` and the fetched ImGui (`_deps/imgui-src` of a normal build) to a scratch folder; apply `probe_imgui.patch` (in the ImGui copy; it adds `imgui_probe.h`)
   and `probe_main.patch` (in the `native` copy; it adds the switch `PL_ATOM_TILT=1`, the landscape).
2. Build that copy with `-DPL_BUILD_TESTS=OFF -DFETCHCONTENT_SOURCE_DIR_IMGUI=<the patched ImGui>`; point `PL_PROBE_BIN` at its `PhysicsLab`.
3. `render_audit.sh <out> 1280x800 normal dark orbit atom_orbit ...` writes `<screen>.bmp` and `<screen>.probe`;
   `walk_scroll.py <out> 1280x800 normal dark <screens>` repeats each screen scrolled down in steps of 400 px (ImGui does not draw text that is scrolled out of sight);
   `analyze_probe.py <out>` prints the table.
Limits: Linux, DejaVu Sans instead of Windows' Segoe UI (DejaVu is wider), display scale 1.0, Dark theme only.

## Mockup pictures (Part D)
`probe_chart_clean.patch` (apply to the scratch `native` copy too) adds the switch `PL_MOCK_CLEAN=1`: the chart draws no status line and no legend (the mockups draw those). `mockups/src/make_pictures.py` then makes the real lab pictures at the exact size of each layout.
