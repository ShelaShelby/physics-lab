NIST Atomic Spectra Database (ASD) lines for the Atom Lab's "Real spectrum (NIST)" card
=======================================================================================

Retrieved on 2026-10-04 (UTC) by native/tools/make_asd_data.py fetch, from the ASD lines form
  https://physics.nist.gov/cgi-bin/ASD/lines1.pl
One request per spectrum, 236 in all: for every element Z = 1..118, (ion) the hydrogen-like ion "<symbol> <Roman numeral Z>"
(H I, He II, Li III ... Fe XXVI ... Og CXVIII: one electron round a nucleus of charge Z, the ion the lab models) and (atom)
the neutral atom "<symbol> I". Parameters, as in the test comment of 0.10.6 stage 6 (tests/test_atom.cpp, testSpectrumIndependent):
  spectra=<name>  low_w, upp_w = 0.0001 and 10000 (ion) / 100 and 2500 (atom)  unit=1 (nm)  format=3 (tab-delimited)
  show_av=3 (VACUUM wavelengths: the lab is vacuum; no air wavelength is stored or used)  show_obs_wl=1  show_calc_wl=1 (observed and Ritz)
  unc_out=1 (uncertainties)  intens_out=on  allowed_out=1  forbid_out=1  conf_out/term_out/enrg_out/J_out=on  line_out=0  output=0
  bibrefs=1  order_out=0  page_size=15  (the rest left empty). The request is built in make_asd_data.py (function query).
ASD answered with a table, or with an "Input Error" page for a spectrum it does not have. The tool sent its own User-Agent
("PhysicsLab-native-data-tool"); the first try with Python's default one was refused by the network path (403).

What is kept (the same words are in About, "Where the facts come from", and in the app):
  ion : lines of the hydrogen-like ion whose two levels both have principal quantum number n <= 6 (fine-structure components kept separately)
  atom: the 30 strongest OBSERVED lines of the neutral atom in 100-2500 nm (vacuum) that ASD gives a numeric relative intensity for,
        ranked by that intensity; fewer when ASD lists fewer
Files:
  asd_lines.tsv  the kept lines: Z, kind (ion/atom), spectrum, then ASD's own columns (cells are ASD's text, quotation marks removed;
                 an absent column is empty). 7,915 lines (ion 5,029; atom 2,886). About 0.8 MB.
  manifest.tsv   per spectrum: status (ok / none = ASD has no such spectrum / empty = it has it but nothing passes the selection),
                 how many lines ASD returned in the range, how many were kept.
  ../src/atom/asd_data.inc is made from these two by  python3 native/tools/make_asd_data.py build  (about 0.5 MB of C++ tables; wavelengths
                 with the digits ASD gave).
Result of the fetch: 110 ions and 98 atoms have lines. ASD has no ion for Z = 111-118 (Rg ... Og) and no neutral atom for Z = 100-118
except Fr (Fm, Md, No, Lr, Rf ... Og: 19 atoms); Fr I exists but has no observed line with an intensity in 100-2500 nm. The app says so
on screen for each of them. Nothing was filled in from memory.

What the columns mean (ASD's help, https://physics.nist.gov/PhysRefData/ASD/Html/lineshelp.html, read 2026-10-04):
  - observed wavelengths are measured; Ritz wavelengths are worked out from measured energy levels (for the hydrogen-like ions of heavy
    elements ASD has Ritz values only, so no line of them is "measured" in this file); each has the uncertainty ASD gives.
  - relative intensities: ASD says they are qualitative, have no common scale, mean something only within one spectrum (one element in
    one stage of ionisation) and depend on the light source. The lab therefore ranks and draws them within one spectrum only, and draws a line
    ASD gives no number for short and equal, so no brightness is implied. 17 ion lines have only ASD's flags (e.g. "bl*", a blend) in the cell.
    ASD writes some numbers in brackets, e.g. "(51000)"; the text is kept as written. What the brackets mean was not looked up.

0.10.8 stage 3: the transition probabilities (NO new request, no refetch)
  The columns Aki(s^-1) (ASD's transition probability, the Einstein A coefficient: the spontaneous-emission rate of the upper level to the lower one),
  Acc (ASD's accuracy code for it) and Type (ASD's transition type) were already in asd_lines.tsv from the 2026-10-04 retrieval above; stage 3 only
  carries them into the compiled table (tools/make_asd_data.py build, three new columns of kAsdLines: aki, acc, type) and uses them for the lifetimes and
  jump rates of the "Lifetimes and jumps (NIST)" card. Of the 5,029 ion lines 4,925 have an A value; 104 have none (10 of them hydrogen's forbidden lines).
  ASD's own explanations, read from its lines help page https://physics.nist.gov/PhysRefData/ASD/Html/lineshelp.html on 2026-10-04:
  - Accuracy: "An estimated accuracy is listed for each transition strength, indicated by a code letter": AAA <= 0.3 %, AA <= 1 %, A+ <= 2 %, A <= 3 %,
    B+ <= 7 %, B <= 10 %, C+ <= 18 %, C <= 25 %, D+ <= 40 %, D <= 50 %, E > 50 %. "If the accuracy is followed by a prime, then a multiplet in the original
    compilation has been separated into its component lines and the transition probability was derived from the compiled value assuming pure LS coupling.
    This may decrease the listed accuracy." 103 lines have an A value and no accuracy code in this file (4 of them ion lines); the app says "no rating given".
  - Type: blank for allowed (electric dipole, E1) transitions; M1 magnetic dipole, E2 electric quadrupole, M2, E3, M3, M1+E2 mixed, HF hyperfine-induced, UT
    "forbidden transition of an unspecified type". In this file: ion lines E1 5,011, M1 11, E2 5, UT 1, M1+E2 1.
  - ASD's rule for an allowed line: the levels must be of opposite parities and the total angular momentum J must change by no more than +-1 (J = 0 <-> 0 excluded).
  A doubtful ASD entry found by the tests: the line 1s 2S 1/2 - 2s 2S 1/2 of Cs LV (Z = 55) has a BLANK Type (= allowed E1) and A = 7.591e11 /s, but l does not change
  there, so it cannot be electric dipole; the same line is typed M1 for Z = 56 (9.155e11 /s) and UT for Z = 38 (4.068e10 /s). The lab follows the selection rule and
  does not use that value. Nothing was changed in the data file.

Citation, in the form ASD's version history page asks for (read 2026-10-04):
  Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024). NIST Atomic Spectra Database (version 5.12), [Online].
  Available: https://physics.nist.gov/asd [access date 2026-10-04]. National Institute of Standards and Technology, Gaithersburg, MD.
  DOI: https://doi.org/10.18434/T4W30F
  (Version 5.12, 7 November 2024, was the newest entry on that page; the retrieved tables do not state their own version.)
Licence / terms: NIST's general statement on copyright, fair use and licensing of its data was NOT read (www.nist.gov was not reachable from
the session that fetched this; only physics.nist.gov is). The only terms read are the citation request above. The owner should check NIST's
statement before the app is shared widely (OWNER-TODO.md).
