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
- 0.10.5 stage 1: Orbit Lab audit report — done (report only, nothing in the app changed)
- 0.10.2: Atom Lab: phase colours (complex orbitals m = ±1 … ±l and a colour wheel), quantum beats (superposition of two states), smooth photon jumps — done

## Planned, in this order
- 0.10.5 fix tasks from the Orbit Lab audit (small text fixes; values checked against JPL; the astronomical unit; independent tests; speed checks on the PC; stage numbers decided after the owner reads the report) — planned
- Bug and speed audit before 1.0, the two passes still to do: Atom Lab, then app and settings (each: a report only, then small fix tasks for the real findings) — planned
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
- speed work, only if the owner's benchmark shows a problem — planned
- better relativity for Real: the Sun-potential term gives the wrong motion along the orbit; replace it with a better 1PN method and re-check against JPL — planned
- Orbit audit: the web's AU (149.6e9 m) differs from the IAU value 149,597,870,700 m by 1.4e-5, about 2,000 km at 1 au; decide in the audit — planned
- Estonian translation — planned
- volume-fog electron cloud — planned
- Atom Lab labels drawn in depth order (renderer rewrite) — planned
- code signing — planned
- more electrons: the first level would be shells filling with simplified screening of the nucleus's charge (Slater's rules), clearly labelled as a simplification; this changes numbers the web version had, so it needs the owner's decision before it is planned — planned
