# 0.18 stage 1: Molecule Lab viewer (speed NOT tested on a GPU; Windows and a real mouse NOT tested)

## What changed
- New lab "Molecule Lab" (AppState lab index 3, shown 3rd in the Labs menu: Orbit, Atom, Molecule, Defense). Defense Lab code unchanged (only the shared lab-name ternaries learned index 3).
- 15 molecules (H2, N2, O2, HCl, CO2, H2O, O3, NH3, CH4, HCN, C2H2, H2CO, C2H4, CH3OH, C6H6). Every length/angle/rotational constant is the CCCBDB value the owner gave (cccbdb.nist.gov is unreachable from the cloud; text saved in data/molecules/cccbdb_values.txt + README.txt with the symmetry used per molecule). 3D shapes built in code (src/molecule/molecule.cpp), centred on the centre of mass, principal axes along x/y/z.
- **CH3OH: H-C-O angle derived** (109.89 deg, not in CCCBDB) from H-C-H 109.03 assuming three equivalent C-H symmetric about C-O; marked "derived" in the panel and the data sheet.
- Masses: NIST Atomic Weights and Isotopic Compositions (physics.nist.gov, reachable; 5 pages saved as received). The repo had no NIST mass table (AME data holds binding energies only).
- Ball-and-stick (ball = 0.25 x vdW radius, labelled a drawing scale) / space-filling (PubChem vdW radii), CPK colours (PubChem), bond orders 1/2/3/aromatic (convention; double/triple as parallel sticks, benzene one stick, panel says "aromatic"; O3 one double + one single = one resonance structure, said in the app).
- Right panel: molecule list, style, labels, auto-spin (off with Reduced motion), Reset view, the clicked atom/bond (listed value + CCCBDB code + the value from the 3D shape), data sheet (formula, Hill formula, name, mass with uncertainty, rotational constants listed vs from the shape, shape assumption, source), "What the picture does not show" (equilibrium structure, vibration, conventions, no hard edge). Bottom bar in the toolbar's place. Drag turns, wheel zooms, Shift/right/middle drag pans.
- Settings: one versioned line `molecule=v1 ...` (written only once the lab was opened, so older settings files stay byte-identical); lab "Molecule Lab".
- About: a "Molecule Lab" source block (CCCBDB with all reference codes, NIST masses, PubChem); changelog 0.18 entry; project VERSION 0.18; workflow release title 0.18.

## Doubtful value found (not hidden)
- **NH3 rotational constants**: the shape from rNH 1.012 / aHNH 106.67 gives A = B = 9.974 cm-1; CCCBDB value given: 9.4443 (+5.61 %, outside the 3 % rule). C: 6.346 vs 6.196 (+2.4 %). The rule was NOT loosened: the test asserts the reported +5.61 % so any correction shows up; the data sheet says so; OWNER-TODO asks to re-read the CCCBDB page.
- CH4 aHCH 109.471 and H2CO aHCO 121.9 are values fixed by symmetry (Td: arccos(-1/3) = 109.4712; C2v: 121.93); they are checked to their listed rounding (half the last digit), every other value to 1e-6.
- Names (Water, Acetylene (ethyne) ...) are the usual names, not checked against CCCBDB page titles. Point groups and CCCBDB's citation text: not given / not reachable, so not shown.

## Tests
- New molecule_logic (456 checks, written before the code): every listed length/angle reproduced; CH3OH dihedral 180 for exactly one H, derived angle consistent; CH4 six angles = arccos(-1/3); benzene planar, 6 equal C-C/C-H, CCC 120; C2H4/H2CO planar; linear molecules collinear; H2O equal O-H; bond and atom counts; bond orders; COM at origin; Hill order (own rule); masses vs NIST numbers typed again; rotational constants from own inertia/eigenvalue code vs CCCBDB (all within 3 % except NH3 A/B above; worst others H2O C +2.44 %, NH3 C +2.42 %); picking (atom, bond middle, empty, nearer of two overlapping, none in space-filling); camera (2000 drags stay a rotation, reverse drag, zoom clamp, NaN, pan); settings round trip and 9 kinds of bad text.
- Broken on purpose once: picking the farther atom -> "the nearer of two overlapping atoms is picked" failed; restored.
- New molecule_pictures: H2O Dark + C6H6 space-filling Light at 1280x800, draw counts, oxygen pixel red, bottom bar placement, real clicks on an atom and a bond, settings saved and read at the next start, damaged settings line -> defaults.
- app_settings: the About entity count kept (plain apostrophe); native edit count 37 -> 38 (on purpose). shell_pictures: shell_about_dark/light_1280x800 hashes updated on purpose (longer About page); those two pictures were NOT looked at.
- check.sh quick green; check.sh full once: 78/79, only shell_pictures (the two About hashes) -> updated, re-run passed.
- Pictures looked at (3): H2O dark (sticks were painted over the oxygen ball -> sticks now start at the ball surface), H2O crop after the fix, C6H6 space-filling light.

## Draw counts (Linux software GL; counts, not speed)
- The whole molecule is ONE instanced blob draw call in every case: H2O 18 instances, C6H6 ball-and-stick 96, C6H6 space-filling 24, CH3OH 42. Frame total 1 renderer call. Draw-list CPU 4-9 us. No per-frame allocation once the buffers have grown.
- Painter's order (no depth buffer): overlapping space-filling balls show no intersection curve; an approximation of the picture, not of the data.

## Not tested / not done
- fps on a real GPU, Windows, a real mouse (drag/pan/wheel feel): in OWNER-TODO. Auto-spin not exercised by a picture test (tests run with Reduced motion).
- Not built (by design): builder, vibration, reactions, orbitals, big molecules, file import.
