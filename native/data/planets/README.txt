[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/planets/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

The colour-science 0.4.7 source files kept here (colour-science_v0.4.7_*.py) are BSD-3-Clause: data/colour-science_v0.4.7_LICENSE.txt is the LICENSE file of that release tag (https://raw.githubusercontent.com/colour-science/colour/v0.4.7/LICENSE, read 2026-11-15). The CIE tables inside them are CIE data as the package redistributes them; their own terms were not read.

Planet colours (0.50, Orbit Lab; review item 8).

mallama_2017_arxiv1609.05048.pdf
    Saved as received on 11 Nov 2026 from https://arxiv.org/pdf/1609.05048 (Mallama, Krobusek & Pavlov 2017, "Comprehensive wide-band magnitudes and
    albedos for the planets, with applications to exo-planets and Planet Nine", Icarus 282, 19). Table 7 (geometric albedos) covers Mercury:
    U 0.087, B 0.105, V 0.142, R 0.172, I 0.208, Rc 0.158, Ic 0.180, u' 0.095, g' 0.130, r' 0.169, i' 0.200, z' 0.237 (checked against the PDF;
    tools/map_albedo_colour.py had the same row since 0.26). It does not cover Pluto or the moons.
colour-science_v0.4.7_illuminant_sds.py
    Saved as received on 11 Nov 2026 from https://raw.githubusercontent.com/colour-science/colour/v0.4.7/colour/colorimetry/datasets/illuminants/sds.py.
    Used: its "D65" spectral distribution (300-780 nm, 5 nm; 100.0 at 560 nm).
tools/planet_disc_colour.py (standard library only): albedos -> CIE 1931 XYZ under D65 (360-780 nm, 5 nm) -> linear sRGB. Mercury: r/g 1.1734,
b/g 0.7921 (colour-science gives the same to 4 digits). tools/map_tint.py tinted the black-and-white MESSENGER map to these ratios (assets/maps/README.md).
Pluto: no colour global mosaic (USGS has only the black-and-white LORRI/MVIC one) and no measured disc colour was saved: left black and white (ROADMAP).

madhusudhan_burrows2012_arxiv1112.4476.pdf (0.77.0)
    Saved as received on 23 Nov 2026 from https://arxiv.org/pdf/1112.4476 (ApJ 747, 25). Sec. 3.1: a Lambertian surface's geometric albedo is 2/3 of its
    single-scattering albedo. Used for the maps' brightness factors (tools/map_albedo_scale.py).
