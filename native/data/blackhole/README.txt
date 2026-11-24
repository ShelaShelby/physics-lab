[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/blackhole/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

[0.55.1] The thin disc (src/blackhole/disc.h) reads Abramowicz & Fragile 2013 and Yuan & Narayan 2014 through their extracts in arxiv/ (<name>.extract.txt); Page & Thorne 1974,
Luminet 1979, Bardeen, Press & Teukolsky 1972 and Cunningham 1975 were read from the page images of the scans (numbers typed in tests/test_blackhole.cpp with their page).

[0.55.3] The Film look (src/blackhole/looks.h) reads James, von Tunzelmann, Franklin & Thorne 2015 (1502.03808) through its extract arxiv/1502.03808_james2015.extract.txt
(sections 1.2, 4.1-4.3, the captions of Figures 13, 15, 16, appendices A.3.1 and A.6; pages of arXiv v2); with PL_SOURCES_DIR set the test reads the saved text itself.

[0.55.9] The spinning hole's light paths in closed form (src/blackhole/kerr_closed.h) read Gralla & Lupsasca 2020a (1910.12881) and Berens et al. 2026
(2603.05810); their extracts in arxiv/ (<name>.extract.txt) hold the equations used. Found: (B100) of 1910.12881 as printed (v3 text) has g0 (g0 + x4(r+-))
in a denominator where the integral needs a2 (1 - g0 x4(r+-)) (g0 + x4(r+-)) (REPORT 0.55.9).

[0.70.0] The Real look's camera curve (src/blackhole/tone.h) reads Reinhard, Stark, Shirley & Ferwerda 2002 (University of Utah report UUCS-02-01 of the ACM TOG
paper) through its extract tone/reinhard2002_uucs-02-01.extract.txt (section 3.1, Equations (1) to (4)); the PDF and its text are in the private sources repo.

[0.78.1] The realistic gas disc research (native/PLAN_DISC.md) saved in the private repo, data/blackhole/: arxiv/ Guan et al. 2009 (0901.0273), Broderick et al.
2011 (1011.2770), Zhu et al. 2012 (1202.1530), Pandya et al. 2016 (1602.08749), GRAVITY 2020 (2004.07185), Lee & Gammie 2021 (2011.07151), Dhruv et al. 2025
(2411.12647); glare/ Talvala et al. 2007; web/ the Illinois simulation data pages (CC BY 4.0) and AMD's RX 9060 XT page. Each is one line of native/data/SOURCES.md.
No test reads them yet (no extract): tools/discproto names them in its comments.

