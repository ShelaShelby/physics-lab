#pragma once
// 0.46.3: the lab's clock is Universal Time (the date bar says "UT", Greenwich time), but JPL's data (Horizons / DE441, the JPL elements,
// ELP2000-82B, the moons' fits) are in TDB. Every place that turns the date shown into a position goes through tdbFromUT below.
//   TDB - UT is taken as Delta T = TT - UT (TDB and TT differ by less than 2 ms, IERS Conventions 2010 ch. 10; ignored here).
//   From 1 Jan 1972 (UTC with leap seconds): TT - UTC = 32.184 s + (TAI - UTC). TT - TAI = 32.184 s exactly; TAI - UTC from the IERS
//   leap-second table (data/time/iers-leap-second.dat, IERS Bulletin C, "updated through Bulletin 72, July 2026": 37 s since 1 Jan 2017).
//   After the last leap second the lab keeps 69.184 s: no further leap second is announced (UT itself, UT1, wanders from UTC by < 0.9 s).
//   Before 1972: Delta T = TT - UT1 from the polynomials of Espenak and Meeus, "Five Millennium Canon of Solar Eclipses" (NASA TP-2006-214141),
//   as printed on NASA's "Polynomial Expressions for Delta T" page (data/time/nasa-deltat-poly2004.html), for 1800 to 1972; decimal year
//   y = year + (month - 0.5) / 12 as that page defines it (here: from the Julian day, which gives the same value to a few hundredths of a year).
// The size: about 69 s in 2026, 13.7 s in 1800, -2.8 s in 1900. Earth moves about 2,060 km and the Moon about 70 km in 69 s.

namespace timescale {

// TT - UT in seconds at the Julian day jdUT (counted in UT). Valid 1800 onwards (before 1800: the 1800 value).
double deltaTSeconds(double jdUT);

// The same moment as a Julian day in TDB (the time scale of the JPL data).
inline double tdbFromUT(double jdUT) { return jdUT + deltaTSeconds(jdUT) / 86400.0; }

// The other way: the UT Julian day of a TDB moment (for a JPL date, e.g. in the tests). Exact except within Delta T of a leap second.
inline double utFromTDB(double jdTDB) { const double u = jdTDB - deltaTSeconds(jdTDB) / 86400.0; return jdTDB - deltaTSeconds(u) / 86400.0; }

// Unix milliseconds (the lab's UT clock) of a TDB Julian day, rounded to the millisecond.
long long unixMsOfTDB(double jdTDB);

}  // namespace timescale
