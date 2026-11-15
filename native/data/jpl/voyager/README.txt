[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/jpl/voyager/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

The Voyagers' places before 30 Sep 2026 (0.49).

horizons_voyager{1,2}_1990_2026_yearly.txt: JPL Horizons (ssd.jpl.nasa.gov/api/horizons.api), read 11 Nov 2026:
  COMMAND='-31' (Voyager 1) and '-32' (Voyager 2), EPHEM_TYPE=VECTORS, CENTER='500@10' (the Sun's centre), START_TIME='1990-01-01',
  STOP_TIME='2026-01-02', STEP_SIZE='1y', VEC_TABLE='1', OUT_UNITS='AU-D', REF_PLANE='ECLIPTIC', REF_SYSTEM='ICRF', CSV_FORMAT='YES'.
  One row on 1 January 00:00 TDB of every year 1990 to 2026: x, y, z in au, ecliptic and mean equinox of J2000 (the frame of the app's sky).
horizons_voyager{1,2}_2026-09-30.txt: the same for 30 Sep 2026 12:00 TDB, the date of the app's kProbes table (sky_data.inc):
  Voyager 1 171.912 au, longitude 256.772, latitude 35.158; Voyager 2 144.081 au, 290.718, -38.504 (the app's table: 171.89, 256.769, 35.159 and
  144.06, 290.716, -38.505: within 0.02 au).
tools/make_voyager_data.py reads the yearly files and writes src/orbit/voyager_data.inc. The app joins the rows with straight lines and ends at its
kProbes place on 30 Sep 2026; after that it goes on in a straight line at the kProbes speed, as before 0.49.

nasa_voyager1_read_2026-11-11.html: NASA Science, Voyager 1 (science.nasa.gov/mission/voyager/voyager-1/): "On Feb. 17, 1998, Voyager 1 became the most
distant human-made object in existence when, at a distance of 69.4 AU from the Sun, it overtook Pioneer 10." Used by the caption and by tests/test_orbit.cpp.