[0.80.0] Sagittarius A*'s hot flow (src/blackhole/hot.h; nothing drawn). New extracts in arxiv/: Broderick & Loeb 2006 (astro-ph/0509237: Table 1, sect. 2.2),
Broderick & Loeb 2005 (astro-ph/0506433: Fig. 7's mass and distance), Broderick et al. 2011 (1011.2770: the 1.3 mm flux and its variation), GRAVITY 2020
(2004.07185: Table 1, the slopes). Newly saved in the private repo: Yuan, Quataert & Narayan 2003 (astro-ph/0304125) and Broderick & Loeb 2005a
(astro-ph/0508386). Pandya et al. 2016 (24)-(27) were read on the PDF's page images (pages 10 and 12); YQN Fig. 2 and BL06 Fig. 1 were read on page images
as plot readings (REPORT 0.80.0).

Sources for the Black Hole Lab (0.54.2 research job, 15 Nov 2026; the plan is native/PLAN_BLACKHOLE.md)
=======================================================================================================
Nothing here is used by the app yet. Every file was fetched on 2026-11-15 (UTC) and is kept as received, except where "text" is said:
  scans/*.pdf            the publisher's scan from the NASA Astrophysics Data System, https://articles.adsabs.harvard.edu/pdf/<bibcode>, as received.
                         They are page images (the text layer is OCR and has errors): read an equation from the page picture, not from the OCR text.
  arxiv/<id>_<name>_abs.html   the arXiv abstract page https://arxiv.org/abs/<id> as received (it carries the journal reference used below).
  arxiv/<id>_<name>.txt.gz     text of https://arxiv.org/pdf/<id>, made with `pdftotext -layout` and gzip -9 (the PDF itself is not stored for size; its
                         sha256 and size are listed so the same file can be fetched again). Equations in this text are broken across lines: check a formula
                         against the PDF before coding it. Only 1502.03808 (the base paper) is also stored as its PDF.
The journal reference of each arXiv paper was confirmed on its arXiv page; Marck, Yuan & Narayan, Abramowicz & Fragile, GRAVITY 2022 and Berens et al. also on
INSPIRE (inspirehep.net/api/literature). The ADS scans show the journal, volume and page in their page headers.

The base paper
--------------
arxiv/1502.03808_james2015.pdf (+ .txt.gz, _abs.html)   O. James, E. von Tunzelmann, P. Franklin, K. S. Thorne, "Gravitational lensing by spinning black holes in
    astrophysics, and in the movie Interstellar", Class. Quantum Grav. 32, 065001 (2015), arXiv:1502.03808v2. sha256 2b1cbdb7816fd7585bbe1d9fecba2cb932be67d02f9208d175dda9aa88d4738f, 2,075,301 bytes.
    Used for: the Kerr metric and the hovering (FIDO) observers (A.1-A.3); a ray's constants (A.4, A.12); which rays come from the horizon, in closed form
    (A.5, A.6, A.13, A.14); the ray equations (A.15); the moving camera: aberration (A.9), a circular orbit (A.7), the blueshift (A.16); the ray-bundle
    (magnification) method (App. A.2); point stars and filtering (A.3.1); the film's choices (sections 4.1, 4.2, 4.3, App. A.6).

Orbits, special radii, the shadow
---------------------------------
scans/bardeen_press_teukolsky_1972_ApJ178_347.pdf   J. M. Bardeen, W. H. Press, S. A. Teukolsky, "Rotating black holes: locally nonrotating frames, energy
    extraction, and scalar synchrotron radiation", Astrophys. J. 178, 347 (1972). Used for: horizon (2.6), static limit (2.7), circular-orbit energy, angular
    momentum and angular velocity (2.12, 2.13, 2.16), photon orbit (2.18), marginally bound orbit (2.19), innermost stable orbit (2.21), and the limits quoted
    in the text under them (p. 351).
arxiv/1910.12881_gralla_lupsasca2020a   S. E. Gralla, A. Lupsasca, "Null geodesics of the Kerr exterior", Phys. Rev. D 101, 044032 (2020), arXiv:1910.12881v3.
    sha256 1ff6ec0061d38fafe5c788bad12e688238115333ae40257d84cd75ac120c794a, 932,627 bytes. Used for: the closed forms of every light path outside a Kerr hole
    (angular integrals (29)-(31), inversion (38), (49); radial roots and integrals, section IV and Appendix B).
arxiv/1910.12873_gralla_lupsasca2020b   S. E. Gralla, A. Lupsasca, "Lensing by Kerr black holes", Phys. Rev. D 101, 044031 (2020), arXiv:1910.12873.
    sha256 12aaadad3e059441bdce8ef60c3488887d82d084c6cd5388d5cd1a60e20fd235, 4,122,972 bytes. Used for: the shadow's edge seen from far away (critical
    curve: (38)-(40) with the screen coordinates (55)); the non-spinning limit (71), (72); how much fainter each further image is ((43), (68), (105)-(112)).
arxiv/2105.07101_perlick_tsupko2022   V. Perlick, O. Yu. Tsupko, "Calculating black hole shadows: review of analytical studies", Phys. Rep. 947, 1 (2022),
    arXiv:2105.07101v2. sha256 05d97e753256fe062b873238120e0f548b46aa6dd483405232e60f779a229d5a, 1,699,455 bytes. Used for: the shadow's angular size for a
    hovering observer at any distance from a non-spinning hole (Synge's formula, eqs (2), (6), (7)).
arxiv/2603.05810_berens2026   R. Berens, D. O. Chang, T. Gravely, A. Lupsasca, "Black Hole Vision: an interactive iOS application for visualizing black holes",
    Publ. Astron. Soc. Pac. 138, 114501 (2026), arXiv:2603.05810v1. sha256 e34e0f9941a633d6479ed076792df0dd043936f11d31eff93ae1b0978cde5614, 739,460 bytes.
    Used for: proof that the closed forms run interactively on a phone (far observer, sources on a sphere); the collected equations (non-spinning: (5)-(19)).
arxiv/1206.4259_mueller_frauendiener2012   T. Mueller, J. Frauendiener, "Interactive visualization of a thin disc around a Schwarzschild black hole",
    Eur. J. Phys. 33, 955 (2012), arXiv:1206.4259v1. sha256 241999be04bc109d95464b1b92c5b756291a865a39212a0aef6bdcc68a4b1f94, 407,427 bytes.
    Used for: the closed-form disc picture computed per pixel in a fragment shader, its measured frame rates and its single-precision limit (section 5).

The gas disc
------------
scans/page_thorne_1974_ApJ191_499.pdf   D. N. Page, K. S. Thorne, "Disk-accretion onto a black hole. I. Time-averaged structure of accretion disk",
    Astrophys. J. 191, 499 (1974). Used for: the disc's brightness against radius for any spin: (11b), (12), (14), (15a-n); its assumptions (section II).
scans/luminet_1979_AA75_228.pdf   J.-P. Luminet, "Image of a spherical black hole with thin accretion disk", Astron. Astrophys. 75, 228 (1979).
    Used for: the non-spinning light paths in closed form ((3)-(7), (11)-(13)), the critical impact parameter 5.19695 M and "about 10.38 M" (p. 229), the
    disc flux (15) with its maximum (p. 233), the redshift (19), observed flux = emitted / (1+z)^4 (p. 233), Figs 5-7 (isoradial curves, z curves).
    NOTE (found by this job): eq. (15) is printed with the factor sqrt(3)/3 before the logarithm; Page & Thorne (15n) at zero spin gives sqrt(3)/2, and
    only sqrt(3)/2 reproduces Luminet's own stated maximum (r* about 9.55, (8 pi / 3 M Mdot) F = 1.146e-4). Use Page & Thorne.
scans/cunningham_1975_ApJ202_788.pdf   C. T. Cunningham, "The effects of redshifts and focusing on the spectrum of an accretion disk around a Kerr black
    hole", Astrophys. J. 202, 788 (1975). Used for: the spinning case: the energy ratio g and its extremes (Fig. 1), the transfer function (Table 1) for
    a/M = 0 and 0.9981.
scans/cunningham_bardeen_1973_ApJ183_237.pdf   C. T. Cunningham, J. M. Bardeen, "The optical appearance of a star orbiting an extreme Kerr black hole",
    Astrophys. J. 183, 237 (1973). Used for: brightness and positions of the several images of a point source (Figs 8-13), as a later test reference.
arxiv/1104.5499_abramowicz_fragile2013   M. A. Abramowicz, P. C. Fragile, "Foundations of black hole accretion disk theory", Living Rev. Relativ. 16, 1 (2013),
    arXiv:1104.5499. sha256 d12209d28b3fbb6e6f02ce24ef02484a429feac93556b272f7e8c57923fd2a1b, 6,637,760 bytes. Used for: the Novikov-Thorne solution in
    numbers (section 5.3: F = 7e26 erg cm^-2 s^-1 (mdot/m) r*^-3 B^-1 C^-1/2 Q and the functions A..Q), i.e. how the temperature scales with mass and feeding.
arxiv/1401.0586_yuan_narayan2014   F. Yuan, R. Narayan, "Hot accretion flows around black holes", Annu. Rev. Astron. Astrophys. 52, 529 (2014),
    arXiv:1401.0586. sha256 fbb7abcc884b319abbd0c6670b82b8636ae06d1409c0a2b2ed3a48438a4b4b04, 1,738,913 bytes. Used for: where thin discs exist and where
    they do not (sections 1, 4.2), Sagittarius A*'s luminosity and why it has no thin disc (4.1), the hot flow's temperature (16), the Eddington rate (footnote 1).

What is measured
----------------
arxiv/1906.11238_eht_m87_I   Event Horizon Telescope Collaboration, "First M87 Event Horizon Telescope results. I. The shadow of the supermassive black
    hole", Astrophys. J. Lett. 875, L1 (2019), arXiv:1906.11238. sha256 be6604770bb0d0a6c2a81071518e89d5bc2f9b460f91390518804e0a1f454472, 2,578,342 bytes.
    Used for: wavelength 1.3 mm, ring 42 +- 3 microarcsec, mass (6.5 +- 0.7)e9 Suns, distance 16.8 +- 0.8 Mpc, theta_g 3.8 +- 0.4 microarcsec (Table 1),
    20 microarcsec resolution and brightness-temperature colours (Fig. 3), 17 degrees, zero spin disfavoured (section 6), Rc = sqrt(27) r_g (section 1).
arxiv/1906.11242_eht_m87_V   the same collaboration, "... V. Physical origin of the asymmetric ring", Astrophys. J. Lett. 875, L5 (2019), arXiv:1906.11242.
    sha256 8e62f3733b238e360d84e9b14fb2d56118ab9c151d2d430e28e4cc5a73ee46a1, 4,922,419 bytes. Used for: the gas is a thick, optically thin, hot flow
    shining by synchrotron light; Mdot/Mdot_Edd about 2.0e-5.
arxiv/2311.08680_eht_sgra_I   the same collaboration, "First Sagittarius A* Event Horizon Telescope results. I. The shadow of the supermassive black hole in
    the center of the Milky Way", Astrophys. J. Lett. 930, L12 (2022), arXiv:2311.08680v2. sha256 df498bcaa7f11a5cf6ad15cec6f8d37cd0fad05f6a251505637206cdc01e3c47,
    2,786,789 bytes. Used for: ring 51.8 +- 2.3 microarcsec, 1.3 mm, changes within an hour, Table 1 (theta_g 5.125 microarcsec, distance 8277 pc, mass
    4.297e6 Suns from the VLTI; the Keck values), luminosity and accretion rate (section 2), what the models favour (abstract, section 6).
arxiv/1904.05721_gravity2019   GRAVITY Collaboration, "A geometric distance measurement to the Galactic center black hole with 0.3% uncertainty",
    Astron. Astrophys. 625, L10 (2019), arXiv:1904.05721. sha256 c9a033d3753754740fe994e1a9d20b9b5c9b5d55c34b401802bd9139653062ff, 2,536,311 bytes.
    Used for: the earlier values (R0 = 8178 pc, 4.152e6 Suns), to check the app's text.
arxiv/2112.07478_gravity2022   GRAVITY Collaboration, "Mass distribution in the Galactic Center based on interferometric astrometry of multiple stellar
    orbits", Astron. Astrophys. 657, L12 (2022), arXiv:2112.07478. sha256 541a6bcd46fdcb76a399e166f01742abafa3f3afbb528013a00c7e91d37651e4, 7,691,083 bytes.
    Used for: R0 = 8277 +- 9 pc and M = (4.297 +- 0.012)e6 Suns (abstract, section 4, Table B.1): the values src/orbit/sky_data.inc and far_masses.h cite.

What an observer near a hole sees
---------------------------------
arxiv/1511.06025_riazuelo2019   A. Riazuelo, "Seeing relativity - I. Ray tracing in a Schwarzschild metric to explore the maximal analytic extension of the
    metric and making a proper rendering of the stars", Int. J. Mod. Phys. D 28, 1950042 (2019), arXiv:1511.06025v2.
    sha256 23a0d54e2eaaa981e430bfa0d91cfb58a05598ef40a339f2e32622fe9275e3fc, 27,012,373 bytes. Used for: the one deflection function for all pixels (section IV),
    stars as points with several images (44) and their brightness (51)-(53), the falling observer's view (56)-(65): 84.2 degrees at the horizon, the
    hovering observer's blueshift (B18, B19) and Fig. 20.

Background only (the owner's skin, DECISIONS.md 15 Nov 2026, is NOT an embedding diagram: it bends written equations by the real view's light paths)
------------------------------------------------------------------------------------------------------------------------------------------------------
arxiv/1803.08346_resca2018   L. Resca, "Spacetime and spatial geodesic orbits in Schwarzschild geometry", Eur. J. Phys. 39, 035602 (2018), arXiv:1803.08346.
    sha256 dc7a7114d5cef84fc27a255c931522fb52bb319e799475d3bb1615d7b9c1eb85, 205,290 bytes. Background: Flamm's paraboloid (27) and what it does not mean.
arxiv/1607.06535_han_lee2016   Y.-J. Han, H.-C. Lee, "Embedded diagrams of Kerr and non-Kerr black holes", arXiv:1607.06535v2 (a preprint: no journal found,
    not refereed). sha256 82eb097ead6fdca8b014907da5d23c74d466ed392b84b7876a4161b8f6e4fc42, 295,491 bytes. Background only; not used.

Not fetched (3 tries each, HTTP 403/404 or no answer; they are book chapters with no free copy). Nothing in the plan rests on them:
  J. M. Bardeen, "Timelike and null geodesics in the Kerr metric", in Black Holes (Les Houches 1972), eds C. DeWitt, B. S. DeWitt (Gordon and Breach, 1973), p. 215.
      Its results are taken from James et al. (A.5), (A.6) and Gralla & Lupsasca 2020b (38)-(40), (55), which cite it.
  I. D. Novikov, K. S. Thorne, "Astrophysics of black holes", same book, p. 343. Its disc is taken from Page & Thorne 1974 and Abramowicz & Fragile 2013.
Leads of the job text that were looked at and not used: Shakura & Sunyaev 1973 (covered by the two disc papers above); Marck 1996 (Class. Quantum Grav. 13, 393).

glare/ (0.55.4, the eye's glare in the Real look; the originals are in the private sources repo, the extracts here)
bara2023_arxiv2212.09103.pdf (+ .txt.gz, _abs.html)
    S. Bara, C. Bao-Varela, "Skyglow inside your eyes: intraocular scattering and artificial brightness of the night sky", International Journal of
    Sustainable Lighting (2023), arXiv:2212.09103v3, saved 16 Nov 2026. Eqs. (1), (2): the CIE general glare equation, valid from 0.1 to about 100 degrees,
    the PSF normalised to 1, the average observer of Figure 2 (35 years, p = 0.5). Used in src/blackhole/light.h; tests/test_blackhole.cpp testGlare.
vandenberg2013_straylight_review.pdf (+ .txt.gz)
    T. J. T. P. van den Berg, L. Franssen, B. Kruijt, J. E. Coppens, "History of ocular straylight measurement: A review", Z. Med. Phys. 23, 6-20 (2013),
    as hosted by the Netherlands Institute for Neuroscience, saved 16 Nov 2026. p. 15: the same equation as equation 11 of the CIE report (Vos and van den
    Berg, CIE collection 1999, 135); Figure 4's caption: the total PSF normalised to 1. A second copy, checked against the first in testGlare.
    The CIE report itself and Vos 2003 (Clin. Exp. Optom. 86, 363) could not be saved (the publisher refused the session).

Moving gas (0.56.3 research, native/PLAN_MOVINGGAS.md; the originals in the private sources repo, saved 17 Nov 2026; journal references from each arXiv page,
GRAVITY 2018's from Crossref for its DOI). Read by tools/movinggas/movinggas.cpp's comments and checks; no app code reads them yet.
arxiv/1810.12641_gravity2018 (.pdf, .txt.gz, _abs.html)  GRAVITY Collaboration, A&A 618, L10 (2018): flares orbiting Sgr A* near the ISCO; "31 minutes" at the
    ISCO for 4 million Suns; R ~ 7 +- 0.5 Rg, P = 40 +- 8 min; "the differential rotation in an accretion disk would in a single orbital period shear out an initially compact gas cloud".
arxiv/2209.09926_wielgus2022 (...)  M. Wielgus et al., A&A 665, L6 (2022): ALMA loops, period 74 +- 6 min = 10.0-11.2 r_g (a = 0), t_g = 20.46 s; a Gaussian
    hot spot "diameter of ~6 rg"; light travel time and secondary images included in their model.
arxiv/astro-ph_0506433_broderick_loeb2005, arxiv/astro-ph_0509237_broderick_loeb2006 (...)  A. E. Broderick, A. Loeb, MNRAS 363, 353 (2005) and 367, 905 (2006):
    the hot-spot model, a Gaussian in the spot's rest frame ((5) of 2006), R_S = 1.5 M, on a Keplerian orbit.
arxiv/astro-ph_0606615_schnittman_krolik_hawley2006 (...)  J. D. Schnittman, J. H. Krolik, J. F. Hawley, ApJ 651, 1031 (2006): patches in a simulated
    (MHD, Schwarzschild) disc: 25 degrees by dr/r = 0.3, life 0.3 of an orbit ((36)); Table 2, light of the 6-10 M ring 43.8 % rms (thermal, > 10 Hz).
scans/balbus_hawley_1991_ApJ376_214.pdf  S. A. Balbus, J. F. Hawley, ApJ 376, 214 (1991): the magnetic shear instability (abstract) that makes discs turbulent.
Cunningham & Bardeen 1973 (scans/, saved 15 Nov) read again: p. 254, the one-orbit image's peak "shifts to earlier times" off the plane.

[0.77.0] li2005_arxiv0411583.pdf (Li, Zimmerman, Narayan & McClintock 2005, ApJS 157, 335) and shimura_takahara1995_crossref_2026-11-23.json (the Crossref
record of Shimura & Takahara 1995, ApJ 445, 780): the two simplifications of the disc's light named in What is real in this picture (limb darkening by
electron scattering, Chandrasekhar 1950; the colour-correction factor). In the private sources repo (data/SOURCES.md).
