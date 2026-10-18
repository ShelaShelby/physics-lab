# 0.20: Molecule Lab builder (stage 2) and three layout fixes (Windows, a GPU and a real mouse NOT tested)

## What changed
- Molecule Lab, Controls, Mode: Viewer | Build. Build has a Sketch (click adds H/C/N/O/Cl; drag next to an open valence and let go: snap + single bond; click a bond: 1 -> 2 -> 3 -> 1, a refused order is skipped with its reason; Delete key / button, Undo / Redo 100 steps (Ctrl+Z / Ctrl+Y), Clear; open valences as open stubs) and a 3D shape tab (ESTIMATED: ideal VSEPR angles + one CCCBDB length per bond type, walk placement with anti torsions, a flat first ring, FIRE spring relaxation; deterministic). Strict valences; refusals say why ("O already has 2 bonds", "no measured bond length for N=N in our sources"). Hill formula, NIST mass with uncertainty, complete / N open valences, a name for 23 molecules (15 viewer + 8), "Show measured shape", the benzene note. New core: src/molecule/builder.* (no graphics). Up to 30 atoms (design limit).
- Settings: `molbuild=v1 ...` only after Build mode was used; a damaged line gives Viewer + an empty sketch.
- Layout: (a) Atom Lab, Nucleus / Nucleon / Quarks views: the info text folds above the nucleus ("Show n more lines (click)"; a click opens / folds it). (b) Orbit Lab: the All values sheet starts below a collapsed card it would cover (the Legend chip). (c) Molecule Lab: pan limited to 1.5 molecule radii (drag and stored pan); Reset view unchanged.
- The top chip and breadcrumb show the built molecule in Build mode. Test scripts gained `rdrag` (right-button drag). About: the builder's sources (CCCBDB table with all 20 reference codes, VSEPR / Gillespie and Nyholm 1957, valences and names as conventions). Changelog 0.20, version 0.20, release title 0.20.
- Docs: DECISIONS (the 6 owner decisions), ROADMAP, RESUME, README, OWNER-TODO, AUDIT (three items marked fixed).

## Tests
- New, written first: molbuild_logic (605 checks: the 20-entry table and nothing else, valences of all 5 elements x orders 1-3 with the exact reasons, cycling and refusals, undo / redo exactness, formula and mass = stage 1 for the 15, 23 names incl. shuffled atoms, 9 near misses not named, shape vs measured, determinism, settings round trip + 11 bad lines + 20,000-line fuzz). Break-on-purpose: a changed C-C length and rounded-up lone pairs each failed it.
- New picture / app tests (tests/check_molbuild.py): molbuild_sketch, _edit (snap, bond click, Ctrl+Z / Ctrl+Y, Delete, add), _refuse, _shape (+ Show measured shape), _noshape (Light), _settings, and the layout fixes _u238 (U-238, Th-232, Pb-208), _sheet (1280x800, 1920x1080), _molpan. Run against the 0.19 build: u238, sheet (1280x800) and molpan failed; all pass now.
- Changed on purpose: app_settings About edit count 37 -> 38 and About entity count 261 -> 286 (the new About entries); a molbuild prefs check added; shell_pictures guard hashes of 6 pictures (About 1280x800 Dark / Light: longer source list; the data sheet at 1280x800 and 1000x640, Dark / Light: now below the Legend chip). Physics tests unchanged and passing.
- check.sh full once: 89 / 90, only shell_pictures failed (the 6 on-purpose changes above); after the hash update that test alone: passed.
- Pictures looked at once each: sketch (ethanol-like, Dark), refused bond, water's estimated shape, no shape (Light); U-238 before the fix.

## Estimated vs measured (printed by molbuild_logic; NOT hidden)
- Lengths: CO2 C=O 1.205 vs 1.162 (+3.7 %), HCN / C2H2 C-H 1.087 vs 1.064 / 1.063, H2CO C-H 1.087 vs 1.111, O3 1.208 / 1.475 vs 1.278 (ozone needs formal charges: the editor refuses its central O's third bond; built from its graph in the test only).
- Angles at lone-pair atoms are the ideal VSEPR value exactly: H2O 109.47 vs 104.48, NH3 vs 106.67, O3 120 vs 116.8 (its central O: (6-3)/2 = 1.5 lone pairs, rounded down to 1).
- KNOWN LIMIT: formaldehyde's H-C-H is 120 (ideal) vs 116.133 measured, 3.87 degrees: the job's "within 3 degrees" cannot hold for it with ideal angles. The test checks it is exactly 120 and records the 3.867; all other no-lone-pair angles are within 3 degrees. Owner decides (OWNER-TODO).
- Benzene (Kekule): closes, flat (< 1e-6 A), alternating 1.339 / 1.536 vs the real six equal 1.397; the panel says so.

## Draw counts and speed (counts only; this cloud machine has no GPU)
- Sketch: one instanced blob draw (ethanol-like: 26 blobs, 1 draw call in the frame); 3D estimated water: one instanced draw (18 blobs). No per-frame allocation in either (buffers reused; the shape is rebuilt only when elements or bonds change).
- Shape build: 0.035 ms for 30 atoms (nonan-1-ol), 0.10 ms for benzene, on the cloud CPU (target under 10 ms).

## Not tested / not done
- Windows, a real mouse (drag, snap feel, double clicks), a real GPU, display scales: OWNER-TODO.
- tools/native_edits.js was not given the new About edit (as in 0.18; the test list in test_app.cpp has it).
- native/in.txt, o1.txt, o2.txt (committed in 0.19, stray test outputs) were left as they are.
