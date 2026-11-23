# 0.77.0: honest pictures (fixes from an outside physics review)

## What changed
- Item 1, brightness: each map times one factor (kPlanetFS uAlbedoK; CPU twin shading.h albedoScaled) so the fully lit ball has the measured V geometric albedo. A Lambert ball gives p = 2/3 A (Madhusudhan & Burrows 2012, Sec. 3.1). The Moon's McEwen law is flat at full phase: p = A. A pixel past 1 keeps its hue. The factor is then raised until the clipped ball gives p.
- Feasibility (tools/map_albedo_scale.txt), as map mean Y / p / factor: Mercury 0.109/0.142/1.96, Earth 0.093/0.434/1 (ground only), Mars 0.134/0.170/1.92, Jupiter 0.366/0.538/2.92, Saturn 0.559/0.499/1.36, Uranus 0.573/0.488/1.28, Neptune 0.514/0.442/1.29, Moon 0.482/0.12/0.249, Pluto 0.161/0.52/7.67, Charon 0.251/0.42/2.73, Io 0.153/0.62/7.57, Europa 0.313/0.68/3.26, Ganymede 0.075/0.44/10.4, Callisto 0.049/0.19/6.02, Rhea 0.160/0.70/6.55, Triton 0.326/0.72/3.32, Phobos 0.276/0.07/0.380.
- Factors outside 1/4..4 checked: the USGS mosaics are display products (saved pages: Io "empirically adjusted in brightness and contrast", Pluto "linearly stretched to 8bit"). Only their mean is now measured. The OPAL maps did not match under Lambert (2/3 of p): now scaled. Not reachable: Neptune 0.434, Europa 0.583, Rhea 0.587, Triton 0.590 (p above 2/3, the Lambert limit). Clipped pixels: Neptune 76 %, Pluto 76 %, Io 58 %, Jupiter 40 %.
- Item 2, Earth's clouds: NASA Blue Marble 2002 cloud layer (terms: "freely available"), in Earth's map alpha. Blend in linear light with cover = byte/255: it rebuilds NASA's own composite best (ocean rms 0.10 against 0.24 blending the bytes). Fully lit Earth: 0.208 with clouds and air (ground 0.062) against 0.434 measured. New Hard scene orbit.earth_clouds (benchmark plan 310.8 s, limit 5.2 min).
- Item 3: PIA07782's page says planetocentric: Jupiter unchanged. Mars (USGS) planetocentric. Blue Marble's page does not say (at most 0.19°). OPAL maps were already converted. Item 4: far markers drawn after balls and rings, hidden behind a ball, dimmed by the ring's drawn transmission (orbit/far_occlusion.h).
- Item 5: Bending text (spinning holes use closed forms too since 0.55.9), clump and patches off and marked as models. Row out of date: Stars Full also brightens with zoom (0.45 to 1). Item 6: two simplifications in "What is real", sources saved (Li et al. 2005 names both; Shimura & Takahara 1995 by its Crossref record only).

## Tests run
- orbit_tests: 2093 checks, 0 failed. testAlbedo077 fails 16 of 16 bodies with factor 1 (the 0.76.0 drawing). testFarOcclusion077: behind the ball 0; B ring = 1 - cover (1 - mean exp(-tau/mu)) of Cassini's data within 1e-4.
- check_orbit_ui.py planets (new, ui_orbit_card): GPU vs CPU twin tests/planet_picture.cpp, pixels over 8 levels: Jupiter 0.07 %, Mars 0.66 %, Moon 0.31 %, Europa 0.32 %, Pluto 0.24 %, Earth 0.27 %. app_tests: 0 failed.
- check.sh job: passed (quick 18 of 18 in 206 s, ui_smoke 7 s). By hand: check_orbit_ui.py rings, flyby, cardlines, ringslant, airedge, nightside, planets: 50 checks, 0 failed. check_blackhole.py realest 2, sky 42, precisioncard 19 checks: 0 failed. check_sources.py: ok.

## Not done
- Earth stays at half its measured brightness: no cloud albedo map (ROADMAP). Pluto's linear bytes still decoded as sRGB (ROADMAP). Far markers still drawn over orbit lines.
- The disc's limb darkening and colour correction are named, not modelled (ROADMAP). No GL path exists any more: the shader change is in the one GLSL text the Vulkan path compiles.

## References changed on purpose
- test_app.cpp: 36 Hard scenes, 52 in All, plan up to 312 s (orbit.earth_clouds). check_6b.py: orbit.earth_clouds's text. No picture reference file changed (none holds planet brightness).
