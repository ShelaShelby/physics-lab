# 0.16 stage 5: nucleus view, fewer draw calls with gluons and sea (counts measured; GPU fps NOT tested)

## What changed
- Sea antiquark rings (Classic, Accurate, Cinematic on light themes) were Renderer2D::ring: 24+ separate line pieces each, plus a flush of the blob batch per ring. Now one blob (new shader kind 7, a hard-edged band like the line pieces) in the same batch, order kept.
- Classic flux tubes (lineTo) were a flush + a triangle run each; now one blob (kind 8, hard-edged, no caps, like Renderer2D::line), same batch.
- The soft layer list is plain blob runs (no std::function / shared_ptr). The 12 sea pairs' random numbers (rnd, cbrt, acos, sin, cos) depend only on (pair, cycle), so they are cached; the same doubles in the same order, so the random sequence is unchanged.
- Measuring aids: PL_DRAWSTAT also prints the nucleus part's blobs, blob runs, line calls, ring calls and CPU µs (atom::nucleusStats()); PL_ATOM_SPIN=x turns the camera x rad a frame.
- Not changed: physics, draw order, blending, the additive (Cinematic on dark themes) path.

## Why light themes were slower (found by measuring, see table)
- Accurate: the draw stream is identical on light and Midnight (same calls, vertices, blobs). So the gap is not in Accurate's code.
- Cinematic: on a dark theme the ring was already one additive blob; on a light theme (no additive light) it fell back to the ring of line pieces. x2063 + Sea: light 454 calls / 33,582 vertices vs Midnight 115 / 30. That matches the owner's "light 6-8x, Midnight 2x" if the run was Cinematic (the default quark style); Accurate does the same work in both themes. Now light 5 calls.

## Before -> after, per frame (1280x800, Ds-281, Cloud view, Fuzzy, camera turning 0.15 rad/frame, average of the last 30 of 90 frames)
Zoom x5182 and x2063 at Auto scale; at 1280x800 BOTH show quarks (x2063 shows about 230 nucleons with quarks, x5182 about 56): no quark-free zoom in this pair. CPU µs is the nucleus section on the cloud CPU, 4 runs in parallel with software OpenGL: noisy, only the trend counts.
| zoom | detail | quarks | theme | draw calls | tri vertices | blobs | line pieces before | CPU ms |
|---|---|---|---|---|---|---|---|---|
| x2063 | + Sea | Accurate | light | 449 -> 5 | 33,323 -> 30 | 1,411 -> 1,642 | 5,549 | 3.3 -> 1.6 |
| x2063 | + Sea | Accurate | Midnight | 449 -> 5 | 33,323 -> 30 | 1,411 -> 1,642 | 5,549 | 4.2 -> 1.7 |
| x2063 | + Sea | Cinematic | light | 454 -> 5 | 33,582 -> 30 | 1,434 -> 1,668 | 5,592 | 2.7 -> 1.1 |
| x2063 | + Sea | Cinematic | Midnight | 115 -> 115 | 30 -> 30 | 1,711 | 0 | 1.1 -> 0.8 |
| x2063 | + Sea | Classic | both | 575 -> 5 | 34,406 -> 30 | 843 -> 1,205 | 5,729 | 2.1-4.3 -> 0.4-0.5 |
| x2063 | + Gluons / Valence | Classic | both | 129 -> 5 | 806 -> 30 | +130 | 129 | ~ |
| x5182 | + Sea | Accurate | both | 99 -> 5 | 8,094 -> 30 | 315 -> 371 | 1,344 | 1.9-2.5 -> 0.3-0.7 |
| x5182 | + Sea | Cinematic | light / Midnight | 100 -> 5 / 36 -> 36 | 8,156 -> 30 / 30 | 334 -> 390 / 406 | 1,354 / 0 | 1.2 -> 0.7 / 0.5 |
| x5182 | + Sea | Classic | both | 125 -> 5 | 8,381 -> 30 | 197 -> 287 | 1,392 | 1.4-1.8 -> 0.1-0.4 |
| x5182 | + Gluons / Valence | Classic | both | 32 -> 5 | 231 -> 30 | +33 | 33 | ~ |
- Valence and + Gluons in Accurate and Cinematic: identical before and after (5 calls light; Cinematic Midnight 22-89 calls from switching between additive and normal blending, needed for the look, kept).
- Not done (measured small): the per-frame list sort and has() lookups (at most 45 entries).

## Tested (Linux, software OpenGL)
- Pictures before/after, x2063 + Sea, turning, 3 themes (light, Midnight, Paper) x 3 quark styles: Cinematic Midnight pixel-identical; the others differ only on the ring and line edges (stair pixels where the old line pieces overlapped at their joints, up to 2,800 pixels with any difference for Accurate, 6,900 for Classic). Looked at 4x: same ring size, place, colour and alpha; the tubes the same. No saved pixel reference changed.
- New test ui6c_ds281sea (tests/check_6c.py): Ds-281 + Sea, Accurate, turning: <= 20 draw calls and <= 1,000 vertices a frame. Fails on the parent commit (457 calls, 34,014 vertices), passes now (5, 30).
- check.sh full: 75 of 75 passed (before the changelog text was added; after it: app_settings, windows_decorated, orbit_ui_coverage re-run, pass).

## Not tested
- fps on a real GPU / Windows (no GPU in the cloud). Whether the GPU now does more work per blob (a ring's quad is r+1.6 px square; a tube's quad is its bounding square) than it saved: to be measured by the owner (OWNER-TODO).
