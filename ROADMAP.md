# Roadmap — native Physics Lab

Plain list of what is done and what is planned. No dates. Nothing is promised that is not on this list.

## Done
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
- **BLOCKED ON DATA (the owner must download by hand: the IAEA pages are behind a bot challenge that no script can pass, and the other hosts are refused by the cloud session's network policy):** the nucleus's SHAPE from measured electric quadrupole moments, and HALO nuclei drawn at their measured matter radius. Not started; needs two files from the owner (details and where to put them in OWNER-TODO.md):
  - Stone's "Table of nuclear electric quadrupole moments" (N. J. Stone, IAEA report INDC(NDS)-0650, 2013; published also in Atomic Data and Nuclear Data Tables 111-112 (2016) 1; IAEA page www-nds.iaea.org/nuclearmoments), as a CSV or text file, or the PDF. These names are what the 0.10.9 stage 1 search listing showed; not checked on the pages
  - a table of measured matter radii of the light halo isotopes (He-6, He-8, Li-11, Be-11, B-17, C-19 ...), for example Tanihata, Savajols and Kanungo, Progress in Particle and Nuclear Physics 68 (2013) 215, or Ozawa, Suzuki and Tanihata, Nuclear Physics A693 (2001) 32: a named, citable table the owner can open, saved as text, CSV or PDF. Not checked either
  - then: the shape from Q through a stated, sourced relation (beta2 from Q and the radius), tests first; halo isotopes drawn at the measured matter radius with the halo neutrons outside the core. A charge radius cannot do this (neutrons carry no charge)
- 0.10.6 Atom Lab audit: closed. Fixed: findings 1-9, 11, 13-18 (stages 2-9; finding 11, the cloud jump, in stage 8). Three measured optimisation items are DEFERRED by the owner (not dropped; do not work on them until the owner says):
  - phase-colour scan (audit finding 10, benchmark scene 8): 7.21 ms against a 4.03 ms baseline
  - overlapping superposition (scene 14): 9.31 ms, 107 fps
  - outline first use (audit finding 12, scene 16): worst frame 19.93 ms, 1 % low 60.4 fps. Note: since 0.10.7 stage 1 the radial graph asks for the state's outline when the orbital changes, so this one-time cost can now also fall on a change of orbital with the outline off (not measured) — deferred
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

## Later (not scheduled)
- speed work after the owner's timing (Orbit audit rows 24-27: Real at the top Time speed, the Milky Way mesh and stars redone every frame, label halos and the belts, a long replay after a very long run), only if the benchmark shows a problem — planned
- better relativity for Real: the Sun-potential term gives the wrong motion along the orbit; replace it with a better 1PN method and re-check against JPL — planned
- Estonian translation — planned
- volume-fog electron cloud — planned
- Atom Lab labels drawn in depth order (renderer rewrite) — planned
- code signing — planned
- more electrons: the first level would be shells filling with simplified screening of the nucleus's charge (Slater's rules), clearly labelled as a simplification; this changes numbers the web version had, so it needs the owner's decision before it is planned — planned
