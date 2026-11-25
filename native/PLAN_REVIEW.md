# The outside review of 25 Nov 2026: the plan for its fixes (0.84.1 research)

Written by the research job 0.84.1. No app code changed. The review's own text is not in the repo. This plan follows the job's list and the owner's decisions of 25 Nov 2026 (DECISIONS.md).
Every number below comes from a saved source. Each source has a line in native/data/SOURCES.md, and the lines quoted are in its extract (<name>.extract.txt, path given).
"Measured here" marks a check this job ran on the saved file. It is not a value from the source.

## The order
- **0.85.0 Orbit Lab**: R1 true dimming and exposure, R2 the Sun's edge (and the same law in the eclipse code), R3 sunspots, R4 the Oort cloud.
- **0.86.0 Atom Lab and Film**: R5 the made-up learning pictures labelled, R6 Film cleaned, R7 the Dirac citation, R8 the old Lamb-shift comment.
- **0.87.0 Black Hole disc**: R9 colour-correction factor, R10 limb darkening, R11 returning radiation, R12 spinning thickness.
- **Later (sources found, not in the three jobs)**: R13 Earth's air (ozone, aerosol), R14 the giants' limb darkening and the moons' opposition surge.
- The owner's answers of 25 Nov 2026 (DECISIONS.md): R3's table is in the public repo; R4's dots sample a published model and Sedna-like objects are shown with the sentence.

Rules for every job below: the tests are written first and fail on the old build. Each source used goes into About, "Where the facts come from", and into a code comment. The CPU stays the exact reference and the fallback. The GPU path is required before the job is done. A job that adds drawing adds a Hard benchmark scene.

---

