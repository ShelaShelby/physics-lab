// JPL "Planetary Satellite Mean Elements" (https://ssd.jpl.nasa.gov/sats/elem/, read 23 Nov 2026; saved data/jpl/sats_mean_elements_2026-11-23.html, the lines used are
// in native/data/jpl/sats_mean_elements_2026-11-23.extract.txt). Each field is the table's printed text, so the test can read its rounding. Epoch 2000-01-01.5 TDB (JD 2451545.0).
// { name, frame, a km, e, omega deg, M deg, i deg, node deg, P days, P apsis yr, P node yr, pole RA deg, pole Dec deg (Laplace frames only), kind, apsis sign, node sign, L from fit }
// The page prints the precession periods without a sign, and the column P is called "sidereal period" for every moon. Read against the lab's Horizons fit (tests/test_orbit.cpp,
// testMoonMeanElements): for Phobos, Io, Europa, Ganymede and Callisto the P is the period of the MEAN ANOMALY (kind 'A': the mean longitude's rate is 2 pi / P plus the two precession rates);
// for the other eight it is the period of the MEAN LONGITUDE (kind 'S'). The signs of the Galilean moons and Phobos are the ones that make that match; the rest are the signs celestial
// mechanics gives for a planet's oblateness (the apsis goes forward, the node backwards; Triton goes round backwards, so its node goes forward).
// "L from fit" = 1: JPL's mean longitude at the epoch (omega + M + node) is further from Horizons than the printed rounding and the lab's fit error allow (Rhea and Titan 157 deg, Enceladus 5.9, Triton 4.3),
// so the mean longitude of the lab's Horizons fit at J2000 (moon_sats_data.inc) is used for these four; listed in REPORT 0.88.0.
    {"Phobos", "Laplace", "9375.", "0.015", "216.3", "189.7", "1.1", "169.2", "0.3187", "1.1", "2.3", "317.7", "52.9", 'A', -1, 1, 0},
    {"Deimos", "Laplace", "23457.", "0.000", "0.0", "205.0", "1.8", "54.3", "1.2625", "0.0", "56.2", "316.6", "53.5", 'S', 1, -1, 0},
    {"Io", "Laplace", "421800.", "0.004", "49.1", "330.9", "0.0", "0.0", "1.762732", "1.333", "0.000", "268.1", "64.5", 'A', -1, -1, 0},
    {"Europa", "Laplace", "671100.", "0.009", "45.0", "345.4", "0.5", "184.0", "3.525463", "1.394", "30.202", "268.1", "64.5", 'A', -1, -1, 0},
    {"Ganymede", "Laplace", "1070400.", "0.001", "198.3", "324.8", "0.2", "58.5", "7.155588", "68.301", "137.812", "268.2", "64.6", 'A', 1, -1, 0},
    {"Callisto", "Laplace", "1882700.", "0.007", "43.8", "87.4", "0.3", "309.1", "16.690440", "277.921", "577.264", "268.7", "64.8", 'A', 1, -1, 0},
    {"Enceladus", "Laplace", "238400.", "0.005", "119.5", "57.0", "0.0", "0.0", "1.370218", "2.916", "0.000", "40.6", "83.5", 'S', 1, -1, 1},
    {"Rhea", "Laplace", "527200.", "0.001", "44.3", "31.5", "0.3", "133.7", "4.517503", "33.939", "35.775", "40.6", "83.5", 'S', 1, -1, 1},
    {"Titan", "Laplace", "1221900.", "0.029", "78.3", "11.7", "0.3", "78.6", "15.945448", "346.680", "687.370", "36.4", "84.0", 'S', 1, -1, 1},
    {"Titania", "equatorial", "436298.", "0.002", "184.0", "68.1", "0.1", "29.5", "8.705869", "579.928", "1644.649", "", "", 'S', 1, -1, 0},
    {"Oberon", "equatorial", "583511.", "0.002", "132.2", "143.6", "0.1", "76.8", "13.463237", "158.604", "192.798", "", "", 'S', 1, -1, 0},
    {"Triton", "equatorial", "354800.", "0.000", "0.0", "63.0", "157.3", "177.7", "5.8792", "675.2", "299.4", "", "", 'S', 1, 1, 1},
    {"Charon", "equatorial", "19600.", "0.000", "0.0", "304.1", "0.0", "0.0", "6.387222", "-", "-", "", "", 'S', 1, -1, 0},
