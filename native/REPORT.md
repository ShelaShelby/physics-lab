# 0.55.3: the Real and Film looks (Black Hole Lab, plan step 4, first part)
## What the user gets
- Controls › Look: Real (the closest-to-reality dot; Most realistic picks it) / Film; key `blackhole.look`, a file without it opens as Real. Real is 0.55.2's picture to the pixel;
  its caption: "A calculated model: the standard thin disc. Real for quasars and for small black holes feeding from a star. A real disc is blinding; the glare an eye would see is not drawn yet."
- Film: each choice labelled in the panel, the card, the chip and the guide's new part "What the film changed" (one row per choice: the physics, the film's choice, the paper's reason,
  where). Spin and disc controls show the film's values, locked, with a line why; the user's own spin and disc come back with Real. "The paper's view" sets 74.1 GM/c², 86.56° from the axis.
  About: J15's entry names its use for the Film look. No fourth preset: **the paper gives no mass for the film's black hole** (its text searched for Gargantua, solar mass, M⊙, million and 10^8: none gives one).
## The Film choices, each read in James, von Tunzelmann, Franklin & Thorne 2015 (arXiv v2 pages; saved text in the sources repo, extract data/blackhole/arxiv/1502.03808_james2015.extract.txt)
- Spin 0.6: §4.1.1 p. 26, §1.2 p. 5, App. A.3.1 p. 39; reason p. 26 ("too confusing for a mass audience"); the story needs a/M ≃ 1 (§1.2), the paper's disc figures use 0.999 (Figs 13-15).
- Disc: thin, "marginally optically thick" (drawn opaque), not accreting, one temperature T = 4500 K, black body (§4.1.1 p. 26). Edges 9.26 and 18.70 M: Fig. 13's caption p. 24 (the
  paint-swatch disc; Fig. 14 "the same geometry", Fig. 15a = Fig. 14 at 0.6). The paper gives no other edges for the film's disc.
- No colour and no brightness shift: Fig. 15a and 16 captions, §4.1.1 p. 24 ("transported ... unchanged"), reason §4.2 ("exceedingly lopsided").
- Colour: App. A.6 p. 43: a 6500 K black body with equal R, G, B by one gain per channel. The paper's camera used film-stock curves (its ref. [64], not given): the app's CIE 1931 table
  into sRGB in their place, gains keeping 6500 K's luminance. The Film disc = gains × blackBodyLinear(4500 K): (255, 227, 188) at automatic exposure.
- Glow: Fig. 16 / §4.3.1: the flare was the measured point-spread function of the IMAX lenses, not published. Ours (labelled "our own glow, in place of the film's lens flare"):
  **the light above half the screen's white, averaged in 4 × 4 blocks, blurred by a Gaussian of σ = height/80 (to 3σ, light outside the picture counted as none), 0.6 of it added back in
  linear light**. It only adds light; it does not conserve light (it adds 0.6 × the light above the threshold, spread out; the reference loses none away from the edges, test 2e-3).
  Made when the picture is drawn anew and kept with it (3 small passes); switching looks refills no table unless the spin changes (tested at the film's spin).
- Paper's view: Fig. 13's caption "rc = 74.1M and θc = 1.511 (86.56o)": the two angles disagree in the last digit (1.511 rad = 86.574°; 86.56° = 1.51076 rad, which rounds to 1.511):
  the lab uses 86.56° (more digits). The caption gives no field of view nor the camera's motion; the lab's camera hovers there.
- Not in the paper, so not in the app: the mass, the textures, the real flare, the colour grading, the "embellishments" (§4.2 p. 28). The app text says Film is a copy of stated choices.
## Module (src/blackhole/looks.h, .cpp: FilmChoices with every number's place, lookRule, white balance, glowReference, the guide's rows); lab.h: Settings::look, shown(), paperView(),
  discLight() (Real: 0.55.2's rule to the bit; Film: one colour), discGeom (Film: the paper's edges). Same ray tables and images; the Kerr shader gets one uniform (the fixed colour).
## Tests
- blackhole_physics 764 checks with PL_SOURCES_DIR (38 new; 762 without): every Film number parsed from the paper's text (the saved original via stb's zlib when PL_SOURCES_DIR is set,
  all 14 extract blocks found in it; the extract otherwise); the Film colour = CIE black body at 4500 K through the gains (6500 K → equal channels to 1e-12); same light at g = 1.5 and 0.4
  and at every radius (Real: 5.4 × brighter and bluer on the approaching side); Real's light = 0.55.2's rule exactly; settings, readout, paper's view; the glow reference adds only, loses none.
- Picture test (check_blackhole precision, Mesa llvmpipe): Real's saved case (feeding preset, spin 0.6, the paper's view, fov 30) = 0.55.2's picture, hash 7fff50741d132f22 made with
  0.55.2's code before any change. Film at the paper's view against the double lookup: 0 pixels of another image, edges moved ≤ 0.0018 px (limit 0.1),
  radii 9.26005..18.69989, one colour over 56,179 fully covered disc pixels; with the glow = its double reference to 1 level of 255, 0 channels darker, added light 0.3 % from the reference's.
- **Found and fixed (test harness only)**: the renderer keeps the last spin/disc table by its version number, and each view counted from 1, so a second view in one process (the
  picture tests draw several) could be shown the first view's table. The first run of precision failed on spin 0.5 that way (and Film's edges moved 0.16 px, a table of 1.511 rad).
  Versions are now unique across views (blackhole_view.cpp nextVersion). The app has one view: no user saw it.
- UI part `looks` (new, 39 checks, 1280 × 800): Look and its dot; Film locks (typed spin and Off refused), labels with the module's numbers, chip changes, paper's view, no refill at the same
  spin, Most realistic → Real, restart keeps Film, a 0.55.2 file opens as Real; six themes: card, chip and Look heading ≥ 4.5:1 over the Film disc.
- check.sh quick (PL_SOURCES_DIR set): 17 of 17. By hand: looks 39, disc 23, spin 19, layout 12 (its section list now has "look": reference changed on purpose), precision 74 (on the second run, after the fix above).
  **The full UI suite was not run (small version).**
## Measurements (llvmpipe, 4 cores): glow pass 11-19 ms at 1280 × 800, 131-135 ms at 3840 × 2160 (two runs; Film without it 43-51, 344-352 ms); only on a new picture.
- docs/blackhole/reference_film_r74.png (+ .txt naming every choice): Film at the paper's view, 1280 × 800, 1.85 MB, by `blackhole_picture --film`.
## Skipped / left: quality levels High and Ultra, the eye's glare, one true exposure (ROADMAP lines, sources needed). Looks: Film at the paper's view, from above, Real/Film side by side, the guide.
