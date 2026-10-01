// The real Solar System data. Copied number for number from the web Orbit Lab
// (index.html: SOLAR, DWARFS, MOONS and COMETS); see solar_data.h for sources and units.
#include "solar_data.h"

namespace orbit {

const PlanetData kPlanets[] = {
    {"Mercury", "#8f8f8f", "rocky planet",
     {0.38709927, 3.7e-7, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.2503235, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081},
     2439.7, 0.0553, 0.378, 167, "mean surface",
     "Closest to the Sun, a year lasts 88 days"},
    {"Venus", "#d9a441", "rocky planet",
     {0.72333566, 0.0000039, 0.00677672, -0.00004107, 3.39467605, -0.0007889, 181.9790995, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418},
     6051.8, 0.815, 0.907, 464, "surface",
     "Hottest planet: thick CO₂ traps heat"},
    {"Earth", "#2f6fed", "rocky planet (home)",
     {1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0, 0},
     6378.1, 1, 1, 15, "mean surface",
     "Home: the only known world with life"},
    {"Mars", "#d0553a", "rocky planet",
     {1.52371034, 0.00001847, 0.0933941, 0.00007882, 1.84969142, -0.00813131, -4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343},
     3396.2, 0.107, 0.377, -65, "mean surface",
     "Has the tallest volcano known"},
    {"Jupiter", "#c98f4f", "gas giant",
     {5.202887, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106},
     71492, 317.8, 2.36, -110, "at the 1-bar level",
     "2.5× heavier than all other planets"},
    {"Saturn", "#b8a24a", "gas giant",
     {9.53667594, -0.0012506, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794},
     60268, 95.2, 0.916, -140, "at the 1-bar level",
     "Less dense than water"},
    {"Uranus", "#4fb8c4", "ice giant",
     {19.18916464, -0.00196176, 0.04725744, -0.00004397, 0.77263783, -0.00242939, 313.23810451, 428.48202785, 170.9542763, 0.40805281, 74.01692503, 0.04240589},
     25559, 14.5, 0.889, -195, "at the 1-bar level",
     "Rolls on its side (98° tilt)"},
    {"Neptune", "#3a53d6", "ice giant",
     {30.06992276, 0.00026291, 0.00859048, 0.00005105, 1.77004347, 0.00035372, -55.12002969, 218.45945325, 44.96476227, -0.32241464, 131.78422574, -0.00508664},
     24764, 17.1, 1.12, -200, "at the 1-bar level",
     "Winds over 2,000 km/h, the fastest known"},
};
const DwarfData kDwarfs[] = {
    {"Ceres", "#a3a3a3", "Only dwarf planet inside the asteroid belt",
     2461200.5, 0.07969229514816586, 2.765552595034094, 10.58802780183462, 80.24862682043221, 73.29421453021587, 274.4193463761342, 0.21430445064843, 939.4, 9.384e20},
    {"Pluto", "#c9a98a", "Ranked as a dwarf planet since 2006",
     2457588.5, 0.2518378778576892, 39.58862938517124, 17.14771140999114, 110.2923840543057, 113.7090015158565, 38.68366347318184, 0.003956838955553025, 2376, 1.303e+22},
    {"Haumea", "#d8d0c0", "Spins in 3.9 hours, so it is egg-shaped",
     2461200.5, 0.1944430148898797, 43.06029023650952, 28.20847393040364, 121.7860561329425, 240.6905472508661, 223.2104118812299, 0.003488097731816818, 1544, 3.952e+21},
    {"Makemake", "#c98f6a", "Named after the Rapa Nui creator god",
     2461200.5, 0.1588889953992523, 45.57093317300052, 29.02785603743067, 79.2948338209406, 297.0922733397207, 169.9379962048232, 0.003203850120050116, 1430, 2.69e+21},
    {"Eris", "#e4e4e4", "Its discovery led to Pluto's reclassification",
     2461200.5, 0.4382385347971672, 67.93394687853566, 43.9258279471791, 36.00477044417249, 150.7949235840312, 211.774434275007, 0.001760247770619088, 2326, 1.638e+22},
};
const MoonData kMoons[] = {
    {"Earth", "Moon", 384400, 27.3217, 0.0549, 1737.4, 5.145, false, "Always shows the same face to Earth"},
    {"Mars", "Phobos", 9377, 0.3191, 0.015, 11.1, 25.2, false, "Will break apart in ~100 million years"},
    {"Mars", "Deimos", 23460, 1.263, 0.0005, 6.2, 25.2, false, "Only about 12 km across"},
    {"Jupiter", "Io", 421800, 1.769138, 0.004, 1821.5, 3.1, false, "The most volcanic body known"},
    {"Jupiter", "Europa", 671100, 3.551181, 0.009, 1560.8, 3.1, false, "Has an ocean of liquid water under its ice"},
    {"Jupiter", "Ganymede", 1070400, 7.154553, 0.001, 2631.2, 3.1, false, "Largest moon, bigger than Mercury"},
    {"Jupiter", "Callisto", 1882700, 16.689017, 0.007, 2410.3, 3.1, false, "One of the most cratered surfaces known"},
    {"Saturn", "Enceladus", 238020, 1.370218, 0.0045, 252, 26.7, false, "Ice geysers shoot water into space"},
    {"Saturn", "Rhea", 527040, 4.5175, 0.001, 763, 26.7, false, "Saturn's second-largest moon"},
    {"Saturn", "Titan", 1221870, 15.945421, 0.0292, 2575, 26.7, false, "The only moon with a thick atmosphere"},
    {"Uranus", "Titania", 436300, 8.705867, 0.0011, 788.9, 97.8, false, "Uranus's largest moon"},
    {"Uranus", "Oberon", 583500, 13.463234, 0.0014, 761.4, 97.8, false, "Uranus's second-largest moon"},
    {"Neptune", "Triton", 354760, 5.876854, 0.00002, 1353.4, 28.3, true, "Orbits backwards: probably a captured object"},
    {"Pluto", "Charon", 19596, 6.3872, 0.0002, 606, 122.5, false, "Half the size of Pluto: they orbit each other"},
};
const CometData kComets[] = {
    {"Halley", "#8ecbff", 0.5928, 0.96658, 161.96, 58.42, 111.33, 2446469.974, 27563.5, 11, 1,
     "Halley’s Comet",
     "Seen from Earth about every 76 years: last in 1986, next on 28 July 2061. It goes round backwards"},
    {"Hale–Bopp", "#b4a0ff", 0.8907, 0.99498, 89.29, 282.73, 130.41, 2450537.135, 863300, 60, 1.7,
     "Comet Hale–Bopp",
     "Bright enough to see with the naked eye for 18 months in 1996–97. It will be back in about 2,400 years"},
    {"NEOWISE", "#7fe0c8", 0.295, 0.99917, 128.94, 61.01, 37.279, 2459034.179, 2443000, 5, 1,
     "Comet NEOWISE (C/2020 F3)",
     "Put on a show in the evening sky of July 2020. It will not be back for about 6,800 years"},
    {"Encke", "#ffd28a", 0.339, 0.84711, 11.344, 334, 187, 2460240.009, 1206.5, 4.9, 0.35,
     "Comet Encke (2P)",
     "The shortest trip of any fairly bright comet: 3.3 years. Its dust makes the Taurid meteor showers"},
    {"67P", "#ff9fb0", 1.21, 0.64989, 3.87, 50.1, 12.8, 2459520.5, 2350, 3.4, 0.3,
     "Comet 67P/Churyumov–Gerasimenko",
     "In 2014 the Rosetta spacecraft went into orbit around it and the Philae lander touched down on it. Shaped like a rubber duck"},
    {"Swift–Tuttle", "#d0e88a", 0.9595, 0.9632, 113.45, 139.44, 153, 2448968.5, 48789, 26, 0.9,
     "Comet Swift–Tuttle",
     "The comet behind the Perseid meteor shower every August. Its nucleus is 26 km wide, the largest that regularly crosses Earth’s path"},
};

const std::size_t kPlanetCount = sizeof(kPlanets) / sizeof(kPlanets[0]);
const std::size_t kDwarfCount = sizeof(kDwarfs) / sizeof(kDwarfs[0]);
const std::size_t kMoonCount = sizeof(kMoons) / sizeof(kMoons[0]);
const std::size_t kCometCount = sizeof(kComets) / sizeof(kComets[0]);

}  // namespace orbit
