# 0.87.0: the disc's real light

## What changed
- CPU before the shaders changed: lab.cpp discLight took T(r) from DiscPhys (double, Page and Thorne's flux), g T as the colour temperature, the fast 4096-point black-body table (blackBodyLinearFast, visibleVsSunFast); callers: discPixelG (spin 0), spinPixel, the exposure samples, the picture tool. kLensFS, kKerrFS and kKerrSkyFS did the same in float from uDT, uDF and uDC.
- R9 (disc.h colourCorrection): Done et al. 2012 (1) and (2) at each ring's own temperature. The light is f^-4 of a black body at g f T. The total light is unchanged.
- R10 (disc.h limbDarkening): Chandrasekhar 1946 Table 2, cubic, over N = 0.99142. mu = g q^(1/2) / r in the gas's frame. The GPU reads a 41-point table of the cubic (uLimb), within 7.3e-5: kKerrSkyFS had no room for the cubic (100,168 of 100,437 bytes).
- R11 (returning.h): Cunningham 1976 (7a), (8a) as sums over rays from 32 rings, solved by iteration. Made once per spin, by the GPU's light paths when they are on (pieces of 8,192 rays a frame), else by a CPU thread. Spin 0: 2 % of the disc's flux. Spin 0.99: about 15 % at every radius. The plan expected the inner disc only.
- Shared GLSL part kDiscLightPart. Card lines: f and the returned light at the hottest ring. Guide, captions and About name the four sources and the limits. Response line response.blackhole_return, done once (each spin's table is made once; 3 tries would pass the 4.9-minute limit). No Hard scene: no new pass is drawn.

## Tests run
- Written first, failed on 0.86.0 (missing functions): testColourCorrection087 18 checks, testLimb087 26, testReturning087 38. All pass. blackhole_tests: all pass.
- R11: T_f against Tables 1 and 2, 200 cells, bound 0.0005 + 3 sigma (jittered rays, Richardson in the bin). The lab's own rays: 52 cells, worst 0.21 of the bound. KerrBL with Cunningham's falling gas: every cell but one.
- GPU vs CPU (check_blackhole.py sky, 50 checks, 0 failed): new cases hot, edgesky, edgetable 0 % over 8 levels (max 1, 6, 1). Every case draws the returning light. precisioncard (20 checks): the GPU-made returning table = the CPU's within 1.5e-10.
- check.sh job: quick 18 of 18 and ui_smoke passed (the first run failed on the 1000 M reference, below). check_sources.py: ok. check_shaders.py: 42, 0 failed.

## Not done
- Doubtful printed value: Cunningham 1976 Table 2, T_f(y_o 0.85, y_e 0.75), printed 0.234. Computed 0.380 +- 0.041; its column neighbours are 0.394 and 0.375. No other source gives the value. The test names it and checks it lies between them.
- Left out (ROADMAP): the returned light's push on the gas (8b), the falling gas shining, limb darkening on the thick disc's tilt.
- Needs pc_tests.ps1 on the owner's PC: the sky part (close case on the Radeon), the returning-light job's time on the card, response.blackhole_return.

## References changed on purpose
- toneCompare: pixels whose radius range reaches past the disc's edge are left out (the shader's coverage). Case close failed at 1.5 % once the disc dimmed: corner rays move r by 13 M a pixel. Likely the Radeon's 1.99 % too.
- Old checks of the earlier rules (camera shift, 0.55.2's light, 0.57's eye picture) run with the real light off (RealLightOff). The reference picture's T_obs includes f. The 1000 M check's reference light is at the pixel's own mu (it failed at 48 %: the limb factor, not the camera).
- Benchmark: 46 waits (was 45), test_app's count.
