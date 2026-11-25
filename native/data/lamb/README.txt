[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/lamb/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

[0.84.1] The Dirac energy that atom/dirac.h uses is printed in the saved Yerokhin & Shabaev source (Hlike8.tex, Equations (2)-(4), and the DIRAC rows of the
energy table). Extract: yerokhin_shabaev_2015_dirac.extract.txt (native/PLAN_REVIEW.md).

The Lamb shift of the one-electron atoms (0.49.3: the Atom Lab's Dirac levels and lines for n = 1 and 2).

yerokhin_shabaev_1506.01885_source.tar.gz
    Saved as received on 11 Nov 2026 from https://arxiv.org/e-print/1506.01885 (the paper's LaTeX source, gzip-compressed tar; it holds Hlike8.tex and one figure):
    V. A. Yerokhin and V. M. Shabaev, "Lamb shift of n = 1 and n = 2 states of hydrogenlike atoms, 1 <= Z <= 110",
    J. Phys. Chem. Ref. Data 44, 033103 (2015), arXiv:1506.01885.
    Used: the table "Ionization energy of the ground state and the energy level separation intervals in hydrogenlike atoms, in Rydbergs" (label tab:energy), its row LAMB:
    the Lamb shift of 1s, and of the intervals 2s-1s, 2p1/2-2s and 2p3/2-2p1/2, for each Z = 1..110 with the isotope and the rms charge radius R that the table names.
    The unit is hc R_inf: the table's DIRAC 1s of hydrogen, -1.0000133131931, is -(1 + alpha^2 / 4 + ...) in that unit.
    The paper's other table (label tab:lamb) gives the same shifts in units of F(Z alpha), Delta E = (alpha / pi) (Z alpha)^4 / n^3 F m c^2; the tests compare the two.
    tools/make_lamb_data.py turns the table into src/atom/lamb_data.inc (the shifts of the levels 1s, 2s, 2p1/2, 2p3/2, in hc R_inf).