## R1. Orbit Lab, realistic choice: planets lit by the true sunlight at their distance, a camera-like exposure (0.85.0) -- DONE in 0.85.0
- **Source**: no new number. The sunlight on a body falls as 1/d² with its distance d from the Sun (inverse square: the Sun's power spread over a sphere). The distances come from the lab's own ephemeris (JPL, already sourced).
  The Sun's spectrum is already saved (data/sun/astm_g173_*.csv). Only the ratio (1 au / d)² is new.
- **Formula**: irradiance factor s(d) = (1 au / d)². A surface's linear light = albedo x shading x s(d) x exposure. Exposure = 1 / s(d_followed) for the followed body (a camera set for it). With no followed body, the exposure is set for 1 au (Earth).
- **What the user sees**: in Real (the dot) and Most realistic, Neptune followed looks as bright as now. The Earth would then get (d_Neptune / d_Earth)² times more light and clips white. Followed Earth: Neptune is nearly black.
  The readout: "Exposure set for <body> (<d> au): <other> gets N times less sunlight". The toggle that keeps today's even lighting stays as a non-realistic choice, labelled.
- **Limits stated in the app**: a camera, not an eye (no adaptation). A screen shows a limited range: bodies outside it clip or go black, as in a photograph.
- **Tests**: (a) the same albedo and angle at d and 2d gives exactly 1/4 of the linear light (inverse square). (b) Exposure x s(d_followed) = 1. (c) GPU against CPU for three views (followed Earth, Jupiter, Neptune): at most 1 % of pixels over 8 levels.
  (d) realest.cpp: Most realistic sets the true dimming.
- **GPU path**: one more per-body uniform (s(d) x exposure) in the planet shader. The CPU reference multiplies the same factor. A Hard scene orbit.true_light: follow Neptune, then Earth, the readout on.

## R2. The Sun's edge (limb darkening) from a measured law, the same law in the eclipse code (0.85.0) -- DONE in 0.85.0 (the coefficients are made in C++, render/sun_limb.h; tools/make_sun_limb_data.py writes only the inputs)
- **Source**: H. Neckel, D. Labs, Solar Physics 153, 91 (1994), Table I (scan from ADS, typed). Extract: native/data/sun/neckel_labs_1994_SoPh153_91.extract.txt.
  It gives P5(mu) = A0 + A1 mu + ... + A5 mu^5 with Sum A_n = 1 (Equation 5), for 30 wavelengths from 303.327 to 1098.950 nm, and F/I by Equation (6). mu = cos theta on the disc.
- **Checked here**: for 29 of the 30 rows the A_n sum to 1 and give the printed F/I within the last printed digit. The 365.875 nm row as printed sums to 0.99800, and Equation (6) gives 0.74354 against the printed 0.7445 (REPORT 0.84.1).
  It is in the ultraviolet, below the visible range the app draws.
- **Range of validity**: 303-1099 nm. Measured 1986-1990. The signals within 7 arcsec of the limb were excluded (section 3.2), so the last 7 arcsec of the radius are the polynomial's extrapolation. No activity dependence was detectable (abstract).
  Pierce & Slaughter 1977 (Solar Physics 51, 25) is saved as the older candidate. NL94's abstract says the two agree, NL94 with less scatter. It is not used.
- **Formula for the lab's channels**: for channel c (linear sRGB), I_c(mu) / I_c(1) = Sum_lambda w_c(lambda) E(lambda) / (F/I)(lambda) x P5(mu, lambda), normalised by the same sum at mu = 1.
  w_c is the linear-sRGB weight of the CIE observer (the table tools/make_rayleigh_data.py already uses). E(lambda) is the ASTM G173 extraterrestrial spectrum (saved). Dividing by F/I turns the disc-average spectrum into the disc-centre one (Equation 6).
  The A_n are interpolated linearly in wavelength between the rows (NL94 p. 97: "almost smooth functions of wavelength"; the linear interpolation is the app's choice, stated).
  A weighted sum of quintics is a quintic, so each channel is exactly 6 coefficients. A tool (tools/sun_limb.py) writes them with the inputs' sha256.
- **What the user sees**: the Sun's edge darker and redder than its middle, by the measured law. The Sun's card names Neckel & Labs 1994 in place of "the app's own rule".
- **The eclipse code**: render/shading.h sunlightPast takes the Sun as evenly bright (line 65). It becomes the uncovered part of the disc weighted by I_c(mu), per channel.
- **Tests**: (a) tool: every row except 365.875 nm meets Sum A_n = 1 within 3e-5 (six coefficients rounded to 5 decimals) and Equation (6) within 6e-5 (F/I printed to 4 decimals, plus the coefficients' rounding). (b) Each channel's polynomial is 1 at mu = 1. (c) A one-wavelength weight at a table wavelength returns that row exactly.
  (d) Eclipse: no occulter gives 1. A centred occulter of the Sun's radius gives 0. Partial cases against an independent 2D quadrature of the polynomial over the uncovered area (1e-6). Limb darkening makes a central transit dim more than an edge one.
  (e) GPU (mode 3 in shaders.h) against the CPU: at most 1 % of pixels over 8 levels.
- **GPU path**: 18 coefficients as a uniform block in the Sun's shader (mode 3) and in the eclipse shading. A Hard scene orbit.sun_edge: the Sun filling the view, then a solar eclipse from Earth.

## R3. Sunspots: real spots on dates where they were measured, a plain Sun with a sentence where not (0.85.0) -- DONE in 0.85.0 as outlines; LEFT: the spots' darkness (no fitting measured contrast found in 3 tries, REPORT 0.85.0)
- **Sources**: (1) the Royal Greenwich Observatory - USAF/NOAA sunspot group records, from NASA Marshall (D. Hathaway): one row per group per day, May 1874 to 2 Oct 2016 (g2016.txt is the last file; g2017.txt does not exist).
  Columns (format.txt, extract native/data/sun/spots/format.extract.txt): date and time, group number, observed and corrected whole-spot area (millionths of the hemisphere), distance from disc centre,
  position angle, Carrington longitude, latitude, central meridian distance. The umbral area exists only for 1874-1976. MissingDays.txt lists the days with no record.
  The page says its derived data raise the USAF/NOAA areas by a factor of 1.4 after 1976, and the raw gYYYY.txt files are uncorrected (extract native/data/sun/spots/msfc_greenwch_page.extract.txt). The app uses the raw files and states the factor.
  (2) NOAA's daily Solar Region Summary (SRS), archived by NCEI, one file per day, 1996-2026 (archive listing read on 2026-11-25; one sample and the format sheet saved): number, location (e.g. N25W83), Carrington longitude, area (millionths), class.
  SWPC calls these preliminary data (the format sheet). They cover the dates after 2 Oct 2016.
- **Range and size (measured here)**: 1874-2016 has 246,265 rows, 241,416 with an area above 0. Packed as (day, latitude x 10, longitude x 10, area): 8 bytes a row, 1,931,328 bytes, 1,329,369 bytes after gzip -9.
  The repo's largest data files today are 2.0-2.5 MB, so a packed table fits. NOAA SRS for the dates after 2 Oct 2016 is one small file per day (not yet fetched: 0.85.0 fetches them).
- **Licence**: NASA and NOAA are US government. The RGO part's terms are not stated on the page. **owner decides**: the packed table in the public repo (recommended: it holds only measured positions and areas), or a download step at first use.
- **Drawing**: each group as one dark round spot of its measured whole-spot area, at the record's latitude and central meridian distance at the record's time. It is moved to the shown time by the Sun's rotation W = 84.176 + 14.18440 d
  (IAU, native/data/jpl/naif_pck00011.tpc BODY10_PM; the shown time is at most half a day from the record). No umbra and penumbra split after 1976, no differential rotation within the half day. The limits are said in the app.
  Dates before May 1874, after the last record, or on a missing day: a plain Sun and the sentence "No sunspot record for this date".
- **source needed**: the spots' darkness. A measured umbra and penumbra contrast against wavelength, for the lab's channels. Not searched in this job. Until it is found, 0.85.0 draws no spots.
  It does the table, the positions and the sentence, with the spots' outlines only and labelled "outline". Candidates to check (from memory, not checked): Maltby et al. 1986 (umbral model), Solanki 2003 (review).
- **Tests**: (a) the packer: row counts per year against the files; three rows read back exactly (one 1874, one 1976, one 2016). (b) A spot at central meridian distance 0 and latitude 0 draws at the disc centre. A spot at CMD 90 draws on the east limb (position angle 90 in format.txt).
  (c) A missing day shows the sentence. (d) The GPU spot pass against the CPU.
- **GPU path**: the day's groups (a few dozen) go into a small storage buffer. The Sun's shader darkens inside each disc on the sphere. A response line (ui/benchmark.cpp waitDefs) for loading the table at the first Sun view.

## R4. The Oort cloud: no invented dots; a labelled region (0.85.0) -- DONE in 0.85.0 by the owner's answer: the dots sample a published model (Vokrouhlicky et al. 2019); the three objects on their JPL orbits. LEFT: the inner part's lean (a figure only)
- **Sources**: Brown, Trujillo & Rabinowitz 2004 (ApJ 617, 589): 2003 VB12 (Sedna) "is best considered a member of the inner Oort cloud" "in the two most likely cases". The title says "candidate".
  Sheppard et al. 2019 (AJ): 2015 TG387 is "the third known IOC after Sedna and 2012 VP113". "Any object with an aphelion beyond a few thousand au is considered an outer Oort cloud object."
  Chen et al. 2013 (ApJL): 2010 GB174 "a new member of the inner Oort cloud". Kaib et al. 2009 (ApJ 695, 268): 2006 SQ372 "a likely long-period comet from the inner Oort cloud".
  Chen et al. 2025 (Nature Astronomy): the same kind of orbit is called "Sedna-like TNOs" there. JPL's Small-Body Database classes Sedna, 2012 VP113 and 2015 TG387 as "TNO" (TransNeptunian Object). JPL has no Oort cloud class.
  Extracts: native/data/oort/arxiv/*.extract.txt, native/data/oort/sbdb_orbit_class.extract.txt. The Minor Planet Center was not checked.
- **Answer**: no known object is observed in the outer Oort cloud. Membership of the inner Oort cloud is claimed by the discovery papers and **disputed** in practice: other authors and JPL call the same objects TNOs or Sedna-like.
- **Drawing**: the made-up dots go (belts.h "the Oort cloud dots are made up"; date_text.h line 107). In their place a labelled region between NASA's edges (kOortInnerAU 2,000 to kOortOuterAU 100,000 au, already sourced: data/oort NASA facts):
  a faint shell outline and the label "Oort cloud (region; no object in it has been seen)". The card says why: the comets come from it, its objects are too far and faint to see.
- **owner decides**: show Sedna, 2012 VP113 and 2015 TG387 as objects with the sentence "called inner Oort cloud objects by their discoverers; JPL lists them as trans-Neptunian objects" (their orbits from JPL SBDB), or not at all.
- **Tests**: the realistic set has no Oort dots. The region's radii are the belts.h constants. Text check of the label and card.
- **GPU path**: the shell is one line loop per view (the existing line pass). A Hard scene orbit.oort_region: zoomed out to 100,000 au.

---

## R5. The made-up learning pictures: kept, labelled as made up, never in the realistic choice (0.86.0)
- **Source**: none needed (no physics number). The owner's decision of 25 Nov 2026.
- **Which**: the Simple quark style (qstyle Simple, with its flux tubes), the nucleus colours and glow (the Nucleons style's colours and glow), the Orbit view's electron ball and glide (atom_sim.h gAng, glideOrbit).
- **What the user sees**: each of them has the label "Made up, for learning" next to its choice and in its caption. The realistic choice (the dot) and Most realistic never show them.
  ui/realest.cpp already picks Accurate, Density and Cloud 3D. The job checks that no other setting brings any of them back.
- **Tests**: after applyAtom, none of the made-up pictures is on, for every element and view. The label text is there in each of the three. This closes ROADMAP's "Simple quark style's flux tubes: remove, or keep as a labelled cartoon" (kept, labelled).
- **GPU path**: no new drawing.

## R6. Film shows only the film paper's choices (0.86.0)
- **Source**: James, von Tunzelmann, Franklin & Thorne 2015 (J15), the extract native/data/blackhole/arxiv/1502.03808_james2015.extract.txt (sections 4.1-4.3, App. A.6, already saved).
- **What goes**: the lab's own glow (blackhole/looks.h kGlow*, glowReference, the glow passes) and the see-through, soft-edged disc (kFilmFade, kFilmTau, 0.81.0). Their tests go with them. Film's label loses "our own glow".
- **What the user sees**: Film with J15's choices only: the hard-edged opaque disc, J15's colour choices. The caption says the film's lens flare is not drawn: its measured point-spread function is not published (J15 4.3.1, already quoted in looks.h).
- **Tests**: Film pixels against a CPU Film reference without glow and fade. The ROADMAP lines on Film's see-through disc and the glow's lens spread function are removed.
- **GPU path**: passes removed. The benchmark scenes that used Film still run (fewer passes).

## R7. The Dirac energy's citation (0.86.0)
- **Source**: Yerokhin & Shabaev 2015, J. Phys. Chem. Ref. Data 44, 033103, the LaTeX source already saved (data/lamb), Hlike8.tex Equations (2)-(4).
  Extract: native/data/lamb/yerokhin_shabaev_2015_dirac.extract.txt. E_D = m c^2 [ sqrt(1 - (Z alpha)^2 / N^2) - 1 ], N = sqrt((n_r + gamma)^2 + (Z alpha)^2), gamma = sqrt(kappa^2 - (Z alpha)^2), n_r = n - |kappa|.
- **Check against the app (done here, algebra)**: |kappa| = j + 1/2, so n - delta = n_r + gamma, and sqrt(1 - (Z alpha)^2/N^2) = (n_r + gamma)/N = (1 + (Z alpha)^2/(n - delta)^2)^(-1/2). It is the app's formula (atom/dirac.h line 21).
- **Check against numbers (measured here)**: the table's DIRAC row for Z = 1, 2 and 9 (1s) is matched to every printed digit by the formula with CODATA 2018 or 2022 alpha, written without cancellation (-x^2 / (1 + sqrt(1 - x^2))).
- **The fix**: atom/dirac.h line 23 cites YS15 (2)-(4) in place of "written from knowledge, NOT looked up". tools/make_lamb_data.py also reads the DIRAC rows (1s, 2s-1s, 2p3/2-2p1/2 for Z = 1-110) into lamb_data.inc.
- **Tests**: diracInfiniteEV against the DIRAC rows for every Z, in units of h c R_inf. The bound is derived from the difference of the two alpha values (CODATA 2022 in the app; the paper's alpha is not named in the extract), not chosen.

## R8. The old Lamb-shift comment (0.86.0)
- atom/dirac.h line 7 says "No Lamb shift, no other QED effect". Since 0.49.3 the lab adds the Lamb shift of 1s, 2s, 2p for Z = 1-110 (lambShiftEV, the same YS15 table). The comment is rewritten to that.
  The app text that says the same is found by grep and fixed with it. No source needed beyond YS15 (saved).

---

## R9. The disc's colour-correction factor (0.87.0)
- **Sources**: Shimura & Takahara 1995 (ApJ 445, 780; scan, typed; extract native/data/blackhole/scans/shimura_takahara_1995_ApJ445_780.extract.txt): f ~ 1.7 at about 10 % of the Eddington rate, f ~ 1.8-2.0 near it.
  Below 1 % no f fits. Only for black holes of 1.4, 3 and 10 M_sun (abstract). **Not enough** alone: the lab's mass runs from 1 to 1e11 M_sun (lab.h kMassMin, kMassMax).
  Done et al. 2012 (MNRAS, doi:10.1111/j.1365-2966.2011.19779.x, arXiv:1107.5429; extract native/data/blackhole/arxiv/1107.5429_done2012.extract.txt): f_col ~ (72 / T_keV)^(1/9) where electron scattering dominates (Equation 1, above 1e5 K).
  f_col ~ (T_max / 3e4 K)^0.82 for 3e4 K < T_max < 1e5 K (Equation 2). f_col -> 1 below 3e4 K. "approximately unity in the optical part of the spectrum". They combine (1) and (2) "to calculate fcol at each radius".
- **Checked here**: at 1e5 K Equation (2) gives 2.68 and Equation (1) gives 2.73. The paper says "unity to 2.7".
- **Enough to build**: yes, as an approximate rule ("~" in the source). The limits stated in the app: an approximation fitted to radiative-transfer models, non-spinning discs.
- **Formula**: each disc annulus emits f^-4 B_nu(f T) in place of B_nu(T), f = f_col(T) per radius. This keeps the bolometric flux.
- **What the user sees**: Real: hot discs (small masses) a little bluer and dimmer in visible light. Discs below 3e4 K are unchanged. The card names the factor and its range.
- **Tests**: (a) Integral of f^-4 B_nu(f T) d nu = sigma T^4 / pi for f = 1.0, 1.7, 2.7 (conservation). (b) f_col at 2e4 K = 1. f_col at 1e5 K from (1) and from (2) within 2 % of each other (both "~2.7"). (c) GPU disc pixels against the CPU.
- **GPU path**: f_col(T) is an analytic function in the disc shaders (kKerrFS, the lens and sky passes), the same as the CPU reference. No table.

## R10. The disc's limb darkening (0.87.0)
- **Source**: Chandrasekhar 1946, ApJ 103, 351 ("Radiative equilibrium of a stellar atmosphere. X"), Table 2, p. 365: the emergent intensity of a semi-infinite electron-scattering (Rayleigh phase function, polarised) atmosphere.
  11 values of mu from 0 to 1, in the "third approximation". Extract: native/data/blackhole/scans/chandrasekhar_1946_ApJ103_351.extract.txt (column [Il + Ir]/F).
  The typed table passes all its internal relations (sums, ratios, the degree of polarisation) to 1-3 units of the last digit.
- **Checked here**: I(0)/I(1) = 0.4151 / 1.2628 = 0.329. 2 x Integral I(mu)/F mu dmu = 0.991 from a 4th-order fit through the 11 points, not 1. The table is an approximation.
  The exact table (Chandrasekhar 1950, Radiative Transfer, Table XXIV) is not saved (owner's files list).
- **Formula**: in the gas's frame, I(mu_e) = F_NT x [I/F](mu_e) / (2 Integral [I/F] mu dmu), so the emitted flux stays the Novikov-Thorne flux. mu_e is the cosine of the emission angle to the disc's normal in the gas's frame.
  The GPU and CPU light paths already give each ray's constants and g. mu_e follows from them (Cunningham 1975, already saved, the frame of the emitting gas). The table is interpolated in mu (cubic, a choice, stated).
- **Range**: grey, pure electron scattering, semi-infinite, plane-parallel. Absorption and the disc's real vertical structure are not included. Polarisation is computed by the source but not drawn.
- **What the user sees**: the disc seen nearly edge-on is fainter than face-on, by the measured-law factor (about 1/3 at grazing angles). The guide's "the lab draws neither" becomes true for one of the two.
- **Tests**: (a) the flux normalisation integral equals 1 after the division (quadrature). (b) Face-on (mu_e = 1) and grazing (mu_e -> 0) ratios equal the table's. (c) GPU against CPU at 85 degrees from the pole.
- **GPU path**: an 11-point table as a constant array in the disc shaders.

## R11. Returning radiation (0.87.0)
- **Source**: Cunningham 1976, ApJ 208, 534 (scan, typed; extract native/data/blackhole/scans/cunningham_1976_ApJ208_534.extract.txt). The flux returning to a thin disc by gravitational focusing is "unimportant for thin disks around non-rotating black holes".
  It is significant for a/M ~ 1. Computed for a/M = 0.9, 0.99, 0.9981, 0.9999. Transfer functions T_f(y_o, y_e) and T_s (Equation 7a), y = (r/r+)^-2 (6b).
  The returning flux f_o(y_o) = Integral_0^1 T_f(y_o, y_e) [f_o(y_e) + f_g(y_e)] dy_e (8a). Tables 1-4 give T_f and T_s on a 10 x 10 grid (y = 0.05 ... 0.95) for each spin.
- **Enough to build and test**: yes. The lab's own ray tracer computes T_f by (7a) with the same emission law. It is tested against Tables 1 and 2 (a/M = 0.9 and 0.99, the lab's range; the build job types the two tables from the page images, p. 537).
  Then (8a) is solved by iteration for the returning flux, which adds to the disc's own heat.
- **Limits**: thin disc, the returning light absorbed and re-emitted (Cunningham's assumption), steady state.
- **What the user sees**: round a fast-spinning hole the inner disc is a little hotter and brighter. Without spin nothing changes visibly.
- **Tests**: T_f at the 100 grid points of Table 1 and 2 within a bound derived from the tables' printed digits and the lab's ray-count error. f_o -> 0 as spin -> 0 at the ISCO (the abstract's statement, as a trend).
- **GPU path**: T_f is computed once per spin by the GPU light-path pass (a 10 x 10 or finer grid), solved on the CPU (a small linear system), and enters the disc's temperature table. A response line for the table at a new spin.

## R12. The disc's thickness round a spinning hole (0.87.0)
- **Sources**: Abramowicz, Lanza & Percival (arXiv:astro-ph/9611101, "ALP97"; extract native/data/blackhole/arxiv/astro-ph_9611101_abramowicz_lanza_percival1997.extract.txt): (27) with L*^2 = L^2 - a^2 (E - 1), E = u_t, metric (+,-,-,-).
  Abramowicz & Fragile 2013 (Living Rev. Relativity 16, 1; saved; extract native/data/blackhole/arxiv/1104.5499_abramowicz_fragile2013_vertical.extract.txt): (92) Pi / (Sigma H^2) = (L^2 - a^2 (E^2 - 1)) / (2 r^4), E = -u_t.
  The vertical epicyclic frequency (40): omega_theta^2 = Omega_K^2 (1 - 4 a x^-3/2 + 3 a^2 x^-2).
  Zhu et al. 2012 (6), which disc.h line 97 follows, prints R_z = (u_phi^2 + a^2 (u_t - 1)) / r.
- **Checked here**: the three print the spin term differently. The restoring term L^2 - a^2 (E^2 - 1), with circular-orbit E, L, u^t, reproduces (40) exactly for a = 0.5, 0.9, 0.99 at r = 6, 10, 30 M.
  ALP97's (E - 1) is off by up to 0.9 %, Zhu's form by up to 3 % (a = 0.99, r = 6 M). The E, L and u^t used were the textbook circular-orbit forms. The build job takes them from Bardeen, Press & Teukolsky 1972 (2.12)-(2.16), saved.
  REPORT 0.84.1 names the discrepancy.
- **The fix**: R_z with the AF13 (92) term: (u_phi^2 - a^2 (u_t^2 - 1)) / r in the lab's units, with the sign of u_t as the lab defines it. The first test pins the convention (below).
- **Tests (first)**: the midplane restoring acceleration per unit height equals omega_theta^2 from AF13 (40) for a = 0, 0.5, 0.9, 0.99 and r from the ISCO to 100 M (1e-10). Spin 0 reproduces today's thickness exactly.
- **What the user sees**: a spinning hole's disc gets its true thickness (today it is drawn in its plane).
- **GPU path**: the thickness table is already on the GPU for spin 0. The spin term enters the same table.

---

## R13. Earth's air: ozone and aerosol (later; sources found)
- **Ozone**: Serdyuchenko et al. 2014 (AMT 7, 625, CC BY 3.0) and the IUP Bremen data file (213-1100 nm, 0.01 nm grid, 11 temperatures 193-293 K; extract native/data/atmosphere/ozone/serdyuchenkogorshelev5digits.extract.txt).
  The file's header states a total relative systematic uncertainty below 3 % (extract). The download page gives a smaller figure for the absolute measurements. The app quotes the file's.
  Enough to build: the Chappuis band in visible light, weighted by the sRGB channels as the Rayleigh table is. The ozone column is not in this source (**source needed**: a standard ozone column and profile).
- **Aerosol**: the Handbook of Geophysics and the Space Environment (AFGL 1985), chapter 18 (Fenn, Clough, Gallery, Good, Kneizys, Mill, Rothman, Shettle, Volz), from NOAA's site. It has the rural, urban and maritime models
  (Tables 18-6, 18-7: size distributions by humidity; 18-9: refractive indices) and the vertical extinction at 0.55 um by visibility (Table 18-10a). Its text layer is OCR with errors: numbers are typed from the page images in the build job.
  Shettle & Fenn 1979 (AFGL-TR-79-0214) and OPAC (Hess et al. 1998) were refused (DTIC 403 twice, AMS 403). Enough to build with Mie theory from the size distributions and indices. The asymmetry factors are only in figures.
- **GPU path**: the planet shader's air already uses per-channel optical depths. Ozone absorption and aerosol extinction add two more per-channel terms.

## R14. The giants' limb darkening and the moons' opposition surge (later; partly found)
- **Saved**: Deau et al. 2009 (opposition phase curves of the Galilean moons, Enceladus, Rhea, Iapetus, Phoebe, the Uranian moons, Nereid, Triton: Table 3; extract native/data/planets/photometry/0902.0345_deau2009.extract.txt).
  It covers only the phase angles near opposition of each body's data. Dyudina et al. 2016 (Saturn's Barkstrom limb-darkening coefficients from Dones et al. 1993: Table 3; Jupiter's Henyey-Greenstein fits: Table 2).
  Mayorga et al. 2016 (Jupiter's phase curve in six Cassini filters: Table 2). Heng & Li 2021 (Jupiter, double Henyey-Greenstein: Table 1). Irwin et al. 2021 and 2022 (Uranus and Neptune Minnaert parameters, in figures only).
- **Enough**: Jupiter and Saturn yes (tables). Uranus and Neptune no (figures only: **source needed**, their tabulated R0(lambda), k(lambda)). The moons' surge yes near opposition. A full phase law for each moon also needs the curve beyond the surge (**source needed**).

## Files the owner saves (the cloud could not fetch them)
- Chandrasekhar 1950, Radiative Transfer, Table XXIV (the exact electron-scattering limb darkening; the 1946 third approximation is saved).
- The Debrecen Photoheliographic Data (individual spots and umbrae 1974-2018; the site refused: 403 and no answer, 2 tries).
- A measured sunspot contrast that fits group records: Albregtsen & Maltby 1981 (umbral intensity against wavelength), Maltby et al. 1986 (ApJ 306, 284), Solanki 2003 (A&ARv 11, 153). 0.85.0 found only Mathew et al. 2007 (one wavelength, per spot).
- Duncan, Quinn & Tremaine 1987 (AJ 94, 1330): ADS's scan service answered 500 and 504 (0.85.0). The SWPC warehouse of SRS files (no answer, 503; the NCEI copy works).
- Shettle & Fenn 1979, AFGL-TR-79-0214 (DTIC refused twice). Hess, Koepke & Schult 1998, OPAC, BAMS 79, 831 (AMS refused once). Not needed if the Handbook chapter is enough.
