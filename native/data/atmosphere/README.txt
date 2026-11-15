[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/atmosphere/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

The colour-science 0.4.7 source files kept here (colour-science_v0.4.7_*.py) are BSD-3-Clause: data/colour-science_v0.4.7_LICENSE.txt is the LICENSE file of that release tag (https://raw.githubusercontent.com/colour-science/colour/v0.4.7/LICENSE, read 2026-11-15). The CIE tables inside them are CIE data as the package redistributes them; their own terms were not read.

Earth's air in the Orbit Lab (0.51.2): tools/make_rayleigh_data.py -> src/render/rayleigh_data.inc.

colour-science_v0.4.7_rayleigh.py
    colour/phenomena/rayleigh.py of colour-science 0.4.7, from its wheel on PyPI (colour_science-0.4.7-py3-none-any.whl,
    sha256 fe2383d86e507fc11a4b5206ff31a7399031e0dce527e22ec06eeb491d45d9a7; this file's sha256 67e14ed1671258ee3f02c798eaba4c65bec18d4e3f01760d902b4c6f249a4744),
    saved as received on 14 Nov 2026. Its implementation of Bodhaine, Wood, Dutton & Slusser (1999), J. Atmos. Oceanic Technol. 16, 1854 (the paper itself:
    journals.ametsoc.org refused the session). The tool ports these formulas and asserts the file's own example, tau(555 nm) = 0.0936290... at the equator.
colour-science_v0.4.7_illuminants_sds.py
    colour/colorimetry/datasets/illuminants/sds.py of the same wheel (sha256 0f018d185192d6b09be8c38eb7516a9a11acd27cc6c2259c4f7e46eacf37a3e5), saved as received.
    Used: the CIE D65 spectrum ("D65", 5 nm steps).
bmng_page_2026-11-14.html
    https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/ saved on 14 Nov 2026: Blue Marble NG (the Earth map) shows "the color of the Earth's
    surface", so the air is added on top; "the creator of the Blue Marble uses a uniform blue color for deep ocean regions" (not measured: ROADMAP).
