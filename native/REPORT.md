# Native Physics Lab 0.9.5 (alpha) — report

Orbit Lab: a smooth Milky Way band and star mass labels. Older reports are in git history. `index.html` was not touched.

## Part A — Milky Way band (`orbit/orbit_view.cpp`, `drawSky`)
- The three thick polylines (120 segments each) are now **one triangle mesh** (240 pieces along the band, 34 rows across) with a colour at every corner, so there are no joints. The light fades to nothing on both edges (a cos² profile); three profiles of different width are added (wide faint glow + narrower core, as the three lines gave). Brightness along the band is the same as before (brightest towards the galactic centre); it fades out where the sky map ends.
- **Geometry unchanged and checked**: the band is the great circle through the galactic centre with the stored pole. I converted the standard J2000 galactic centre (RA 266.40499°, Dec -28.93617°) and north galactic pole (RA 192.85948°, Dec 27.12825°) to ecliptic coordinates: (266.8395°, -5.5363°) and (180.0232°, 29.8114°), against the code's 266.84/-5.536 and 180.023/29.811. They agree to 0.001°. **Those reference values are quoted from memory** (the IAU/Hipparcos definition), not re-fetched: arXiv, ADS and the journals were blocked in this session.
- New label "The Milky Way (the plane of our galaxy, simplified)", same style as the constellation names (size 11, blue-grey, centred), shown with the constellation names, at the free spot on the band nearest the middle; it keeps clear of the constellation and star names (skipped if no spot is free).

## Part B — star masses (`orbit/far_masses.h`, `orbit_view.cpp`)
- New table `kFarMass` (value, range, kind, source in a comment next to each number). A label "Mass: ..." is drawn under the star's name (the Sun's label and all older labels are untouched), and one line at the bottom says: "Star masses are published estimates, in solar masses (M☉); some are uncertain." Kinds: `orbit` (dynamical), `lensing`, `models` (an estimate worked out with stellar models; the label says "from models"). A range wider than +-10 % is printed and called uncertain.
- **How the numbers were read:** the papers' own sites (arXiv, ADS, A&A) were blocked, so each value was read from a search-engine excerpt of the paper, with the paper named. They are NOT checked against the papers' tables. A first search that contained my remembered numbers just echoed them, so it was discarded: only searches without numbers were used. That search also showed my remembered Alpha Centauri masses (a different paper) were wrong, so none of the remembered numbers was used.

| Star | Shown | Source |
|---|---|---|
| Alpha Centauri A, B | 1.13, 0.97 | Pourbaix & Boffin 2016, arXiv:1601.01636 (abstract; own error bars not read, range in the table is only the rounding +-0.005) |
| Proxima Centauri | 0.15 (0.099 to 0.21), uncertain | Zurlo et al. 2018, arXiv:1807.01318, microlensing: 0.150 +0.062 -0.051 |
| Sirius A, B | 2.063 +-0.023, 1.018 +-0.011 | Bond et al. 2017, arXiv:1703.10625 |
| Procyon A, B | 1.478 +-0.012, 0.592 +-0.006 | Bond et al. 2015, arXiv:1510.00485 |
| Fomalhaut | about 1.92 +-0.02, from models | Mamajek 2012, ApJ 754, L20 (isochrone mass) |
| Arcturus | about 1.08 +-0.06, from models | Ramirez & Allende Prieto 2011, ApJ 743, 135 |
| Sagittarius A* | 4.3 million | GRAVITY Collaboration 2022, arXiv:2112.07478: 4.30 million, precision about 0.25 % (from an excerpt; the paper's systematic errors, mainly the distance, were not read) |

- **Left out (no source reached, or only an unnamed one): Barnard's Star, Lalande 21185, Epsilon Eridani, Tau Ceti, Altair, Vega, 61 Cygni, Polaris, Betelgeuse, Rigel.** Polaris: the excerpts gave 4.5 +2.2 -1.4 (Evans et al.) and a preliminary 3.45 +-0.75 with the papers not pinned down. Betelgeuse and Rigel: no mass was in the excerpts found (Rigel's evolutionary-model mass of about 18 M☉, Moravveji et al. 2012, had no uncertainty). Proxima's other value, 0.12 +-0.02 (Mann et al. 2015), is a mass-luminosity estimate and is not shown. The Pleiades and the Orion Nebula are not stars and show none.
- **Sagittarius A* checked:** the existing note "about 4.3 million Suns" agrees with the GRAVITY value (4.30 million). Not changed.

## Tests (`app_tests` 1,158 checks; `orbit_tests` 1,369 and `atom_tests` 17,818 unchanged; all pass)
- Every mass entry has a source string, a known kind, belongs to a far-away thing, and `low <= value <= high` with a non-empty range; a star shows a mass text exactly when it has an entry; the ten left-out stars and the two objects show none; the label texts; the Sagittarius A* note agrees with the sourced value. No expectation was written for a mass that was not sourced.
- No saved reference changed. The Sun's label and all older labels are the same code (the diff only adds lines).
- The tests do **not** prove the values are those in the papers (see above), nor how the band looks.

## Looked at (Linux, Xvfb, software OpenGL; 2 screenshots, zoomed far out, 3D, constellation names on)
- Dark theme and Paper theme: the band is soft, with no joints or stripe edges, and the label sits on it. In both, the band is faint and only a short piece is on screen (most of it lies past the edge of the sky map, where it fades out on purpose), so it is subtle. The picture is a dark night sky in both themes (the sky does not follow the page theme), so "light theme" only changes the panels. Mass labels and the note are readable; the note can touch a constellation name in a crowded corner.

## Not tested
- No Windows PC, no GPU: no speed numbers (the mesh is about 17,000 triangles, drawn every frame). Other angles of the band and other themes were not looked at.
