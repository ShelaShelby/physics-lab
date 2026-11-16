# EXTRACT of a saved original that is NOT in this repository (it is kept in the private repo ShelaShelby/physics-lab-sources, see native/data/SOURCES.md).
# Original: data/stars/hyg_readme.md
# Reference: D. Nash (astronexus), the HYG Database's README for the hyg folder (HYG v4.x), licence CC BY-SA 4.0
# URL: https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/README.md
# Date read: 2026-11-16
# sha256 of the original: 81f765951e60131e49ccc91376815791e130ad1db46047eb1384d3ba6bcb5af4
# Place in the original: the list of fields, items 12 and 25
# Used by: src/blackhole/light.h (the catalogue's mag is the V magnitude) and tests/test_blackhole.cpp testTrueLight.
# Every block between a "#--- begin" line and its "#--- end" line is copied exactly from the original (line ends as LF, trailing blanks dropped);
# native/tools/check_sources.py looks each block up in the original. Lines that begin with "#---" or "# " are not part of the original.
#--- begin field 12, mag
12. `mag`: The star's apparent visual magnitude.
#--- end
#--- begin field 25: the V scale of the mag field
25. `var_min,` `var_max`:  Star's approximate magnitude range, for variables.  This value is based on the Hp magnitudes for the range in the original Hipparcos catalog, adjusted to the V magnitude scale to match the "mag" field.
#--- end
