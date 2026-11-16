# 0.55.6 (Part A, checkpoint): the stars and the exposure on all cores
No source read or saved. Numbers: this Linux box (4 cores; Mesa llvmpipe, a SOFTWARE renderer), blackhole/spin_sky.h stages timed by a scratch harness on the
benchmark's case (spin 0.9, 20 M, 80 deg from the axis), 2,874 catalogue stars; the owner's PC (0.55.5 benchmark) beside it. Estimates are estimates.
| stage (ms) | owner 0.55.5 | here before | here after | owner's PC after, estimate (his/mine ratio of the same stage before; all-core ratio 0.32 as the lower bound) |
|---|---|---|---|---|
| star bins, fine table | 86.6 | 126-152 | 41-47 | 26 (13) |
| stars' images, fine table (turning) | 36.9 | 78 | 20 | 9 (6) |
| star bins, coarse table (tilt, distance) | 15.8-17.3 | 18.7-20 | 5.0-5.5 | 4.5 (1.7) |
| stars' images, coarse table | 14.5-15.1 | 25-31 | 7.0-7.5 | 3.9 (2.3) |
| spin 0: stars' images (starImagesFast) | not measured | 6.8-7.7 | 1.9-2.1 | - |
- How: SpinStars builds its triangles row by row on all cores (each node's |x| and camera direction once, not once per cell corner), numbers them in row
  order, and fills the bins band by band (one thread per band of bins, each listing its triangles in number order): the same triangles, numbers and bin
  lists as 0.55.5's one-thread build, which stays as SpinStars::kReference for the test. The view finds each star's images on all cores (64 stars per job)
  and lays the dots down afterwards in the catalogue's order: the same dots in the same order. Spin 0 (starImagesFast) the same; lab.cpp discLook (the spin-0
  exposure) on all cores (only maxima are kept: the same result). The spin exposure's scan: up to 16 threads (was 8).
- Background pools leave one core for the frame (low_priority.h parallelFor): with every core busy the frame thread was preempted (UI part smooth: 50-75 ms).
- The exposure still comes from the processor (bounded while moving, as 0.55.5); from the card it would need a read-back of the frame before (a lag of one picture).
## Tests
- New (blackhole_physics, testSpinTable): the bins on all cores and on 2 threads against 0.55.5's build for every catalogue star and 4,000 directions:
  every image's place and magnification equal to the last bit (0 differ). The Real saved picture's hash (UI precision) unchanged.
- UI smooth changed: a "moving" frame is now one whose settings moved; with the faster worker the picture catches up in the pauses between the scripted
  drag steps and makes its glare, which on llvmpipe runs inside the frame's calls (on a card it is queued). Limit unchanged (8 ms); main.cpp prints want=.
- By hand: precision 78, spin 19, disc 23, looks 39, truelight 28, smooth 10: all pass. check.sh quick: see below. **The full UI suite was not run (small version).**
- Part B (the card fills the table) is in this commit's code but switched off (BlackHoleView::gpuPaths false), untested; it is the next push.
