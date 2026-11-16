[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/stars/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

The colour-science 0.4.7 source files kept here (colour-science_v0.4.7_*.py) are BSD-3-Clause: data/colour-science_v0.4.7_LICENSE.txt is the LICENSE file of that release tag (https://raw.githubusercontent.com/colour-science/colour/v0.4.7/LICENSE, read 2026-11-15). The CIE tables inside them are CIE data as the package redistributes them; their own terms were not read.

Star colours and brightness (0.50, Orbit Lab; review items 4 and 5).

ballesteros_2012_arxiv1201.1809.pdf
    Saved as received on 11 Nov 2026 from https://arxiv.org/pdf/1201.1809 (F. J. Ballesteros, "New insights into black bodies", EPL 97, 34008 (2012),
    arXiv:1201.1809v2). Eq. (14): T = 4600 (1 / (0.92 (B-V) + 1.7) + 1 / (0.92 (B-V) + 0.62)) K, the black-body temperature of a B-V colour.
colour-science_v0.4.7_cmfs.py
    Saved as received on 11 Nov 2026 from https://raw.githubusercontent.com/colour-science/colour/v0.4.7/colour/colorimetry/datasets/cmfs.py.
    Used: its "CIE 1931 2 Degree Standard Observer" table (DATA_CMFS_STANDARD_OBSERVER), 360-830 nm in 1 nm steps, from CVRL. Spot values
    x(600) = 1.0622, y(555) = 1, z(450) = 1.77211, the same as the table tools/sun_colour.py used in 0.49.4. CIE's own CSV (files.cie.co.at) and
    CVRL (cvrl.org) could not be reached from the session (proxy refused / no DNS). No published Planckian-locus table could be saved, so the test
    (tests/test_orbit.cpp, testStarColours050) computes the black body's chromaticity itself from this table and Planck's law.
    tools/star_colour.py: B-V -> T (Ballesteros) -> Planck spectrum -> XYZ -> linear sRGB (IEC 61966-2-1, D65) -> largest channel 1 -> sRGB bytes,
    written to src/orbit/star_colours.inc (B-V -0.40 to 2.00 in steps of 0.01). B-V 1.5: 3,794 K, (255, 206, 155); B-V -0.3: 16,602 K, (177, 198, 255);
    B-V 0.65: 5,778 K, (255, 241, 234). No white is blended in (until 0.49.4: an unnamed curve fit, then 30 % white).
hyg_v41_far_stars_rows.csv
    The header and the 18 rows of the far-away stars (Alpha Centauri A and B, Proxima, Barnard's Star, Sirius, Lalande 21185, Epsilon Eridani, Procyon,
    61 Cygni A and B, Tau Ceti, Altair, Vega, Fomalhaut, Arcturus, Polaris, Betelgeuse, Rigel), cut unchanged from HYG v4.1,
    https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv (fetched 11 Nov 2026, 33,932,548 bytes,
    sha256 d9f69fd86bbf90a4e4d52b4c5c53eacfa6dfc0bfdef85bfd94f095e0bebe4ebd; the sky's own catalogue, licence CC BY-SA 4.0). The whole file is
    not kept (34 MB). Used: the column "ci" (B-V, Hipparcos) in src/orbit/sky.cpp farBV; the far stars' dots use it instead of a rough B-V from the
    spectral letter (spBV, no source, removed). The spectral types in these rows equal the app's, so it is the same catalogue. Mamajek's dwarf table
    (pas.rochester.edu) was refused by the proxy and is not needed: every far star has its own measured B-V.
iau2015_resolution_b2_arxiv1510.06262.pdf
    Saved as received on 11 Nov 2026 from https://arxiv.org/pdf/1510.06262 (IAU 2015 Resolution B2 on bolometric magnitudes, Mamajek et al.).
    Eq. (4): m = -2.5 log(f / f0), Pogson's scale: a star's light f is proportional to 10^(-0.4 m). The app draws every star by one rule
    (src/orbit/sky.h starDraw): drawn light (opacity x disc area) = 1.2 x (f / f(m = 6.6))^0.5 square pixels.

hyg_readme.md (0.55.4)
    Saved as received on 16 Nov 2026 from https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/README.md (the HYG Database's description of
    its fields, CC BY-SA 4.0). Field 12: "mag: The star's apparent visual magnitude"; field 25: the variables' range "adjusted to the V magnitude scale to match
    the "mag" field". So the sky's magnitudes are V magnitudes: src/blackhole/light.h compares them with the Sun's V magnitude. Extract: hyg_readme.extract.txt.
