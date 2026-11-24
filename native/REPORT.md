# 0.80.1: the Atom Lab's Classic GPU-vs-CPU check fixed (a test fix; nothing changes in the app)

## What changed
- Read: CLAUDE.md, ROADMAP.md (the Atom Lab fix line), tests/check_atom_chip.py classic_part, src/atom/atom_bench.cpp benchFrame, src/ui/benchmark.cpp (PL_BENCH_SHOT), the logs of the full-test runs of 0.78.1, 0.79.0 and 0.80.0.
- Cause: the check took each run's picture 1.5 s into its counted time by the clock. The side view's zoom and the Slice's cut move by the scene's frame count (atom_bench.cpp benchFrame). The slower run (the CPU's) had drawn fewer frames at 1.5 s, so its picture was at another zoom: the scale bar 319 px against 350 (atom.side_all_classic, this build machine).
- benchmark.cpp: PL_BENCH_SHOT_FRAME="n" saves the picture at the scene's n-th frame. check_atom_chip.py classic parts use frame 30 and run the GPU and CPU runs side by side.
- ROADMAP: the Atom Lab fix line deleted. Changelog entry, VERSION 0.80.1, TESTS.md.

## Tests run
- check_atom_chip.py, by hand, before: classicside 9,017 of 256,000 pixels over 8 levels (fail). After, each part 0 failed: classic 0, classicside 0 (one after the other) and 112 (side by side), classicsideglow 128, classicslice 0, classicsliceglow 141 pixels over 8 levels (limit 2,560).
- Part times on the build machine: 43-77 s (classicsideglow 90 s one after the other, 77 s side by side). GitHub's runner took 300 s for classicsideglow before: its time there is checked by the next full run.
- check.sh job: quick 18 of 18 passed, ui_smoke passed.

## Not done
- The few remaining pixels side by side (112-141): the cloud's dots a frame adds are set by time, so a loaded run can hold a few more or fewer dots. Within the limit; not chased.

## References changed on purpose
- None. The picture limit (1 % over 8 levels) is unchanged: the check now compares pictures of the same frame, as its comment always said ("of the same moment").
