# 0.55.0: the Black Hole Lab (plan step 1b), two pushes (checkpoint c8c753f, then this)

## What the user gets
- A fourth tab "Black Hole" (lab "Black Hole Lab") everywhere the labs appear: tabs, Labs menu, keys 1-4, search, Controls, "Most realistic", Defaults, settings and
  setups (keys `blackhole.*`), screenshot names, facts, Learn, About, Data panel (says plainly: no tables yet; your own data still works), benchmark scene 13.
- The picture: our sky (Gaia map at the Orbit Lab's level 1, catalogue stars) bent per pixel round a non-spinning hole that stays in the middle; exact shadow edge
  (anti-aliased by covered area); every star image a point at its magnified brightness (no image drawn brighter than Sirius: the cap, said in the guide).
- Drag turns round the hole, wheel / + − change the distance (2.2 to 1000 GM/c², log slider + typed value, in GM/c², km, AU), field of view 10-360° (a fisheye
  above 180°, corners kept within 180°), Reset view; bottom bar: Bending, Reset view. Presets Sgr A* and M87* (one sentence each), mass (log + typed, with the
  "picture does not change with mass" note), Bending On/Off (dot on On), Sky Our sky / Black. Card: mass, horizon km/AU, distance, shadow size and share of sky,
  clock rate, blueshift and brightening (not drawn), preset's shadow from Earth beside the EHT ring with the caveat. Chip line: "A calculated model: no spin, no gas · our sky, seen from Earth".
- Guide "What is real in this picture" (exact / soft / left out / the unit); About lists the 11 papers with arXiv or ADS addresses. Orbit Lab: Sgr A* card button.

## Module additions (src/blackhole, all tested in blackhole_physics)
- Fix: a camera exactly at 3M lost all digits below 1e-6 rad from the edge (b²−27 formed from b): now from b − b_c. Table error at 3M 1.07e-2 → 1.5e-7 rad (both 0.54.3 open items closed).
- lab.h/.cpp (settings, camera, readout from the module only), LensTable::lookupX/values, HoleView projection (any scale, up to pi), lensPixel (reference),
  starImagesFast (table inversion), hoverBlueshift/Brightening/ClockRate, shadowSkyFraction.

## Measurements (this container: Mesa llvmpipe, 4 cores; not a real card)
- Table refill 0.6-0.7 ms; all stars' images 6.1-6.3 ms (refilled only when the view changes; the old bisection was 287 ms).
- Lens shader drawn anew: 13.7 ms at 1280x800, 101.6 ms at 3840x2160 (kept between changes; benchmark scene 13 redraws every frame for a real card).
- Card against double reference (1280x800, 1000/27/2.5 M): shadow edge within 3.9e-7 px, 0 pixels on the wrong side; directions within 4.4e-6 rad (≥1 px from the
  edge), 2.5e-6 (0.1-1 px), 2.2e-6 (<0.01 px). The edge variable x = (r²−e²)/(r+e) with exact r² keeps single precision honest. Plain sky = Orbit Lab skyMap: max 1/255.

## Tests and references
- blackhole_physics (432 checks): rO=3 integrator vs closed form (to 1e-6 rad) and table vs closed form to 1e-8 rad; view = orbit::skyDirection; lensPixel = LensView;
  fast stars vs starImages (≤6e-8 rad, ≤4.4e-6 rel.); clock/blueshift/brightening vs Riazuelo Fig. 20 z values; half sky at 3M; settings round trip and fallbacks.
- app_settings: prefs round trip with blackhole keys, 0.54 file byte for byte, unknown lab → Orbit Lab, labKey, About links and presets numbers vs presets.h, file name;
  test_shell: lab tab/menu ids and words; analysis: no tables for lab 3; benchmark list 13/21 (reference changed on purpose).
- ui_blackhole (check_blackhole.py, 10 parts): tab, drag, presets vs presets.cpp, 3 distances vs Synge's formula and the drawn disc, bending/sky, Most realistic,
  Orbit card, restart and a CRLF 0.54 file, layout 880x560/1280x800, contrast in 6 themes, precision (PL_BH_COMPARE).
- Full UI run: 31 of 33 first; fixed and re-ran: ui_popups (check_5k windows: the Benchmark window, one scene longer, reached over the top bar at 880x560: now centred
  below it and capped), ui_look (check_5t solid: 4 pixels of one glyph's anti-aliasing changed with the font atlas's order; 0.54.4 gives 0, the
  see-through fault it guards against gives hundreds: limit now 10, reference changed on purpose). Both then passed; check.sh quick 17/17.

## Not done / notes
- Nothing skipped. Owner looks: far view, inside the photon orbit, Controls open, Orbit card button (sent in chat). Gaia map licence question unchanged (0.54.4).
