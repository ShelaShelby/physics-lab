# 0.57.2: the spinning hole's tables fast again (a 0.56.4 regression) and film_tilt's slow frames
Based on "native 0.57.1". This box: Xeon 2.1 GHz, 4 cores, Mesa llvmpipe (software: its card times are NOT the owner's).
## Part 0: measured first (0.56.3 = 2152e14, 0.57.1 = eca308c, each in its own build; 5 runs each, two rounds, median (spread))
- Coarse spin table, spin 0.9, the benchmark's camera (r 20, 80 degrees; kSpinMoving): 0.56.3 15.2-15.6 ms (14.9-17.9); 0.57.1 74.9-77.4 ms (71.6-88.4).
- Fine spin table (kSpinFineFill): 0.56.3 194-197 ms (185-214); 0.57.1 1,133 ms (1,103-1,234). traceClosed per ray: 3.3-3.6 us against 9.4-9.5 us.
- The lead was right: 0.56.4's phiTimeAt at every crossing of every ray (four elliptic sets per crossing, the camera's end taken again each time) is the
  whole difference (after the fix below: coarse 15.3-16.5 ms (15.2-19.2), fine 203-211 ms (191-220), traceClosed 3.2-3.3 us: inside 0.56.3's spread).
- blackhole.real_move and film_tilt alone run here at 1-20 pictures a second (llvmpipe draws the sky per pixel), a few frames per slot: they give no table
  numbers worth comparing (real_move with the card's tables off: the coarse table 145-166 ms in 0.57.1 against 25-44 ms now). The table program above is the measure.
## Part 1: phi and t only on request (tests first: blackhole_tests testTimedOnRequest, 10 checks)
- traceClosed(a, cam, look, maxCross, timed = false) and SpinFill::timed (spinRayClosed's timed): phi and t are made only when asked; nothing in the app asks
  yet (the moving gas will). The camera's end of the path integrals is now taken once per ray (CamInts), the same numbers. ClosedEnd::timed says which.
- (a) Against 0.56.3's own build, this session, same compiler and machine: 3,840 rays (spins 0, 0.6, 0.9, 0.99; the benchmark's, the Film's, a close and an
  in-plane camera; rays at 1e-9, 1e-6, 1e-3 of the shadow's edge) and 16 tables (coarse and fine, two cameras, four spins), every field: 90 rays and most
  table entries differ in the last bits, all from 0.56.4's Psi_0 by atan2 (a deliberate accuracy fix, kept): with asin put back in a scratch copy every number
  was equal bit for bit. The committed test uses this build's own timed run as reference: without the request every other number (ends, directions, b, q,
  u, rows, edges, the textures) is equal bit for bit, phi and t not made; 3,840 rays and 9 tables (the fine one at spin 0.9).
- (b) With the request phi and t equal bit for bit to 0.57.1's on the same rays and tables (this session). The moving gas's tests in testDiscFlux's crossings
  and testClosedTime are unchanged except for asking for the time (traceClosed(..., true), the table's fill with timed).
## Part 2: film_tilt's frames (the frame thread's own processor time per frame, thread CPU clock, renderer's GL end left out; temporary timers, removed)
- What the slow frames were: (1) the card sky's start-up check made its reference (traceClosed at 3,840 pixels) on the frame: one counted frame of 45-48 ms
  in 0.57.1 (25 ms with the faster rays); (2) the panel's readout, asked 7 times a frame, ran its 4,001-point edge loop each time (0.1 ms each) and, while the
  tilt moves, the shadow's 132 edges once (about 4 ms here); (3) with the card's tables (this box's default) the fills' dispatches run on llvmpipe inside the
  frame's GL calls (2.4-5.3 ms of thread time in CardFill::pump here; on a real card a dispatch costs microseconds). No frame waited on a worker or read-back
  (install, post and pollFine under 0.04 ms; the card's fences are only polled).
- Fixed: the check's reference on a thread of its own from the start of the sky's steps (sky_card skyReference; the frame only compares; never waited for:
  the frame asks again next time); the readout's edge loop kept per spin and tilt. The check's numbers are the same (PLBHSKYCHECKCASE lines identical).
- Over 8 ms, counted frames, 20 s runs. Processor tables (PL_BH_GPUPATHS=0, as the owner's numbers suggest): 0.57.1 5 of 260 (worst 44.9 ms); now 0 of 326
  (worst 8.0, median 2.2). Card tables: 0.57.1 14 of 67 (worst 44.8); now 5 of 88 (8.3-11.8 ms, each with 2.4-5.3 ms of llvmpipe's dispatch; without it 5.2-6.5).
- Disc at the stop ("pixels that change between disc and not-disc"): card tables 3,441 / 3,438 before / after; processor tables 3,439 / 3,436; largest patch 682 in all.
- Left (ROADMAP): the readout's 132 edges on the frame while the view moves.
## Estimate for the owner's PC (an estimate, not a measurement: his next benchmark decides)
- Coarse table back to about 0.56.3's 7.8 ms (here it went 15.4 -> 76 -> 16 ms), so at his 8 ms target's edge; the fine table at start about 71 ms, the sharp
  picture after a stop about 0.46 s again; the stars' new pictures while tilting back toward 60 a second. film_tilt: the 5x tables kept every core busy
  while tilting and the check's frame was 15-20 ms; both gone, so its 1 % low should come back toward 0.56.3's 168; no frame over 16.7 ms is likely, not sure.
## Tests
- check.sh quick: 18 of 18. blackhole_tests 915 checks (testTimedOnRequest new: 10 of them). Nothing new on the graphics card; no physics changed.
- The full UI suite was not run (a small version).
