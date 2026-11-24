# 0.81.0: Film's disc made better (PLAN_DISC.md D4): soft edges, partly see-through, streaks; all labelled as the lab's own

## What changed
- Read: CLAUDE.md, REPORT 0.80.1, ROADMAP (Black Hole Lab), PLAN_DISC.md sections 5, 6, 7, 9, disc078_film.txt, disc079_real_feeding.txt; by grep and line ranges looks.h/.cpp, gas.h StreakField, lab.cpp discLight and discGeom, shaders.h kKerrSkyFS and kKerrFS, vk_blackhole.cpp, blackhole_view.cpp (toneCompare, sky checks, PLBHFILM), sky_card.cpp, discproto.cpp film mode, test_blackhole.cpp testLooks, bench_scenes.cpp, benchmark.cpp; J15 App. A.6 in the saved original.
- looks.h/.cpp: filmEdge (smoothstep fade over 0.12 in ln r inside 9.26 and 18.70 M), filmTau (1.5 S edge), filmCover (1 - exp(-tau)), layFrontToBack. LookRule gets fade and tau (Film only). J15's numbers are unchanged. Film's exposure rule is unchanged.
- GPU: kKerrSkyFS (with the streaks' S) and kKerrFS (S = 1) scale each crossing's cover by 1 - exp(-tau). The existing front-to-back laying now lets the far side, the thin rings and the sky show through. kKerrTableCS is not touched.
- Text: the Film panel ("Ours"), the chip line, "What is real" and the guide say what is the lab's own and what is not published (texture, Houdini volume, lens flare). The guide's J15 App. A.6 facts are added to the J15 extract (two blocks, checked against the original). The changelog has an entry. VERSION 0.81.0.
- Benchmark: Hard scene blackhole.film_soft (Film, paper's view, streaks at 10^7, turning). No new wait. docs/blackhole/disc081_film.png, disc081_fades.png and .txt come from discproto film081.
- The look: no hard rims, depth (the lower ring shows through), light spread across the disc 1.70 in ln Y against 0.88. At the same exposure the disc is darker (a third of the light: cover 0.78 at S = 1). The fade stays 0.12: 0.06 and 0.2 are not clearly better.

## Tests run
- blackhole_physics: 1083 checks, 0 failed. New testFilmSoft (18 checks): fade value, slope and ends, cover closed form, two-layer algebra, conservation, tau -> infinity gives the opaque colour to the bit. With PL_SOURCES_DIR: J15 16 of 16 blocks in the original.
- check_blackhole.py sky (48 checks, 0 failed). PLBHFILMSOFT: paper's view 2 of 13,936 pixels over 8 levels (2 over 1 level); 3 degrees below the plane 1 of 13,251. Far side seen through: 125 and 95 pixels.
- check_blackhole.py precision (81 checks, 0 failed). PLBHFILM: 0 of 55,891 pixels more than 1 level from the laid colour. Seen through: 663 pixels. Glow within 1 level.
- check.sh job: quick 17 of 18 at first. app_settings failed on the benchmark id list, which lacked blackhole.film_soft. Fixed, then app_settings passed and ui_smoke passed. The benchmark run with the new scene alone works (CPU path here, as film_tilt). check_sources.py: no J15 problem (the rest of its list is files missing from the partial checkout).
- On 0.80.1 the new unit tests do not compile (the functions are new). The Python checks need lines and fields 0.80.1 does not print. They were not run on 0.80.1.

## Not done
- The CPU table fallback still hides the stars behind the whole disc (ROADMAP). Its picture has no streaks (the existing ROADMAP line).

## References changed on purpose
- PLBHFILM: "one colour over the whole disc" is now "the laid colour within 1 level", read from the disc-only picture because the sky now shows through.
- Benchmark: 39 Hard scenes and 55 in All (were 38 and 54). The planned-time limit is 5.6 min (static_assert, test_app 336 s; was 5.5 and 330). 335.1 s is planned.
- Floors in the new checks set after the first run: inner pixels > 10,000 (about 14,000 at 640 x 400) and at least 50 seen-through pixels.
