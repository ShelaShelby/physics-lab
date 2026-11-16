# 0.55.6: the Black Hole Lab's light-path table filled by the graphics card; the stars on all cores
No source read or saved. Numbers from this Linux box (4 cores, Mesa llvmpipe: a SOFTWARE renderer, so the "card" here is the processor); the owner's from his 0.55.5 benchmark.
## Part A (pushed first as a checkpoint): the processor's share per new picture (benchmark case: spin 0.9, 20 M, 80 deg from the axis; 2,874 stars)
| stage (ms) | owner 0.55.5 | here before | here after | owner after, estimate: his/mine of the stage before (all-core ratio 0.32 as lower bound) |
|---|---|---|---|---|
| star bins, fine table (tilt, distance on the card's path) | 86.6 | 126-152 | 41-47 | 26 (13) |
| stars' images, fine table (turning; every path) | 36.9 | 78 | 20 | 9 (6) |
| the card's nodes into a table (new, worker) | - | - | 4-9 (now on all cores) | ~2 |
| exposure while moving (bounded, as 0.55.5; up to 16 threads) | 5.1-6.3 | 24-40 | not remeasured | 5 |
| spin 0: stars' images (starImagesFast) | not measured | 6.8-7.7 | 1.9-2.1 | - |
- Total processor share per new picture on the card's path, estimate for his PC: turning 9 + 5 = about 14 ms (6 + 5 at best); tilting or distance 26 + 9 + 5 + 2 =
  about 42 ms (13 + 6 + 5 + 2 = 26 at best). Not the "few ms" asked: the star bins (2.9 million bin entries per table) are the largest part. ROADMAP line.
- How: SpinStars' triangles row by row on all cores, numbered in row order, bins filled band by band in number order: the same bins as 0.55.5's one-thread
  build (kept as SpinStars::kReference). The view finds the images star by star on all cores and lays the dots down in the catalogue's order. Spin 0 and its exposure
  (lab.cpp discLook) likewise. Background pools leave one core for the frame (with all 4 busy here the frame was preempted: UI smooth 50-75 ms).
## Part B: the card fills the table (render/kerr_compute, shader kKerrTableCS; blackhole/spin_card)
- The same grid and stored values: geodesic.cpp's launch, rhs, ended, farDirection, Dormand-Prince step and plane crossings, spinRay's continued u, shadowEdge's
  bisection and the node's psi, ported line by line, all in double. GLSL has no double sin, cos, atan, exp, log: written (Taylor after range reduction with pi/2 and
  ln 2 in three parts; atan2 by two Newton steps from the float one; cbrt by Newton). Against the C++ library: sin, cos 1.1e-16; exp, log, atan2, acos, cbrt, x^-0.2 within 6e-16.
- Pass A (edges + probe rays) and pass B (the rows' edges + 147,456 nodes, in pieces of 8,192 rays against the OS's graphics timeout) are queued; the frame looks at
  fences and never waits; the rows' azimuths are placed by the processor (SpinTable::rowsOf, shared with its own fill); the nodes are read by the worker from mapped memory.
- Fine table on the card vs the processor, 4 threads: benchmark case 3.4 s vs 9.0 s; Film 3.2 vs 8.4; spin 0.99 3.6 vs 9.7. On his card: NOT measured, and I cannot
  estimate it from these (a software renderer); scene 13 now reports it. If one fill takes longer than a few frames, tilting gives as many pictures a second as fills.
- **Accuracy: the first case holds.** Card vs processor, every node of the three fine tables: same ends everywhere, none lost; far direction 1e-11-1.5e-10 rad
  (3.3e-6 within 1e-3 of the edge; the table's own limits against the integrator: 2e-3, 2e-2), b 3e-9, u 1.5e-5, edges 6e-15 rad. On 300 card rays per case the
  null condition 6-8e-10 and Carter's q 1.3-1.4e-9 at the end, the same as the processor's own rays at this tolerance. So the card's table is the picture at
  rest too: no fine thread on the card's path. The precision checks' limits are untouched (precision runs the processor's path; its Real hash unchanged).
- Start-up check (first spinning picture): 8 edges and 32 rays vs the processor. Limits from the integrator's own noise (psi moved by 1e-15: 2.5e-12 rad, u 5.5e-10)
  and float-like arithmetic (psi moved by 1e-7: 2.5e-5 rad, u 1.4e-6): edge 1e-12, direction 1e-9, b 1e-10, u 1e-7. Here: 0, 1.4e-13, 8e-17, 1.6e-9. Fails, or no
  OpenGL 4.3, or the shader does not compile: the processor's path of 0.55.5, said in Settings, Performance and in the benchmark. OpenGL 3.3: untouched.
- Settings, Performance (Black Hole Lab): one line which path and why, and the switch "GPU light paths (OpenGL 4.3)" (on; not saved). Benchmark scene 13: the same
  three movements at spin 0.9 then at spin 0 (1 s each, same length), the path and the check, the card's fill time, new pictures a second per movement.
## Tests
- blackhole_physics: the bins on all cores = 0.55.5's build, every image of every catalogue star and 4,000 directions to the bit (0 differ). app_tests: the scene's lines.
- precision (new run PL_BH_COMPARE=card): the shader's functions, the check, the three tables node by node and the conservation laws, limits above with room.
- smooth: the old run on the processor's path (a "moving" frame is now one whose settings moved: the faster worker catches up between drag steps and starts its
  glare, which on llvmpipe runs inside the frame); new: the card's path (small grid PL_BH_CARDGRID, this renderer's speed): a new picture after every frame that
  moved, every picture the card's, no coarse table, no fine thread; PL_BH_CARDFAIL: the processor's path and the Settings line. Failing before: the processor's path
  makes a coarse table per picture and starts the fine thread at rest.
- By hand: smooth 17, precision 97, spin 19, disc 23, looks 39, truelight 28, settings 4: all pass. check.sh quick 17 of 17. **The full UI suite was not run (small version).**
