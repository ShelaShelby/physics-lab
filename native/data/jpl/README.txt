JPL Horizons vector tables for the Real planets (Orbit Lab) -- test reference only
===============================================================================

Downloaded on 2026-10-03 (by the owner) from the NASA/JPL Horizons API, https://ssd.jpl.nasa.gov/api/horizons.api
One request per body. The URL pattern (COMMAND is the body code):

  https://ssd.jpl.nasa.gov/api/horizons.api?format=text&COMMAND='<code>'&EPHEM_TYPE=VECTORS&CENTER='@0'
      &START_TIME='1800-01-01 12:00'&STOP_TIME='2100-01-01 12:00'&STEP_SIZE='1y'
      &REF_PLANE=ECLIPTIC&OUT_UNITS=KM-S&VEC_TABLE=2

Parameters: COMMAND, EPHEM_TYPE=VECTORS, CENTER=@0 (Solar System Barycenter), START_TIME=1800-01-01 12:00,
STOP_TIME=2100-01-01 12:00, STEP_SIZE=1y, REF_PLANE=ECLIPTIC, OUT_UNITS=KM-S, VEC_TABLE=2.
(The exact quoting of the request is not recorded in the files; the parameters above are what the files' headers show.)

Body codes -> file:
  10  Sun                          horizons-sun.txt
  1   Mercury Barycenter (199)     horizons-mercury.txt   (Mercury has no moons: the same as the planet centre)
  2   Venus Barycenter (299)       horizons-venus.txt     (the same: no moons)
  3   Earth-Moon Barycenter        horizons-emb.txt
  4   Mars Barycenter              horizons-mars.txt
  5   Jupiter Barycenter           horizons-jupiter.txt
  6   Saturn Barycenter            horizons-saturn.txt
  7   Uranus Barycenter            horizons-uranus.txt
  8   Neptune Barycenter           horizons-neptune.txt
  9   Pluto Barycenter             horizons-pluto.txt     (0.32: downloaded in a cloud session on 2026-11-05 with the same
                                                          parameters; the Pluto system is the tenth body of Real)
  1;  Ceres                        horizons-ceres.txt     (0.36: downloaded in a cloud session on 2026-11-06 with the same
  136108; Haumea                   horizons-haumea.txt     parameters; COMMAND is the small-body number with a ';'. Small-body
  136472; Makemake                 horizons-makemake.txt   orbit solutions JPL#48, #132, #130, #103, integrated by Horizons with
  136199; Eris                     horizons-eris.txt       the SB441-N16 perturbers; the four ride in Real as test particles)
  10, 1  Sun, Mercury every 7 hours  horizons-sun-7h.txt, horizons-mercury-7h.txt (0.36 stage 4: the same parameters but
                                   START 2026-01-01 12:00, STOP 2026-04-01 12:00, STEP 7h; they test the places between whole days)

Each file: 301 rows, 1 January 12:00 TDB of every year 1800 to 2100, position (km) and velocity (km/s) in the
ecliptic and mean equinox of J2000, centre = Solar System Barycenter. The headers say {source: DE441}.
DE441 is Horizons' default ephemeris (it spans -13200 to +17191); it is NOT the DE440 that the lab's GM values come from.

Header lines of horizons-emb.txt, copied unchanged:


*******************************************************************************
Ephemeris / API_USER Sat Oct  3 07:34:05 2026 Pasadena, USA      / Horizons
*******************************************************************************
Target body name: Earth-Moon Barycenter (3)       {source: DE441}
Center body name: Solar System Barycenter (0)     {source: DE441}
Center-site name: BODY CENTER
*******************************************************************************
Start time      : A.D. 1800-Jan-01 12:00:00.0000 TDB
Stop  time      : A.D. 2100-Jan-01 12:00:00.0000 TDB
Step-size       : 1  calendar years
*******************************************************************************
Center geodetic : 0.0, 0.0, 0.0                   {E-lon(deg),Lat(deg),Alt(km)}
Center cylindric: 0.0, 0.0, 0.0                   {E-lon(deg),Dxy(km),Dz(km)}
Center radii    : (undefined)
Output units    : KM-S
Calendar mode   : Mixed Julian/Gregorian
Output type     : GEOMETRIC cartesian states
Output format   : 2 (position and velocity)
Reference frame : Ecliptic of J2000.0
*******************************************************************************
JDTDB

The files are unchanged: they are exactly as downloaded (no edits, no line-ending conversion).
native/tools/make_jpl_data.py checks them and writes native/tests/reference/jpl_table.inc and native/src/orbit/jpl_epoch.inc.

0.50 (11 Nov 2026):
naif_pck00011.tpc
    Saved as received from https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc (NAIF generic PCK; the IAU WGCCRE 2015 report, Archinal et al.
    2018, CMDA 130, 22, as corrected, and the 2009 values where 2015 changed nothing: the Moon's). The Moon's model is typed in src/orbit/moon_orientation.h;
    the 8 mapped moons are read by tools/make_iau_rotation.py into src/orbit/iau_rotation.inc.
horizons-moon-subearth.txt
    JPL Horizons API, target 301, observer 500@399, quantity 14 (ObsSub-LON, ObsSub-LAT; MOON_ME, east positive), 7 TT dates 1800-2100. The app's
    drawn sub-Earth point is within 0.006 deg of it (tests/test_orbit.cpp, testMoonOrientation050).
horizons-io-subjupiter.txt
    JPL Horizons API, target 501, observer 500@599, quantity 14 (IAU_IO, WEST positive), the same 7 dates. The drawn sub-Jupiter point is within 0.47 deg
    (the app's Io place is a fit with a worst error of 4,514 km = 0.61 deg seen from Jupiter; testIauMoons050).

[0.88.0] sats_mean_elements_2026-11-23.extract.txt: the lines of JPL's "Planetary Satellite Mean Elements" table that src/orbit/moon_jpl_elements.inc and
tests/test_orbit.cpp (testMoonMeanElements) use: the warning, the definition, the column list and one row for each of Earth's Moon and the 13 big moons.
The page (saved in the private sources repo, data/jpl/) states no numerical error for the table.

JPL planetary satellite GM values (0.92.0)
------------------------------------------
sats_phys_par.extract.txt  The rows of JPL's "Planetary Satellite Physical Parameters" page (https://ssd.jpl.nasa.gov/sats/phys_par/, saved in the sources repo as data/jpl/sats_phys_par_2026-11-23.html)
                           for the Moon, Io, Europa, Ganymede, Callisto, Titan and Triton, and the page's table "Planet and Pluto GM Values" for Earth, Jupiter, Saturn and Neptune. These are the GM
                           values (km^3/s^2) of the Lagrange pairs in src/orbit/lagrange.cpp; tests/test_celestial.cpp reads them again from this file.

