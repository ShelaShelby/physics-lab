# 0.80.0: Sagittarius A*'s hot flow: the physics tested, nothing drawn (the owner's decision, 24 Nov 2026)

## What changed
- Read: CLAUDE.md, REPORT.md, ROADMAP.md (Black Hole Lab), PLAN_DISC.md 0, 3, 6, 7, 9, disc078_real_sgra.txt, DECISIONS.md (Black Hole Lab); by grep and ranges: discproto.cpp (sgra), disc.h, disc.cpp, lab.h, lab.cpp, presets.h, schwarzschild.h, geodesic.h, vk_blackhole.h, test_blackhole.cpp. BL06, BL05, BL05a, Broderick et al. 2011, Pandya et al. 2016 (pages 10, 12), GRAVITY 2020, Yuan, Quataert & Narayan 2003 (YQN; new, with BL05a, in the sources repo).
- 0.79.0's full run (45397594642) failed only ui_atom_classic2: the known check_atom_chip classic parts (ROADMAP Atom Lab fix). Nothing else to fix.
- src/blackhole/hot.h: BL06's flow (Table 1), Pandya (24)-(27), the gas's motion and field angle, the transfer (I/nu^3 invariant) along spin-0 rays, the far flux, the colours.
- Found: Pandya (27) as printed (3^(p/2), Gamma((3p+12)/12)) is 0.563 of the direct integral. The code uses 3^((p+1)/2), Gamma((3p+2)/12) (Rybicki & Lightman's form), and discproto too. (24) prints K_2(1/Theta_2); Theta_e is used. At 1.3 mm (26) is 8 times too high (gamma near gamma_min), so the reference uses the integral with the cut.
- The 1.3 mm check (3 Jy +- 30 %, Broderick et al. 2011) and the 2.2 um check (0.28-6.9 mJy, GRAVITY 2020), BL06's mass, distance, 45 degrees. Lengths in M: 0.3 of the energy 4.54 Jy, 0.155 mJy; 0.3 of the field 2.13 Jy, 0.040 mJy. Lengths in r_S (Broderick 2011; YQN Fig. 2's profiles fit this unit, plot reading): 19.8 Jy, 2.77 mJy; 13.4 Jy, 0.70 mJy. No reading passes both. The owner chose: draw nothing (DECISIONS).
- The missing source: BL06 Fig. 1 (plot reading) shows about 4.5 Jy at 230 GHz, as the energy reading gives, but about 3 mJy at 10^5 GHz, 20 times ours. YQN gives the field as a share of the energy (the energy reading) and a power law that would lower, not raise, the near infrared. BL05a adds nothing. Not closed.

## Tests run
- blackhole_physics: passed (88.8 s). New, 47 checks, 0 failed: Table 1 and the text against the extracts, ISCO radii against BPT, B 36.3 G and 19.9 G against Broderick (4) and BL06's 30 G scale; (26), (27) against the integral (3e-6), slopes, the cut, Kirchhoff (1e-16); slab I = j L, (j/a)(1 - e^-tau), thick slab B_nu(gT) at three shifts (I/nu^3), Doppler and aberration far away (4e-4); flux grid converged (0.07 %); the four readings and the gate (drawn only if one passes both); colours at alpha 0.5, 1.25, 3 within 6e-5 of PLAN_DISC section 3.
- The new tests fail on the old build: it has no hot.h, so they do not compile.
- check.sh job: quick 18 of 18 passed, ui_smoke passed. check_sources.py: ok (695 lines, 46 extracts).

## Not done
- Everything drawn: the GPU table, the Hard scene blackhole.hot_flow, its response line, the readout, the guide and About text. They wait for a model that passes both checks (owner, 24 Nov). So no feasibility timing, no GPU against CPU test, no UI part run by hand (no UI changed).
- Spin 0.5 and 0.998, M87*'s model: ROADMAP.

## References changed on purpose
- None. No existing test changed.
