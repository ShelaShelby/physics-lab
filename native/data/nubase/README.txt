NUBASE2020: spins and the kind of half-life, for the Atom Lab's chart of nuclides (0.11 stage 2)
==================================================================================================

Retrieved on 2026-10-04 (UTC) by `curl` with the tool's User-Agent ("PhysicsLab-native-data-tool"); native/tools/make_nubase_data.py fetch does the same plain GET of
  https://www-nds.iaea.org/amdc/ame2020/nubase_4.mas20.txt
(HTTP 200, Content-Type text/plain, Last-Modified: Fri, 11 Nov 2022 13:13:54 GMT, 761,906 bytes, 5,868 lines,
sha256 1585a5eea86c5e17e90307c7e6e786d060049c4039e392a261ff6db977df9859). Saved exactly as received: nubase_4.mas20.txt.

Citation, as the file's own first line gives it:
  F.G. Kondev, M. Wang, W.J. Huang, S. Naimi, and G. Audi, Chin. Phys. C45, 030001 (2021)   ("Nubase2020 citation")
What was and was not reachable (from the cloud session): this one static file, yes. The AMDC pages around it (www-nds.iaea.org/amdc/, /amdc/ame2020/) and
amdc.in2p3.fr answer with a bot challenge ("Just a moment...", HTTP 403 or no answer), so the page that may state how the table is to be cited or used
could NOT be read: the credit is only what the file says (the citation above). OWNER-TODO asks for it.

What the file is (read from the file's own header): fixed-width text, one line per nuclide state; columns (1-based) 1-3 mass number, 5-8 atomic number + state
(the last digit 0 = ground state, 1-2 isomers, 3-4 levels, 5 resonance, 8-9 isobaric analogue states), 70-78 half-life ("stbl" = stable, "p-unst" = particle
unstable, a trailing # = from systematics), 79-80 its unit, 82-88 its uncertainty, 89-102 spin and parity (* = directly measured, # = from systematics, T= isospin),
120-209 decay modes and their intensities in % (IS = isotopic abundance).

What the lab takes from it (src/atom/nubase_data.inc, written by `make_nubase_data.py build`, counted again by the test):
  - the 3,558 ground-state lines (the lines of which the state digit is 0) are 3,557 nuclides + the free neutron (Z = 0, left out). They are EXACTLY the nuclides the lab already
    had (src/atom/atom_data.inc, made from the web page through mendeleev, which also holds NUBASE2020): the tool refuses to build if the two sets differ by one nuclide.
  - per nuclide: the spin and parity text as written (185 have none), and the kind of half-life: 253 stable ("stbl"), 2,831 a plain value, 369 an estimate from systematics
    (#; the lab never shows these numbers: no measurement), 84 no half-life field, 11 only a limit (> or <), 6 approximate (~), 3 "p-unst".
  - for the 253 "stbl" nuclides: the lower limit the file gives in the uncertainty column where there is one, and the decay-mode entries that carry a question mark ("A ?", "2B+ ?").
    The file's header does not define the question mark; the app shows the entry as written and does not explain it beyond "listed with a question mark".
The half-lives, the decay modes and their odds that the chart shows still come from the lab's existing table (atom_data.inc); this file adds what that table lacks.
