[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/iers/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

IERS Conventions (2010), IERS Technical Note No. 36, G. Petit & B. Luzum (eds.), chapter 10 "General relativistic models for space-time coordinates and equations of motion".
tn36_c10.pdf: downloaded 2026-11-05 from https://iers-conventions.obspm.fr/content/chapter10/tn36_c10.pdf (saved as received).
Used for: the first (Schwarzschild) term of eq. (10.12), with beta = gamma = 1, as the Sun's 1PN field on the Real planets (src/orbit/nbody.cpp, 0.29).
