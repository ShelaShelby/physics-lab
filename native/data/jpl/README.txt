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
