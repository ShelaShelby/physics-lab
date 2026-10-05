# Roadmap — native Physics Lab

Plain list of what is done and what is planned. No dates. Nothing is promised that is not on this list.

## Done
- 0.13 stage 3 (of 4), "GPU update": the 90 % outline's 64^3 density grid and 90 % level on the GPU (compute, no CPU wait: job + fence, the outline appears a frame or two later) and the Slice contour's density values the same way (a frame behind the cut); exact orbital normalisation (a physics fix on purpose, closed form, every orbital to 1e-10); measured first; CPU functions stay as reference and fallback; Performance card switch, report line, scenes 29 and 30 with the CPU path; the speed-up is EXPECTED, not measured — done in code and tests (the real GPU, Windows and speed to be confirmed by the owner)
- 0.13 stage 2 (of 4), "GPU update": OpenGL 4.3 with a 3.3 fallback, and the superposition's and the cloud jump's per-dot colour groups on the GPU (a compute shader; measured first); the CPU functions stay as the reference and the fallback; Performance card line and switch, benchmark report line, scenes 25 to 28 with the CPU path; tests: textbook physics check, GPU against CPU dot by dot on llvmpipe with every differing dot bounded, 3.3 fallback, glslang for the 430 shader; the speed-up is EXPECTED, not measured — done in code and tests (the real GPU, Windows and speed to be confirmed by the owner)
- 0.13 stage 1 (of 4), "GPU update": phase colours on the GPU: every cloud dot carries its colour number and the vertex shader picks the colour (a GLSL port of `atom::phaseRgb`); the old per-colour CPU scan is kept behind the Performance card's "GPU colours" switch (default On) and as benchmark scenes 23 and 24; tests: dot by dot against the old scan, glslang for every shader, old way against new way pixel by pixel on llvmpipe; the speed-up is EXPECTED, not measured; benchmark text fixes (scene 19 compares with scene 2, scene 22 says scenes 19 to 21, "15 extra") — done in code and tests (speed, the real GPU and Windows to be confirmed by the owner)
- 0.12 stage 2 (of 2), which finishes 0.12: the accuracy clean-up: what NUBASE2020 says about nuclides with no half-life number, no extrapolated or limit value shown or timed as a half-life, Kr-78 and Xe-124 stable as in NUBASE2020, 113 "~" and 16 stale superheavy half-lives replaced by NUBASE2020's values, modes listed without an intensity said to leave the odds incomplete, the unsourced magic numbers 114 and 184 labelled — done in code and tests (the Windows build and look to be confirmed by the owner)
- 0.12 stage 1 (of 2): decay chains on the chart of nuclides: the chosen nuclide's chain drawn as a trail from square to square (every way, the likeliest solid), a walk down it step by step with the lab's real random draws, the time stated as never real time, no time made up for half-lives that are only a limit or an extrapolation or not given, the four classic series tested, a chain benchmark scene, the app version is now 0.12 — done in code and tests (the Windows build and look to be confirmed by the owner)
- 0.11 stage 4 (of 4), which finishes 0.11: the chart's second height, the binding energy per nucleon (AME2020 fetched 2026-10-04 into data/ame; the peak read from the table; 1,008 estimates drawn flat), the valley of stability named as measured, the heaviest known nuclides marked, the island of stability stated as a prediction and NOT drawn (no source could be reached), a binding-landscape benchmark scene — done
- 0.11 stage 3 (of 4): the chart as a landscape: tilt from the flat chart to a fixed isometric landscape, block height = log10 of the half-life between 10^-23 s and 10^32 s (a stated, chosen scale), stable nuclides on one plateau above it, no height for unknown or extrapolated half-lives, zoom and pan, hover and click on the topmost block, a landscape benchmark scene — done
- 0.11 stage 2 (of 4): the chart of nuclides, flat and clickable: a new Atom Lab view with one tile per nuclide (3,557, the same set as NUBASE2020's ground states), coloured by how it decays, legend, magic-number lines, the chosen nuclide marked, hover tooltip, click-to-choose, arrow keys; NUBASE2020 fetched 2026-10-04 into data/nubase for spins and the kind of half-life; the app version is now 0.11; benchmark scene for the chart — done
- 0.11 stage 1 (of 4): Atom Lab, Auto photons say why when they find no jump (a complex orientation or a superposition used to leave Auto silent): once per reason, same words as the Emit/Absorb buttons; the physics question of allowing jumps from a complex state is written up with options in native/REPORT.md and left to the owner (no change of behaviour) — done
- 0.10.9 stage 2: the nucleus's shape from measured electric quadrupole moments (the IAEA recommended values, fetched 2026-10-04 into data/quadrupole: 598 ground-state rows; the file's second column is the mass number; 498 of the slider's 4,252 isotopes now take their shape from a measured moment, 11 from B(E2), 3,743 from the estimate, and the card and the readout name which; the spectroscopic moment is converted to the intrinsic one by the strong-coupling relation K = I, said in the app to be a model assumption; a missing moment is never called round: spin 0 and 1/2 have no moment whatever the shape; deformed nuclei keep their volume and still give the measured charge radius) — done in code and tests; the card's drawing is unseen and the Windows result is in native/REPORT.md.
- 0.10.9 stage 1: every nucleus at its measured size (the IAEA table of measured nuclear charge radii, fetched 2026-10-04 into data/radii: 908 isotopes of 90 elements; the nucleus is drawn as a uniform ball of radius sqrt(5/3) x the measured rms charge radius, the nucleon cloud is scaled so its protons have exactly the measured rms; where there is no value the usual rule stays and the card says so; card "Nuclear size (measured)") — done in code and tests; the card's drawing is unseen and the Windows result is in native/REPORT.md. It is a SIZE only: no shape, no halo.
- 0.10.8 stage 3: lifetimes and selection rules from NIST ASD (card "Lifetimes and jumps (NIST)": lifetime 1/ΣA of the state on screen, A and branching share of each jump with ASD's accuracy rating, the rule that forbids a jump named, metastable 2s with no invented number) — done in code and tests; the drawing is not seen yet (see OWNER-TODO). Not done, not promised: rates ASD does not have (the lab calculates none), the two-photon decay of 2s, jumps between levels of one n
- 0.10.8 stage 2: the Dirac formula's arithmetic (fix of the red Windows build of stage 1): the tests' reference was badly conditioned and used `long double`; now double-double and 80-digit typed values; the lab's correction and readout formulas rewritten without cancellation — done in code and tests; Windows CI result in REPORT
- 0.10.8 stage 1: the Atom Lab's "Relativity (Dirac levels)" setting (off by default): exact Dirac levels depending on n and j, split lines and levels in the spectrum cards, the real-spectrum card compares the lines the lab draws with NIST component by component — done in code and tests; the drawing is not seen yet (see OWNER-TODO). Not done, not promised: Dirac energies for the photon of a jump, the readouts and the quantum beats (the lab's orbitals have no spin or j), the Lamb shift and other QED, the size of the nucleus, recoil terms beyond the reduced mass
- 0.10.7 stage 2: the real NIST ASD spectrum next to the lab's (card "Real spectrum (NIST)": (a) the hydrogen-like ion the lab models, which validates it, and (b) the neutral atom, which shows its limit; data fetched 2026-10-04 into data/asd) — done in code and tests; the drawing is not seen yet (see OWNER-TODO)
- 0.10.7 stage 1: the Atom Lab's radial probability graph (card "Where the electron is found": P(r) with the peak, the average distance and the 90 % outline's reach; hidden with an explanation during a superposition) — done in code and tests; the drawing is not seen yet (see OWNER-TODO)
- 0.9.3: bug fixes — done
- 0.9.4: glowing, denser electron cloud — done
- 0.9.5: Orbit Lab: smooth Milky Way band and star mass labels — done
- 0.9.6: quarks readable on light themes — done
- 0.9.7: housekeeping (roadmap, release version, Windows warning help, "Reset to today" keeps the view) — done
- 0.9.8: 90% probability outline for the electron cloud — done
- 0.9.9: theme picker grid, clearer web history button, readable nucleus label, sources added to About — done
- 0.10: real N-body planets as an option (Planet physics: Simple / Real; Real is not the default) — done
- 0.10.1: general relativity (1PN) for the planets in Real mode (Mercury's extra turn), the Sun's wobble round the centre of mass drawn in Real mode, Milky Way band fix, secret theme names fix — done
- 0.10.3: Real planets start from JPL (Horizons data, DE441); the relativity tick box is off by default; Real stays an option until the owner has tried it — done
- 0.10.4: the real Moon (ELP2000-82B checked against Horizons; used in the Orbit Lab's Real mode: Moon position, phases on its card, Real note and About text) — done (stage 1: data and validation, stage 2: the Orbit Lab)
- 0.10.6 stage 9: the stage 8 jump test made portable (old path against new path in the same run; recorded numbers with stated tolerances instead of byte fingerprints) — done; the Windows build was red after stage 8 and is fixed by this
- 0.10.6 stage 8: the Atom Lab cloud jump made cheaper (per-dot work done once per frame; the start spread over 4 frames; output bit for bit unchanged) — done in code, speed to be confirmed on the owner's PC (audit finding 11)
- 0.10.6 stage 7: the Atom Lab's speed-risk paths added to the benchmark as 11 scenes (a tick box), never timed in the cloud, to be timed on the owner's PC — done (audit findings 10, 11, 12 made measurable; nothing optimised)
- 0.10.6 stage 6: nuclear mass from AME2020 (the spectrum lines of deuterium and tritium), independent spectrum checks (hand formula, NIST ASD), four basic orbital checks — done (audit findings 5, 16, 17)
- 0.10.6 stage 5: wrong and misleading Atom Lab text (one relativity line, two Learn steps, one visible range, the sampled average radius) — done (audit findings 6, 7, 13, 15)
- 0.10.6 stage 4: the 90 % outline of complex states drawn for the right state, superposition normalised for overlapping pairs — done (audit findings 1, 2)
- 0.10.6 stage 3: one energy for one atom (reduced-mass level in the Cloud and Orbit readouts), Orbit view sub-steps (no wobble at speed 30), Scale Fixed holds the n = 4 clouds — done (audit findings 3, 14, 8)
- 0.10.6 stage 2: Atom Lab constants from CODATA 2022 (NIST pages read 2026-10-04) — done (audit findings 4, 9, 18 and the Bohr radius item; the web's rounded 0.529 Å, 27.211 eV and 24.19 as are no longer followed)
- 0.10.6 stage 1: Atom Lab audit report — done (report only, nothing in the app changed)
- 0.10.5: Orbit Lab audit and fixes — done (stages 1-5: report, text fixes, data fixes, the IAU au, tests and sources)
- 0.10.5 stage 5: Orbit tests and sources — done (audit rows 18, 20-23; the speed rows 24-27 wait for the owner's timing)
- 0.10.5 stage 1: Orbit Lab audit report — done (report only, nothing in the app changed)
- 0.10.5 stage 4: the IAU astronomical unit — done (audit row 2; the web's 149.6e9 m is no longer followed)
- 0.10.5 stage 3: Orbit data fixes — done (audit rows 3, 4, 9, 12: planet gravity, Sun radius, comet records, Simple accuracy)
- 0.10.5 stage 2: Orbit text fixes — done (audit rows 1, 5, 6, 7, 8, 10, 11, 13, 14, 16, 19; no number changed)
- 0.10.2: Atom Lab: phase colours (complex orbitals m = ±1 … ±l and a colour wheel), quantum beats (superposition of two states), smooth photon jumps — done

## Planned, in this order
- The predicted island of stability, drawn from a sourced prediction: waits for a source (one the owner can open and send as text or PDF: a review of the predicted magic numbers and region of superheavy nuclei); until then the chart says it is a prediction and draws nothing — waiting for the owner, not scheduled
- Photon jumps from a complex state or a superposition: the owner decides between the options in native/REPORT.md (0.11 stage 1) — waiting for the owner, not scheduled
- **BLOCKED ON DATA (the owner must download by hand: the IAEA pages are behind a bot challenge that no script can pass, and the other hosts are refused by the cloud session's network policy):** HALO nuclei drawn at their measured MATTER radius. Not started. A charge radius cannot do this (neutrons carry no charge), and the quadrupole table of stage 2 does not help. Needs one file from the owner (details in OWNER-TODO.md):
  - a table of measured matter radii of the light halo isotopes (He-6, He-8, Li-11, Be-11, B-17, C-19 ...), for example Tanihata, Savajols and Kanungo, Progress in Particle and Nuclear Physics 68 (2013) 215, or Ozawa, Suzuki and Tanihata, Nuclear Physics A693 (2001) 32: a named, citable table the owner can open, saved as text, CSV or PDF. Both names came from a search result and were not checked
  - then: halo isotopes drawn at the measured matter radius with the halo neutrons outside the core, tests first
- Magnetic dipole moments are REACHABLE (checked on 2026-10-04 by a plain GET, not stored): `https://www-nds.iaea.org/nuclearmoments/magn_mom_recomm.csv`, 295,121 bytes, 2,365 lines, the same columns as the quadrupole file with `magnetic dipole [nm]`. For a later hyperfine-structure stage (the hyperfine splitting of the hydrogen-like levels needs a nuclear magnetic moment and spin); not scheduled, not started
- 0.10.6 Atom Lab audit: closed. Fixed: findings 1-9, 11, 13-18 (stages 2-9; finding 11, the cloud jump, in stage 8). All three measured optimisation items of the audit that the owner had DEFERRED are now done in 0.13: the phase-colour scan (finding 10, scene 8) in stage 1, the overlapping superposition (scene 14) in stage 2, and the outline first use (finding 12, scene 16: worst frame 19.9 ms; including the radial graph's one-time cost on a change of orbital with the outline off) in stage 3 (the improvements are expected, not measured, until the owner runs the benchmark).
- Bug and speed audit before 1.0, the last pass still to do: app and settings (a report only, then small fix tasks for the real findings) — planned
- 1.0 alpha stages — planned:
  - design plan (written in a fresh short chat)
  - new layout (smaller header, readable text, real-looking sliders)
  - Orbit Lab look
  - Atom Lab look
  - quality levels Low to Ultra
  - new themes "Frutiger Aero" and "Y2K futurism"
- 1.0 beta — planned
- 1.0 release — planned

## 0.13 "GPU update" plan (the owner's, 5 Oct 2026): move repeated per-item work from the CPU to the GPU across the whole app, so it runs well on old PCs (target: a 2013 Core i7-4770K, about 2-3 times slower per core than the owner's Ryzen 5 7500X3D)
- stage 1: phase colours on the GPU — done (OpenGL 3.3)
- stage 2: switch to OpenGL 4.3; superposition and cloud jumps on the GPU (compute shaders) — done (the dot positions, projection, draw order and upload of the cloud are still on the CPU; the jump's own dot building stays there because its random chain cannot run in parallel)
- stage 3: the outline's density grid on the GPU (compute), and exact orbital normalisation — done
- stage 4: Orbit Lab rocks on the GPU; chart and landscape geometry cached on the GPU — planned
- OpenGL 4.3 from stage 2 means PCs older than about 2013 (Intel HD 4600 / Haswell, GeForce 600, Radeon HD 7000 or newer are the oldest that support it) will not run the app; the owner has accepted this (see DECISIONS.md)

## Later (not scheduled)
- speed work after the owner's timing (Orbit audit rows 24-27: Real at the top Time speed, the Milky Way mesh and stars redone every frame, label halos and the belts, a long replay after a very long run), only if the benchmark shows a problem — planned
- better relativity for Real: the Sun-potential term gives the wrong motion along the orbit; replace it with a better 1PN method and re-check against JPL — planned
- Estonian translation — planned
- volume-fog electron cloud — planned
- Atom Lab labels drawn in depth order (renderer rewrite) — planned
- code signing — planned
- more electrons: the first level would be shells filling with simplified screening of the nucleus's charge (Slater's rules), clearly labelled as a simplification; this changes numbers the web version had, so it needs the owner's decision before it is planned — planned
