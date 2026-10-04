# Native Physics Lab 0.10.7 (alpha) — stage 2 report: the real spectrum next to the lab's

New card "Real spectrum (NIST)" in the Atom Lab, under "Light spectrum". `index.html` untouched (sha256 e5501387ae64…). The three deferred optimisation items stayed deferred.

## The query, the date, the data
- **ASD lines form** `https://physics.nist.gov/cgi-bin/ASD/lines1.pl`, retrieved **2026-10-04** by `tools/make_asd_data.py fetch`: format=3 (tab-delimited), **show_av=3 (vacuum wavelengths; no air wavelength anywhere)**, show_obs_wl=1, show_calc_wl=1, unc_out=1, intens_out=on, allowed_out=1, forbid_out=1, conf/term/J/energy columns on. This is stage 6's query; the full parameter list is in `data/asd/README.txt`. Ranges: ion 0.0001–10,000 nm, atom 100–2,500 nm.
- **236 requests** (118 elements × the hydrogen-like ion "He II" … and the neutral atom "He I" …). Raw replies (12 MB) were not kept; the tool's `ASD_CACHE` option keeps them outside the repo.
- **Kept (stated in README, About and the app):** ion = both levels n ≤ 6, fine-structure components separate; atom = the 30 strongest **observed** lines in 100–2500 nm that have a numeric ASD intensity.
- **Result:** 110 ions and 98 atoms have lines. **Missing, said on screen:** ASD has no ion for Z = 111–118 and no neutral atom for Z = 100–118 except Fr (19 atoms); Fr I exists but has no observed line with an intensity in range. 7,915 lines kept (ion 5,029; atom 2,886).
- **Size:** `asd_lines.tsv` 841 KB, `manifest.tsv` 5.6 KB, generated `asd_data.inc` 647 KB (compiled into atom_core). Fields kept per line: wavelength, observed/Ritz flag, ASD's uncertainty, Ritz wavelength, intensity text as written, n of both levels, resolved flag, the transition text.
- **Trouble met:** Python's default User-Agent got 403 (the tool sends its own, named); my first run double-encoded the spectrum name (all "Input Error"), found from the first log, nothing from it was kept.
- **Citation** (read from ASD's version-history page): Kramida, Ralchenko, Reader & NIST ASD Team (2024), version 5.12, DOI 10.18434/T4W30F, in README, About and the card. **Not read:** NIST's general licence statement (www.nist.gov is not reachable here): OWNER-TODO.
- **Intensities:** ASD's help says they are qualitative, have no common scale, mean something only within one spectrum and depend on the source. So they are ranked and drawn within one spectrum only; a line with none (all Ritz-only ion lines, 17 flag-only cells) is drawn short and equal. ASD writes some numbers in brackets, "(51000)": kept as text, parsed as numbers; **what the brackets mean was not looked up.**

## How (a) and (b) are shown (`atom_panel.cpp`, `asd.*`)
Two separate blocks, each with its own title, strip and note, never one list: **"(a) The ion the lab models: Fe²⁵⁺ (NIST: Fe XXVI)"**, lab's lines over NIST's lines on one log axis, a line of text per series (lab nm, NIST components, "inside / X % longer than all"), note: "one electron round a nucleus of charge Z … should nearly coincide … fine structure and relativity … this comparison checks the lab". **"(b) The neutral atom of the element: Fe I"**, same strip, note: "this is what 'the spectrum of iron' means … 26 electrons … the lab has ONE … not expected to match … shows the limit of the lab". For hydrogen (b) says it is the same system as (a). Missing data are said in words. Then the measured/Ritz/intensity note, the selection, the credit. Solid tick = observed, faint = Ritz only. Cached per element, not per frame.

## Tests (first the data facts typed from the TSV/manifest, then code). Linux GCC Release
- **Data:** totals and per-spectrum counts as fetched (H I ion 169 of 441, Fe I 30 of 9,548 …); every wavelength positive and in range; observed flags survive (ion 210 observed, 4,819 Ritz only; all 2,886 atom lines observed); resolved 5,014/2,164; numeric intensities 107 ion, 2,886 atom; 17 flags-only; spot values (H I Lyα obs 121.56699 ± 0.00020, Ritz 121.5668237310/121.5673644608; He II 30.3780409484, the stage 6 value; Fe XXVI 0.177801471).
- **(a) H I and He II, all 15 transitions among n ≤ 6 each:** the lab lies inside ASD's Ritz components widened by (Zα)² of the wavelength. Justification: the lab is level-averaged and non-relativistic, relativistic corrections are of order (Zα)² of the energies. **Measured:** outside the bare range by at most 0.205 (H I) and 0.214 (He II) of that allowance, i.e. 1.1e-5 and 4.6e-5 of the wavelength.
- **Heavy ions against Dirac** (formula written in the test from knowledge, reduced mass): Fe XXVI Lyα, lab 0.179738854 nm; ASD 0.177801471 (2p3/2), 0.178344041 (2p1/2); Dirac 0.177701170, 0.178241988. The lab is **1.08 % / 0.78 % too long**; Dirac accounts for 0.951 / 0.932 of that (guard 0.0689 = measured 0.0682 + 1 %). Over all 110 ions: lab > ASD > Dirac in wavelength in every case, share from 0.8017 (H) to 0.9780 (guard ≥ 0.793). ASD lies between lab and Dirac, as QED (Lamb shift, lifting 1s) and nuclear size would do; **that size was not checked against a source**.
- Words: (a) and (b) titles and notes, missing-data messages, citation, selection (7 checks). New About entry in test_app.
- **Mutations (reverted):** ion/atom swapped → test crashed; offset sign flipped → 1 failed; lab line 0.03 % off → 1 failed. A real catch on the way: my first parser dropped bracketed intensities.
- Counts: orbit 1,540; **atom 18,326** (was 18,299: +27); app 1,515 (one About edit added, count of edits 16→17); nbody 37; jpl 55; moon 24; moon_real 59. All seven pass.

## Not compiled, not seen, not tested
- `atom_panel.cpp` (`realStrip`, `realTicks`, `realSpectrumCard`) is not built here; only the new functions were compiled against stand-ins of the ImGui types (`-Wall -Wextra`): clean. **Never seen:** layout, theme colours, text sizes, the long notes at large text, tick overlap, whether the font draws every glyph. OWNER-TODO has the checks.
- Not measured: start-up and Windows build time with the extra 0.65 MB of tables. The comparison for Z > 100 relies on ASD's calculated values only.

## Windows CI result
Pending at the time of writing (filled in after the push).
