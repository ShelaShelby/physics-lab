Comets (0.78.0): the sources of the calculated comet tails (src/orbit/comet_tail.h).

Saved originals are in the private repo ShelaShelby/physics-lab-sources at data/comets/ (listed in native/data/SOURCES.md):
- msfc_solarwind_2026-11-24.html: NASA Marshall Space Flight Center, "The Solar Wind", https://solarscience.msfc.nasa.gov/SolarWind.shtml, read 2026-11-24.
  Used: the solar wind's speed, about 400 km/s (300 km/s over streamers, 800 km/s over coronal holes). Extract: msfc_solarwind_2026-11-24.extract.txt.

Not saved: M. L. Finson and R. F. Probstein, "A theory of dust comets. I. Model and equations", ApJ 154, 327-352 (1968), doi:10.1086/149761.
The ADS scan (https://articles.adsabs.harvard.edu/pdf/1968ApJ...154..327F) answered "500 Internal Server Error" on 2026-11-24. The model the lab uses
(grains on Kepler orbits around a Sun of GM (1 - beta) GM, syndynes and synchrones) is checked against conservation laws and the Kepler solution, not
against a figure of the paper.
