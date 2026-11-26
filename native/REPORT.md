# 0.88.0: Orbit Lab accuracy: the big moons outside 1800-2100, Pluto's map

## What changed
- Part A: outside 1800 to 1 Jan 2100 12:00 TDB the 13 big moons follow JPL's table of mean elements (saved data/jpl/sats_mean_elements_2026-11-23.html; rows in orbit/moon_jpl_elements.inc; satMeanState, satPlace in orbit/moon_sats.cpp). Size, shape, tilt, plane (the table's Laplace pole) and period are JPL's. Inside 1800-2100 nothing changes. Card, date note and About say what the code does (no "made-up places").
- JPL states no error for the table. Its page only warns "not intended for ephemeris computation". The test bound is built from the table's printed rounding plus the lab's own measured error for the same kind of model (satWorstErrKm), not chosen.
- Reading the table: P is the period of the mean anomaly for Phobos, Io, Europa, Ganymede and Callisto, and of the mean longitude for the other eight. The page prints no signs for the precession periods. The five anomaly rows' signs are the ones that match Horizons' mean motion. The rest follow oblateness theory (apsis forward, node backward, Triton's node forward). Rounded periods turn the angle along the orbit by a full turn after satMeanDriftYears (Phobos about 5 years, Io thousands): the card says so.
- Doubtful rows, used as printed except the mean longitude of four (Rhea, Titan, Enceladus, Triton: the Horizons fit's value at J2000, moon_sats_data.inc). Horizons (tests/reference/moon_sats_table.inc, JD 2451545.0) against JPL: Rhea mean longitude 157.4 deg off, Titan 156.9, Enceladus 5.9, Triton 4.3. Node in Jan 2026 (Horizons positions against JPL's node and rate): Rhea 98.1 against 232.1 deg, Titan 13.3 against 65.0. Triton inclination 156.82 against 157.3, period 5.8766 d against 5.8792. Deimos period 1.262441 d against 1.2625. Phobos period is 4.8e-3 rad/day off.
- The Moon: JPL's table has a row for it (ecliptic frame, no pole, no stated error). Left as it was (simple ellipse outside ELP2000's 1800-2100), because no error is stated to test a mean ellipse against. ROADMAP says how to fix it.
- Part B: Pluto's bytes are linear light (USGS page: "linearly stretched to 8bit (1-255) using the input range of 0.03344 to 0.99981"). MapInfo::linear, mapDecode, a UNORM texture on the GPU. Map mean 0.1606 to 0.3970, factor 7.6678 to 2.1411, clipped pixels 76 to 36 %. Other rows of map_albedo_scale.txt are unchanged. Extracts of both pages, SOURCES.md, About, changelog, VERSION 0.88.0.

## Tests run
- orbit_tests: 2248 checks, 0 failed. Before the code: 31 failed (every new check: rows, plane, text, card, Pluto decode and mean).
- check.sh job: see the last line of this section.
- check_sources.py: both new extracts found. With the sources checkout it lists 110 files there that are not in the manifest (already so before this job).

## Not done
- Not run: Pluto on the owner's GPU (a new texture format) and the planets' GPU-vs-CPU picture on a real driver (pc_tests.ps1 on the new release).
- The doubtful rows above need a newer table or the SAT/URA kernels (ROADMAP). Big moons still cast no shadow on their planet outside 1800-2100 (ROADMAP).

## References changed on purpose
- test_orbit.cpp: fullPhaseAlbedo reads a map through mapDecode (Pluto linear); Pluto's factor 7.6678 to 2.1411 (planet_maps.cpp, map_albedo_scale.txt). The jump note's old sentence "simple orbits that start at made-up places" is replaced (the "only from 1800 to 1 Jan 2100" checks stay). tests/planet_picture.cpp decodes through mapDecode.
