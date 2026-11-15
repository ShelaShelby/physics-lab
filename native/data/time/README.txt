[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/time/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

The lab's clock: UT (the date bar) to TDB (the JPL data). Used by src/orbit/time_scale.cpp (0.46.3).
iers-leap-second.dat: IERS Bulletin C leap-second table (TAI - UTC from 1972), downloaded 2026-11-08 from https://hpiers.obspm.fr/iers/bul/bulc/Leap_Second.dat
  (saved as received; "updated through IERS Bulletin 72 issued in July 2026": 37 s since 1 Jan 2017, no leap second since).
nasa-deltat-poly2004.html: F. Espenak, NASA GSFC, "Polynomial Expressions for Delta T" (from Espenak and Meeus, Five Millennium Canon of Solar Eclipses),
  downloaded 2026-11-08 from https://eclipse.gsfc.nasa.gov/SEhelp/deltatpoly2004.html (saved as received). The pieces for 1800 to 1972 are used.
nasa-deltat2004-tables.html: the same site's Tables 1 and 2 of Delta T (historical values; Astronomical Almanac 2006 for 1955-2005), downloaded 2026-11-08
  from https://eclipse.gsfc.nasa.gov/SEhelp/deltat2004.html (saved as received). The test (tests/test_orbit.cpp, testTimeScale) checks the polynomials against them.
