# 0.55.1: the thin gas disc round the non-spinning hole (plan step 2), two pushes (checkpoint 02e8c0a, then this)

## What the user gets
- Controls › "Gas disc (model)" Off/On; with it Feeding rate (1.5 % to 30 % of the Eddington limit, default 10 %), Outer edge (7 to 1000 GM/c², default 30),
  Exposure (Automatic: the picture's brightest part of the disc at full brightness; or your own, as "screen white = N × the Sun's surface"). Settings keys `blackhole.disc/feed/outer/exposure`.
- Presets: Sagittarius A* and M87* open with the disc off; on there, the chip, card and panel say "What a bright disc would look like here; this black hole does not
  have one." Third preset "A feeding black hole (model)": disc on, 10⁸ Suns (AF §1: quasars 10⁶–10⁹ Suns, where YN §1 says luminous AGNs are modelled as thin discs;
  of AF's two ranges, ~10 Suns and quasars, the quasar's disc is the cooler, 7.0×10⁴ K at 0.1 L_Edd, so the visible is a larger share of its light).
- The picture: the disc over and under the hole, the underside and two thinner rings, approaching side brighter, true colour, opaque (sky and stars hidden: 560
  star images at 27 M); drag tilts from face-on to edge-on (the disc's axis is the sky frame's z: a chosen plane, said in the guide). Bending Off hides it.
- Card: inner edge (6 GM/c², km, "nothing shines inside it"), hottest ring (T and where), brightest part (× the Sun's surface in visible light), exposure.
  Guide: a part "The gas disc (a model)" with every limit the job lists; About: Page & Thorne, AF, YN, Cunningham, JPL's 5,772 K; Luminet's line extended.
- The Sun's 5,772 K has one copy now: orbit/sun_facts.h kSunTeffK (JPL Horizons' Sun header, data/jpl/horizons-sun.txt); the star card uses it.

## Module (src/blackhole/disc.h, lab.h) and blackhole_physics (614 checks, 182 new)
- PT (11b), (15n) any spin; BPT (2.12), (2.13), (2.16) = PT (15f-h) to 1e-12; r_ms = 0.54.3's radii; 1 − E² = (2/3)M/r_ms (BPT (2.20)); F = 0 at and inside r_ms.
- L79 p. 233: max at 9.5509 M (9.55), (8π/3)F_max = 1.1459e-4 (1.146e-4). L79 (15) as printed (√3/3) misses it; with √3/2 it equals PT to 1e-11 of F_max.
- AF (97)-(99) = PT to 1e-10 (any spin); AF's 7e26 recomputed 6.57e26 (rounds to 7); L_Edd by YN's 4πGMc/0.4 = 1.2499e38 erg/s (AF: 1.2e38), Ṁ_Edd 1.391e18 g/s (YN: 1.39e18).
- Total light = (1 − E(r_ms))Ṁ by quadrature (PT (30), (31a)) to 1e-8 at a = 0, 0.5, 0.9, −0.9; σ = CODATA 2022 to 2e-10; T ~ (ṁ/m)^¼; hottest ring over
  10–10⁹ Suns and 1.5–30 % L_Edd within YN's 10⁴–10⁷ K (2.45e4 .. 5.19e6 K). Colours = tools/star_colour.py's table (241 rows, within 1 count).
- Rays (Binet's form of L79 (3), Dormand-Prince 1e-13): ends = closed form to 6e-10 rad; far camera vs L79 isoradials r = 6-30 at θ0 = 60°, 80°, direct and secondary:
  346 points within 1e-8 (38 near-side points have b < b_c, outside L79 (13)); 1 + z vs L79 (19) within 9e-11; L79's ellipse far out within 4(M/r)^½;
  vs the ray integrator (geodesic.h) at 4 M and 15 M, 10° and 60°: 54 crossings within 3.6e-10. Face-on circles, left-right mirror (same r, opposite Lz).
- Card's table (768 × 256 × 2, s = ln|x| + |x|/0.1) vs the reference: u within 2e-7, ends within 4e-8; each further image e^−π as wide (0.0428, 0.0431);
  the 4th image ≤ 0.17 px at 2160 px over 10°, the 5th (not drawn) ~0.007 px: the stated cut-off.

## Disagreements with the papers and the plan (the paper wins)
- L79 (13) prints sn²{γ/2 (P/Q)^½ + F}; inverting his own (11) gives (Q/P)^½ (the printed form is 0.4 % off a direct quadrature at P = 497); the test uses (11).
- C75 p. 792: g_max(edge-on)/g(face-on) at r_ms = 1.7 at a = 0; ours 1.89 (direct image, approaching side, 89.9°), as L79 (19) gives there. Not a pass/fail;
  open in ROADMAP. Tested instead: C75's statements (unique face-on g = (1−3M/r)^½, always redshifted; blueshift edge-on at every r > r_ms).
- AF's ṁ is Ṁc²/L_Edd, not L/L_Edd (converted by the efficiency). The plan's 4e6 / 1.5e5 / 2.5e4 K agree (computed 3.94e6 / 1.54e5 / 2.47e4).
## Picture test and measurements (Mesa llvmpipe, 4 cores; not a real card)
- check_blackhole precision (PL_BH_COMPARE), 27 M and 4 M × 10° and 60°: card = double lookup at every pixel (0 pixels of another image, edges moved ≤ 7e-4 px,
  r within 2e-5, g within 3.3e-6: float rounding of the table's coordinates); table vs reference every 16th pixel: r 1.1e-7, g 1.5e-8.
- Disc table refill 53-64 ms (4 threads, only when the distance changes); a turn with the disc 15-27 ms processor time; card pass 35 ms at 1280×800,
  238 ms at 3840×2160 with the disc (19 / 104 without), kept between changes. Benchmark scene 13 now has the disc.
- docs/blackhole/reference_disc_r60.png (+ .txt): the feeding preset from 60 M, 10° from the disc's plane, 1280×800, 1.66 MB.

- check.sh quick 17/17 (PL_SOURCES_DIR set). By hand: check_blackhole disc (new, 23), precision (26), layout (12; its section list now has "disc": reference
  changed on purpose), presets, light: all pass. Full UI suite not run (small version). Looks: above, edge-on, close in, Sgr A* what-if. Nothing skipped.
