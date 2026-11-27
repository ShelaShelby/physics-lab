Atoms in fields (0.93.0): the hydrogen Stark effect and the Breit-Rabi levels
================================================================================

The papers and notes this folder's extracts come from are NOT in this public repo. They are in the private repo ShelaShelby/physics-lab-sources at
data/fields/ (same names). native/data/SOURCES.md lists each saved file with its reference, URL, date read (2026-11-27) and sha256.
Each <name>.extract.txt here holds the exact lines a test or the code uses, with the original's sha256; native/tools/check_sources.py checks them.

- anh-tai_2024: the parabolic states and the Stark shifts to second order (src/atom/stark.*).
- jentschura_2001: the series of the ground state's and the (3, 0, 1) state's energy in the field (tests/test_stark.inc).
- kishimoto_2002: the classical saddle-point field F_c = W^2/4 (the top of the lab's field slider).
- martin_wiese: hydrogen's 1s hyperfine splitting (src/atom/breit_rabi.cpp).
- steck_sodium, steck_rubidium85, steck_rubidium87, steck_cesium: the Breit-Rabi formula (eq. 23 to 29), and each ground state's A, g_J, g_I and
  clock-transition shift (src/atom/breit_rabi.cpp, tests/test_breit_rabi.inc).
