[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/nssdc/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

NASA NSSDC planetary fact sheets (D. R. Williams), https://nssdc.gsfc.nasa.gov/planetary/factsheet/<planet>fact.html, downloaded 2026-11-08, saved as received.
Used for: "Surface acceleration (eq.)" / "Acceleration (eq., 1 bar)", what you would feel at the equator (gravity minus the spin), src/orbit/solar_data.cpp feltMS2 (0.46.3).

[0.77.0] joviansatfact, moonfact, plutofact (and the saturnian and neptunian satellite sheets, restored: they were listed but missing), read 2026-11-23:
the moons', the Moon's, Pluto's and Charon's geometric albedos for the maps' brightness (tools/map_albedo_scale.py, src/render/planet_maps.cpp mapAlbedo).
