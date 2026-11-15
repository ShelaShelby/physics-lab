[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/asd/holdings/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

NIST Atomic Spectra Database (ASD) holdings pages (0.47.1, research for jumps in every element)
================================================================================================
Retrieved on 2026-11-09 11:29 UTC with curl, saved exactly as received:
  lines_pt.html   https://physics.nist.gov/cgi-bin/ASD/lines_pt.pl    (periodic table of line counts per spectrum)
  levels_pt.html  https://physics.nist.gov/cgi-bin/ASD/levels_pt.pl   (periodic table of level counts per spectrum)
Each spectrum is a link whose title reads "<X> <ion>: <n> lines" or "<X> <ion>: <n> levels"; the neutral atom is ion "I".
110 neutral atoms have an "X I" entry on both pages (H..Hs, Z = 1..108); Z = 109..118 have none.
The level counts include the ionization limit(s) and terms listed without an energy; tools/make_jumps_table.py counts only levels with an energy.
Citation: A. Kramida, Yu. Ralchenko, J. Reader and NIST ASD Team, NIST Atomic Spectra Database (ver. 5.x), NIST, Gaithersburg, MD (2026).
Used by: tools/fetch_jumps_asd.py (which spectra to fetch) and as a cross-check of data/jumps/elements.tsv (its line counts equal these pages for all 110).
