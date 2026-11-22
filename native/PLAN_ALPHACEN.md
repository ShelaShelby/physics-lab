# Plan: the Alpha Centauri system in the Orbit Lab (0.72.1 research, 22 Nov 2026; no code yet)

Every number below is copied from a paper saved in the private sources repo under data/alphacen/ (native/data/alphacen/README.txt lists them; native/data/SOURCES.md has the sha256), with its table, equation or section. A value marked **(computed here)** is this job's own arithmetic from such numbers, done by native/tools/alphacen_numbers.py (run it to see every one; the build job recomputes them in tests).
Short names: **PB16** Pourbaix & Boffin 2016; **K16** Kervella et al. 2016 (conjunctions, A&A 594, A107); **K17a** Kervella, Bigot, Gallenne & Thévenin 2017 (radii, A&A 597, A137); **K17** Kervella, Thévenin & Lovis 2017 (Proxima's orbit, A&A 598, L7); **A21** Akeson et al. 2021 (ALMA, AJ 162, 14); **AE16** Anglada-Escudé et al. 2016; **SM20** Suárez Mascareño et al. 2020 (ESPRESSO); **F22** Faria et al. 2022; **N25** Suárez Mascareño et al. 2025 (NIRPS, arXiv:2507.21751); **D20** Damasso et al. 2020 (Sci. Adv. 6, eaax7467; not saved: refused, its numbers as K20 Table 1 quotes them); **K20** Kervella, Arenou & Schneider 2020; **G20** Gratton et al. 2020; **Ar22** Artigau et al. 2022; **L25** Libralato et al. 2025 (ARES I); **W21** Wagner et al. 2021 (NEAR); **B25** Beichman, Sanghi et al. 2025 (JWST paper I, arXiv:2508.03814); **S25** Sanghi, Beichman et al. 2025 (paper II); **Z18** Zurlo et al. 2018; **M15** Mann et al. 2015; **R19** Rabus et al. 2019; **An26** Anugu et al. 2026 (preprint, arXiv:2610.33373); **R17** Ribas et al. 2017; **B12** Boyajian et al. 2012; **De09** Demory et al. 2009; **Kl21** Klein et al. 2021; **JC18** Joyce & Chaboyer 2018; **Th26** Thévenin et al. 2026; **Ay14** Ayres 2014; **DW10** DeWarf et al. 2010; **W17 / W24** Wargelin et al. 2017 / 2024; **H18** Howard et al. 2018; **V19** Vida et al. 2019; **M18 / M21** MacGregor et al. 2018 / 2021; **A17** Anglada et al. 2017; **J19** Jenkins et al. 2019; **Gi21** Gilbert et al. 2021; **De15** Demory et al. 2015; **Ra16** Rajpaul et al. 2016; **HW99** Holman & Wiegert 1999; **QL16** Quarles & Lissauer 2016; **Zh18** Zhao et al. 2018; **K14** Kopparapu et al. 2014 (data/hz); **BJ15 / BJ18 / BJ22** Bailer-Jones 2015 / 2018 / 2022; **P12** Perryman 2012; **Wi18** Willmer 2018 (data/photometry); **To26** Tuthill et al. 2026 (TOLIMAN); **Pa18** Parkin 2018 (Starshot); **HYG** the HYG Database v4.1 (data/stars, data/alphacen).
Units: au, M_sun, R_sun, L_sun, Earth masses M_E, years (yr = Julian year), kau = 1000 au, kyr = 1000 years, mas = milliarcsecond; BJD = barycentric Julian date. Errors are the papers' 1-sigma unless said.

## 0. In plain words (for the owner)
1. **What is known for sure.** Alpha Centauri is three stars. A (a little bigger and brighter than the Sun) and B (a little smaller and cooler) circle each other every 80 years on a stretched orbit: at their closest they are 11 au apart (Sun to Saturn), at their farthest 35 au (Sun to Pluto). Their orbit, masses and sizes are measured to about 0.1-0.5 % per paper. The third star, Proxima, a small red dwarf, is the nearest star to the Sun (4.25 light years); it circles A and B from about 12,000 au away, one turn in roughly half a million years, and it is proven to be bound to them.
2. **One planet is certain**: Proxima b, at least about 1.05 Earth masses, one orbit every 11.2 days, 0.05 au from its star, in the zone where the light it gets (about 64 % of what Earth gets) could allow liquid water. Three teams with three instruments found it. A second one, **Proxima d** (at least about a quarter of an Earth, 5.1 days), was seen by two instruments; the 2025 NIRPS paper calls it confirmed.
3. **What is disputed or unconfirmed.** Proxima c (a planet of about 6 Earth masses or more at 1.5 au, 5 years per orbit) was proposed in 2020; later analyses could not confirm it and one says it is an instrument effect: **disputed**. Around A, a possible giant planet was seen once by JWST in August 2024 (and maybe by a ground telescope in 2019), not seen again in 2025: **unconfirmed candidate**. A planet around B (2012) was shown to be a false signal: **refuted**. A dust belt around Proxima (2017) was explained by a flare: **refuted**.
4. **What nobody knows.** How the planets' orbits are tilted, so their true masses (only a minimum is measured); their sizes (they do not pass in front of the star as seen from Earth); whether they have air, water or what their surfaces are like; whether the candidates are real. The stars' age is also disputed (about 5 or about 8 billion years, depending on the method).
5. **What the user would see** (proposal): a switch "Solar System / Alpha Centauri". The view shows A and B circling each other at the true speed of time (an 80-year orbit, exact), zooming out to Proxima 12,000 au away on its half-million-year orbit, zooming in on Proxima to its planets b and d on their orbits (tilt unknown: drawn with a labelled assumed tilt, masses shown as "at least"); c and the A candidate drawn only as dashed, labelled "disputed" / "unconfirmed" (if the owner agrees); habitable zones in green; our Sun as a bright star in their sky (in Cassiopeia, about as bright as the brightest stars in ours).

## 1. The system: distance, position, motion
| quantity | value | source |
|---|---|---|
| A-B parallax (orbital) | 750.81 ± 0.38 mas → 1.3319 ± 0.0007 pc = **4.3441 ly** (computed here, 1 pc = 3.26156 ly) | A21 Table 8, sect. 3.2 |
| same, earlier | 743 ± 1.3 mas (4.3897 ly, computed here) | PB16 Table 1 |
| same, earlier | 747.17 ± 0.61 mas (4.3652 ly) | K16 Table 1 |
| A-B trigonometric (Hipparcos + ALMA) | 747.1 ± 5.2 mas, "statistically consistent" with the orbital one | A21 sect. 3.1.4 |
| Hipparcos 1997 / 2007 | 742 ± 1.42 / 754.81 ± 4.11 mas (as PB16 quote them; PB16 "rules out" the 2007 value) | PB16 sect. 4 |
| A-B barycentre, ICRS J2019.5 | RA 219.85892215°, Dec −60.83163195° (14:39:26.1413, −60:49:53.875) | A21 Table 9 |
| barycentre proper motion | µα* −3639.95 ± 0.42, µδ +700.40 ± 0.17 mas/yr | A21 Table 9 |
| barycentre radial velocity | V0 −22.3796 ± 0.0020 km/s (RV acceleration +0.421 m/s/yr) | A21 Table 8 |
| Proxima parallax (Gaia DR3) | **768.067 ± 0.050 mas** (1.3020 pc, 4.2465 ly) | L25 Table 2 "Gaia DR3 value"; N25 Table 2 |
| Proxima, other parallaxes | 768.373 +0.194/−0.162 (HST, L25 Table 2); 768.529 ± 0.220 (Gaia DR2 + 29 µas, K20 Table 1); 768.77 ± 0.37 (Benedict 1999, K17 Table 2); 768.500 ± 0.203 (R19 Table 1) | as named |
| Proxima position (Gaia DR3, epoch 2016.0) | RA 217.392321472°, Dec −62.676075117° | L25 Table 2 |
| Proxima proper motion (Gaia DR3) | µα* −3781.741, µδ +769.465 mas/yr | L25 Table 2 |
| Proxima radial velocity (absolute) | −22.204 ± 0.032 km/s (corrected for gravitational redshift +504 m/s) | K17 sect. 2.3 |
| Proxima nearer than A-B | 44.8 ± 1.5 light-days (K17 sect. 3.1); with today's parallaxes 6,368 au along the line of sight (computed here) | K17; computed |
| closest approach to the Sun | **Proxima: 0.958 pc (5-95 %: 0.951-0.965) in 27 kyr** (26.622 kyr, 90 % CI 26.580-26.653), Gaia DR3 | BJ22 Table 1 row 56, sect. 4 |
| same, A (Hipparcos) | 0.90 pc in 27.71 kyr (catalogue hip2gcs; 0.91 pc, 27.74 kyr pulkovo; 1.00 pc, 27.62 kyr xhip) | BJ15 Table 3, sect. 5 |

**What Gaia gives and does not give.** A and B are too bright for Gaia: "not in Gaia DR2 on account of its brightness" (BJ18 sect. 4); "Due to the brightness and binarity of α Cen AB, Gaia standard processing is unavailable and custom data reductions may be limited in absolute accuracy" (A21 sect. 3.1). Gaia measures Proxima, and field stars near A-B. So the A-B distance comes only from the orbit (radial velocities + visual orbit: PB16, K16, A21) or from Hipparcos.
**Disagreement and recommendation.** The three orbital parallaxes differ by 7.8 mas (PB16 to A21), six times PB16's error bar: the error bars do not cover the differences (the choice of RV data set and the new ALMA astrometry, A21 sect. 3.1.4). **Use A21 (750.81 mas, 4.344 ly)**: the newest, the only one with ALMA's 0.3-0.6 mas relative positions, and its orbit reproduces the ALMA positions within 1.3 mas while PB16's misses them by 89-93 mas and K16's by 28 mas (computed here, sect. 2). The app now uses PB16's 4.390 ly (sky_data.inc): 1.1 % too far by A21.

## 2. The A-B orbit
Elements of B about A (ω is B's argument of periastron, PB16 eq. 1); all three in A21 Table 8, PB16's own in PB16 Table 1, K16's in K16 Table 1:

| element | A21 (epoch 2019.5) | K16 | PB16 (own table) |
|---|---|---|---|
| P (yr) | 79.762 ± 0.019 | 79.929 ± 0.013 | 79.91 ± 0.013 |
| a (arcsec) | 17.4930 ± 0.0096 | 17.592 ± 0.013 | 17.66 ± 0.026 |
| e | 0.51947 ± 0.00015 | 0.5208 ± 0.0011 | 0.524 ± 0.0011 |
| i (deg) | 79.2430 ± 0.0089 | 79.320 ± 0.011 | 79.32 ± 0.044 |
| Ω (deg) | 205.073 ± 0.025 | 205.064 ± 0.033 | 204.75 ± 0.087 (K16 and A21 quote 204.85 ± 0.084, PB16's 2002 column) |
| ω (deg) | 231.519 ± 0.027 | 232.006 ± 0.051 | 232.3 ± 0.11 |
| T0 (periastron, yr) | 1955.564 ± 0.015 | 1955.604 ± 0.013 | 1955.66 ± 0.014 |
| parallax (mas) | 750.81 ± 0.38 | 747.17 ± 0.61 | 743 ± 1.3 |
| mass fraction mA/(mA+mB) | 0.54266 ± 0.00011 | 0.54116 ± 0.00027 | 0.5383 ± 0.00044 (PB16: κ = mB/(mA+mB) = 0.4617) |
| mA (M_sun) | **1.0788 ± 0.0029** | 1.1055 ± 0.0039 | 1.133 ± 0.0050 |
| mB (M_sun) | **0.9092 ± 0.0025** | 0.9373 ± 0.0033 | 0.972 ± 0.0045 |

(computed here) a in au = a″/parallax: A21 23.299 au (periastron 11.20, apastron 35.40 au); K16 23.545; PB16 23.769. **Kepler's third law** M = a³/P² (au, Julian yr; GM_sun JPL DE440): A21 1.9880 ± 0.0046 (published mA+mB 1.9880), K16 2.0431 ± 0.0068 (2.0428), PB16 2.1029 ± 0.0144 (2.1050): each paper's masses follow from its own a, P, parallax; the errors (added in quadrature, correlations ignored) are the formula's, not the papers'. The masses differ by 5 % between PB16 and A21, far beyond the quoted errors (A21's mA is 11 PB16-sigmas below PB16's): the systematics (which RV data, how the zero points are modelled, the parallax) dominate.
(computed here) Thiele-Innes positions against ALMA (A21 Table 6, three epochs 2018.78-2019.65): A21's elements: 0.0-1.3 mas off; K16's: 28 mas across, 1 mas in separation; PB16's: 89-93 mas across. Next periastron (A21): 2035.33; apastron 1995.4. On 2026.9 B is at PA 14.3°, 9.72″ from A (12.95 au projected).
**Recommendation (also REPORT.md, doubtful values):** replace PB16's 1.13 / 0.97 M_sun (far_masses.h; PB16's own errors are ± 0.0050 / ± 0.0045, not "unread") by **A21: 1.0788 ± 0.0029 and 0.9092 ± 0.0025**, and say on the card that the published orbits disagree by about 5 % (PB16 1.133 / 0.972; K16 1.1055 / 0.9373), more than their error bars. Asteroseismology alone gave 1.10 ± 0.03 and 0.97 ± 0.04 (Lundkvist et al. 2014 as PB16 sect. 5 quotes it; not saved).

## 3. The stars
| | A (Rigil Kentaurus) | B (Toliman) | Proxima |
|---|---|---|---|
| spectral type | G2 V | K1 V (PB16 sect. 1, K17 sect. 1) | M5.5 V (Bessell 1991 as N25 Table 2, F22 Table 1 quote it; HYG and the app say M5Ve) |
| mass (M_sun) | 1.0788 ± 0.0029 (A21) | 0.9092 ± 0.0025 (A21) | 0.1221 ± 0.0022 (M15's relation, K17 Table 1; N25, F22 use it); lensing **0.150 +0.062 −0.051** (Z18, the only direct one); 0.124 ± 0.003 (R19 Table 2, from M_Ks by a mass relation) |
| radius (R_sun) | 1.2175 ± 0.0055 (A21, with A21's parallax); 1.2234 ± 0.0053 (K17a) | 0.8591 ± 0.0036 (A21); 0.8632 ± 0.0037 (K17a) | 0.141 ± 0.007 (De09, B12); 0.1542 ± 0.0045 (M15 relation, K17 Table 1); **0.154 ± 0.001 (R19 Table 1, interferometry)**; 0.162 ± 0.004 (An26 preprint, re-reduced interferometry) |
| angular diameter (limb darkened) | 8.502 ± 0.038 mas | 5.999 ± 0.025 mas (K17a abstract) | 1.103 ± 0.007 mas (R19); 1.154 ± 0.026 mas (An26) |
| Teff (K) | 5795 ± 19 (K17a eq. 16) | 5231 ± 21 (K17a eq. 17) | 2900 ± 100 (Pavlenko 2017 as N25 quotes); 3054 ± 79 (B12); 3098 ± 56 (De09); 2901 ± 68 (R19); 2833 ± 49 (An26) |
| luminosity (L_sun) | 1.5059 ± 0.0019 (A21); 1.521 ± 0.015 (K17a Table 5) | 0.4981 ± 0.0007 (A21); 0.503 ± 0.007 (K17a) | 0.00151 (R17 sect. 4: 5.80 ± 0.30 × 10³⁰ erg/s; the paper prints "± 0.0008", its erg/s error gives ± 0.00008, as N25 quotes); 0.00155 ± 0.00002 (B12) |
| metallicity [Fe/H] | +0.24 | +0.22 (Jofré 2015 / Heiter 2015 as K17a sect. 4 quote them) | +0.21 (An26 Table 3, model input); "super-solar" for A and B (Zh18 abstract) |
| age | **disputed**: 5.3 ± 0.3 Gyr (JC18, seismic + classical, masses ~3 % above A21's); 7.8 ± 0.6 or 8.7 ± 0.6 Ga by the solar Z/X adopted, 7.2-7.8 Ga preferred (Th26, A21 masses) | same system | coeval with A and B because bound (K17 sect. 4) |
| rotation | about 15-20 d (DW10 sect. 2, quoting other work; not measured there) | 36.2 ± 1.4 d (DW10) | 83.2 ± 1.6 d (N25 Table 2); 83 d (W17); 89.8 ± 4.0 d (Kl21); 87 ± 12 d (SM20) |
| activity cycle | 19.1 ± 0.7 yr "if interpreted as an over-long cycle"; a Maunder-minimum-like low since 2005 (Ay14) | X-ray 8.2 ± 0.2 yr (Ay14); 8.84 ± 0.4 yr (DW10) | about 7 yr (W17); 8.0 yr (W24); 6560 ± 85 d (18.0 yr, computed here) in N25's photometry fit (N25 Table 2): **disputed** |
| spin-axis tilt to us | – | – | 47 ± 7° (Kl21) |
| V magnitude (from Earth) | −0.01 | 1.35 | 11.01 (HYG); 11.13 ± 0.01 (Jao 2014 as N25 Table 2 quotes) |
| B−V (the app's colour rule, sky.cpp farBV) | 0.71 (HYG; the app uses it for the pair) | 0.90 (HYG) | 1.807 (HYG) |

**Flares (Proxima).** 2016 March: a naked-eye superflare, optical flux ×68, bolometric 10^33.5 erg, ten times any earlier one; 23 more flares of 10^30.6-10^32.4 erg in two years; "at least five superflares occur each year" (H18 abstract). TESS: 72 flares in about 50 days, 1.49 a day, 7.2 % of the time flaring; 10^33 erg superflares expected three times a year, 10^34 erg every second year (V19 abstract). 2019 May 1: the brightest flare seen in millimetre (×1000) and FUV (×14,000), initial burst under 10 s (M21 abstract).
**Proxima's mass, recommendation.** Keep **two numbers, both shown**: use 0.1221 ± 0.0022 for the physics (the planets' minimum masses and orbit sizes in K17, F22 and N25 are all computed with it; a different mass would make the published planet numbers inconsistent) and show it as "from the measured brightness, by a relation calibrated on binary stars (M15)"; show Z18's 0.150 +0.062 −0.051 as the direct (lensing) measurement, which agrees within its error. Nothing newer was found (searches of arXiv to Nov 2026): Z18's conclusions say "no star as bright as Source 2 is expected to pass within 0.5″ of Proxima Cen in the next 20 years"; the planets' radial velocities measure only m sin i relative to the star's mass, so they cannot weigh the star. The present far_masses.h comment "Mann et al. 2015 give 0.12 ± 0.02" is not in M15 (Proxima is not in its tables): the source of the 0.1221 is K17 Table 1.
**Proxima's radius, recommendation.** R19 0.154 ± 0.001 (refereed); mention An26's 0.162 ± 0.004 as a 2026 preprint that re-reduced the same kind of data; De09's 0.141 (used in N25, F22, AE16 tables) is older and smaller.

## 4. Proxima's orbit around A-B
| | A21 Table 10 (newest) | K17 Table 3 |
|---|---|---|
| a | 8.2 +0.4 −0.3 kau | 8.7 +0.7 −0.4 kau |
| e | 0.497 +0.057 −0.060 | 0.50 +0.08 −0.09 |
| P | 511 +41 −30 kyr | 547 +66 −40 kyr |
| i | 124.9 +2.9 −3.2° | 107.6 +1.8 −2.0° |
| Ω | 165 ± 3° | 126 ± 5° |
| ω | 151.0 +5.7 −4.9° | 72.3 +8.7 −6.6° |
| periastron passage | +278 +36 −28 kyr from now | +283 +59 −41 kyr |
| periastron / apastron | 4.1 +0.7 −0.6 / 12.3 +0.2 −0.1 kau | 4.3 +1.1 −0.9 / 13.0 +0.3 −0.1 kau |
| bound? | differential velocity 280 ± 32 m/s, limit 554 ± 7 m/s: bound at 8.3 σ (A21 sect. 3.4) | 273 ± 49 m/s vs 545 ± 11 m/s: 5.4 σ (K17 sect. 3.2) |

The orientation angles changed a lot between the two (i by 17°, Ω by 39°, ω by 79°): only a, e, P are reasonably firm. **Use A21.** Where Proxima is now (computed here): by A21's elements 12.21 kau from A-B, true anomaly 174°, apastron in about 22 kyr; from today's measured positions and parallaxes (A21 barycentre, Gaia DR3 Proxima) 12,052 au, 2.184° apart on our sky, Proxima 6,368 au nearer to us (A-B's parallax error alone moves this by about 73 au). K17's 12,947 ± 260 au used the older parallaxes. **For drawing: put Proxima at its measured place today (positions and parallaxes) and move it on the A21 orbit**; the orbit's angles are uncertain by tens of degrees, which the card says.

## 5. Every planet ever claimed
| name | status | key numbers | for | against |
|---|---|---|---|---|
| Proxima b | **CONFIRMED** | P 11.18465 ± 0.00053 d; K 1.226 ± 0.062 m/s; m sin i 1.055 ± 0.055 M_E; a 0.04848 ± 0.00029 au; e fixed 0 (< 0.1 when free); T0 (inferior conjunction) BJD 2,460,548.59 ± 0.12; S 0.641 ± 0.065 S_E; Teq (A = 0.3) 218 ± 18 K (N25 Table 3, sect. 6.2.1) | AE16 (HARPS + UVES: P 11.186, K 1.38, m sin i 1.27, e < 0.35, Teq 234 K, irradiance 65 %, Table 1); SM20 (ESPRESSO: 11.18427 d, 1.173 ± 0.086 M_E, e 0.124 +0.070 −0.068, Table 3); F22 (1.07 ± 0.06 M_E, Table C.1); N25 (NIRPS, near infrared) | – |
| Proxima d | **CONFIRMED by N25** (F22 called it a candidate) | P 5.12338 ± 0.00035 d; K 0.392 ± 0.057 m/s; m sin i 0.260 ± 0.038 M_E; a 0.02881 ± 0.00017 au; e fixed 0 (< 0.25); T0 BJD 2,460,557.55 ± 0.16; S 1.814 ± 0.098 S_E; Teq 282 ± 23 K (N25 Table 3) | SM20 (5.15 d signal, 0.29 ± 0.08 M_E); F22 (5.122 d, 0.26 ± 0.05 M_E); Ar22 (2-σ in HARPS LBL); N25 (in HARPS + NIRPS, ~7 σ in all data) | – |
| Proxima c | **DISPUTED** | P 1900 +96 −82 d; K 1.2 ± 0.4 m/s; m sin i 5.8 ± 1.9 M_E (D20, as N25 sect. 3.3 and K20 Table 1 quote it; K20: 5.7 ± 1.9); a 1.489 ± 0.049 au (K20 Table 2); inferior conjunction BJD 2,455,892 +101 −102 (K20 Table 1) | D20 (HARPS + UVES); K20 (Gaia DR2 proper-motion anomaly: i 152 ± 14° or 28 ± 14°, true mass 12 +12 −5 M_E, Table 2); G20 (a SPHERE candidate, S/N 6.1, "we cannot confirm"); Benedict & McArthur 2020 (HST astrometry, 18 ± 5 M_E: not saved) | Ar22 ("argues against", a HARPS-TERRA systematic); N25 (no significant signal; "hints" of a lower-amplitude signal near 2000 d, sect. 6.2.3, 6.2.6); L25 (HST + Gaia: 3.4 +5.2 −3.4 M_E face-on, "limited by our current uncertainties") |
| α Cen A "S1 + C1" | **UNCONFIRMED CANDIDATE** | C1 (VLT/NEAR, 1 June 2019): ρ 0.85 ± 0.05″, PA 228.9 ± 3.3°, 11.25 µm, 1.2 ± 0.4 mJy, S/N 3; S1 (JWST/MIRI, 10 Aug 2024): ρ 1.51 ± 0.13″, PA 83.5 ± 4.9°, 15.5 µm, 3.5 ± 1.0 mJy, S/N 4-6 (B25 Table 2). If the same object: orbits a 1.58-2.23 au, e 0.27-0.46, mutual inclination ~50° or ~130° to A-B (B25 Table 4); T ≈ 225 K, R ≈ 1-1.1 R_Jup, 90-150 M_E (B25 abstract) | W21 (C1: a planet of R 3-11 R_E at ~1.1 au, or exozodiacal dust); B25 (S1 is neither background nor foreground) | not seen in JWST Feb and Apr 2025; 52 % of its orbits explain that (B25 abstract); "With only a single JWST/MIRI sighting (and non-detections at two other epochs), it is challenging" to establish it (B25, discussion of Table 4). B25 named August 2026 as the next good chance; **no result of it was found** (22 Nov 2026) |
| α Cen Bb | **REFUTED** | P 3.2357 ± 0.0008 d, K 0.51 ± 0.04 m/s, m sin i 1.13 ± 0.09 M_E (Dumusque et al. 2012 as Ra16 sect. 1 quotes them) | Dumusque 2012 (not saved) | Ra16 (the signal is a "ghost" of the time sampling); De15 (no transit at 96.6 %); PB16 calls it "a false detection" |
| α Cen "Bc" | **UNCONFIRMED (single event)** | one transit-like dip, HST July 2013, centre BJD 2,456,482.195, depth about 90 ppm (91.3 +20 −18), 0.14-0.15 d long (De15 Table 1); if a planet: period 2-σ upper limit 20.4 d (median 12.4), about Earth size (De15 sect. 3-5; the name "Bc" is not De15's) | De15 | not seen again; never confirmed |
| Proxima dust belts | **REFUTED / UNCONFIRMED** | belt at 1-4 au, ~40 K; hints of a ~30 au cold belt tilted ~45° and a 1.2″ source (A17 abstract) | A17 | M18: a flare (×1000, about 1 minute, 2017 March 24) explains the excess; "no need to invoke the presence of a dust belt at 1-4 AU" |

Inclination and true masses: Proxima b does **not transit**: Spitzer rules out transits at 200 ppm (J19), TESS: b cannot transit with R > 0.4 R_E (Gi21; N25 Appendix H, four sectors). So i is not near 90° (by an amount set by the radius, not given as an angle in these papers). If the planets share the star's equator (an assumption): i = 47 ± 7° (Kl21), true masses 1.44 ± 0.21 (b) and 0.357 ± 0.072 M_E (d) (N25 Table 3). If b shares c's plane (K20): 2.1 +1.9 −0.6 M_E. (computed here) m sin i / sin i: b 1.05 (90°), 3.08 (20°), 6.08 M_E (10°).
(computed here, checks of the published numbers) With M = 0.1221: b a = 0.04856 au (N25 0.04848, F22 0.04856), K from m sin i 1.226 m/s (= N25); d 0.02885 au, 0.392 m/s; S = L/a² 0.640 and 1.814 (= N25). N25's Teq 218 / 282 K follow from Teff and R (Teff √(R/2a) (1−A)^¼ = 218 / 283 K); from R17's luminosity the same formula gives 228 / 295 K: the published Teff, R and L do not agree with each other (σT⁴4πR² with 2900 K and 0.141 R_sun is 0.00127 L_sun, not 0.00151). Seen from Earth the planets are at most 37.3 mas (b) and 22.2 mas (d) from Proxima.
**Where each planet is on a date:** N25's model is y(t) = −K sin(2π(t − T0)/P) with T0 the inferior conjunction (planet between Proxima and us) (N25 eq. 12, sect. 4); BJD (TDB). Phase = frac((BJD − T0)/P): e.g. 1 Dec 2025 0h TT, b 0.583, d 0.847 after conjunction (computed here). N25 gives the ephemeris to 2.88 h (b) and 3.8 h (d).

## 6. Where planets can still be, and what is ruled out
- **Stable orbits** (computed here, HW99 eq. 1 with A21's masses, e and a): around A up to 2.74 au, around B up to 2.49 au (HW99 Table 4, with 1999's numbers: 2.79 and 2.54 au; outside both stars beyond 87 au). QL16 (100 Myr runs): circular prograde orbits about A stable to ~2.56 au, B's fit gives ~2.43 au (computed here from QL16's m = −0.367, b = 0.890); retrograde orbits are stable farther out (QL16 abstract).
- **Habitable zones** (computed here, K14 eq. 4-5, 1 M_E; inner edge runaway greenhouse, outer maximum greenhouse; in brackets the optimistic recent Venus / early Mars): A (L 1.5059, Teff 5795) **1.165-2.054 au** (0.920-2.167); B (0.4981, 5231) **0.692-1.242 au** (0.546-1.310); Proxima (0.00151, 2900) **0.0405-0.0810 au** (0.0320-0.0854). N25 Table 2 gives Proxima 0.03731-0.088 au "following Kopparapu et al. (2014, 2017)": its inner edge is from the 2017 paper (not saved) and its outer edge is reproduced with L = 0.0016 (0.0879 au), not its listed 0.00151. Proxima b (0.048 au) and d (0.029 au): b inside, d inside the recent-Venus limit (too hot by this model).
- **RV limits**: in the habitable zones m sin i ≥ 53 M_E around A, 8.4 around B, 0.47 around Proxima would have been seen (Zh18 abstract). N25 sect. 6.2.6 (Proxima): nothing above 20 cm/s inside 10 d (0.08-0.15 M_E), 20-30 cm/s in the habitable zone (0.1-0.3 M_E), 0.3-0.6 M_E at 30-100 d, 0.5-1.0 M_E at 100-1000 d, ~4 M_E beyond 1000 d.
- **Imaging limits**: JWST/MIRI around A reached a sensitivity to detect Teff ≈ 225-250 K planets of 1-1.2 R_Jup between 1″ and 2″, and exozodiacal dust above 5-8 times our zodiacal cloud; no dust was seen (B25 abstract). NEAR: warm sub-Neptunes over much of A's habitable zone (W21).

## 7. The sky seen from there (all computed here)
- **Our Sun from A-B**: V = 4.81 + 5 log10(1.33189 pc / 10 pc) = **0.43** (M_V 4.81, Wi18 Table 3; A21 distance); from Proxima **0.38** (Gaia DR3 distance). About as bright as the brightest stars of our sky (no popular number copied).
- **Its direction**: the point opposite A-B in our sky: RA 2h39m26.1s, Dec +60°49′54″ (the antipode of A21 Table 9; ecliptic longitude 59.45°, latitude +42.60°). All 67 HYG stars within 2° of it are in **Cassiopeia** (data/alphacen/hyg_v41_rows_near_sun_from_alphacen.csv); the IAU boundary table could not be reached.
- **From Proxima today** (12,052 au): A V **−6.80**, B −5.44, together −7.07; at Proxima's apastron (12.3 kau) A −6.75, at periastron (4.1 kau) A −9.14 (from the V magnitudes and A21's parallax).
- **Proxima from A or B today**: V **4.39** (11.13 and the Gaia parallax): a faint naked-eye star.
- The A-B pair seen from Proxima b is a double star at most 35.4 au / 12,052 au = 0.17° apart (computed here): one point to the eye.

## 8. Names and history
- IAU names: **Rigil Kentaurus** (A), **Toliman** (B), **Proxima Centauri**: the names in HYG v4.1 (data/stars/hyg_v41_far_stars_rows.csv, "proper"). Their WGSN approval dates were not reachable (iau.org and the WGSN bulletins were refused): ROADMAP "Waiting for the owner". The app's "Alpha Centauri" key keeps the Bayer name.
- First parallax: Thomas Henderson "published a parallax for the nearby star Alpha Centauri in 1839, derived from observations made even earlier in 1832-33 at the Cape of Good Hope", after Bessel's 61 Cygni (P12 p. on Henderson). Henderson's own value is not in a saved source: not shown.
- Proxima: discovered by Innes (1915) (K17 sect. 1; N25 sect. 1, both citing Innes 1915); its link to α Cen noticed "immediately" (K17 sect. 1).

## 9. Plans and missions (only what the saved papers say)
- **Breakthrough Starshot**: light-driven nanocrafts; a 0.2 c point design to Alpha Centauri with a 4.1 m sail, $8.0B for the ground beam director (Pa18 abstract).
- **TOLIMAN**: a 12.5 cm space telescope on a 16U CubeSat, narrow-angle astrometry of A-B to find temperate rocky planets around either star, "under construction" (To26 abstract).
- **JWST**: the next chance to recover S1 was August 2026 (B25); NIRPS / ESPRESSO keep monitoring Proxima (N25); future instruments named in N25 sect. 6.2.5: RISTRETTO, ANDES, LIFE.

## 10. Open questions (what the app shows as "unknown")
1. The true masses of Proxima b and d (only minimum masses; the tilt of their orbits is not measured).
2. Their sizes, densities, atmospheres, surfaces, temperatures (they do not transit; the equilibrium temperatures are model numbers for an assumed albedo).
3. Whether Proxima c exists (disputed); whether the A candidate S1/C1 exists and what it is (planet or dust).
4. Whether A or B has any planet at all (none confirmed; small planets in the habitable zones are not yet excluded).
5. The orientation of Proxima's orbit (i, Ω, ω changed by tens of degrees between K17 and A21) and where exactly on it Proxima is (a few hundred au).
6. The age of the system (5.3 vs 7-9 Gyr) and the length of Proxima's activity cycle (7-8 vs 18 yr).
7. A-B's distance and masses beyond about 1 %: the published orbital solutions disagree by more than their errors.

## 11. Drawing it in the Orbit Lab (a proposal; no code in this job)
Files read for this: far_masses.h (kFarMass "acen" A/B, "prox"), sky_data.inc (kFar "acen" 4.390 ly, "prox" 4.2465 ly, ecliptic directions), sky.cpp (farBV), solar_data.h (PlanetData: JPL mean elements for the Sun's planets; MoonData), kepler.h (keplerE, keplerState with GM = kGMSun fixed), nbody.h (the Sun's N-body states), belts.cpp (the Oort card quotes Proxima's distance), orbit_actions.cpp (the Far away tile "acen"), journey_dates.cpp, shell_orbit.cpp.
- **The switch.** A system choice "Solar System / Alpha Centauri" (Orbit Lab, top of the panel). Alpha Centauri is its own scene: the Sun's planets, belts, comets and N-body states are not drawn; time, zoom, Follow and the cards work the same.
- **Frame.** The sky frame seen from Earth (x East, y North, z towards the Sun): the papers' Ω, i, ω are in it, and "our Sun" is simply the +z direction. Origin: the A-B barycentre.
- **A and B.** An exact two-body Kepler orbit from A21's elements: B's relative orbit (Thiele-Innes as tools/alphacen_numbers.py), A at −mB/(mA+mB) and B at +mA/(mA+mB) of it. kepler.h needs one change: keplerState with a GM argument (today it fixes GM = kGMSun), GM = G(mA+mB) from A21's masses, so a, P and the masses stay consistent (Kepler's law holds by construction: test it).
- **Proxima.** Placed at today's measured offset (A21 Table 9, L25 Table 2, both parallaxes) and moved on A21's Table 10 orbit (a two-body orbit about the A-B barycentre with GM of all three). Its card says the orbit's angles are uncertain by tens of degrees.
- **Planets of Proxima.** Kepler circles (e = 0) about Proxima, P and a from N25 Table 3, phase from T0 (inferior conjunction = the point of the orbit nearest us). The tilt is unknown: drawn at an **assumed** i = 47° (the star's equator, Kl21; the card says "assumed"), Ω unknown: drawn with the node on the sky's east-west line, said on the card. The mass shown as "at least 1.055 Earth masses" (and "1.44 if in the star's equator plane"). Radius: "unknown" (no transit); the dot drawn at a size labelled "not to scale, size unknown".
- **Disputed and candidates** (owner decides, sect. 14): Proxima c as a dashed circle at 1.489 au, P 1900 d, label "disputed"; the A candidate as a dashed band of its orbit families (a 1.6-2.2 au) with the two sightings, label "unconfirmed". Bb and the dust belt: only in the guide's history page, not drawn.
- **Habitable zones** as green bands (K14 as for the Sun: sun_facts.h has the formula at 5780 K only; it needs the Teff term of eq. 4 for A, B and Proxima). For A and B the band moves with each star.
- **Scales.** A-B 11-35 au, Proxima 12,000 au, b 0.048 au, the stars' radii 0.0057 au (A): 7 orders of magnitude. The lab's zoom already reaches the Oort cloud's outer edge (kOortOuterAU = 100,000 au, belts.h), so the existing zoom covers the range; "Fit" buttons: A-B pair, whole system, Proxima's planets.
- **Time.** The lab's clock (JD, TT) drives all of it: A-B from T0 (decimal years, A21 used Julian years), the planets from BJD T0 (TDB; the lab's clock is TT, the two differ by far less than the ephemeris' hours; the light-time across the system, up to 0.19 yr between Proxima and A-B (computed here), is not applied, said in the guide). Light-travel time from the system to us (4.3 yr) is not applied (the scene shows the system "now", said on the card).
- **What the switch changes.** The night sky: from Alpha Centauri the same star catalogue seen from 1.33 pc away (parallax shifts the nearest stars; the Sun added as a star of V 0.43 at the antipode); the Far away tiles show "Sun" instead of "Alpha Centauri"; the Journey and the Data panel's tables get the system's rows (A-B elements, stars, planets with status). The Oort card's Proxima line stays for the Solar System.
- **Values that change in the app** (fix jobs, REPORT.md): far_masses.h acen A/B 1.13 / 0.97 → 1.0788 / 0.9092 (A21) with real error bars; the "Mann 0.12 ± 0.02" comment; sky_data.inc acen 4.390 → 4.344 ly (A21), prox 4.2465 ly's source → L25 / N25 Table 2 (saved; the Gaia archive read of 5 Nov 2026 was never saved).
- **Files the build touches**: src/orbit/kepler.h/.cpp (GM argument), a new src/orbit/alphacen.h/.cpp (data + positions, with the source comments), src/orbit/far_masses.h, sky_data.inc, sky.cpp (the sky from another place), sun_facts.h (HZ with Teff), orbit_view.cpp and src/ui/shell_orbit.cpp (switch, drawing, cards), journey_dates.cpp, learn_data.inc (guide page), the About page's sources, tests/test_orbit.cpp, tests/check_orbit.py, ui/bench_scenes.cpp (a new Hard-list scene), the graphics card's line and dot passes (CLAUDE.md: new drawing on the card from the start).

## 12. Tests the build job writes first (limits fixed before looking)
1. **Kepler's third law**: a³/P² from A21's a, P, parallax equals A21's mA + mB within 3 × the propagated error (A21 gives 1.9880; computed here 1.9880); the same for K16 and PB16 against their own masses.
2. **The orbit against measured positions**: A21's elements, Thiele-Innes, at the three ALMA epochs of A21 Table 6: position angle and separation within **5 mas** (A21 Fig. 6 shows its ALMA residuals within a few mas; computed here 0.0-1.3 mas).
3. **RV semi-amplitude**: K = (2πG/P)^⅓ m sin i / (M + m)^⅔ with M = 0.1221: b 1.226 ± 0.062, d 0.392 ± 0.057 m/s (N25 Table 3): within 1 % of N25's K (computed here 0.0 %).
4. **Planet orbit size**: a from Kepler's law with M = 0.1221 against N25's a within 1 σ (b 0.04856 vs 0.04848 ± 0.00029).
5. **Habitable zone**: K14 eq. 4-5 at Teff = 5780 K, L = 1 returns 0.9504 / 1.6760 au (the app's existing test); for Proxima with L = 0.0016 (F22 Table 1) and 2900 K the early-Mars edge within 0.0005 au of N25's 0.088 (computed here 0.0879).
6. **Bound Proxima**: the A21 orbit's energy is negative; its periastron and apastron equal A21's 4.1 / 12.3 kau within the table's rounding.
7. **Phase**: the planets' positions at their N25 T0 are at inferior conjunction (z towards the Sun maximal); after P they return within 1e-9 of the orbit.
8. **The sky**: the Sun's V from A-B 0.43 ± 0.01 (Wi18 4.81 and A21's distance); its direction within 0.01° of A21 Table 9's antipode.

## 13. Build steps (one session each, something to look at each time)
1. **Fix the existing values** (small): far_masses.h (A21 masses with errors, the Mann comment), sky_data.inc (4.344 ly, saved Gaia source), About page lines. Look: the Far away card.
2. **A and B**: kepler.h GM argument, alphacen.h data, the switch, A and B on their exact orbit with the date, cards (mass, radius, Teff, L, type, age "disputed"), habitable zones, a benchmark scene. Look: the pair moving over 80 years.
3. **Proxima and its planets**: Proxima on its orbit at its measured place, b and d with phase, assumed tilt and "at least" masses, Proxima's habitable zone, flares as a card fact. Look: zoom from A-B out to Proxima and in to b and d.
4. **Disputed and candidates** (if the owner agrees): c and the A candidate dashed and labelled; the guide's history page (Bb, the dust belt, names, Henderson, Innes).
5. **Their sky**: the star catalogue from 1.33 pc, our Sun in Cassiopeia, A and B from Proxima b. Look: the sky from Proxima b.
6. **Journey and Data panel** rows; Starshot / TOLIMAN lines in the guide.

## 14. Questions for the owner (each with a recommendation)
1. Proxima c: show it **dashed and labelled "disputed"** (recommended: the public knows the name, and the dispute itself teaches how planets are found), or leave it out?
2. The JWST candidate around A: show its possible orbits as a **faint dashed band labelled "unconfirmed, seen once"** (recommended), or leave it out until it is confirmed?
3. Proxima's mass: **0.1221 for the physics, with the lensing 0.150 shown beside it** (recommended), or the lensing value alone (as the app shows now)?
4. A and B's masses and distance: switch to **Akeson et al. 2021** (recommended) now, in build step 1?
5. "Our Sun in their sky" in the first build, or as its own later step (recommended: its own step 5, because the star catalogue seen from elsewhere is a large job)?
6. The planets' unknown tilt: draw at the star's equator tilt (47°, labelled "assumed", recommended) or edge-on (the minimum-mass case)?
