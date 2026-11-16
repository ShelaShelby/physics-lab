# 0.55.8: the spinning hole's light paths about 3.7 times faster on the processor (moving table) and 4.7 times (fine table)
No source read or saved. The fine table's numbers changed on purpose (new form of the same equations): Real's saved hash was changed on purpose (below).
## Step 1: where the time went (0.55.7's code, this box: 4 cores, GCC; tables as the lab makes them, all cores)
| table (rays) | whole | edges | probes | rows | node rays | finish | threads | steps/ray | rejected | rhs/ray | steps beyond 10 rC |
|---|---|---|---|---|---|---|---|---|---|---|---|
| benchmark coarse (4096) | 92 ms | 1.7 | 6.2 | 0.3 | 85.4 | <1 % | 0.1/stage | 262 | 19 | 1820 | 16 (6 %) |
| benchmark fine (147456) | 6916 ms | 12.5 | 107 | 1.7 | 6490 | <1 % | 0.1/stage | 662 | 2.6 | 4120 | 35 (5 %) |
| Film coarse | 86 ms | 1.4 | 5.7 | 0.2 | 71.4 | <1 % | 0.1/stage | 236 | 23 | 1678 | 15 (6 %) |
| Film fine | 6323 ms | 11.8 | 103 | 1.4 | 6163 | <1 % | 0.1/stage | 597 | 2.3 | 3719 | 32 (5 %) |
- Rays within 1e-3 psi_e of the edge: 43-45 % of the rays, 2.5x the steps (397 against 113 far out): about 2/3 of the time. ~40 ns per right-hand side (a sine,
  a cosine, ~8 divisions). Decided from this: the far tail (5-6 % of steps), the threads (0.1 ms a stage) and the edges/probes/rows (9 %) cannot give much;
  a looser tolerance gives only 1.8x at 1e-6 and its error near the edge grows as 1/x (a = 0.01: 4e-4 -> 1.7e-2 rad): the rays themselves had to get cheaper.
## What changed, and what each gained
- **mino.h/.cpp (new)**: the same equations (J15 (A.4), (A.12), (A.15)) in Mino time with u = 1/r, mu = cos theta: polynomial right-hand sides, no sine, two
  divisions; derived in the header. Each escaping ray is followed to u = 0 (infinity) itself: no tail left out. Rays passing within sin^2 theta < 1e-3 of the
  spin axis keep KerrBL (phi turns by ~pi in less than a step there; measured: error grows to 4e-3 rad at 1e-6). Gain, same tolerance: coarse x3.2-3.4, fine x3.9-4.3.
- **spin_sky SpinFill** (tolerance, form, xlo, rtolFar): a node's tolerance grows with |x|/psi_e beyond 1e-4 (moving: to 1e-5, fine: to 1e-8): a further x1.13-1.17 on both.
- **After**: benchmark coarse 92 -> 24.7 ms (x3.7), Film 85 -> 22.1 (x3.9); fine 7304 -> 1549 ms (x4.7), Film 6824 -> 1412 (x4.8). Left: 107 steps/ray, 6 % of rays on KerrBL (rows chi = 0, pi), edges + probes 3.4 ms.
- Tried and dropped (measured): an error-scale floor (no gain); the null conditions kept by projection (6x worse near the edge at spin 0.01); a larger xlo or fewer columns (disc's further images 14 % worse at 0.01); a thread pool (0.3 ms a table).
## Accuracy shown (blackhole_tests 826 checks, 0 failed)
- testMino: Mino's rates = rho^2 times KerrBL's at 200 states (2e-12); against KerrBL at 1e-12 (rtol 1e-10), spin 0.01/0.6/0.9/0.99 + 0.9 near the axis, 192 rays
  each: same ends, same b; far direction near the edge (x/psi_e 1e-6..1e-4) <= 5.8e-6 rad (limit 3e-5), far <= 1.6e-8 (5e-8); crossings' radii <= 4e-4 (1.5e-3);
  null conditions at the end <= 4.5e-10 (3e-9); next to the pole (sin^2 theta 1.1e-3) 3.7e-8 rad (1e-6).
- testMovingTable: the moving table against KerrBL at 1e-12 at 480 places, 5 cases: every band of the far direction, b, the disc's r and g within 2 % of
  0.55.7's coarse table on the same places (all equal to 3 digits or better; the limit is 0.55.7's own measured numbers, printed per case), 0 mismatches.
- testSpinTable (fine table), limits unchanged: its numbers = 0.55.7's to 4 digits (a 0.9 far direction 3.26e-5 / 1.86e-5 / 1.43e-5 rad).
- **Real's saved hash changed on purpose**: 7fff50741d132f22 -> b37c532bb4916030. The precision part draws the case twice: with 0.55.7's rays still 7fff50741d132f22;
  the new picture differs by at most 1 level of 255, in 695 of 1,024,000 pixels.
## Benchmark (ui/benchmark.cpp, app/bench.*)
- "Spin 0, changing distance: no frames counted": the scene went by the wall clock from its set-up, which holds the set-up frame (the fine table); the counting
  goes by the frames' summed times, so it ended before the last slot. Now the scene uses the run's own clock. Rests 1.4 s after tilting and distance at spin 0.9
  (spin 0: 0.5 s moves, 0.1 s rests; fits the 8 s, static_assert). New lines: the sharp picture's time after each stop (or none), the fine table's time at the start; run here (PL_BENCH_ONLY=13): all six counted, fine table 1677 ms.
## Estimate for the owner's PC (ratio of his 0.55.7 numbers to this box's, same stage; an estimate, not measured)
- Coarse table tilting 64 -> ~17 ms, distance 56 -> ~15 ms; with the other stages (~15 ms) about 30 pictures a second while tilting at spin 0.9 (was 12),
  turning stays ~88. Fine table 2837 -> ~600 ms: the sharp picture ~1 s after a stop. **The tenth was not reached (x3.7)**: bound by the rays near the edge
  (2/3 of the time; their tolerance cannot be loosened, the error grows as 1/x) and the step count of the Dormand-Prince pair at 1e-8.
## Tests
- check.sh quick: 17 of 17. By hand (LP_NUM_THREADS=1 as ctest): precision 98/98, smooth 36/36 (new: a new picture after 16 of 16 tilt frames at spin 0.6 on
  the processor; with 0.55.7's coarse table 12 of 16, so the 90 % check fails there), spin 19/19, disc 23/23, looks 39/39, truelight 28/28. app_tests 0 failed.
- **The full UI suite was not run (small version).** The card's light paths are untouched (parked).
