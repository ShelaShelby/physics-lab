#include "changelog.h"

#include <algorithm>
#include <string>

#include "app/app_data.h"
#include "fonts.h"
#include "look.h"

#include "imgui.h"

namespace ui {

namespace {

struct Entry { const char* version; const char* title; const char* text; };

// The native app's own history. Newest first.
const Entry kNative[] = {
    {"0.14", "Themes, stage 1 of 5: richer theme styles and the Frutiger Aero theme",
     "A theme is no longer only seven flat colours. It can now also have a page that fades from one colour to another, glass panels (see-through, with a bright line along the top and a soft shadow under floating windows), glossy buttons, tabs, check boxes and sliders (a bright top half, a tinted lower half, a pressed look), its own corner roundness and its own font. All of this is data in the theme, not code for one theme, and every theme that existed before draws exactly the same pixels as in 0.13 (checked by pictures of six screens in all 12 versions, dark and light). "
     "New theme: Frutiger Aero (light): a sky-blue page, frosted glass, glossy aqua and fresh-green buttons, rounder corners. It uses Segoe UI when Windows has it; the font is Microsoft's and is not shipped. Every text and background pair in it passes the W3C's WCAG 2.x contrast rule (4.5:1 for text, 3:1 for large text and interface parts), glass over the lightest and darkest part of the page included; the tests check the formula against values worked by hand. "
     "Colours that used to be written inside drawing code (error red, the red button, the faint illustration tint, the grey of tracks, the fill colour of chosen buttons) now come from the theme with the same values. The pictures of the labs (space, cloud, chart) keep their colours in this stage. "
     "NOT TESTED: the look on a real PC with Segoe UI and a real graphics card, and speed. Combo boxes keep the flat frame. The owner judges the look."},
    {"0.13", "GPU update, finished: the Orbit Lab's rocks and the chart of nuclides on the graphics card, and what 0.13 is in all (stage 4 of 4)",
     "0.13 \"GPU update\" in one place. WHAT MOVED to the graphics card (OpenGL 4.3, with OpenGL 3.3 as the fallback that keeps the old processor code): the phase colours of the electron cloud's dots (stage 1); the colour groups of a superposition and of a photon jump (stage 2); the 90 % outline's density grid and level, and the Slice contour (stage 3); and now the places of the Orbit Lab's belt rocks and the chart of nuclides' tiles and blocks (stage 4). Stage 4: "
     "(1) The 6,000 rocks (48,000 in the stress scene) used to be placed one by one by the processor every frame, Kepler's equation included; now a compute shader does it, with the time kept in double precision on the graphics card so a rock is right after 10,000 years at the fastest speed too (tested against Kepler's equation solved separately, at times up to ten million years, to 0.003 of a point). While checking it I found that the belt code solved Kepler's equation with only three Newton steps: exact for every rock the app makes, wrong for orbits with e of 0.8 and more (none in the app); it now iterates until converged, and the app's rocks do not move. "
     "(2) The chart's 3,557 squares or blocks are made once and kept in a graphics-card buffer instead of being rebuilt and sent every frame (a landscape frame sent 3.2 MB); tilt, zoom and pan are applied on the graphics card; a rebuild happens only when the picture size, the theme colours or the height meaning changes. The hover and click still answer exactly as before (tested against the drawn blocks). "
     "(3) The Atom Lab's Cloud 2D view cost more than Cloud 3D for a reason that was not the colours: its list of 48,000 dots was built again from nothing every frame, grown about 17 times; it is now kept, and the renderer takes it without copying (measured here: scene 10 from about 3.9 to 2.6 ms of processor time). "
     "THE SWITCHES (none is saved; all start On): GPU colours, GPU superposition and jumps, GPU outline, GPU rocks (Orbit Lab) and GPU chart cache, in the Performance card; the benchmark report says which way each ran, and scenes 23 to 34 run the matching scenes again with the old way (31 and 32: the Orbit Lab; 33 and 34: the chart). "
     "ALSO IN 0.13: the orbitals' normalisation is exact now (stage 3), a physics fix of up to 4.4e-5 in amplitude. "
     "MEASURED BY YOU (5 Oct 2026, stage 2 build): scene 1 2.24 ms, scene 6 5.34 ms, scene 10 4.43 ms, scenes 19 to 22 1.3 to 2.1 ms. NOT MEASURED: every speed-up in 0.13 is EXPECTED, not measured, because there was no graphics card or Windows PC in the sessions that wrote it; the tests compare the graphics card's results with the processor's on a software OpenGL (Mesa llvmpipe), where a real driver may round 32-bit numbers differently. Run the benchmark with the extra scenes and compare each scene with its Off twin."},
    {"0.13", "GPU update: the 90 % outline on the graphics card, and exact orbital normalisation (stage 3 of 4)",
     "The first time a 90 % outline is needed for an orbital, the processor used to make a 64 x 64 x 64 grid of densities and sort it to find the 90 % level: about 30 ms in a measurement here (13 ms for the densities, 17 ms for the sort), the only real hitch left in the app. "
     "Now, on a PC with OpenGL 4.3, compute shaders on the graphics card make the grid and find the level (by a search over the values, no sort), and the picture does not wait for them: the outline appears a frame or two after it is switched on. In the Slice view the cut's contour values are made the same way, so the contour is a frame behind the cut while you drag it. "
     "The 90 % mark of the radial graph no longer makes the panel wait either. A PC with only OpenGL 3.3 keeps the old way. The Performance card has a switch (GPU outline) and says which way your PC runs; the benchmark report prints it; scenes 29 and 30 run scenes 16 and 17 again with the old way. "
     "The outline is the same as before: the graphics card's level agrees with the processor's to 1e-4 and the cells to float rounding, and the tests compare them cell by cell. "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. "
     "A physics fix on purpose: the orbitals' normalisation was a numerical sum that left the wave function too big by 3e-6 (2p) up to 4.4e-5 (4f z^3) in amplitude; it is now exact (closed form), checked for every orbital the app offers to 1e-10. The only numbers that move are the recorded jump amplitude sums in the tests; nothing you see changes."},
    {"0.13", "GPU update: superposition and photon jumps on the graphics card (stage 2 of 4)",
     "In a superposition (and while a photon jump is in flight) the processor used to work out, for every one of the 48,000 dots and every frame, whether the dot is shown, and its colour. This was the slowest part of the app (the overlapping superposition was the slowest scene of 0.12). "
     "Now, on a PC with OpenGL 4.3, a compute shader on the graphics card does that from the amplitudes stored with each dot, and the processor only sends the beat phase and the weights. A PC with only OpenGL 3.3 keeps the old way and nothing else changes: the app asks for 4.3 first and falls back to 3.3. "
     "The Performance card says which way your PC runs (\"OpenGL 4.3: GPU superposition on\" or \"OpenGL 3.3: CPU path\"), has a switch to compare the two, and the benchmark report prints it; scenes 25 to 28 run the jump and the three superpositions again with the old way. "
     "The physics is the same: the formulas are ported line by line and tested against the processor's for every dot. The graphics card works in 32-bit numbers, so a dot within rounding of a threshold (a few in a million) can come out differently; each such dot is tested to be that close. "
     "The tests also check the superposition against the textbook hydrogen formulas written out separately, and found that the lab's orbital normalisation is off by 3e-6 to 1.6e-5 in amplitude (not changed, reported). "
     "Not changed: the building of the dots of a jump (the random sampling is a chain that cannot run in parallel). "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. Scene 14 should drop from about 10 ms towards the 4 ms of one plain orbital."},
    {"0.13", "GPU update: phase colours on the graphics card (stage 1 of 4)",
     "The electron cloud's dots used to be coloured by the processor: for the 24 Wheel colours it went over all 48,000 dots once for each colour, in each of three depth bands. Now each dot carries only the number of its colour and the graphics card picks the colour. "
     "The picture is meant to be exactly the same (the same colours, the same depth bands, the same drawing order); the tests compare the old way and the new way dot by dot and, with a software OpenGL, pixel by pixel, and found no difference. "
     "This applies to every cloud view (Cloud, Cloud 3D, Slice), Classic and Glow dots, Sign and Wheel, every orbital and density. "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. The benchmark's scene 8 should drop towards scene 9, and scene 10 likewise; scenes 23 and 24 run them again with the old way for comparison. "
     "A switch in the Performance card, GPU colours (Atom Lab cloud dots), goes back to the old way (it is On again at every start). "
     "Two texts in the benchmark's scene list pointed at the wrong scene (scene 19 said to compare with scene 8, it is scene 2; scene 22 said scenes 16 to 18, it is 19 to 21) and the tick box said 11 extra scenes (it is 15 now): fixed. "
     "The next stages plan OpenGL 4.3, which needs a graphics chip of about 2013 or newer (Intel HD 4600, GeForce 600, Radeon HD 7000); older PCs will not run those versions. The app's version is now 0.13."},
    {"0.12", "Atom Lab: the accuracy clean-up (stage 2 of 2)",
     "This stage fixes data and wording that earlier versions found doubtful and only reported. Nothing about how the atom or the chart works has changed; some numbers have, and each is listed here. "
     "Nuclides with no half-life number were all described as \"seen only fleetingly\". The data does not say that. They are now described by what NUBASE2020 says: 369 only have an extrapolation from systematics, 84 have none, 11 only a limit, 6 an approximate value, 3 are only known to be unstable to emitting a particle. "
     "The one-line text and the decay readout no longer show an extrapolation or a limit as if it were a half-life (fluorine-31 used to say \"half-life 2 ms\", boron-16 \"half-life 4.6e-21 s\" for a limit), and the decay clock no longer makes such a nucleus decay on a timer: it stays as you chose it, and the readout says why. "
     "Krypton-78 and xenon-124 had half-lives that are in neither NUBASE2020 nor the mendeleev package, which both call them stable; the numbers had no source, so they are now shown as NUBASE2020 shows them (observationally stable, with its limits). "
     "113 superheavy half-lives had been rounded and marked \"approximate\" although NUBASE2020 gives plain measured values: they are now NUBASE2020's own. 16 more superheavy values came from an older evaluation and were up to 2.8 times off (seaborgium-269: 14 minutes before, 5 minutes now). "
     "For 644 nuclides NUBASE2020 lists a decay mode with no intensity (marked with a question mark); the odds leave it out, and the hover, the line in the panel and the chain now say so. "
     "The nucleus's shape estimate counts Z = 114 and N = 184 as magic numbers; they come from the web version with no source, so they stay but are now labelled in the nucleus card (286 nuclides depend on them). "
     "This finishes version 0.12."},
    {"0.12", "Atom Lab: decay chains walk the chart (stage 1 of 2)",
     "On the Chart of nuclides, the chosen nuclide now shows where it goes next. Every way it can decay is drawn as a line from square to square, in the colour of the mode: an alpha jumps two squares down and two left, a beta minus moves one square up and one left, a beta plus the other way. "
     "The likeliest way is thick and solid, the other ways whose odds are measured are thinner, ways below 0.1 % are faint hairlines, and a way whose odds are not measured is marked with a question mark. Uranium-238 reaches lead-206 in 14 steps along its likeliest path; bismuth-212 splits in two paths (beta minus 64 %, alpha 36 %) that meet again at lead-208. "
     "A nuclide whose half-life is only a limit, only an extrapolation or not given is marked with a cross: the chain stops there and says why, and no time is made up for it. A chain that reaches a stable nuclide names it, and says when it is only observationally stable (nothing seen, but a limit is listed). "
     "You can also walk the chain: one step at a time, or automatically. Each step says what happened (for example: alpha, two protons and two neutrons leave; uranium-238 to thorium-234) and uses the lab's real random draws, a lifetime from the exponential law with the real half-life and a branch from the real odds, so it is different every time. "
     "The walk is NOT real time. Uranium-238 really lives billions of years; the screen shows a step every 1.6 seconds, and each step says how many times faster than real time it was shown. The nuclide you chose stays where you put it. "
     "The decay clock that runs in the other views now adds up the listed odds to 1 (before, a rare way listed after a certain one, such as fission for uranium-238, could never happen). "
     "A fourth benchmark scene measures the chain drawn on the landscape. The app's version is now 0.12."},
    {"0.11", "Atom Lab: what the height means, and the valley and the island of stability (stage 4 of 4)",
     "The Chart of nuclides has a new choice, Height means. Besides the half-life it can now be the binding energy per nucleon, the quantity behind the whole chart: it rises steeply from hydrogen, reaches its highest value in the data at nickel-62 (8,794.56 keV per nucleon, with iron-58 and iron-56 next; the app reads this from the table, it is not typed in), and falls slowly towards uranium. That is why fusing light nuclei releases energy and splitting heavy ones does. The scale is linear from 0 to 9,000 keV per nucleon, and the legend says which height is in use. "
     "The numbers are from the 2020 atomic mass evaluation (AME2020). It marks 1,008 of the 3,557 nuclides as estimates, not measurements, including every nuclide above Z = 110, so those are drawn flat and pale and never as a height. "
     "Switching the height changes nothing else: the colours, the magic-number bands, the hover text and the chosen nuclide stay exactly as they were. "
     "The chart now names the valley of stability, the band of stable nuclides that the data already shows, and says it is measured. It also marks the heaviest known nuclides (Z = 118) and says that nothing has been observed beyond them. "
     "The island of stability is a prediction: theory expects a region of superheavy nuclides beyond the heaviest known that might live much longer than their neighbours. The app says so plainly and draws nothing there, because no source for where it lies could be reached from the project's tools; it says only that where it lies is disputed. "
     "This finishes the chart of nuclides (version 0.11)."},
    {"0.11", "Atom Lab: the chart of nuclides as a landscape (stage 3 of 4)",
     "The Chart of nuclides view has a new Tilt slider. At 0 it is exactly the flat chart of the last version; as you move it up, the chart turns into a landscape seen from a fixed isometric angle, and every nuclide becomes a block whose height is its half-life. The stable nuclides stand in a ridge along the middle, and the short-lived ones fall away on both sides like a shore. "
     "The height is a chosen scale, not a measurement: the logarithm of the half-life, from 10^-23 seconds (flat) to 10^32 seconds (the top), where each step up is ten times longer; the legend says so. Stable nuclides have no half-life, so they all stand at one fixed height above the top of the scale, meaning no decay seen, not a long number. "
     "A nuclide with no measured half-life, or only an extrapolated one, has no height at all: it is drawn flat, hollow or pale. A pale block is only a limit or an approximate value. "
     "The wheel zooms about the pointer, a drag moves the picture when zoomed in, and a reset button brings back the flat chart. Hover and click work on the block you see on top, including its sides. "
     "A benchmark scene measures the landscape. The next stage is about what the height can mean."},
    {"0.11", "Atom Lab: the chart of nuclides (stage 2 of 4)",
     "A new view, \"Chart of nuclides\", is a map of every nuclide the lab knows: 3,557 squares, neutrons across and protons up. Each square is coloured by how that nuclide decays (stable, beta minus, beta plus or electron capture, alpha, fission, proton or neutron emission), and the legend says in words what each colour means. The stable nuclides run along the middle like a valley, with neutron-rich nuclei on one side and proton-rich ones on the other. "
     "The lines are the magic numbers 2, 8, 20, 28, 50, 82 and 126, where a shell of nucleons is full. The nuclide you have chosen is marked with a box and cross-hairs, so you do not lose your place. Hover a square for its half-life, how it decays and with what odds, its spin, and whether the lab has a measured charge radius and a measured quadrupole moment; where the lab has no value, it says so. "
     "Click a square to choose that nuclide: the other views then show it. The arrow keys move one proton or one neutron, which is the natural way to walk a decay chain. While the chart is shown the chosen nucleus does not decay on its own, so it stays where you put it. "
     "The spins and the kind of each half-life (a measurement, a limit, only an extrapolation, or none) come from NUBASE2020, which is now listed under Where the facts come from. The colours are a convention of this lab, not a measurement; where a decay has several branches, the colour is the kind with the largest total odds. The chart shows what has been observed: it says nothing about the island of stability or about heights, which come in the next stages. "
     "The app's version is now 0.11."},
    {"0.11", "Atom Lab: Auto photons now say why they cannot jump (stage 1 of 4)",
     "With Auto photons on, a complex orientation (m = ±1, ±2, ±3) or a superposition used to leave the atom doing nothing, with no word about it. Now the lab says why, in the same words the Emit and Absorb buttons use, once for each reason (not again at every look), and again if you change the state or switch Auto off and on. Auto stays on and starts jumping by itself as soon as you pick a real orientation (px, pz, ...). "
     "Nothing else changed: real orientations jump exactly as before (the same photons in the same test run). "
     "Still true: the lab does not do photon jumps from a complex state or a superposition. Light can in fact take such a state to another level (the rule l changes by 1 still holds; the light's polarisation decides which m it ends in); the lab refuses because it does not model the polarisation, and whether to allow it is left to the owner's decision. The next stages of 0.11 are the chart of nuclides."},
    {"0.10.9", "Atom Lab: the nucleus's shape from measured quadrupole moments (stage 2)",
     "The shape of a nucleus now comes from a measured quadrupole moment wherever one can be used: the IAEA's recommended values (fetched 2026-10-04, 598 ground-state rows). A positive moment means stretched (like a rugby ball), a negative one flattened (like a pancake), and the card and the readout line say which the nucleus on screen is. "
     "Where there is no usable moment the lab keeps what it had, and now says which: a B(E2) value (11 isotopes) or an estimate; of the neutron slider's 4,252 isotopes, 498 now take their shape from a measured moment, 11 from B(E2) and 3,743 from the estimate. "
     "The table gives the spectroscopic moment, the one a laboratory measures in the nucleus's spin state; the shape needs the intrinsic moment in the nucleus's own frame. The two are turned into each other with the strong-coupling relation (K = I), which the card calls a model assumption, not a measurement; it is poor for light nuclei and near closed shells, and where it would need |β₂| above 0.6 the moment is shown and not used. "
     "A missing moment never means round: a nucleus of spin 0 (every even-even nucleus) or spin 1/2 has no spectroscopic moment whatever its shape, and the card says so. Values without a sign, with a tentative sign or spin, or typed wrongly in the file are listed and not used. A deformed nucleus keeps its volume, and its protons still give the measured charge radius and have exactly the measured quadrupole. "
     "Doubly magic nuclei, drawn round by expectation, are now labelled an estimate. The shape is static: real nuclei also vibrate and rotate. Halo nuclei are not in this stage; they wait for data."},
    {"0.10.9", "Atom Lab: every nucleus at its measured size (stage 1)",
     "The nucleus is now drawn at its measured size wherever there is a measurement, always on (there is no setting). The numbers are the IAEA Nuclear Data Section's table of measured nuclear charge radii (fetched 2026-10-04): 908 isotopes of 90 elements. A new card, \"Nuclear size (measured)\", shows the measured radius with its uncertainty next to the usual rule's (r = 1.2 A^(1/3) fm) and the difference as a number. "
     "A charge radius is the rms size of the protons' electric charge, and the nucleus is drawn as a uniform ball of the radius that has that rms size (an assumption: real nuclei have soft edges). The nucleon cloud is scaled so its protons have exactly the measured rms radius. Light nuclei change most: helium-3 is 47 % bigger than the rule's ball, lithium-6 53 %, the deuteron 83 %; lead-208 changes by 0.1 %. "
     "Helium-4 is smaller than helium-3 and lithium's radius falls to lithium-9 and rises at lithium-11: the card shows those measured numbers and claims no explanation. Where there is no measurement (3,345 of the 4,252 isotopes the neutron slider offers) the old rule stays and the card says \"no measured radius for this isotope; size from the usual rule\"; 48 isotopes have only a preliminary value in the file, which is named and not used. Hydrogen-1 keeps the CODATA proton radius. "
     "Limits, said on the card: a charge radius is a size, not a shape, and not a matter radius (neutrons carry no charge), so halo nuclei and measured nucleus shapes are not in this stage; they wait for data. The picture is static: real nuclei also vibrate and rotate. The text \"The nucleus is ~N× smaller\" and the chance of the electron being inside the nucleus use the new size."},
    {"0.10.8", "Atom Lab: real lifetimes and selection rules, from NIST (stage 3)",
     "A new card, \"Lifetimes and jumps (NIST)\", says how long the state on screen lives and how likely each jump is. The numbers are NIST's: the rate A of each jump (the chance per second that the state makes it by itself) with the accuracy rating NIST gives it. "
     "The lifetime is 1 divided by the sum of the rates of every allowed way down, and the share of a jump is its rate over that sum. \"Check a jump\" asks about any two shells: if the jump is allowed you see its rate, rating and share; if not, the card names the rule that forbids it (parity, l or j) and, where NIST lists it as a forbidden jump, shows NIST's rate under that name. "
     "Where NIST has no rate the card says so and shows no number; the lab never makes one up. The 2s state of hydrogen has no allowed way down: the card gives no lifetime for it and says that a real 2s state still decays by emitting two photons, which NIST does not list and the lab does not model. "
     "The numbers and the rules are tested against NIST's own data (every allowed line in it is allowed in the lab, none of the forbidden ones is, rates scale as Z\u2074 across 109 ions); the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.8", "Atom Lab: the Dirac formula's arithmetic made exact, and a Windows test fixed (stage 2)",
     "The Windows build of the previous stage failed in its new tests. The cause was in the tests, not in the physics: a reference value was computed in a way that throws away about 11 of 16 digits (it subtracts 1 from 0.99997), and only passed on Linux because the Linux compiler's extra-long numbers hid it; the lab's own value was the accurate one. "
     "The tests now use a reference that is exact on every computer. The lab's own arithmetic was also looked at for the same weakness: the small relativistic correction to each level was a difference of two nearly equal energies and lost up to 5 digits for hydrogen; it is now worked out directly, as are the percentage and the 1s energy in the heavy-atom readout. "
     "Nothing you can see changes: every line, level, note and number shown is the same text as before (checked for all 118 elements), and with the setting off nothing at all changed."},
    {"0.10.8", "Atom Lab: relativity as a real option, the Dirac levels (stage 1)",
     "New setting under Light spectrum, \"Relativity (Dirac levels)\", off by default. Switched on, the two spectrum cards use the energy levels of the Dirac equation (one electron, point nucleus) instead of the non-relativistic formula: a level then depends on j, the total angular momentum, as well as n, so 2p splits into 2p 1/2 and 2p 3/2 and every line splits, more and more as Z grows. "
     "The strip shows the split lines, a short list gives the split levels, and the Real spectrum card now compares the lines the lab draws, component by component, with NIST's measured lines: the lab lands 8 times closer for hydrogen, 10 times for He\u207a and 32 times for Fe\u00b2\u2075\u207a. "
     "What it does not do, and the app says so: the pictures (clouds, orbitals, outlines, radial graph) are still non-relativistic, and so are the readouts, the photon of a jump and the quantum beats; there is no Lamb shift, no other QED effect and no size of the nucleus, so NIST's lines still differ a little (in Dirac theory 2s and 2p 1/2 have exactly the same energy). "
     "With the setting off, nothing changes (compared byte for byte with the previous version). The numbers are tested against hand arithmetic and NIST's data; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.7", "Atom Lab: the real spectrum from NIST next to the lab's (stage 2)",
     "A new card, \"Real spectrum (NIST)\", puts the measured spectrum from the NIST Atomic Spectra Database next to the lines the lab predicts, as two separate comparisons. "
     "(a) The ion the lab actually models (one electron round a nucleus of charge Z: H I, He II, ... Fe XXVI): the lab's lines should land almost on the measured ones, and what is left is mostly fine structure and relativity, which the lab leaves out. This checks the lab. "
     "(b) The neutral atom of the element (Fe I, Ne I, ...), which is what \"the spectrum of iron\" means: it has Z electrons and the lab has one, so the lines are not expected to match. This shows the lab's limit. "
     "Where NIST has no data the card says so. The lines were fetched on 4 October 2026 and are stored in the app (a selection, stated in About); intensities are NIST's rough relative values and are drawn only within one spectrum. "
     "The numbers are tested; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.7", "Atom Lab: how far from the nucleus the electron is found (stage 1)",
     "The Atom Lab's cloud views have a new card, \"Where the electron is found\": a graph of how likely the electron is to be at each distance from the nucleus, for the orbital on screen. "
     "It marks the most likely distance, the average distance (the same number as the readout's \"theory\") and how far the 90 % outline reaches, with the distance in Bohr radii and in Å, scaled for the orbital and the nucleus. "
     "Every orientation of a shell has the same curve, and the card says so. It is not drawn while a superposition is shown, and it waits for the landing during a photon jump. One electron, non-relativistic, point nucleus. "
     "The numbers are tested against textbook formulas, exact peaks and the dots themselves; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.6", "A test that did not hold on Windows, fixed (stage 9)",
     "A test of the faster cloud jump compared the results byte for byte with numbers recorded on another compiler; the last bits of cos, log and exp may differ between compilers, so it failed on the Windows build although nothing was wrong. "
     "It now runs the old way and the new way side by side on the same computer and compares them exactly, and compares the recorded numbers with a stated tolerance. Nothing in the app changed."},
    {"0.10.6", "Atom Lab: the cloud jump made cheaper (stage 8)",
     "The benchmark on a real PC showed the Atom Lab's cloud jump (an electron jumping between orbitals in the cloud views) was the slowest thing in the app: about 33 frames a second for the 1.2 seconds it lasts, with a long hitch at its start. "
     "The jump now does the work that is the same for every dot once per frame instead of once per dot, and builds its new dots over the first four frames instead of in one. The pictures and the random dots are exactly the same as before (tested bit for bit). "
     "It was not timed in the cloud session: the speed has to be confirmed by running the benchmark again."},
    {"0.10.6", "Atom Lab: speed scenes added to the benchmark (stage 7)",
     "The benchmark has a new tick box, \"Include Atom Lab speed-risk scenes\": 11 scenes for the Atom Lab's heaviest paths (phase colours with Classic dots, a cloud jump, a superposition, the 90 % outline's first use and with the cut dragged, the nucleus view). "
     "They were not timed yet (the cloud session has no graphics card); the numbers will come from a real PC, and nothing was made faster in this stage."},
    {"0.10.6", "Atom Lab: the nucleus's real mass, and spectrum checked against NIST (stage 6)",
     "The nucleus's mass in the Atom Lab now comes from the measured binding energies (AME2020), not from the mass number times a mass unit, so the light of deuterium and tritium is worked out with the right mass (deuterium's red line moves by 1.3 trillionths of a metre; nothing else visible changes). "
     "The tests now check the spectrum against the Rydberg formula worked out by hand and against the NIST Atomic Spectra Database, and check more basic facts about the orbitals."},
    {"0.10.6", "Atom Lab: clearer, more honest words (stage 5)",
     "The Atom Lab now says \"non-relativistic\" for every atom and warns the same way, from the same atom (cobalt, Z = 27) on, in the readout, the hint and the spectrum note. Visible light is 380 to 780 nm everywhere. "
     "The average radius from the dots appears only once the cloud is full, and says it is an estimate. Two Learn steps no longer claim more than the lab can show (the neon button, and the 2013 electron-wave picture)."},
    {"0.10.6", "Atom Lab: the right outline, a correct superposition (stage 4)",
     "The thin 90% outline of a complex state (m = +1, +2 ...) now has that state's own shape, a doughnut for 2p with m = +1 (it used to show the dumbbell of the real orbital). A superposition of two states that overlap, such as the same state twice, "
     "is now properly normalised, so its cloud is the right density and all its dots are shown; the line over the picture says when the mixture is renormalised. Other superpositions are unchanged."},
    {"0.10.6", "Atom Lab: one energy, a steadier orbit, Fixed scale (stage 3)",
     "Hydrogen's energy now reads -13.60 eV in every view (it said -13.61 in two of them): the readout uses the same level as the spectrum, with the nucleus's real mass. In the Orbit view the electron no longer breathes at the fastest playback speed "
     "(the orbit is calculated in smaller steps; the speed itself is the same). With Scale set to Fixed the biggest clouds (n = 4) now fit in the picture. A new note says the energy is the one-electron, non-relativistic level."},
    {"0.10.6", "Atom Lab: exact constants (stage 2)",
     "The Atom Lab now uses the official CODATA 2022 values (read from NIST) for the Bohr radius, the Hartree energy, the atomic unit of time, the Rydberg energy, the masses and more, instead of the web version's rounded ones. "
     "The Bohr radius is 0.03 % larger than before, so the electron's circle is a hair bigger; everything else looks the same. The About page lists the constants and where they come from."},
    {"0.10.6", "Atom Lab checked for errors", "Atom Lab checked for errors, fixes to follow; nothing changed yet."},
    {"0.10.5", "Orbit Lab: more checks and clearer sources (stage 5)",
     "Orbit Lab: tests now compare the lab with NASA/JPL Horizons for the famous dates (Mars closest, 2003; Jupiter and Saturn, 2020) and for the 2026 equinoxes (the Earth card's \"next season\" is within 12 minutes), and the texts of the jump notes, the Real and Simple notes, the date line and the hidden-selection rule are tested too. Sources: the speed of light is JPL's; the Sun's 25.38-day turning period is Horizons'; comets' nucleus sizes are the Small-Body Database's (Encke 4.9 → 4.8 km). Numbers that have no named source are now marked rough on the cards (the Sun's surface temperature and its 25- and 34-day turning, the belts' counts and masses, NEOWISE's size), and About lists them. Nothing else changed."},
    {"0.10.5", "The official astronomical unit (stage 4)",
     "The Orbit Lab now uses the official astronomical unit, 149,597,870,700 m (the IAU value, as on NASA/JPL's astrodynamic parameters page). Before, it used the web version's rounded 149.6 million km, which is 2,129 km too long at 1 AU (64,000 km at Neptune). Every distance in the lab, the same orbits in Simple, the comets, the belts and the Oort cloud, is now 0.0014 % shorter; the times and the orbits' shapes do not change. Against NASA/JPL Horizons no planet got worse by more than that change can explain (Uranus and Neptune about 30,000 and 39,000 km farther off in their worst year, the others within 1,300 km)."},
    {"0.10.5", "Orbit Lab data fixes (stage 3)",
     "Some of the Orbit Lab's numbers were replaced by JPL's. Planet surface gravity is now JPL's value at the equator (Jupiter 2.5 g, Saturn 1.1 g, Uranus 0.9 g, Neptune 1.1 g; the old values were up to 14 % lower for Saturn) and the cards say \"at the equator\". The Sun's radius is the IAU 2015 value, 695,700 km (it was 696,000). Each of the six famous comets now takes all its elements from one JPL record (Halley's, Hale-Bopp's, NEOWISE's, Encke's and Swift-Tuttle's from the Small-Body Database, 67P's from Horizons for the same orbit at 2021) instead of a mixture; against Horizons on 1 January 2026 they are closer to the real comet (67P 62 million km before, 1.7 million now). A comet is still one fixed orbit, so it drifts from the real one away from its record's date, and Halley's next return comes out in January 2062 on this orbit (JPL's prediction is 28 July 2061). The Simple note now says how far Simple is from JPL (worst: Saturn 8 million km by 2100). About lists the new sources."},
    {"0.10.5", "Orbit Lab text fixes (stage 2)",
     "The Orbit Lab's texts were corrected after the check. Dwarf planets and comets now say that they move on a simple orbit fixed at one date, so they drift from their real places the further the date is from it; the jump note after 2050 about extrapolation is only for Simple (Real says its errors were measured for 1800 to 1 Jan 2100); the Voyager caption is right for 1977 to 1989; the Oort cloud travel times are worked out from the distances next to them; the habitable zone, frost line and lifetime are marked rough and planet cards no longer say whether they are in the habitable zone; the real Moon's range is written 1800 to 1 Jan 2100; the Venus 2012 tile jumps to the middle of the transit; the no-atmosphere temperature says it assumes 30 % reflected; the Moon card says the phase names between the four main phases are this app's own; the Real note is shorter. No number or orbit changed."},
    {"0.10.5", "Orbit Lab checked for errors", "Orbit Lab checked for errors, fixes to follow; nothing changed yet."},
    {"0.10.4", "The real Moon (Orbit Lab, Real planet physics)",
     "With Planet physics set to Real, Earth's Moon is now the real one: its direction, distance and phase on the date shown come from the lunar theory ELP2000-82B (a 1988 theory), checked against NASA/JPL "
     "Horizons (DE441): at the dates checked between 1800 and 2100 it is within 5.2 km (2.9 arcseconds as seen from the Earth) at worst. Select the Moon to see its phase (new moon, waxing crescent, first quarter and so on) "
     "and how much of it is lit. Earth is now drawn where it really is, a few thousand km off the Earth–Moon balance point, which is what moves smoothly round the Sun. The Moon is still drawn at the "
     "exaggerated distance used for moon orbits, so you can see it. The other moons, and Simple, are unchanged; outside the years 1800 to 2100 the Moon goes back to its simple ellipse. At very fast Time "
     "speeds the Moon's orbit is lit instead of the Moon (as for every moon) and Earth is held at the balance point so it does not flicker. Real is still an option, and Simple is still the default."},
    {"0.10.3", "Real planets start from JPL's data (Orbit Lab)",
     "With Planet physics set to Real, the planets now start from NASA/JPL Horizons' positions and velocities (the DE441 ephemeris) for 1 January 2026 12:00 TDB, instead of the lab's own simplified orbital "
     "elements. Measured against Horizons' tables for 1 January of every year from 1800 to 2100, the worst error of any planet's position is now under 30,000 km (Mercury) with relativity off, and under 8,000 km "
     "for the others; before, the worst errors were millions of km for the inner planets and Jupiter, and over a hundred million km for Saturn, Uranus and Neptune. The lab's calendar dates are treated as TDB (the time scale "
     "of the JPL data) with no conversion from UTC; the note under Planet physics says so, and gives the worst errors. The General relativity (1PN) tick box is now off by default: measured against JPL, the Sun-potential "
     "term gets Mercury's extra turn right (about 43 arcseconds per century) but the motion along the orbit slightly wrong, so it moves every planet's position away from JPL (Mercury, worst case 1800 to 2100: "
     "70,045 km with it, 29,231 km without). The box is still there, and a setting you saved earlier is kept (press Defaults once to put it off). Real is still an option, and Simple is still the default, "
     "until it has been tried on a PC. Still left out: moons, dwarf planets (Pluto is not in) and asteroids pulling on the planets. Simple is unchanged."},
    {"0.10.2", "Phase colours, quantum beats and smooth photon jumps (Atom Lab)",
     "The Atom Lab can now show the phase of the electron's wave. New orientations m = +1, −1 (and ±2, ±3 for d and f) are complex waves, and a new setting, \"Phase colours: Sign / Wheel\", "
     "colours each dot by the phase of the wave with a colour ring (Sign is the old two-colour look and is still the default). New: \"Superposition\", two orbitals at once, such as 1s + 2p. "
     "The cloud then changes in time at the beat frequency (E₂ − E₁)/h: 1s + 2p sloshes back and forth, 1s + 2s breathes, 2s + 2p does not change at all (same energy here; fine structure and the Lamb shift are not modelled). "
     "Real beats are far too fast to see, so the picture is slowed down and the line under it says by how much. Photon jumps are now smooth: in the cloud views the cloud slides from one shape to the other, "
     "and in the Orbit view the electron glides between the circles (only to help the eye: Bohr's model has no path between circles). An emitted photon appears when the jump ends, and an absorbed photon starts the jump when it arrives. "
     "This is the textbook picture of a transition as a mixture of two states, not a calculation of how a photon is absorbed. With Reduced motion on, jumps are instant."},
    {"0.10.1", "General relativity, the Sun's wobble, and two fixes",
     "With Planet physics set to Real there is a new tick box, \"General relativity (1PN)\" (on by default), that adds Einstein's first-order correction to the Sun's pull, so Mercury's "
     "orbit turns about 43 arcseconds per century more, as measured. It is an approximation with the Sun's field only: it leaves out relativity between the planets, the Sun's spin and "
     "shape, and higher orders. Also in Real: the picture is drawn round the true balance point (the barycentre) from the planets' motion, the Sun's own path round it is drawn as a "
     "trail, and \"Exaggerate the Sun's wobble (x50)\" (on by default) draws that path 50 times too big so you can see it (it is labelled exaggerated, not to scale). "
     "Fixed: the Milky Way band no longer stops in the middle of the sky or goes missing when you turn the 3D view (most of it had been dropped), and the two secret themes show "
     "their names once unlocked instead of \"???\"."},
    {"0.10.0", "Planet physics: Simple or Real",
     "A new setting in the Orbit Lab, \"Planet physics\", has two choices. Simple (the default) is what the lab always did: every planet moves alone round the Sun and the planets do "
     "not pull on each other. Real lets the Sun and the eight planets pull on each other with Newtonian gravity, moved by a Wisdom-Holman integrator (a method made for planets). "
     "It leaves out relativity and the pull of moons, dwarf planets and asteroids on the planets, and the picture stays centred on the Sun. The start values are the lab's own simplified "
     "orbital elements for 1 January 2026, not NASA's ephemeris, so far from that date the planets drift away from their true positions (Mercury by about 12 degrees in 1800). "
     "Dates and Time speed work in both. The benchmark always uses Simple."},
    {"0.9.9", "Theme grid, clearer history button, readable nucleus label",
     "The 🎨 Theme menu is now a grid of cards, three in a row, each with its colour dot, its name and a tick on the one in use; the short description of a theme shows when "
     "you hold the mouse over its card, and names are never cut off, even at Extra large text. In the Atom Lab the header button that used to say \"Update 7.7.3\" now says "
     "\"Web Atom Lab history 7.7.3\" (it shows the history of the web Atom Lab that this Atom Lab follows), and the window it opens has the same name. The nucleus label in the "
     "Atom Lab (for example \"Cadmium-114 · 48p, 66n\") has a thin edge in the page colour, so it can be read over the electron cloud on dark and light themes. "
     "Sources added to About: the papers behind the star masses and the textbooks for the hydrogen-like orbitals."},
    {"0.9.8", "A 90% outline for the electron cloud",
     "A new setting in the Atom Lab's Cloud, Cloud 3D and Slice views, \"90% outline\" (Off by default), draws a thin line round the smallest region where the electron is found "
     "90% of the time. It is not a sphere: for p, d and f orbitals it follows the real shape (dumbbells, rings, clovers), and an orbital with nodes can have several surfaces. "
     "In Slice it is the line where the cut plane crosses that surface, in Cloud it is the region's shadow seen from the side, and in Cloud 3D it is a faint wireframe that turns "
     "with the cloud. It uses the same model as the cloud: one electron, no relativity, hydrogen-like orbitals; atoms with more electrons have different shapes. "
     "The benchmark always has it off."},
    {"0.9.7", "Reset to today keeps your view choices",
     "\"Reset to today\" (and the Today buttons in the Journey card and the Learn window) now sets only the date and time back to today, and clears the selection and Follow. "
     "It no longer changes your zoom or the Moons, Dwarf planets, Comets and Belts switches (it already kept your Time speed). Defaults still puts everything back. "
     "There is also a new roadmap file, ROADMAP.md, in the project, and the Windows download page is set up to say what the version is and how to get past Windows' warning for an app that is not code-signed yet."},
    {"0.9.6", "Quarks readable on light themes",
     "In the Atom Lab's quark zoom, the dark patch that was drawn behind the glowing quarks is gone on the light themes (Light and Paper), where it looked like a dark "
     "smudge. There the quarks, their halos, the gluon field and the sea pairs are drawn in darker, more saturated colours that read on a light page, and the red, green "
     "and blue colour charges are still easy to tell apart. On the dark themes nothing changes: the quarks glow as before. Only the drawing changed, not the physics."},
    {"0.9.5", "A smooth Milky Way, and star masses",
     "Orbit Lab: the Milky Way band in the night sky is now one smooth band with soft edges instead of three overlapping stripes, so there are no joints or "
     "stray grey streaks when you zoom far out; it is in the same place on the sky as before, and with the constellation names on it has its own label, "
     "\"The Milky Way (the plane of our galaxy, simplified)\". The far-away stars now show their mass under the name, in solar masses, taken from published "
     "papers: Alpha Centauri, Sirius, Procyon (each star of the pair), Proxima Centauri, Fomalhaut, Arcturus and Sagittarius A*. Masses worked out "
     "from stellar models say \"from models\", and a very uncertain one (Proxima) shows its range. Stars whose mass could not be checked against a "
     "published source are left without one. A line near the bottom says the masses are published estimates and some are uncertain."},
    {"0.9.4", "A glowing, denser electron cloud",
     "In the Atom Lab's Cloud, Cloud 3D and Slice views the electron cloud is drawn as soft glowing dots instead of tiny hard squares: where the electron is most likely to be "
     "the light adds up and glows, and thin places fade. In Cloud 3D, size and brightness change smoothly with depth. The nodes, where the wave is zero, stay as clear gaps, "
     "and the blue and orange phase colours are kept (on light themes the dots are darker so they show on white). Two new settings, Cloud style (Classic is the old look, "
     "Glow is new) and Cloud density (Normal 12,000 dots, High 24,000, Ultra 48,000), are remembered and put back by Defaults. More dots show the same probability more "
     "smoothly; they do not change the physics: the dots are found in exactly the same way, and the total light of the cloud is the same at every density. The benchmark's "
     "Cloud 3D scene uses Classic and Normal, so its results stay comparable with older ones, and the stress scenes got a Glow, Ultra Cloud 3D scene."},
    {"0.9.3", "Four fixes: benchmark scenes, guide buttons, benchmark input lock, Time speed on Today",
     "1) The Atom Lab benchmark scenes no longer pick up your own Atom Lab settings: they start from the lab's defaults, so \"Bohr 1913\" can no "
     "longer hide the nucleons (earlier results for the nucleus, one-nucleon and biggest-nucleus scenes taken with Bohr 1913 on are not valid). "
     "The result now says which settings each Atom Lab scene used. "
     "2) The two \"What am I even looking at?\" buttons in the Atom Lab panel no longer clash, so the red \"conflicting ID\" message is gone. "
     "3) While the benchmark runs, nothing but Cancel and Esc works: not the panels, the buttons at the top, the keys, nor the picture. "
     "The result says so. The window title also follows the lab that is on screen. "
     "4) \"Reset to today\" and the other Today buttons keep your Time speed; only Defaults puts it back."},
    {"0.9.2", "Alpha label, drop-down text fits",
     "The Orbit Lab's drop-downs (Sizes, Trails, Stars in the background, Constellations) show only the short name in the closed box, so "
     "it is never cut off at big Text sizes; the explanation is under each choice in the open list. "
     "The app is now labelled alpha: features are still being added. Beta will follow when everything planned for 1.0 is in, then the plain release."},
    {"0.9.1", "Sliders that look like sliders",
     "The Time speed and Zoom sliders have a clearly visible track and a bigger, easier-to-grab thumb in every theme, and their values "
     "are written above them instead of under the thumb. The \"Drag rotates the view\" button is no longer cut off."},
    {"0.9", "Keeps running while the window is moved or resized, a checked GPU timer, stress scenes",
     "The simulation and the drawing no longer freeze while you drag the window by its title bar or an edge (Windows pauses the app's own "
     "loop then; the app now draws from the window system's own refresh signal instead). The picture and the panels follow the new size "
     "while you drag, and after you let go nothing jumps or catches up. The Performance card counts the frames drawn during a drag. "
     "The benchmark's GPU time now covers the whole frame's drawing, and the timer is checked with a known load before the test: if it "
     "cannot be trusted, the result says \"not reliable\" instead of a number. A checkbox \"Include stress scenes\" adds two much "
     "heavier scenes (the biggest nucleus, and eight times the belt rocks) after the four normal ones."},
    {"0.8", "Benchmark, history with the native updates on top",
     "A Benchmark button in the Performance card (both labs): it runs four fixed scenes (the heaviest Orbit Lab view, the uranium-238 "
     "nucleus, Cloud 3D, one nucleon) with VSync off at the window's size, 2 s of warm-up and 8 s counted each, and shows the average "
     "frame time and fps, the 1 % low, the worst frame, the CPU work and the GPU time (where the driver gives it). The result is "
     "copied to the clipboard and saved as a text file next to the saved pictures, with the graphics card, the OpenGL version and the "
     "window size on top. It gives all your settings back, and Esc stops it. The 🕰 history now lists the native updates first, then "
     "the web's own history with its note."},
    {"0.7", "fps counter in both labs, the window remembers itself, icon",
     "The frames-per-second counter is now in the Atom Lab too (it was only in the Orbit Lab): top right of the picture, in the "
     "web's small see-through rounded box, in every theme and text size, and it is drawn in one place for every lab. It is not in "
     "a saved picture. The window opens with the size and place it had last time, and maximised if it was; if the place is no "
     "longer on a screen it opens centred, and it never starts smaller than 800 x 500. F11 turns full screen on and off. "
     "PhysicsLab.exe has an icon (an atom) on the file, in the taskbar and in the window. If the window or OpenGL 3.3 cannot "
     "start, a message box now says what is wrong and suggests updating the graphics driver."},
    {"0.6", "Save image, timeline pictures, cleaner text, small windows",
     "📷 Save image, next to Defaults in both labs, saves exactly the picture you see (the readout included, in every theme, "
     "grey in Monochrome) as a PNG in your Pictures folder (Downloads if that fails), named with the lab and the date and "
     "time. The button says Saved ✓ and the path is shown under it; if saving fails it says so. The Orbit Lab's timeline "
     "now has the web's small step pictures, like the Atom Lab's. The words that name the Black Hole Lab, the Spacetime Lab "
     "and the slingshot game are taken out of About, the Units note, the Sagittarius A* card and the guides, and the dead "
     "\"Try the slingshot game\" button is gone. Small windows: long button labels wrap instead of being cut off, and the "
     "Settings window and the big periodic table never get bigger than the window."},
    {"0.5", "The web app's own features: settings, themes, header, history",
     "Settings as in the web version: text size (Small, Normal, Large, Extra large), font (Theme default, Serif, Easy to read, "
     "Rounded, Monospace; Atkinson Hyperlegible and Nunito are built in, under the SIL Open Font License), units, motion (Auto "
     "follows Windows' animation setting; Reduced starts the labs paused, makes camera moves jump, stops Cloud 3D spinning), "
     "Reset my progress with the web's two pop-ups, and Developer > Unlock everything. The 🎨 Theme menu has the web's eleven "
     "themes, including the secret Synthwave and Monochrome; the panels, cards, text and pictures all follow it. Everything is "
     "remembered between runs in the app-data folder: the look, units, unlocks and every setting of both labs. A header across "
     "the top has the web's buttons: 🎨 Theme, 🌐 language (English; Eesti keel is coming), 👤 About (the web's page with its "
     "sources), ⚙ Settings, 🕰 history (the web's history, and this list) and the lab's Update badge with its changelog. "
     "Under it, the 💡 Did you know? line with all the web's facts (tap for the next one; it waits while the mouse is on it) "
     "and its hidden 🎁 surprise, and the tabs: 📚 Learn lists every guide and timeline of both labs and opens them in their "
     "lab; 🏆 Achievements shows Neon Nights, Sharp Eyes, Half-Life and Galactic Center, locked until earned, with the "
     "web's pop-up when one is earned. Point of No Return is left out: it belongs to the Black Hole Lab, which is not in "
     "the native app. The \"monochrome\" word in the Atom Lab guide unlocks the Monochrome theme and Sharp Eyes. "
     "Also: the Sun has the web's soft glowing patches that turn with it, antiquark rings in Cinematic add up their light "
     "as on the web, and the Atom Lab readout lost faint grey marks it had in light themes."},
    {"0.4", "Atom Lab",
     "The second lab: pick Orbit Lab or Atom Lab at the top of the panel (each keeps its state while the other is shown). "
     "Atom Lab is the web version's, feature for feature: any nucleus from hydrogen to oganesson with one electron, in four "
     "pictures (Orbit, as Bohr's 1913 circles or with the real nucleus; Cloud; Cloud 3D; Slice), photons that make the electron "
     "jump and build a spectrum, a zoom from the whole atom down to the protons and neutrons and the quarks inside them "
     "(Cinematic, Accurate or Classic quarks), every known nuclide with its real half-life and decay odds, decays happening on "
     "their own with a decay history, the periodic table with element cards and facts, the light spectrum, and the "
     "Temperature card with its little box of atoms. The four Learn guides are here too: What am I even looking at?, Scale, "
     "Weight and the 22-step timeline, whose buttons set the lab up. "
     "The physics uses the web's constants and steps (SI units inside), and the tests compare it with the web's own code: "
     "the electron's orbit, the sampled cloud, photon jumps and decays step by step with the same random numbers, and every "
     "text word for word. Cloud dots, glows and gluon particles are drawn as instanced shapes, a few draw calls for thousands. "
     "Units follow Settings (°F, mph, miles where the web converts them). Defaults asks first; Reset starts a fresh atom; "
     "More / Less info is remembered. The Performance card is on the Atom Lab panel too, with the picture's draw calls."},
    {"0.3", "Orbit Lab complete: Learn guides, Settings with metric or imperial, watch a planet turn",
     "New ⚙ Settings: show distances, speeds and temperatures in metric (km, km/s, °C) or imperial (miles, mi/s, °F). "
     "Every line of every facts card follows it, including the km/h and °C inside the notes, and so does the scale bar; "
     "the choice is remembered. The lab still works in metres and seconds inside. The Defaults button now asks before it "
     "puts Orbit Lab's settings back to how they started. "
     "With Stars set to Off, the picture now follows Windows' light or dark theme, as the web version follows the page "
     "theme. The Journey's last caption is now the same as in the web version. "
     "New Learn card with the three guides of the web version, word for word: What am I even looking at? (one page "
     "with a row of headings to jump to, following your 2D/3D and Sizes choice and the live date), Why do we have "
     "seasons? (with the lab's own Earth: day length and noon Sun height from pole to pole, and each planet's lean), "
     "and the Solar System timeline, whose ▶ buttons take the lab there. "
     "New button in the View card: pick a planet and press Slow time to watch it turn; time slows down so one turn "
     "takes about 24 seconds, and the view follows and zooms in on it."},
    {"0.2", "3D view, night sky, belts, spinning planets, Journey and Jump to a date",
     "The planets now really spin, with the real IAU axes and rates: Earth shows its real coastlines, the others a grid of "
     "latitude and longitude, each with a dashed axis and N; Saturn's rings tilt with its axis. The cards say how long a turn "
     "takes and how far the axis leans, the Sun's card how it spins, and Earth's card which season it is. "
     "Journey and dates card: the Journey zooms out from Earth to the centre of our galaxy, and when it ends (or you touch "
     "anything) all your settings and your view come back. Jump to a date puts everything where it really was or will be "
     "from 1800 to 2100 (Greenwich time); a date outside that range, or one that does not exist, now gets a note instead of "
     "a silent jump. "
     "New 3D view (View card): drag to turn it, right-drag or Shift-drag (or the Drag button) to move it, Spin to turn it "
     "slowly. Nearer things look bigger, and dashed lines drop from each planet to the flat plane of the orbits. "
     "Behind everything is the real night sky: the 2,874 brightest stars at their true directions and colours, the band "
     "of the Milky Way, and (Sky card) constellation lines and names. Zoom far out to find the far-away things at their "
     "real distance: the nearest stars, the Pleiades, the Orion Nebula, the galactic centre and the two Voyagers, all "
     "clickable and in Go to. "
     "The asteroid belt (with its Kirkwood gaps), Jupiter's Trojans, the Kuiper belt and the Oort cloud are here, "
     "with the same 6,000 stand-in rocks as the web version (same random seed, same orbits; the tests check they sit in "
     "the same places). Each belt is drawn with a single draw call, and the rocks are only moved when time moves. "
     "Click a belt's name or its Go to tile for its facts; a Belts and Oort cloud switch is under Solar System extras. "
     "A new Performance card shows how long each frame takes (the last one, the average and the worst of the last "
     "240 frames, with a small graph) and how much of it is the app's own work. A VSync switch turns off waiting for "
     "the screen, so you can see the real headroom instead of the monitor's refresh rate. Each Windows build is now "
     "also published on one fixed GitHub Release page, tagged \"latest\"."},
    {"0.1", "First native version: Orbit Lab (pilot)",
     "A native Windows version of Physics Lab, written in C++ with SDL3, OpenGL 3.3 and Dear ImGui. "
     "It has Orbit Lab only: the real Solar System where everything is today (the eight planets, the five dwarf planets, "
     "fourteen moons and six famous comets), with the same physics, numbers and method as the web version (leapfrog steps "
     "around the Sun, NASA/JPL orbital elements). Play and pause, Time speed, zoom with the mouse wheel or the slider, drag "
     "to move the view, click anything for its facts, Go to tiles, the habitable zone, distance rings, trails, the scale bar "
     "and the barycentre. Not here yet: the 3D view, the night sky and far-away things, the belts and Oort cloud, spinning "
     "planets, Journey, Jump to a date, Learn and the other labs."},
};

// The web Orbit Lab's changelog (the native Orbit Lab follows web Orbit Lab 7.7.2).
const Entry kWeb[] = {
#include "web_orbit_changelog.inc"
};
// The web Atom Lab's history (the native Atom Lab follows web Atom Lab 7.7.3).
const Entry kWebAtom[] = {
#include "web_atom_changelog.inc"
};
const char* const kWebAtomNote =
#include "web_atom_changelog_note.inc"
    ;

}  // namespace

// The stage word shown after the version number (change this one word to move from alpha to beta, then "" for 1.0)
constexpr const char* kStage = "alpha";

const char* appVersion() { return PL_VERSION; }
const char* appStage() { return kStage; }
std::string appVersionLabel() { return *kStage ? std::string(PL_VERSION) + " " + kStage : std::string(PL_VERSION); }

const NativeEntry* nativeHistory(int& count) {
    count = int(sizeof kNative / sizeof kNative[0]);
    static NativeEntry out[sizeof kNative / sizeof kNative[0]];
    for (int i = 0; i < count; i++) out[i] = {kNative[i].version, kNative[i].title, kNative[i].text};
    return out;
}

// The web's changelog pop-up for the lab on screen ("Orbit Lab · changelog"): each update's number, title and text
void labLogWindow(bool* open, int lab) {
    if (!*open) return;
    const app::LabInfo& L = app::kLabInfo[lab];
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = ImGui::GetFontSize() / 16;
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x - 32, 620 * s), vp->WorkSize.y * 0.8f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string title = (lab == 1 ? std::string(app::kAtomHistoryName) + " " + L.update : std::string(L.name) + app::kLogTitleTail) + "###labLog";
    if (ImGui::Begin(title.c_str(), open, ImGuiWindowFlags_NoCollapse)) {
        const Entry* rows = lab == 1 ? kWebAtom : kWeb;
        const int n = lab == 1 ? int(sizeof kWebAtom / sizeof kWebAtom[0]) : int(sizeof kWeb / sizeof kWeb[0]);
        const ImU32 accent = col(look().c.accent), line = col(look().c.line);
        for (int i = 0; i < n; i++) {
            // .logRow: the number in a 52-point column (accent, bold), then the title and the text (muted, smaller)
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float x1 = p.x + 62 * s, w = ImGui::GetContentRegionAvail().x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(p.x, p.y + 10 * s), accent, rows[i].version);
            ImGui::SetCursorScreenPos(ImVec2(x1, p.y + 10 * s));
            ImGui::PushTextWrapPos(p.x + w - ImGui::GetWindowPos().x);   // window-relative
            ImGui::PushFont(fonts().bold, 0);
            ImGui::TextUnformatted(displayable(rows[i].title, fonts().bold).c_str());
            ImGui::PopFont();
            ImGui::SetCursorScreenPos(ImVec2(x1, ImGui::GetCursorScreenPos().y + 2 * s));
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 13 / 16);
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
            ImGui::TextUnformatted(displayable(rows[i].text).c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PopTextWrapPos();
            const float y = ImGui::GetCursorScreenPos().y + 10 * s;
            if (i + 1 < n) dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + w, y), line);
            ImGui::SetCursorScreenPos(ImVec2(p.x, y));
            ImGui::Dummy(ImVec2(w, 0));
        }
        const char* note = lab == 1 ? kWebAtomNote : app::kOrbitLogNote;
        if (*note) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
            ImGui::TextWrapped("%s", displayable(note).c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

}  // namespace ui
