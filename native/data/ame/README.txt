AME2020: binding energy per nucleon, for the Atom Lab's chart of nuclides (0.11 stage 4)
==========================================================================================

Retrieved on 2026-10-04 (UTC) by `curl` with the tool's User-Agent ("PhysicsLab-native-data-tool"); native/tools/make_ame_data.py fetch does the same plain GET of
  https://www-nds.iaea.org/amdc/ame2020/mass.mas20.txt
(HTTP 200, Content-Type text/plain, Last-Modified: Sun, 07 Mar 2021 04:45:05 GMT, 476,239 bytes, 3,594 lines,
sha256 05b054a0538f2c308d061b22096f94a944f1603a5b0948a62e5171657601c674). Saved exactly as received: mass.mas20.txt.

Citation, as the file's own header gives it (the file is "one out of a series of 3 files published in"):
  W.J. Huang, M. Wang, F.G. Kondev, G. Audi and S. Naimi, "The Ame2020 atomic mass evaluation (I)", Chinese Physics C45, 030002 (March 2021);
  M. Wang, W.J. Huang, F.G. Kondev, G. Audi and S. Naimi, "The Ame2020 atomic mass evaluation (II)", Chinese Physics C45, 030003 (March 2021).
What was and was not reachable (from the cloud session): this one static file, yes. The AMDC pages around it were already known to sit behind a bot challenge (0.10.9, 0.11 stage 2); the page that
states how the table may be cited was not read again. The credit is only what the file says.

What the file says that the app uses (read from the file's own header and rows):
  "# in a place of decimal point : estimated (non-experimental) value; * in a place of value : the not calculable quantity".
  Columns after the element: mass excess (keV), its uncertainty, BINDING ENERGY/A (keV), its uncertainty, the beta-decay energy, the atomic mass.
What the lab takes (src/atom/ame_data.inc, written by `make_ame_data.py build`, counted again by the test):
  - the 3,557 nuclides with Z >= 1 (the free neutron, Z = 0, is left out): EXACTLY the lab's nuclides (the tool refuses to build if one differs).
  - per nuclide: the binding energy per nucleon in keV (unrounded, to the file's digits), its uncertainty, and whether it is an ESTIMATE (a "#" in the value): 1,008 are, 2,549 are derived from
    measured masses. The largest Z with a measured value is 110: AME2020 has no measured mass for any heavier nuclide (Z 111-118 are all estimates), although the nuclides themselves are known
    (NUBASE2020 lists them with half-lives).
  - the highest value among the measured rows is READ FROM THE TABLE by the program (it is Z = 28, N = 34, nickel-62, 8794.5555 keV per nucleon in this file); nothing about the peak is typed in the app.
The lab's own binding table (atom_data.inc kBeaLines, whole keV per nucleon) agrees with this file for all 3,557 nuclides to within 1 keV (the test checks it); it does not say which values are estimates.
