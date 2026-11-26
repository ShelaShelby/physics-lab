# 0.86.0: honest pictures

## What changed
- Black Hole Lab (A, B): the disc's captions come from lab.h (discShapeWords, discEdgeOnWords, discThicknessWords) and follow the disc drawn. The thickness uses AF13 (92)'s spin term: Q = (u^t)² ω_θ² of AF13 (40) within 1e-10 for a = 0 to 0.99, r_ms to 100 M; spin 0 is 0.79.0's to the bit. heightOn holds for every spin. Not done as planned: the thickness is drawn at spin 0 only. The spin tables store only where each ray crosses the plane, so a surface above it cannot be found there. The line beside the Spin slider says so, and ROADMAP has the work (CPU fill, kKerrTableCS, closed forms, kKerrSkyFS).
- Orbit Lab stars (C), before: drawSky works out each star on the CPU in double (the projection, starDraw's pow, lv from the zoom only) and adds one R.disc per star (float colour and alpha). The GPU renderer draws the discs (kFragmentShader mode 1). Feasibility: the maps are scaled to each body's measured V geometric albedo, so screen linear 1 is a white Lambert surface lit face-on at the exposure distance d_E: (R_Sun / d_E)² of the Sun's surface luminance.
- Now: with True sunlight each star gets its true light on that exposure (orbit/star_light.h, one formula with the Black Hole Lab: Willmer 2018's V_Sun -26.76, Pogson, the 8-bit sRGB step). A star is drawn only above that step: Earth's exposure 0 stars, Neptune's 2,869 of 2,874 (F = 900). The Gaia Milky Way is hidden then: it has no calibrated brightness (as in the Black Hole Lab). Even lighting draws exactly as before. Hard scene orbit.true_light_stars replaces blackhole.film_soft.
- Words (D): the Sky caption gives the squeeze from the code (m -1.5 to 6.6: 1,700 times in light, drawn 42 times). The guide names both opacities (0.4 cm²/g: ionised hydrogen's σ_T/m_p = 0.398, rounded; 0.3383: Zhu's 70 % H gas), neither changed. The Morse caption gives the computed overestimate: H2 4.5 %, HCl 14.2 %, N2 21.8 %, O2 24.4 %. All four are above, so no threshold was needed.
- Review (E): R5 the Orbit picture, Simple quarks and the Nucleons look labelled "Made up, for learning"; the realistic values now live in atom/atom_tools.h. R6 Film's own glow, see-through disc, their passes, tests and discproto's film modes removed; the flare caption says it is not published. R7 dirac.h cites YS15 (2)-(4); make_lamb_data.py reads the DIRAC rows. R8 the "No Lamb shift" comment and the lifetime text fixed.

## Tests run
- New tests first (each needs functions or texts the old build lacked): blackhole_tests 1126 checks, 0 failed (testDiscHeightSpin, testDiscWords; testFilmSoft removed with its feature). orbit_tests 2877, 0 failed (testStarsExposure086). rotation_tests 465, 0 failed (testMorseGap). atom_tests 12159, 0 failed (testDiracRows086: all 110 rows within half a printed digit with the paper's α; testMadeUp086: 118 elements x 3 zooms).
- vk_compare_tests: the new star dots on Vulkan against the CPU reference, largest difference 2 levels, 0 pixels over 8 (198 and 93 dots). 709 checks, 0 failed (the first run failed only on my own dot-count floor, 100: set to 50).
- check_blackhole.py precision and sky by hand: precision 79 checks, sky 47 checks, 0 failed. check.sh job: quick 18 of 18 and ui_smoke passed. check_sources.py: ok.

## Not done
- The spinning disc's thickness on the GPU against the CPU at spins 0.5 and 0.99: nothing to compare, the disc is drawn in its plane at spin (above, ROADMAP).
- Needs pc_tests.ps1 on the owner's PC: the new Hard scene orbit.true_light_stars, vk_cards (the star dots), and the Film look after R6 (precision part).

## References changed on purpose
- check_blackhole.py PLBHFILM: one colour all over Film's disc again (0.81.0's laid colour, glow and see-through checks removed with the feature). The benchmark id list: film_soft out, true_light_stars in.
