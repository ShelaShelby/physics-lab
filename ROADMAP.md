# Roadmap — native Physics Lab

Plain list of what is done and what is planned. No dates. Nothing is promised that is not on this list.

## Done
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
- 0.10.6 fix tasks from the Atom Lab audit (18 findings in the stage 1 report, in git history `git show 90049fe:native/REPORT.md`; findings 4, 9 and 18 are done in stage 2, 3, 8 and 14 in stage 3; still planned: the outline of complex states, Superposition of equal states, the deuterium and tritium mass, heavy-atom text, timeline texts, speed checks on the PC, stronger tests; stage numbers decided after the owner reads the report) — planned
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
