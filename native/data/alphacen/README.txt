The papers this README names are NOT in this public repo: they are in the private repo ShelaShelby/physics-lab-sources at data/alphacen/ (same names).
native/data/SOURCES.md lists each with its reference, URL, date read and sha256. No test reads them yet (0.72.1 is research only).

The Alpha Centauri system (0.72.1 research, Orbit Lab; the plan is native/PLAN_ALPHACEN.md, which names the table, equation or page of every number).
Every paper was saved as received from arXiv on 22 Nov 2026 (the version is named). Every number the plan uses was read by the lead session in the saved
PDF (pdftotext -layout) and checked there. Journal and publisher pages, ADS, SIMBAD, VizieR, the NASA Exoplanet Archive, the Gaia archive and iau.org
were refused by the session's network: the Gaia DR3 values of Proxima are taken from the papers that quote them (Libralato et al. 2025 Table 2,
Suarez Mascareno et al. 2025 Table 2); the files the owner could save are listed in ROADMAP.md "Waiting for the owner".

Kept in this folder (own outputs and a small licensed extract):
hyg_v41_rows_near_sun_from_alphacen.csv
    The 67 rows of the HYG Database v4.1 (hygdata_v41.csv, CC BY-SA 4.0, the same file and sha256 as data/stars/README.txt names:
    d9f69fd86bbf90a4e4d52b4c5c53eacfa6dfc0bfdef85bfd94f095e0bebe4ebd, fetched again 22 Nov 2026 from
    https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv), lines copied exactly with the header, of every star
    within 2 degrees of RA 39.85892, Dec +60.83163 (the antipode of Akeson et al. 2021 Table 9's A-B barycentre: where our Sun stands in their sky).
    All 67 have con = Cas: the Sun seen from Alpha Centauri is in Cassiopeia (the IAU boundary table itself could not be reached).

Saved in the sources repo:
akeson_2021_arxiv2104.10086.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2104.10086 (arXiv:2104.10086v2): Rachel Akeson et al., "Precision Millimeter Astrometry of the $α$ Centauri AB System" (2021), arXiv:2104.10086v2.
    Used: Table 8 (orbit, masses, radii, luminosities, parallax 750.81 +- 0.38 mas), Table 9 (barycentre position and proper motion J2019.5), Table 6 (ALMA relative positions), Table 10 (Proxima's orbit), sect. 3.4 (bound at 8.3 sigma), sect. 3.1.4 (L_sun 3.828e26 W), sect. 4 (Gaia standard processing unavailable for A and B).

anglada_2017_alma_arxiv1711.00578.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1711.00578 (arXiv:1711.00578v1): Guillem Anglada et al., "ALMA Discovery of Dust Belts Around Proxima Centauri" (2017), arXiv:1711.00578v1.
    Used: abstract (ALMA dust belt 1-4 au, ~40 K; possible outer belt ~30 au tilted ~45 deg; 1.2 arcsec source).

anglada_escude_2016_arxiv1609.03449.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1609.03449 (arXiv:1609.03449v1): Guillem Anglada-Escudé et al., "A terrestrial planet candidate in a temperate orbit around Proxima Centauri" (2016), arXiv:1609.03449v1; Nature 536 (2016) 437-440.
    Used: Table 1 (Proxima b discovery: P 11.186 d, K 1.38 m/s, m sin i 1.27, a 0.0485 au, e < 0.35, Teq 234 K, 65 % irradiance).

anugu_2026_proxdiam_arxiv2610.33373.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2610.33373 (arXiv:2610.33373v1): Narsireddy Anugu et al., "Proxima Centauri's Angular Diameter Revisited" (2026), arXiv:2610.33373v1.
    Used: abstract and Table 3 (preprint: theta_LD 1.154 +- 0.026 mas, R 0.162 +- 0.004, Teff 2833 +- 49; Gaia EDR3 distance 1.30193 pc).

artigau_2022_arxiv2207.13524.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2207.13524 (arXiv:2207.13524v1): Étienne Artigau et al., "Line-by-line velocity measurements, an outlier-resistant method for precision velocimetry" (2022), arXiv:2207.13524v1.
    Used: abstract and sect. 5.2 (LBL: Proxima b and d recovered, Proxima c argued against: a HARPS-TERRA systematic).

ayres_2014_arxiv1401.0847.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1401.0847 (arXiv:1401.0847v1): Thomas R. Ayres, "The Ups and Downs of Alpha Centauri" (2014), arXiv:1401.0847v1.
    Used: abstract (B: X-ray cycle 8.2 +- 0.2 yr; A: 19.1 +- 0.7 yr if a cycle).

bailer_jones_2015_arxiv1412.3648.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1412.3648 (arXiv:1412.3648v1): C. A. L. Bailer-Jones, "Close encounters of the stellar kind" (2014), arXiv:1412.3648v1; A&A 575, A35 (2015).
    Used: Table 3 and sect. 5 (Hipparcos: A at 0.90 pc in 27 kyr).

bailer_jones_2018_arxiv1805.07581.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1805.07581 (arXiv:1805.07581v2): C. A. L. Bailer-Jones et al., "New stellar encounters discovered in the second Gaia data release" (2018), arXiv:1805.07581v2; A&A 616, A37 (2018).
    Used: sect. 4 (A and B are not in Gaia DR2: too bright).

bailer_jones_2022_arxiv2207.06258.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2207.06258 (arXiv:2207.06258v2): C. A. L. Bailer-Jones, "Stars that approach within one parsec of the Sun: New and more accurate encounters identified in Gaia Data Release 3" (2022), arXiv:2207.06258v2.
    Used: Table 1 row 56 and sect. 4 (Gaia DR3: Proxima 0.958 pc in 27 kyr; time 26.622 kyr).

beichman_2020_jwstplan_arxiv1910.09709.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1910.09709 (arXiv:1910.09709v1): Charles Beichman et al., "Searching for Planets Orbiting Alpha Centauri A with the James Webb Space Telescope" (2019), arXiv:1910.09709v1.
    Used: saved for the JWST plan behind the 2024-2025 observations; no number used.

beichman_2025_paper1_arxiv2508.03814.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2508.03814 (arXiv:2508.03814v1): Charles Beichman et al., "Worlds Next Door: A Candidate Giant Planet Imaged in the Habitable Zone of $α$ Cen A. I. Observations, Orbital and Physical Properties, and Exozodi Upper Limits" (2025), arXiv:2508.03814v1; https://iopscience.iop.org/article/10.3847/2041-8213/adf53f.
    Used: abstract, Table 2 (C1 and S1 positions and fluxes), Table 4 (S1 + C1 orbit families), Table 5, sect. 7 (August 2026 recovery chance).

boyajian_2012_arxiv1208.2431.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1208.2431 (arXiv:1208.2431v2): Tabetha S. Boyajian et al., "Stellar Diameters and Temperatures II. Main Sequence K & M Stars" (2012), arXiv:1208.2431v2.
    Used: Table (GJ 551: R 0.1410 +- 0.0070, L 0.00155 +- 0.00002, Teff 3054 +- 79).

cuello_2024_review_arxiv2401.16003.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2401.16003 (arXiv:2401.16003v1): Nicolás Cuello & Mario Sucerquia, "Alpha Centauri: Disc Dynamics, Planet Stability, Detectability" (2024), arXiv:2401.16003v1.
    Used: saved as a review of discs and planet stability in alpha Cen; no number used.

damasso_2020_expect_arxiv2003.09305.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2003.09305 (arXiv:2003.09305v1): Mario Damasso & Fabio Del Sordo, "Expectations for the confirmation of Proxima c from a long-term radial velocity follow-up" (2020), arXiv:2003.09305v1.
    Used: saved for the Proxima c story (expected confirmation by RV follow-up); no number used.

demory_2009_arxiv0906.0602.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/0906.0602 (arXiv:0906.0602v1): B. -O. Demory et al., "Mass-radius relation of low and very low-mass stars revisited with the VLTI" (2009), arXiv:0906.0602v1.
    Used: Table (Proxima's AMBER diameter, radius 0.141 +- 0.007).

demory_2015_arxiv1503.07528.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1503.07528 (arXiv:1503.07528v1): Brice-Olivier Demory et al., "Hubble Space Telescope search for the transit of the Earth-mass exoplanet Alpha Centauri Bb" (2015), arXiv:1503.07528v1.
    Used: abstract and sect. 3 (Alpha Cen Bb: P 3.2357 d, K 0.51 m/s, transit ruled out at 96.6 %; a single transit-like event, period 2-sigma upper limit 20.4 d).

dewarf_2010_arxiv1009.1652.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1009.1652 (arXiv:1009.1652v1): L. E. DeWarf et al., "X-Ray, FUV, and UV Observations of alpha Centauri B: Determination of Long-term Magnetic Activity Cycle and Rotation Period" (2010), arXiv:1009.1652v1.
    Used: abstract and sect. 2 (B: cycle 8.84 +- 0.4 yr, rotation 36.2 +- 1.4 d; A about 15-20 d as quoted there).

faria_2022_arxiv2202.05188.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2202.05188 (arXiv:2202.05188v1): J. P. Faria et al., "A candidate short-period sub-Earth orbiting Proxima Centauri" (2022), arXiv:2202.05188v1.
    Used: abstract, Table 1 (stellar properties), Table C.1 (b and d: P, K, e, m sin i, a).

gilbert_2021_arxiv2110.10702.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2110.10702 (arXiv:2110.10702v2): Emily A. Gilbert et al., "No Transits of Proxima Centauri Planets in High-Cadence TESS Data" (2021), arXiv:2110.10702v2.
    Used: abstract (TESS: no transits; b cannot transit with R > 0.4 R_earth).

gratton_2020_arxiv2004.06685.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2004.06685 (arXiv:2004.06685v1): R. Gratton et al., "Searching for the near infrared counterpart of Proxima c using multi-epoch high contrast SPHERE data at VLT" (2020), arXiv:2004.06685v1; A&A 638, A120 (2020).
    Used: abstract (SPHERE candidate counterpart of Proxima c, S/N 6.1, not confirmed).

holman_1999_arxivastro-ph9809315.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/astro-ph/9809315 (arXiv:astro-ph/9809315v1): Matthew Holman & Paul Wiegert, "Long-Term Stability of Planets in Binary Systems" (1998), arXiv:astro-ph/9809315v1; Astron.J.117:621-628,1999.
    Used: eq. (1) and Table 4 (alpha Cen: critical a 2.79 au around A, 2.54 au around B, 87 au outside both).

howard_2018_arxiv1804.02001.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1804.02001 (arXiv:1804.02001v2): Ward S. Howard et al., "The First Naked-Eye Superflare Detected from Proxima Centauri" (2018), arXiv:1804.02001v2.
    Used: abstract (2016 March naked-eye superflare: x68 optical, 10^33.5 erg; at least five superflares a year predicted).

jenkins_2019_arxiv1905.01336.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1905.01336 (arXiv:1905.01336v1): James S. Jenkins et al., "Proxima Centauri b is not a transiting exoplanet" (2019), arXiv:1905.01336v1.
    Used: abstract (Spitzer: no transit of Proxima b, radius limit 0.4 R_earth if it did).

joyce_2018_arxiv1806.07567.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1806.07567 (arXiv:1806.07567v2): M. Joyce & B. Chaboyer, "Classically and Asteroseismically constrained 1D Stellar Evolution Models of $α$ Centauri A and B using Empirical Mixing Length Calibrations" (2018), arXiv:1806.07567v2.
    Used: abstract (age 5.3 +- 0.3 Gyr).

kervella_2016_conj_arxiv1610.06079.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1610.06079 (arXiv:1610.06079v1): P. Kervella et al., "Close stellar conjunctions of alpha Centauri A and B until 2050 - An mK = 7.8 star may enter the Einstein ring of alpha Cen A in 2028" (2016), arXiv:1610.06079v1; Astronomy & Astrophysics, 594, A107 (2016).
    Used: Table 1 (the K16 orbit and masses; its PB16 column quotes Omega 204.85, PB16's own table says 204.75).

kervella_2016_radii_arxiv1610.06185.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1610.06185 (arXiv:1610.06185v1): P. Kervella et al., "The radii and limb darkenings of Alpha Centauri A and B - Interferometric measurements with VLTI/PIONIER" (2016), arXiv:1610.06185v1; A&A 597, A137 (2017).
    Used: abstract and eqs. (11), (12), (16), (17), Table 5 (angular diameters, radii 1.2234 / 0.8632 R_sun, Teff 5795 / 5231 K, L 1.521 / 0.503); sect. 4 ([Fe/H] +0.24 / +0.22 from Jofre 2015, Heiter 2015).

kervella_2017_proxorbit_arxiv1611.03495.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1611.03495 (arXiv:1611.03495v3): P. Kervella et al., "Proxima's orbit around Alpha Centauri" (2016), arXiv:1611.03495v3; A&A 598, L7 (2017).
    Used: abstract and Table 3 (Proxima's orbit, period about 550 000 yr, near apastron), Table 1, Table 2 (positions, parallaxes, RVs), sect. 3.1 (separation 12 947 +- 260 au, 44.8 light-days nearer), eq. (2), intro (Innes 1915).

kervella_2019_pma_arxiv1811.08902.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1811.08902 (arXiv:1811.08902v2): Pierre Kervella et al., "Stellar and substellar companions of nearby stars from Gaia DR2 - Binarity from proper motion anomaly" (2018), arXiv:1811.08902v2; A&A 623, A72 (2019).
    Used: Table (Proxima: Gaia DR2 parallax 768.529, Hipparcos-Gaia proper motion -3781.629, +769.421).

kervella_2020_proxc_arxiv2003.13106.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2003.13106 (arXiv:2003.13106v1): Pierre Kervella et al., "Orbital inclination and mass of the exoplanet candidate Proxima c" (2020), arXiv:2003.13106v1.
    Used: Table 1 (Damasso et al. 2020's Proxima c values: K 1.2 +- 0.4 m/s, P 1900 d, conjunction, m sin i), Table 2 (i 152 +- 14 deg, mass 12 +12 -5 Earth masses), sect. 3.

kervella_2022_pma_arxiv2109.10912.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2109.10912 (arXiv:2109.10912v2): Pierre Kervella et al., "Stellar and substellar companions from Gaia EDR3 -- Proper motion anomaly and resolved common proper motion pairs" (2021), arXiv:2109.10912v2; A&A 657, A7 (2022).
    Used: saved for the Gaia EDR3 proper-motion anomalies; no number used.

klein_2021_arxiv2010.14311.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2010.14311 (arXiv:2010.14311v1): Baptiste Klein et al., "The large-scale magnetic field of Proxima Centauri near activity maximum" (2020), arXiv:2010.14311v1.
    Used: sect. 3 (Proxima's spin axis inclination 47 +- 7 deg, rotation 89.8 +- 4.0 d).

kopparapu_2013_arxiv1301.6674.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1301.6674 (arXiv:1301.6674v2): Ravi kumar Kopparapu et al., "Habitable Zones Around Main-Sequence Stars: New Estimates" (2013), arXiv:1301.6674v2.
    Used: saved with Kopparapu 2014 (data/hz), whose Table 1 coefficients are used.

libralato_2025_ares_arxiv2512.08533.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2512.08533 (arXiv:2512.08533v1): M. Libralato et al., "Astrometric Reconnaissance of Exoplanetary Systems (ARES). I. Methodology validation with HST point-source images of Proxima Centauri" (2025), arXiv:2512.08533v1.
    Used: abstract and Table 2 (Gaia DR3 astrometry of Proxima: parallax 768.067 +- 0.050 mas, proper motion; Proxima c 3.4 +5.2 -3.4 M_earth face-on).

macgregor_2018_arxiv1802.08257.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1802.08257 (arXiv:1802.08257v1): Meredith A. MacGregor et al., "Detection of a Millimeter Flare From Proxima Centauri" (2018), arXiv:1802.08257v1.
    Used: abstract (a flare explains the ALMA excess; no need for the 1-4 au belt).

macgregor_2021_arxiv2104.09519.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2104.09519 (arXiv:2104.09519v1): Meredith A. MacGregor et al., "Discovery of an Extremely Short Duration Flare from Proxima Centauri Using Millimeter through FUV Observations" (2021), arXiv:2104.09519v1.
    Used: abstract (2019 May 1 flare: >1000x in millimetre, >14000x in FUV).

mann_2015_arxiv1501.01635.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1501.01635 (arXiv:1501.01635v3): Andrew W. Mann et al., "How to Constrain Your M Dwarf: measuring effective temperature, bolometric luminosity, mass, and radius" (2015), arXiv:1501.01635v3.
    Used: the mass-luminosity relation that Kervella 2017 applied (0.1221 +- 0.0022); Proxima itself is not in the paper's tables.

mann_2019_arxiv1811.06938.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1811.06938 (arXiv:1811.06938v2): Andrew W. Mann et al., "How to Constrain Your M dwarf II: the mass-luminosity-metallicity relation from 0.075 to 0.70$M_\odot$" (2018), arXiv:1811.06938v2; 2019 ApJ 871 63.
    Used: saved for the newer mass-luminosity relation; no number used.

mesa_2017_arxiv1611.10362.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1611.10362 (arXiv:1611.10362v1): D. Mesa et al., "Upper limits for Mass and Radius of objects around Proxima Cen from SPHERE/VLT" (2016), arXiv:1611.10362v1.
    Used: saved for SPHERE limits around Proxima; no number used.

parkin_2018_starshot_arxiv1805.01306.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1805.01306 (arXiv:1805.01306v3): Kevin L. G. Parkin, "The Breakthrough Starshot System Model" (2018), arXiv:1805.01306v3; Acta Astronautica, Volume 152, 2018, Pages 370-384, ISSN 0094-5765.
    Used: abstract (Starshot system model: 0.2 c point design, 4.1 m sail, $8.0B beam director).

perryman_2012_history_arxiv1209.3563.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1209.3563 (arXiv:1209.3563v1): Michael Perryman, "The History of Astrometry" (2012), arXiv:1209.3563v1.
    Used: sect. on Henderson (parallax of Alpha Centauri published 1839, observed 1832-33 at the Cape).

pourbaix_2016_arxiv1601.01636.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1601.01636 (arXiv:1601.01636v1): Dimitri Pourbaix & Henri M. J. Boffin, "Parallax and masses of alpha Centauri revisited" (2016), arXiv:1601.01636v1; A&A 586, A90 (2016).
    Used: Table 1 (HARPS + ESO column): a, i, omega, Omega, e, P, T, V0, parallax 743 +- 1.3 mas, kappa, MA 1.133 +- 0.0050, MB 0.972 +- 0.0045; intro: spectral types G2 V, K1 V.

quarles_2016_arxiv1604.04917.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1604.04917 (arXiv:1604.04917v1): Billy Quarles & Jack J. Lissauer, "Long-Term Stability of Planets in the $α$ Centauri System" (2016), arXiv:1604.04917v1; 2016 The Astronomical Journal 151 111.
    Used: sect. 3.1-3.2 (largest stable circular prograde orbit about A ~2.56 au; B's line fit m = -0.367, b = 0.890).

quarles_2018_arxiv1801.06116.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1801.06116 (arXiv:1801.06116v1): Billy Quarles et al., "Long-Term Stability of Planets in the $α$ Centauri System, II: Forced Eccentricities" (2018), arXiv:1801.06116v1; B. Quarles, J. J. Lissauer, and N. Kaib 2018 AJ 155 64.
    Used: saved for forced eccentricities in alpha Cen; no number used.

rabus_2019_arxiv1901.08077.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1901.08077 (arXiv:1901.08077v1): Markus Rabus et al., "A discontinuity in the $T_{\rm eff}$-radius relation of M-dwarfs" (2019), arXiv:1901.08077v1.
    Used: Table (GJ 551: theta_LD 1.103 +- 0.007 mas, R 0.154 +- 0.001, Teff 2901 +- 68).

rajpaul_2016_arxiv1510.05598.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1510.05598 (arXiv:1510.05598v1): Vinesh Rajpaul et al., "Ghost in the time series: no planet for Alpha Cen B" (2015), arXiv:1510.05598v1.
    Used: abstract and sect. 1 (Bb from Dumusque et al. 2012: P 3.2357 +- 0.0008 d, m sin i 1.13 +- 0.09, K 0.51 +- 0.04 m/s; the signal is the window function).

ribas_2017_arxiv1704.08449.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1704.08449 (arXiv:1704.08449v1): Ignasi Ribas et al., "The full spectral radiative properties of Proxima Centauri" (2017), arXiv:1704.08449v1; A&A 603, A58 (2017).
    Used: sect. 4 (L_bol 0.00151 L_sun; the paper prints +- 0.0008, its erg/s value gives +- 0.00008).

sanghi_2025_paper2_arxiv2508.03812.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2508.03812 (arXiv:2508.03812v1): Aniket Sanghi et al., "Worlds Next Door: A Candidate Giant Planet Imaged in the Habitable Zone of $α$ Cen A. II. Binary Star Modeling, Planet and Exozodi Search, and Sensitivity Analysis" (2025), arXiv:2508.03812v1; https://iopscience.iop.org/article/10.3847/2041-8213/adf53e.
    Used: saved with paper I (binary modelling, sensitivity); no number used beyond paper I's.

suarez_mascareno_2020_arxiv2005.12114.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2005.12114 (arXiv:2005.12114v2): A. Suárez Mascareño et al., "Revisiting Proxima with ESPRESSO" (2020), arXiv:2005.12114v2; A&A 639, A77 (2020).
    Used: abstract and Table 3 (ESPRESSO + archive: P 11.18427 d, K 1.377 m/s, e 0.124, m sin i 1.173 +- 0.086; second signal 5.15-5.17 d).

suarez_mascareno_2025_nirps_arxiv2507.21751.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2507.21751 (arXiv:2507.21751v1): Alejandro Suárez Mascareño et al., "Diving into the planetary system of Proxima with NIRPS -- Breaking the metre per second barrier in the infrared" (2025), arXiv:2507.21751v1; 2025, A&A, 700, A11.
    Used: Table 2 (Proxima: Gaia DR3 parallax 768.067, L, Teff, M, R, rotation 83.2 d, cycle 6560 d, HZ), Table 3 (b and d: T0, P, m sin i, a, S, Teq, K), eq. (12) and sect. 4 (T0 = inferior conjunction), sect. 6.2.3 (Proxima c not confirmed), 6.2.6 (limits), Appendix H (no transits).

thevenin_2026_age_arxiv2602.08879.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2602.08879 (arXiv:2602.08879v1): F. Thévenin et al., "A mapping method of age estimation for binary stars: Application to the $α$ Centauri system A and B" (2026), arXiv:2602.08879v1.
    Used: abstract (age 7.8 +- 0.6 or 8.7 +- 0.6 Ga by the solar Z/X adopted; 7.2-7.8 Ga preferred), sect. 2 (uses Akeson 2021 masses).

tuthill_2026_toliman_arxiv2603.14683.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2603.14683 (arXiv:2603.14683v1): Peter Tuthill et al., "The TOLIMAN mission: A low-cost space telescope for high precision narrow-angle astrometry" (2026), arXiv:2603.14683v1.
    Used: abstract (TOLIMAN: 12.5 cm astrometric space telescope, 16U CubeSat, under construction).

vida_2019_arxiv1907.12580.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1907.12580 (arXiv:1907.12580v3): Krisztián Vida et al., "Flaring activity of Proxima Centauri from TESS observations: quasi-periodic oscillations during flare decay and inferences on the habitability of Proxima b" (2019), arXiv:1907.12580v3.
    Used: abstract (TESS: 72 flares in ~50 days, 1.49 a day; 10^33 erg three times a year).

wagner_2021_arxiv2102.05159.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2102.05159 (arXiv:2102.05159v2): K. Wagner et al., "Imaging low-mass planets within the habitable zone of α Centauri" (2021), arXiv:2102.05159v2.
    Used: main text (NEAR candidate C1: S/N ~3, ~1.1 au from A, R 3-11 R_earth, or exozodiacal dust; RV limit 53 M_earth).

wargelin_2017_arxiv1610.03447.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1610.03447 (arXiv:1610.03447v1): B. J. Wargelin et al., "Optical, UV, and X-Ray Evidence for a 7-Year Stellar Cycle in Proxima Centauri" (2016), arXiv:1610.03447v1.
    Used: abstract (83-day rotation, 7-year cycle).

wargelin_2024_arxiv2411.04252.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/2411.04252 (arXiv:2411.04252v1): B. J. Wargelin et al., "X-Ray, UV, and Optical Observations of Proxima Centauri's Stellar Cycle" (2024), arXiv:2411.04252v1.
    Used: abstract (cycle 8.0 yr in optical, UV and X-ray).

zhao_2018_arxiv1711.06320.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1711.06320 (arXiv:1711.06320v1): Lily L. Zhao et al., "Planet Detectability in the Alpha Centauri System" (2017), arXiv:1711.06320v1.
    Used: abstract (RV limits in the habitable zones: 53 M_earth A, 8.4 B, 0.47 Proxima).

zurlo_2018_arxiv1807.01318.pdf
    Saved as received on 22 Nov 2026 from https://arxiv.org/pdf/1807.01318 (arXiv:1807.01318v1): A. Zurlo et al., "The gravitational mass of Proxima Centauri measured with SPHERE from a microlensing event" (2018), arXiv:1807.01318v1.
    Used: abstract (lensing mass of Proxima 0.150 +0.062 -0.051 M_sun).
