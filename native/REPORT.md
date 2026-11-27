# 0.95.0: the spinning black hole, faster and with its disc at its true thickness

Read before any shader change: CLAUDE.md, REPORT 0.94.1, 0.86.0, 0.85.3, ROADMAP (Black Hole Lab), shaders.h (kEllipticPart, kKerrSkyTracePart, kKerrSkyMainPart, kKerrTableCS, kLensMainPart's surface step), kerr_closed, spin_sky, spin.h, disc.h/.cpp, lab.h/.cpp, vk_blackhole.cpp, blackhole_view.cpp, GLa (sources repo) (6a), (6b), (7), (8), (46), (49), (B27), (B46).

## What changed
- A, measured first (PL_BH_COMPARE=skytime: the sky pass alone between GPU timestamps; its cut-down shaders, built as SPIR-V variants, PL_BH_SKYSTOP 1-5). Build machine (lavapipe), 1280 x 800, spin 0.9, no gas: pass 158 ms, of it fixed 1.9, set-up 14.5, radial case 5.7, integrals and crossings 123, the rest and the disc's light 15. With the moving gas the timed integrals double it (305 ms).
- A, try 1: one Carlson duplication for all integrals of one amplitude (up to four R_J). Try 2: Carlson's stop at 0.03, with its bound from the series' degree-6 term (0.11 eps^6 for R_F, 2.8 eps^6 for R_J; coefficients checked against mpmath). Times: no gas 138 -> 136 -> 127 ms, gas 305 -> 227 -> 203, thick view 283 -> 213 -> 196.
- B: the surface |z| = h(R), z = r cos theta, R = r sin theta (BL), at every spin in Real. The line through each crossing (dr/dPsi = (dr/dtau)/sq, (dr/dtau)^2 = R(r)), then one exact step of r(tau) and F(phi) (kerr_closed.cpp surfaceHitMino). Cap 1.2 rad (0.5 failed the grazing view). CPU closed forms, kKerrSkyFS, kKerrTableCS (13 numbers a node), the spin table and kKerrFS (the line only).
- B cost Part A's gain: the sky pass with the surface is 213 / 297 / 266 ms (no gas / gas / thick view; 0.94.1 138 / 305 / 283). Two exact steps cost 588 ms without gas, so one is used. SPIR-V 158,728 -> 163,656 bytes.
- check_shaders.py's size limit (kKerrSkyFS inlined at most 101,201 bytes) failed at 111,536. Same arithmetic in less code: atanS's one atan1, one hgtAt, the camera's and end's I+- through the timed sets' formulas, one strkSample, ellFP's slots as vectors, tan(am - atan g0) as a quotient. Now 100,968.
- Captions, guide and changelog say the disc is drawn thick at spin. Hard scene blackhole.thick_spin in, blackhole.close_spin to All scenes (same view; 4.9-minute limit).

## Tests run
- blackhole_tests 1239/0 (spinsurface: KerrBL at spins 0.5, 0.9, 0.99 within 0.5 px, carlson, discspin). check_shaders 47/0. check_blackhole by hand: sky 53/0 (thick05 1, thick99 1 of about 200,000 pixels over 8 levels), precision 79/0, precisioncard 23/0. `check.sh job`: 18/18 and ui_smoke passed.
- Fixed on the way: the CPU's u+^(1/2) only on rays with a crossing (the card check), spinDisc ignoring g_hgtHookOff (Real-look hash, now 0.94.1's dd0cdd28343147e1).

## Not done
- Needs pc_tests.ps1 on the owner's PC: blackhole.thick_spin and the spinning scenes' fps, the sky pass split on the Radeon (3c.bh.skytime), the GPU/CPU pictures (3c.bh.tone.thick05, thick99), and AMD's compiler on the larger kKerrSkyFS.
- The fallback (spin table, kKerrFS) keeps the line through each crossing: 1.3 % of disc pixels over 8 levels from the exact surface (ROADMAP). mino.h's fallback rays carry no slopes (plane). The moving gas's phi and t stay the crossing's.
- Waits (build machine, medians, 0.94.1 -> 0.95.0): response.blackhole_spin 1876 -> 1929 ms, response.blackhole_tilt 1663 -> 1718 ms. The Radeon's numbers need pc_tests.ps1.

## References changed on purpose
- toneCompare at spin (sky pass): the reference is now the closed forms with the surface, not the spin table (the table pass keeps the table). PLBHSKYEXP's exact log-average takes the surface.
- check_blackhole precision 2c and PLBHREAL draw the disc in its plane (g_hgtHookOff, as 2a since 0.79.0): they check the light paths' crossings, and mino.h's rays have no slopes.
- test_app's benchmark id list: thick_spin in close_spin's Hard place, close_spin moved to All scenes (74 in All).
