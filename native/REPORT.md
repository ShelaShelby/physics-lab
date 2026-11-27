# 0.94.0: Orbit Lab, the Sun's surface (dark sunspots, granulation, NASA's photo of the day)

## What changed
- Read: CLAUDE.md, REPORT 0.93.0 and 0.85.0, ROADMAP (Orbit Lab), PLAN_REVIEW R2/R3, data/sun/README.txt and spots extracts, sunspots.h/.cpp, make_sunspot_table.py, sun_limb.h, renderer2d.cpp sunBall, renderer2d_vk.cpp, vk_draw2d.cpp upload, shaders.h kPlanetFS, orbit_view.cpp (the Sun, zoomCapFor), card_text.cpp, camera.h, planet_maps.cpp, test_review085.inc, planet_picture.cpp, check_orbit_ui.py, benchmark.cpp, bench_scenes, test_app.cpp, pc_tests.ps1, TESTS.md, DECISIONS (Hard list rule). Closest zoom on the Sun: 3D stops at 1.5 radii (disc radius 1.08 x window height: 1,480 px on the owner's 2560 x 1369, granules about 2.5 px); 2D goes to zoom 5.5 (radius about 160,600 px at 1280 x 800).
- Sources (private repo, data/sun/): umbra against wavelength Maltby et al. 1986 Table 3; penumbra Mathew et al. 2007 Table 2 (676.8 nm only); area split Jha, Mandal & Banerjee 2019 (q about 6, 4.5 below 100 millionths); granulation Hathaway et al. 2015 (1.2 Mm, 10 min) and Mathew, Zakharov & Solanki 2009 Table 2 (rms 0.215/0.158/0.111). SDO terms allow the use with the credit line. Group shape: SRS rows give spot count, LL and McIntosh class, RGO rows only areas (umbral area to 1976). The app draws one round spot per group and says so.
- Part 1: a PNG of one day is 79 KB (512 px) to 4.6 MB (4096). Since 6 Dec 2010 (Helioviewer's first HMI picture) that is 0.46-27 GB: not built in. The app downloads the shown day's 2048 px picture (1.3 MB) on a worker (WinHTTP; Linux has no HTTP library: drawn only), caches it, and draws meanwhile. The saved HMI grey is linear (edge profile = Neckel & Labs at 617.3 nm within 0.004).
- GPU: kPlanetFS uMode 4 fills the spots, draws granulation (3D Voronoi, -F1, grid 0.675 Mm so the spectrum peaks at 1.2 Mm) and the photo as a texture (slot 31). CPU twin: render/sun_surface.h, orbit/sun_photo.cpp, planet_picture sun. Card, About, changelog updated.
- Benchmark: Hard scene orbit.sun_surface; waits response.sun_photo.network and .cache (once each). blackhole.moving_gas to All scenes by the 1.5 x rule (1 % low 650.9 fps, bench_0.85.2). Planned 290.9 s.

## Tests run
- orbit_tests with PL_SOURCES_DIR: 3101 checks, 0 failed (testSun094 new: it does not build on 0.93.0). Spot means match the sources to 1e-4. Spectrum peak 1.23 Mm. rms contrast 0.121/0.165/0.215 (source 0.121/0.165/0.216). Disc mean = Neckel & Labs within 1e-4. Correlation 10 min apart 0.007. Photo observer vs IAU frame: 0.06 deg. Region 3664 found at Carrington 347.8, -19.8 (SRS 348, -19).
- check_orbit_ui.py sun094 (new, 22 s): GPU vs CPU 780 of 334,800 pixels over 8 levels (granulation, and a photo in the cache), 93 of 115,920 (whole disc with photo). review085: all 5 views pass (Sun 93 of 115,920). Validation: 0 errors.
- app_tests 202,477 checks, 0 failed. check_sources.py ok (813 lines, 96 extracts). check.sh job: quick 18 of 18, ui_smoke passed. Benchmark: the new scene and waits ran here (software Vulkan: no speed meaning).

## Not done
- Penumbra against wavelength: not found (Kjeldseth-Moe & Maltby 1969: ADS 504; Solanki 2003: whole spectrum only; Albregtsen & Maltby 1981: umbra only; web search). Mathew's 676.8 nm value is used in every channel; card says so.
- The umbra is the core of large spots (Maltby): small umbrae are drawn too dark. RGO's measured umbral areas (1874-1976) are not used yet. Both in ROADMAP.
- Near the disc centre the brightest granules pass the screen's white and clip (owner decides, ROADMAP). Needs pc_tests.ps1 on the owner's PC: orbit.sun_surface at 255 Hz, response.sun_photo, the download and the photo on the Radeon (3c.planets.sunphoto). WinHTTP is built only by the Windows run.

## References changed on purpose
- test_app: 29 Hard and 73 scenes (ids list: orbit.sun_surface in, blackhole.moving_gas moved), 49 waits. check_orbit_ui review085: the Sun's twin reads the new PLSUN fields (spots filled).
