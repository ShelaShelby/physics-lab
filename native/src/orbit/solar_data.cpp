// The real Solar System data; see solar_data.h for sources and units.
#include "solar_data.h"

namespace orbit {

const PlanetData kPlanets[] = {
    {"Mercury", "#8f8f8f", "rocky planet",
     {0.38709927, 3.7e-7, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.2503235, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081},
     2440.5, 0.0553, 3.70, 3.70, 167, "mean surface",   // equatorial radius, NASA Mercury fact sheet (2440.5 km; 2439.7 there is the volumetric mean; 2026-11-05 audit)
     "Closest to the Sun, a year lasts 88 days"},
    {"Venus", "#d9a441", "rocky planet",
     {0.72333566, 0.0000039, 0.00677672, -0.00004107, 3.39467605, -0.0007889, 181.9790995, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418},
     6051.8, 0.815, 8.87, 8.87, 464, "surface",
     "Hottest planet: thick CO₂ traps heat"},
    {"Earth", "#2f6fed", "rocky planet (home)",
     {1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0, 0},
     6378.1, 1, 9.80, 9.780, 15, "mean surface",
     "Home: the only known world with life"},
    {"Mars", "#d0553a", "rocky planet",
     {1.52371034, 0.00001847, 0.0933941, 0.00007882, 1.84969142, -0.00813131, -4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343},
     3396.2, 0.107, 3.71, 3.69, -65, "mean surface",
     "Has the tallest volcano known"},
    {"Jupiter", "#c98f4f", "gas giant",
     {5.202887, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106},
     71492, 317.8, 24.79, 23.12, -110, "at the 1-bar level",
     "2.5× heavier than all other planets"},
    {"Saturn", "#b8a24a", "gas giant",
     {9.53667594, -0.0012506, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794},
     60268, 95.2, 10.44, 8.96, -140, "at the 1-bar level",
     "Less dense than water"},
    {"Uranus", "#4fb8c4", "ice giant",
     {19.18916464, -0.00196176, 0.04725744, -0.00004397, 0.77263783, -0.00242939, 313.23810451, 428.48202785, 170.9542763, 0.40805281, 74.01692503, 0.04240589},
     25559, 14.5, 8.87, 8.69, -195, "at the 1-bar level",
     "Rolls on its side (98° tilt)"},
    {"Neptune", "#3a53d6", "ice giant",
     {30.06992276, 0.00026291, 0.00859048, 0.00005105, 1.77004347, 0.00035372, -55.12002969, 218.45945325, 44.96476227, -0.32241464, 131.78422574, -0.00508664},
     24764, 17.1, 11.15, 11.00, -200, "at the 1-bar level",
     "Winds over 2,000 km/h, the fastest known"},
};
// Sizes and masses (2026-11-05 audit): Ceres and Pluto as before (SBDB / JPL); Haumea, Makemake and Eris had no named source and now come from JPL "Planetary Physical
// Parameters" (ssd.jpl.nasa.gov/planets/phys_par.html, dwarf-planet table, read 2026-11-05): diameter = 2 x mean radius (Haumea 715, Makemake 714, Eris 1200 +- 50 km),
// mass Haumea 4006e18, Makemake 3100e18, Eris 16600e18 kg. (They were 1544 / 1430 / 2326 km and 3.952e21 / 2.69e21 / 1.638e22 kg.)
// 0.46.3 (outside review: the card's 1,430 km sat next to axes whose equal volume is 1,544 km): Haumea's size is 2 x its volume-equivalent radius 772 (+20 -19) km from the same
// paper as its axes, B. Proudfoot, W. Grundy, F. L. Rommel, E. Fernandez-Valenzuela, D. Ragozzine, "Triaxial Shapes and Densities of G!kun||'homdima, Haumea, and Varda from
// Stellar Occultations", Planetary Science Journal 7 (2026), doi:10.3847/PSJ/ae77fa, Table 1 (read on its page 2026-11-08): a 1061, b 844, c 514 km, density 2050 kg/m3.
// Check: 2 x cbrt(1061 x 844 x 514) = 1,544 km; with JPL's 4.006e21 kg that is 2,060 kg/m3, the paper's density within its error. (JPL's 715 km mean radius is older.)
// 0.75.2 (outside review): Eris's diameter is 2 x 1,163 +- 6 km = 2,326 +- 12 km, from its stellar occultation of 6 Nov 2010: Sicardy et al., "A Pluto-like radius and a high albedo for the dwarf
// planet Eris from an occultation", Nature 478, 493 (2011), abstract (saved: data/planets/sicardy2011_eris_nature10550.html). JPL's 2,400 km (radius 1,200 +- 50) was an older value. That makes Eris
// a little smaller than Pluto (2,376 km); its mass is still JPL's.
const DwarfData kDwarfs[] = {
    {"Ceres", "#a3a3a3", "Only dwarf planet inside the asteroid belt",
     2461200.5, 0.07969229514816586, 2.765552595034094, 10.58802780183462, 80.24862682043221, 73.29421453021587, 274.4193463761342, 0.21430445064843, 939.4, 9.384e20},
    {"Pluto", "#c9a98a", "Ranked as a dwarf planet since 2006",
     2457588.5, 0.2518378778576892, 39.58862938517124, 17.14771140999114, 110.2923840543057, 113.7090015158565, 38.68366347318184, 0.003956838955553025, 2376, 1.303e+22},
    {"Haumea", "#d8d0c0", "Spins in 3.9 hours, so it is a squashed rugby ball",
     2461200.5, 0.1944430148898797, 43.06029023650952, 28.20847393040364, 121.7860561329425, 240.6905472508661, 223.2104118812299, 0.003488097731816818, 1544, 4.006e+21},
    {"Makemake", "#c98f6a", "Named after the Rapa Nui creator god",
     2461200.5, 0.1588889953992523, 45.57093317300052, 29.02785603743067, 79.2948338209406, 297.0922733397207, 169.9379962048232, 0.003203850120050116, 1428, 3.1e+21},
    {"Eris", "#e4e4e4", "Its discovery led to Pluto's reclassification",
     2461200.5, 0.4382385347971672, 67.93394687853566, 43.9258279471791, 36.00477044417249, 150.7949235840312, 211.774434275007, 0.001760247770619088, 2326, 1.66e+22},
};
// 2026-11-05 audit: Phobos and Deimos from the NASA Mars fact sheet (nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html: 9378 / 23459 km, 0.31891 / 1.26244 d,
// e 0.0151 / 0.0005; radius = the geometric mean of the three axes, 11.1 / 6.2 km); Charon e 0.0 as the NASA Pluto fact sheet (it was 0.0002, no source).
const MoonData kMoons[] = {
    {"Earth", "Moon", 384400, 27.3217, 0.0549, 1737.4, 5.145, false, "Always shows the same face to Earth"},
    {"Mars", "Phobos", 9378, 0.31891, 0.0151, 11.1, 25.2, false, "Falls to Mars or breaks up in ~50 million years"},   // NASA science.nasa.gov/mars/moons/phobos (read 2026-11-07): "will either crash into Mars in 50 million years or break up into a ring" (it said ~100 million, no source)
    {"Mars", "Deimos", 23459, 1.26244, 0.0005, 6.2, 25.2, false, "Only about 12 km across"},
    {"Jupiter", "Io", 421800, 1.769138, 0.004, 1821.5, 3.1, false, "The most volcanic body known"},
    {"Jupiter", "Europa", 671100, 3.551181, 0.009, 1560.8, 3.1, false, "Strong evidence of a salty ocean under its ice"},   // 0.41: it said "Has an ocean" as a fact; NASA science.nasa.gov/jupiter/jupiter-moons/europa (read 2026-11-08): "strong evidence ... has a saltwater ocean"
    {"Jupiter", "Ganymede", 1070400, 7.154553, 0.001, 2631.2, 3.1, false, "Largest moon, bigger than Mercury"},
    {"Jupiter", "Callisto", 1882700, 16.689017, 0.007, 2410.3, 3.1, false, "One of the most cratered surfaces known"},
    {"Saturn", "Enceladus", 238020, 1.370218, 0.0045, 252, 26.7, false, "Ice geysers shoot water into space"},
    {"Saturn", "Rhea", 527040, 4.5175, 0.001, 763, 26.7, false, "Saturn's second-largest moon"},
    {"Saturn", "Titan", 1221870, 15.945421, 0.0292, 2575, 26.7, false, "The only moon with a thick atmosphere"},
    {"Uranus", "Titania", 436300, 8.705867, 0.0011, 788.9, 97.8, false, "Uranus's largest moon"},
    {"Uranus", "Oberon", 583500, 13.463234, 0.0014, 761.4, 97.8, false, "Uranus's second-largest moon"},
    {"Neptune", "Triton", 354760, 5.876854, 0.00002, 1353.4, 28.3, true, "Orbits backwards: probably a captured object"},
    {"Pluto", "Charon", 19596, 6.3872, 0.0, 606, 119.5, false, "Half the size of Pluto: they orbit each other"},   // 0.39.3: tilt 119.5 = Pluto's obliquity to its orbit, NASA Pluto fact sheet (nssdc.gsfc.nasa.gov/planetary/factsheet/plutofact.html, read 2026-11-07: 119.51; Charon orbits in Pluto's equator plane). It was 122.5, no source, while the card's axis (IAU 2015 pole) says 119.6
};
// Nucleus sizes (km): JPL Small-Body Database "diameter" (SBDB API, phys-par=1, read 2026-10-03): Halley 11.0, Hale-Bopp 60 (+-20), Encke 4.8 (it was 4.9 before 0.10.5 stage 5), 67P 3.4 (+-0.1),
// Swift-Tuttle 26. SBDB gives no diameter for NEOWISE: its 5 km is kept (it only sets the dot's size and a card line) and the card says "rough: JPL gives no size".
const CometData kComets[] = {
    // Halley: JPL Small-Body Database, orbit_id 75 (solution date 2025-11-21 15:57:34), epoch JD 2439875.5 = 1968-01-20 00:00 TDB. q, e, i, Ω, ω, tp and period are all from that one record (read 2026-10-03).
    {"Halley", "#8ecbff", 0.5748638313743413, 0.9679359956953211, 162.1905300439129, 59.09894720612437, 112.2414314637764, 2446469.9736161465, 27728.04608790421, 11, 1,
     "Halley’s Comet",
     // 0.39.3: the real perihelion was 9 Feb 1986 (JPL news "Return of Halley's Comet", 9 Jan 1985: "perihelion will occur on Feb. 9, 1986"); the 1968 record's
     // two-body tp above is 8 Feb 1986 11:22 TDB, which the card's "Last closest approach" shows. Next perihelion 28 July 2061 17:17 TDB: JPL Horizons, orbit solution JPL#75
     // (osculating elements at 2061-07-28, Tp = JD 2474034.220; data/jpl/horizons-halley-2061.txt, read 2026-11-11; 0.39.3 had taken the date from Wikipedia).
     "Seen from Earth about every 76 years: closest to the Sun on 9 Feb 1986 (JPL), next due 28 July 2061 (JPL’s prediction); this lab’s simple orbit puts these on 8 Feb 1986 and in January 2062. It goes round backwards"},
    // Hale–Bopp: JPL Small-Body Database, orbit_id 226 (solution date 2022-08-01 01:56:46), epoch JD 2459837.5 = 2022-09-15 00:00 TDB. q, e, i, Ω, ω, tp and period are all from that one record (read 2026-10-03).
    {"Hale–Bopp", "#b4a0ff", 0.890537663547794, 0.9949810027633206, 89.28759424740302, 282.7334213961641, 130.4146670659176, 2450537.134907144, 863279.5034870314, 60, 1.7,
     "Comet Hale–Bopp",
     // 0.39.3: real perihelion 1 Apr 1997 03:19 UT (NASA NSSDC "Comet Hale-Bopp" page, D. R. Williams); the 2022 record's two-body tp above is 29 Mar 1997, which the card shows
     "Bright enough to see with the naked eye for 18 months in 1996–97, closest to the Sun on 1 Apr 1997 (NASA; this lab’s simple orbit, fixed in 2022, puts it on 29 Mar). It will be back in about 2,400 years"},
    // NEOWISE: JPL Small-Body Database, orbit_id 31 (solution date 2024-07-30 14:57:24), epoch JD 2459036.5 = 2020-07-06 00:00 TDB. q, e, i, Ω, ω, tp and period are all from that one record (read 2026-10-03).
    {"NEOWISE", "#7fe0c8", 0.2946512493809196, 0.9991780262531292, 128.9375027594809, 61.01042818536988, 37.2786584481257, 2459034.1788980444, 2478985.217997125, 5, 1,
     "Comet NEOWISE (C/2020 F3)",
     "Put on a show in the evening sky of July 2020. It will not be back for about 6,800 years", true},
    // Encke: JPL Small-Body Database, orbit_id K273/20 (solution date 2026-10-01 15:11:48), epoch JD 2460147.5 = 2023-07-22 00:00 TDB. q, e, i, Ω, ω, tp and period are all from that one record (read 2026-10-03).
    {"Encke", "#ffd28a", 0.3395910296522707, 0.8469532425568865, 11.33709697552753, 334.0240655739339, 187.2820411908973, 2460240.026674769, 1207.248132847095, 4.8, 0.35,
     "Comet Encke (2P)",
     "The shortest trip of any fairly bright comet: 3.3 years. Its dust makes the Taurid meteor showers"},
    // 67P: JPL orbit solution K284/1 (the same solution as SBDB's current record, whose epoch is 2015-Oct-22), osculating at epoch JD 2459520.5 = 2021-11-02 00:00 TDB (the 2021 perihelion) from Horizons ELEMENTS (EC QR IN OM W Tp PR).
    // SBDB's epoch-2015 elements as a fixed orbit are 1.37 AU off Horizons in Nov 2021 and 0.15 AU in Jan 2026; these are 0.011 AU off in Jan 2026 (tools/make_comet_ref.py). One solution, one epoch.
    {"67P", "#ff9fb0", 1.210612534256783, 0.6497050988505667, 3.871684817044239, 36.33532737405282, 22.1315389981404, 2459520.562505543, 2346.688539238651, 3.4, 0.3,
     "Comet 67P/Churyumov–Gerasimenko",
     "In 2014 the Rosetta spacecraft went into orbit around it and the Philae lander touched down on it. Shaped like a rubber duck"},
    // Swift–Tuttle: JPL Small-Body Database, orbit_id 32 (solution date 2001-10-09 15:34:49), epoch JD 2450000.5 = 1995-10-10 00:00 TDB. q, e, i, Ω, ω, tp and period are all from that one record (read 2026-10-03).
    {"Swift–Tuttle", "#d0e88a", 0.959516155068868, 0.963225755046038, 113.453816997171, 139.3811920815948, 152.9821676305871, 2448968.499784556, 48681.19346262312, 26, 0.9,
     "Comet Swift–Tuttle",
     // Last perihelion 12 Dec 1992 07:47 TDB, next 12 July 2126 09:44 TDB: JPL Horizons, orbit solution JPL#32 (osculating Tp = JD 2448968.824 and 2497757.905;
     // data/jpl/horizons-swift-tuttle-1992-2126.txt, read 2026-11-11; 0.39.3 had taken them from Wikipedia); the 1995 record's tp and period above give 11 Dec 1992 and March 2126
     "The comet behind the Perseid meteor shower every August. Its nucleus is 26 km wide, the largest that regularly crosses Earth’s path. Closest to the Sun on 12 Dec 1992, next on 12 July 2126 (JPL; this lab’s simple orbit puts these on 11 Dec 1992 and in March 2126)"},
};

const std::size_t kPlanetCount = sizeof(kPlanets) / sizeof(kPlanets[0]);
const std::size_t kDwarfCount = sizeof(kDwarfs) / sizeof(kDwarfs[0]);
const std::size_t kMoonCount = sizeof(kMoons) / sizeof(kMoons[0]);
const std::size_t kCometCount = sizeof(kComets) / sizeof(kComets[0]);

}  // namespace orbit
