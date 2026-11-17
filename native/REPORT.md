# 0.58.0: the Black Hole Lab's Real look: the disc to its real extent, "Light: What an eye sees / All light" (based on 0.57.2; box: 4 cores, llvmpipe)
## Part 0: measured first (feeding preset, 1280 x 800, the centre row through the disc; lab.h discPixel, the card's rule in double; scratch tool, not kept)
| view | r (M), side | eye Y / Sun | lin (max ch.) | sRGB 8-bit | all light / Sun | lin | sRGB |
|---|---|---|---|---|---|---|---|
| reference_disc_r60 (60 M, 10 deg) | 26.5, receding | 87 | 0.30 | 148 | 1,566 | 0.024 | 43 |
| | 8.8 / 26.8, approaching | 274 / 144 | 0.99 / 0.51 | 254 / 189 | 60,080 / 7,011 | 0.98 / 0.11 | 253 / 94 |
| default view (20 M, 5.5 deg) | 30.0 / 9.6, receding / approaching | 87 / 272 | 0.29 / 0.98 | 148 / 253 | 1,561 / 58,370 | 0.023 / 0.93 | 42 / 247 |
- Clipped at white: eye 1 of 572,552 disc pixels, all light 8 (the exposure samples every 4th pixel). Nothing is squeezed: the screen gets linear Y / white,
  hue-kept clip, sRGB curve only. The planner's reading confirmed: visible light falls only ~3x from the hottest ring to 30 M and differs ~2.4x between the
  sides; the total light falls ~40x and differs ~16x (the 4th powers of T and g). Today's pale, even disc is what the model gives the eye; the rim was the cut.
- L79 (scan read from page images; typed lines saved in the sources repo, extract data/blackhole/scans/luminet_1979_AA75_228.extract.txt): F0 = FS/(1+z)^4,
  Figs 9-10 in units of F_S max. The app at L79's geometry (spin 0, camera 1e8 M, direct image): 30 deg 2.114 (Fig. 9: largest curve 1.80 with a small
  unlabelled curve inside, the next step 2.10: agrees); 10 deg 2.808 at r 9.04, g 1.299, where L79 prints 2.62 (NOT reproduced; L79's misprinted factor
  gives 3.01, worse; its Fig. 10 draws no 2.40 curve inside 2.10, so the paper is not self-consistent there; ROADMAP). C75 (A9) is the Kerr g used (spin.h).
## Part 1: the edge (source: J. Goodman, MNRAS 339, 937 (2003), arXiv:astro-ph/0201001, saved; extract + SOURCES.md + About)
- Abstract: "thin disks may not exist beyond a thousand Schwarzshild radii or so (0.01 parsec)"; section 1: Q = 1 at ~10^-2 pc for 10^8 Suns (= 2,089 GM/c^2).
  kOuterDefault and kOuterMax 2000 GM/c^2; the feeding preset sets it. Same Page-Thorne flux, no fade added. Reference changed on purpose: testDiscSettings.
- Checks: at outer 30 this build draws reference_disc_r60.png pixel for pixel; the card's flux table keeps 0.57's 1,024 points and only adds points past
  1000 M (else the saved Real case moved by one level in 1,051 pixels: found by its hash, fixed); auto exposure the same 275.7 x Sun.
- The precision tests keep their 6-30 M disc (check_blackhole precision). With the disc to 2000 M they measured: spin-0 table r within 1e-3 (1e-4 for
  6-30); the processor's spin table 4 deg from the plane at spin 0.99 up to 33 of 3,871 pixels another image (grazing far rays): ROADMAP **fix**.
## Part 2: Light (tests first: blackhole_tests testAllLight and testDiscSettings's edge: 915 -> 990 checks)
- disc.h allLightLinear / bolometricVsSun: brightness (gT / T_sun)^4 = g^4 sigma T^4 / pi over the Sun's; colour the eye's. Real only; no glare, no true sky.
- (a) 50 points, spins 0 and 0.9, against L79 (15) / PT (15n) closed forms: 1e-9; colour kept 1e-12. (b) whole sky against the efficiency: not tested (the
  light lost to the hole and back to the disc is in no saved source; far out g ~ 1 cannot test g^4). (c) L79 above. (d) the eye: bit for bit. Card: kLensFS, kKerrFS, kKerrSkyFS get uAll (the eye's path keeps 0.57's operations). Start-up check: a light probe (uMode 11) at 3,840 temperatures
  per mode, each pixel's limit derived from the shader's operations (sky_card.h): here eye 0.078, all light 0.012 of their limits, 0 over. Card exposure in
  All light against the processor's: 3.9e-4 (spin 0.9), 4.0e-6 (spin 0). The changed shader has a new id, so the guard retries.
- SPIR-V plain / inlined (KB): kKerrSkyFS 103.5 / 64.7 (was 101.4 / 62.3), kKerrFS 28.4 / 20.1 (27.3 / 19.5), kLensFS 30.1 / 22.5 (29.2 / 21.6). Panel: Light
  (dot on What an eye sees), its captions; exposure and readout say "all light"; Most realistic sets the eye's. check_blackhole alllight (new, 6 checks).
- Benchmark blackhole.all_light (Hard list, 25 / 41): spin 0.9, turning and tilting through the plane. Views tried here (pictures/s, 1 % low): 20 M 90 deg
  0.29 / 0.33, 2.2 M 90 deg 1.71 / 0.96, 2.2 M 360 deg 1.62 / 0.59, 4 M 150 deg 1.52 / 0.76, 60 M 30 deg 2.93 / 2.01: 20 M, 90 deg kept.
- Part 3, the spin card: inside the ergosphere (BPT (2.7), 2 GM/c^2 in the plane, r+ at the poles) nothing stands still; the lab's camera turns everywhere.
## Pictures (docs/blackhole, each with .txt; looked at once)
- disc2000_r60_eye: the disc now fills the lower half (the camera at 60 M hovers above a disc 2000 M in radius); its far side fades to a dark
  horizon over the hole; still pale blue-white near the camera, as the model says. disc30_r60_all / disc2000_r60_all: a bright lopsided core (approaching
  side right), the rest dim; the 2000 M edge hardly shows. l79_view_all: like L79's Fig. 11 (mirrored: our gas turns the other way). Before: reference_disc_r60.
## Tests: check.sh quick 18/18; check_blackhole precision 131/131 (full run), alllight 6/6 by hand.
- Full UI suite (check.sh full): 31 of 34. ui_blackhole: truelight's glare and Film-sky checks needed sky beside the disc: their runs now keep the 6-30 M
  disc (outer=30 in their settings; 28/28 by hand). ui_popups: check_6b benchwaits capped under the full suite's load; alone 10/10. ui_molecule: check_molecule
  card "names the card for both scenes" fails here alone too, and the same with 0.57.2's own build: not from this job (ROADMAP **fix**).
