Scratch drivers of the 0.47.1 trial runs (data/jumps/trial_runs.tsv). Kept as a worked reference for 0.47.2; NOT production code.
They feed GRASP2018's interactive programs through stdin (rnucleus, rcsfgenerate, rangular, rwfnestimate, rmcdhf_mem, rsave, rci, rlevels, jj2lsj).
GRASP itself is not in the repo: git clone https://github.com/compas/grasp (commit 9006157730a82ac839f2b4ff4e938bcba63a539e), then
  mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_Fortran_FLAGS="-fallow-argument-mismatch -O2" .. && make -j4 install   (binaries in grasp/bin)
  (apt: gfortran cmake libblas-dev liblapack-dev; gfortran 13 refuses lib9290 without -fallow-argument-mismatch.)
Lessons (all seen in these runs):
  - GRASP programs exit with status 0 even when rmcdhf fails: check the output for "Convergence not obtained" (g.py did not at first; staged.py does).
  - Screened-hydrogenic starts failed for Na 3p3/2 alone (no 3p1/2 in the list): use Thomas-Fermi (rwfnestimate option 2) or the ground state's orbitals (option 1).
  - Eu 4f7 6s2: TF start failed on 6s; TF for the core + screened hydrogenic for 6s, then core, 4f, 6s, all in stages, converged (staged.py).
  - Hg 6s6p 1P1 (second root alone) did not converge; optimize both J = 1 roots together (EOL) or use the ground orbitals and vary 6s, 6p only.
  - rmcdhf asks for level weights only when more than one level is chosen in a block (answer 1 = equal); rci input order: see g.py.
  - Optimizing each J alone puts Mg 3P0,1,2 in the wrong order: optimize a term's J levels together.
  - Run long jobs as tracked background jobs; a plain nohup inside a finished shell call was killed half-way (Fe).
AMBiT (CI+MBPT, GPL-3): git clone https://github.com/drjuls/AMBiT (commit 61b8c8a), git submodule update --init, apt libabsl-dev libopenblas-dev
  libboost-all-dev libgsl-dev libeigen3-dev; add #include "absl/strings/str_format.h" at the top of src/ambit.cpp; cmake + make. Its doc/Examples/MgI
  input reproduced the published CI numbers; "-s12" adds the MBPT core corrections.
