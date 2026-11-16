# 0.55.4: real light in the Black Hole Lab (plan step 4, second part)
## A. Film's glow from the disc only (fix)
- The lens pass now also writes the disc's own light and the sky behind it to two float layers (only while a glow or the glare is made; otherwise the draw is
  0.55.3's). The glow reads the disc layer: the sky and the stars are drawn unchanged under it. Rule, label, guide, looks.h and the reference caption updated.
- Test (precision): every pixel the reference leaves alone equals the no-glow picture (802,336 pixels, 0 touched; 0.55.3's rule would have added 253,927 more
  units of light over the sky); the glow = its double reference made from the disc alone (1 level, added light 0.5 % off). docs/blackhole/reference_film_r74.png remade.
## B. One true exposure (src/blackhole/light.h)
- Bridge: the catalogue's mag is V (HYG README fields 12 and 25, saved); the Sun's apparent V = −26.76 (Willmer 2018, ApJS 236, 47, Table 3, Johnson V, col. 5);
  a star's light = Ω_Sun·10^(−0.4(m+26.76)) in (Sun's surface luminance × sr), Ω_Sun = π(R_Sun/au)² (IAU values, units.h). The disc was already on the Sun's
  surface scale. Approximate (stated): V as the eye's visible light, the Sun a 5,772 K black body, evenly bright. A star is spread over its drawn dot (sky.h starDrawLight, now the one rule; starDraw = it with the tone map); a dot shows when its light reaches
  the 8-bit screen's faintest step, 0.5/(255·12.92) of white (IEC 61966-2-1). The Gaia map is not drawn under True (not calibrated; the app says so).
- Factor (feeding preset, 20 GM/c², fov 90°, 1280 × 800, automatic exposure): Sirius (V −1.46) is 2.95 × 10¹¹ × fainter than the disc's brightest part and
  9.6 × 10⁷ × below the screen's step; it first shows at screen white 5.7 × 10⁻⁶ Sun's surfaces. The exposure slider now reaches 10⁻¹⁰ (was 10⁻¹).
- Controls (Real, disc on): Sky beside the disc: Shown (default) / True brightness (the dot); the card gives the factor and the exposure where a star first shows.
- Tests (blackhole_physics testTrueLight): Willmer's row and abstract parsed (and found in the saved original); absolute − apparent = 5 log10(10 pc/au) to 0.01;
  the identity (Sun's magnitude over the Sun's disc = 1 to 1e-14); Pogson (5 mag = 100×); starDraw = starDrawLight; the step; a star shows exactly at the exposure
  the readout names; settings and a 0.55.3 file (Shown).
## C. The eye's glare (light.h, light.cpp)
- Model: the CIE general disability glare equation (Vos & van den Berg, CIE 135/1999 eq. 11), printed identically by Bará & Bao-Varela 2023 (arXiv:2212.09103,
  Eq. 2) and van den Berg et al. 2013 (Z. Med. Phys. 23, 6, p. 15): Ψ = 10/θ³ + (5/θ² + 0.1p/θ)[1 + (A/62.5)⁴] + 2.5·10⁻³p sr⁻¹, θ in degrees, 0.1°–100°.
  Eye: 35 years, p = 0.5 (brown; Bará's Figure 2 observer). The PSF is normalised to 1 (both papers): s = ∫ from 0.1° to 100° = 0.27934 is spread, 1 − s = 0.72066
  stays within 0.1°: light moved, not made. Left out (app text): the pattern within 0.1°, adaptation, afterimages, rings and streaks, colour dependence; a camera differs.
- Picture: disc light (before its colour is scaled into the screen) averaged in blocks of 0.2°, read back; the veil on the CPU by a multi-level sum (near pairs:
  Ψ's mean over the two blocks on the source's local map, a table; far pairs: Ψ at the exact angle with the coarse source's spread as a second-order term; 3 × 3
  points per coarse block, quadratic within each parent); composite = 0.72 × disc + sky + veil. Made only when the picture is drawn anew and only once the view
  has rested 0.25 s (while it moves, no glare; the panel says so). Glare: Off / As an eye sees it (the dot, default with the disc); Most realistic picks both.
- Tests (testGlare, 40 checks with the true-exposure ones): the equation's text in both extracts and saved originals; Ψ = the printed formula at 8 angles × 9 eyes
  (2e-16); s by Simpson in ln θ = Gauss-Legendre (1e-9); a point in a fisheye holding the full 100°: laid/spread 0.99997 (limit 1 %); a point near the edge: laid
  0.2409 + outside the picture 0.0382 (sphere quadrature) = s 0.2793 (1 %); the function itself from 1° to 55° (0.4 % from 3°, 2.3 % at 1°, the block's size);
  the levels against every pair directly 0.15 % (where the veil is above 1 % of its peak; farther, near the 100° edge, coarse blocks straddle it).
- Measured (llvmpipe, 4 cores, Real saved case): 1280 × 800: blocks of 5 px, veil 169 ms, picture 45 → 216 ms; 3840 × 2160: 14 px, veil 181 ms, 366 → 717 ms.
  On the picture 88.6 % of the spread light lands inside it (fov 30°). Unchanged, checked: Real with Glare Off and Sky Shown = 0.55.3 (hash 7fff50741d132f22).
## UI part `truelight` (new, 28 checks; the job called it `light`, a name the 0.55.0 part already has): controls only in Real with the disc; True blacks the sky,
  readout = the rule written in Python; 1000× lower exposure brings ≥ 20 star images back; the glare lights the sky near the disc > 2× farther out and takes light
  from the disc; Most realistic; restart; a 0.55.3 file (Shown, eye); Film's far sky = Real's pixel for pixel; chip and panel ≥ 4.5:1 in six themes.
- Reference: docs/blackhole/reference_eye_r74.png (+ .txt; True brightness with the glare, 0.22 MB), reference_film_r74.png remade (1.91 MB).
## Sources (private repo; extracts + SOURCES.md here): Willmer 2018, the HYG README, Bará 2023, van den Berg 2013. The CIE report and Vos 2003: publisher 403.
## Tests run: check.sh quick (PL_SOURCES_DIR set) 17 of 17 (blackhole_physics 804 checks); by hand: truelight 28, precision 78, looks 39, disc 23, spin 19, all passing.
  disc's "not elsewhere" failed at first (the default glare veils the sky round the disc): its runs now set glare=off (reference changed on purpose). Other
  ui_blackhole parts that draw a disc were not run. **The full UI suite was not run (small version).**
