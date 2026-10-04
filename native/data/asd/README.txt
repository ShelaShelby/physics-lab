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

Citation, in the form ASD's version history page asks for (read 2026-10-04):
  Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024). NIST Atomic Spectra Database (version 5.12), [Online].
  Available: https://physics.nist.gov/asd [access date 2026-10-04]. National Institute of Standards and Technology, Gaithersburg, MD.
  DOI: https://doi.org/10.18434/T4W30F
  (Version 5.12, 7 November 2024, was the newest entry on that page; the retrieved tables do not state their own version.)
Licence / terms: NIST's general statement on copyright, fair use and licensing of its data was NOT read (www.nist.gov was not reachable from
the session that fetched this; only physics.nist.gov is). The only terms read are the citation request above. The owner should check NIST's
statement before the app is shared widely (OWNER-TODO.md).
