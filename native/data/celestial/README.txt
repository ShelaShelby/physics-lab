Celestial mechanics sources (Orbit Lab, Data panel: Lagrange points, Lambert transfers; 0.92.0)
================================================================================================

The originals are in the private repo ShelaShelby/physics-lab-sources, data/celestial/ (native/data/SOURCES.md lists them with their sha256). Here are the extracts the tests read.

izzo_2015.extract.txt          D. Izzo, Revisiting Lambert's problem (arXiv:1403.2705): the algorithm the solver follows (Eq. 14 to 23 and 30, Algorithm 1 and 2).
                               The paper's own numbers are not a test case (it prints no worked example). Its "1F2" in Eq. (20) is a misprint of the hypergeometric function 2F1(3, 1; 5/2; S1):
                               the tests check the series against Eq. (18).
tatum_ch16.extract.txt         J. Tatum, Celestial Mechanics, chapter 16: the collinear points (16.1.9 to 16.1.23), the table of least distances, the stability of L4 and L5 (16.2.26, 16.2.27).
                               The notes number the points differently from the usual names (his L3 is the inner point), and take x of his L2 and L3 as a distance counted the other way:
                               tests/test_celestial.cpp says how it maps them. One printed number is doubtful: n = 0.080 464 5 w for the Sun and Jupiter; 16.2.27 with his own q = 1047.35 gives 0.0804641 w.
poliastro_lambert.extract.txt  The two single-revolution worked cases of the poliastro documentation page (positions, time, expected velocities, and the page's own solver's printed v2).
                               The page does not name the books they come from, and does not print mu: the tests use 398600.4418 km^3/s^2, at which the lab's solver reproduces the page's printed 8-digit v2.

Where the facts come from: About, "Where the facts come from".
