# Native Physics Lab 0.16 stage 3: nucleus view at full zoom, outline hitch

## A. Nucleus at full zoom (atom/atom_view.cpp)
Per-frame work at x10,000,000 (Lr-266, + Sea, Fuzzy, 1280x800, counted in the cloud with a temporary counter; counts, NOT time):

| style | blobs | wholly off picture | draw calls | triangle vertices | blob area (picture areas, soft layer at 1/4) |
|---|---|---|---|---|---|
| Accurate, dark | 843 -> 24 | 819 -> 0 | 149 -> 4 | 311,076 -> 36 | 6.0 -> 6.0 |
| Classic, dark | 261 -> 24 | 237 -> 0 | 184 -> 4 | 311,292 -> 36 | 6.0 -> 6.0 |
| Cinematic, paper (light) | 843 -> 25 | 818 -> 0 | 149 -> 4 | 311,076 -> 36 | 6.25 -> 6.25 |
| Cinematic, dark | 922 -> 32 | 890 -> 0 | 22 -> 6 | 36 -> 36 | 8.0 -> 8.0 |

Cost drivers found:
1. Why "+ Sea" is slower than "+ Gluons" (the gluon field is shared, so not that): the sea antiquark is a ring. Where blending is ordinary (Accurate, Classic, Cinematic on any LIGHT theme) Renderer2D::ring makes up to 720 line pieces per ring; at full zoom the rings are big, so 72 rings = 311,076 vertices (about 15 MB uploaded) a frame, and every ring splits the blob batch (149-184 draw calls). Cinematic on a dark theme draws the ring as an added-up blob (no vertices), so Sea costs little there.
2. About 97 % of the blobs were wholly outside the picture (nucleons near the view; the 45 nearest get quarks).
3. The fill of the visible blobs (6-8 picture areas) is unchanged. The GPU already clips everything outside the picture, so clamping a blob's radius would save no fill and would change its gradient: not done.

Fixes: a shape (blob, ring, line) wholly outside the picture is not made; a nucleon's gluon cloud and its 12 sea pairs are not built when their furthest possible reach (worked out from the code: tube 0.33 rn, halo 1.115 rn, sea 0.75 fm x k, plus blob size) misses the picture; one gluon-particle list per frame instead of a new vector per nucleon; the palette colour is looked up once per colour, not per particle.
Picture: 10 renders (Lr-266: Accurate, Classic, Cinematic dark and paper, Hard balls, Cloud 3D at zoom 6.5-7; zoom 3.6-4.2 with whole nucleons) are identical byte for byte before and after (PL_FIXED_DT; the before runs were repeated once to show they are deterministic).
If the owner's Cinematic case was on a DARK theme, these counts do not explain a 59-80 fps frame (922 blobs, 22 draw calls before); what remains is fill, which needs a real GPU (OWNER-TODO.md asks which theme).
New benchmark scene 35 (last): "Nucleus, full zoom, camera moving": lawrencium-266, Orbit view, Cinematic, + Sea, Fuzzy, zoom 7 (x10,000,000), the camera turning 0.15 rad a frame (a fast drag: 15 points a frame). kBenchAtomScenes 30 -> 31.

## B. Outline hitch (scene 16 worst 55.3 ms vs CPU 19.6 ms)
Read in the code (render/outline_compute.cpp, atom/outline.cpp):
- Compile and link happen at app start (Renderer2D init -> makeOutlineCompute), not at first use. A driver may still finish a program on its first dispatch; the benchmark switches the outline On at scene 16's first counted frame, so the process's first GPU outline dispatch lands in a counted frame, while scene 29 (CPU) has no such first use. Fix: one tiny job of each kind (4^3 grid with its level, 4x4 plane) is run, waited for and read back at start. Whether this was the hitch: only a real GPU can tell.
- No CPU wait: the fence is polled with a zero timeout; the 1 MB read-back happens only after it signalled. Whether the AMD driver stalls inside glGetBufferSubData: not visible here.
- Every job allocates a fresh 1 MB buffer and frees it after the read (each cache clear, every 120 frames). Small; not changed.
- The 90 % level runs in ONE work group (33 passes over 262,144 cells); its "a few tenths of a millisecond" comment was never measured on the owner's GPU. It is per job, on the same queue as the frame.
Benchmark report: every scene now has "Frames slower than 16.7 ms: N of M" (and a "> 16.7 ms" column in the window): 1 = one-time hitch, about one per 120 frames = per job. OWNER-TODO.md says what to send.

## Tests
- New: ui6c_fullzoom (tests/check_6c.py, PL_DRAWSTAT test hook: calls, instances, vertices per frame). Failing before: with the off-picture check disabled it fails 7 of 16 checks (311,076 vertices, 843 / 922 blobs); passes with it.
- app_settings: new checks for slowFrames, the report line and scene 35. Changed on purpose: the scene-1 text check now has the new line between "Worst frame" and "CPU work"; the chart-twin check expects kBenchAtomScenes 31 (was 30).
- check.sh quick 10/10; app_settings, atom_physics, ui6c_fullzoom, ui6b_bench, ui6b_nucleus, atom_panel, shell_pictures, gl_phase_colours (GPU outline vs CPU, Mesa) pass. No full suite.
- No new test for the outline warm-up (its effect is timing only).

## Not tested
- Speed of anything: no GPU, no Windows, no real mouse in the cloud. Nothing above says it is faster; only counts and pixels were compared.
