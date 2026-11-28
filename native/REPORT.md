# 0.96.0: the outside review of 28 Nov 2026, parts A to G

## What changed
- A: the 0.95.0 full-test failures are fixed. B: every lab opens in its Most realistic settings (app/realistic.cpp). Old settings switch once, at the first start of 0.96.0 (prefs.realest). First-start waits on the build machine: orbit to atom 5,019 ms, atom to molecule 38 ms, molecule to black hole 1 ms, black hole to orbit 3 ms.
- C: Jupiter is lit by Dyudina et al. 2016 Eq. (1) with Mayorga et al. 2016's Cassini phase curves. Saturn uses the Barkstrom law of Dyudina Table 3, red row. Both are in kPlanetFS with the CPU twin render/giant_photometry.h. Uranus and Neptune are unchanged. New Hard scene orbit.jupiter_limb. blackhole.over_poles (1 % low 639 fps) moved to All scenes.
- D: the rotation spectrum's Line strength / Absorbance control. Absorbance is each line times nu (1 - exp(-c2 nu / T)), from the saved HITRAN definitions page.
- E: hashPhase set only Earth's Moon, before 1 Jan 1800 12:00 TDB and after 1 Jan 2100 12:00 TDB. The 13 other moons have used JPL's mean elements there since 0.88.0. The Moon there is now on a precessing ellipse fitted to ELP2000-82B (moon_elp.h, tools/fit_moon_ellipse.py). Its worst error is 2.416 deg and 15,324 km against ELP over 1800-2100, and 2.20 deg and 14,027 km against Horizons at 444 dates from 1600 to 2300. The card states both. hashPhase is removed. No eclipses are drawn outside 1800-2100.
- F: Lande g uses g_L = 1 - m_e/m_nuc (Steck, "Sodium D Line Data" Eqs. (19) and (21), after Bethe & Salpeter). The belts.h Oort comment now names Vokrouhlicky, Nesvorny & Dones 2019.
- G: four searches, saved to the sources repo, with ROADMAP lines. Ring phase law: only Deau 2009 (opposition only). Earth: NEO's MODIS cloud fraction, not an albedo. Debrecen: the DPD row format only. Milky Way: GAMBONS (Masana et al. 2021).

## Tests run
- Unit: orbit 3138/0, moon 33/0, moon_real 65/0, atom 12409/0, rotation 476/0, app 202,599/0, analysis (the e/m table) passes. check_sources ok (819 lines, 100 extracts).
- UI by hand: check_orbit_ui review085 35/0 and planets 25/0, check_molecule rotation 11/0. All 13 ui groups on the part C build: 12 passed. ui_molecule failed only because that binary predates part D's control.
- `check.sh job`: 18/18 and ui_smoke passed. ui_orbit, ui_orbit_card and ui_molecule on the final build: all passed.

## Not done
- Needs pc_tests.ps1 on the owner's PC: orbit.jupiter_limb, the first-start waits, the giants' GPU-against-CPU pictures and cardstark on the Radeon.
- Doubtful, not used: Saturn's blue row (0.44 um) of Dyudina Table 3 integrates to a spherical albedo of 0.453. The paper's Table 4 says 0.39.
- HITRAN's line lists need an account (the API answered 404), so there is no test against HITRAN's relative heights. NIST ASD lists no Lande g for atoms lighter than K. The test uses Na 3P from Arimondo et al. 1977, as given by Steck.

## References changed on purpose
- B: check_025 and check_shell_state start from the 0.95.0 choices. check_blackhole disc and gasrow set the old rows. check_molecule sets the ball style and expects "real" after damaged settings. test_review085 R1 reads realistic.cpp. The check_orbit_ui planets check runs in 2D. test_app has the new benchmark lists.
- E: test_orbit expected "simple ellipse" and "made-up starting angle". test_moon_real expected Earth at the barycentre with no phase. Both now expect the fitted ellipse.
- F: test_zeeman now expects the normal triplet at +-g_L, the Na D patterns, the 2P closed form and the weak and strong limits with g_L. test_analysis's e/m table expects the slope g_L mu_B/hc (cadmium: 1 - 4.8e-6).
