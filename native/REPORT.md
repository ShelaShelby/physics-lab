# Native Physics Lab 0.10.7 (alpha) — stage 1 report: the radial probability graph

New card "Where the electron is found" in the Atom Lab's cloud views (above Photons). `index.html` untouched (sha256 e5501387ae64…). The 0.10.6 audit is closed; its three deferred optimisation items are on ROADMAP.md with their measured numbers and were not worked on.

## What the graph shows and how it is computed (`src/atom/radial_graph.*`, no drawing in it)
- **Convention:** P(r) = r²|R_nl(r)|², so **P(r) dr is the probability of being between r and r + dr** (any direction); the area under the whole curve is 1. r in Bohr radii (ticks) and Å (in the legend), P in 1/a0. The scale follows the orbital and Z: P_Z(r) = Z·P₁(Zr), the right edge is where 99.9 % of the probability lies (the axis note says so; the area up to the edge is 0.999).
- **The lab's `radial()` is not the whole R_nl:** it is sgn·L^(2l+1)_(n−l−1)(2q/n)e^(−q/n), with no q^l (that factor is in the angular polynomial) and no normalising constant. I use q^(2l+2)·radial² and normalise it by the closed form I = (n/2)^(2l+3)·2n(n+l)!/(n−l−1)! (the hydrogen radial normalisation, Griffiths ch. 4), not by the curve's own quadrature. The tests check this.
- **Marks:** (1) most likely distance = the highest maximum, found as exact roots of dP/dr (sign changes of R·((l+1)R + qR'), then bisection), not read off the 401 points; (2) average distance = `meanRadius(n,l,Z)` = a0(3n² − l(l+1))/2Z, **the one function the readout's "theory" now calls** (the readout text is unchanged); (3) the 90 % mark = `outlineOf(state).rmax / Z`, the same number the picture's outline uses.
- **Complex states:** the radial part does not depend on m; the card's note says "every orientation (m = 0, ±1, …) of a shell gives the same curve" and the tests check that all m of a shell give an identical curve.
- **Superposition: hidden, with a one-line explanation.** Why: the mixed state's radial distribution is not one fixed curve for every pair (it beats in time for same-l different-n pairs, and an overlapping pair is the renormalised density), so drawing the single-orbital curve would not be what the dots are sampled from. During a photon jump the card keeps its height and says "changing orbital" until the landing (`AtomLab::radialMode()`, tested).
- **Honest wording found while checking:** the outline's reach is **not** the radius holding 90 % of P(r) except for s orbitals. Measured at Z = 1 (r holding 90 % of P / outline reach, a0): 1s 2.662 / 2.671; 2s 9.126 / 9.135; 2p 7.994 / 9.054; 3p 18.39 / 20.61; 3d 15.80 / 18.83; 4p 32.59 / 36.16; 4f 25.99 / 31.39. The mark is therefore labelled "90 % outline reaches" and the note says it is not in general the distance inside which 90 % of P(r) lies.
- Computed when the orbital or Z changes (401 points, plus a scan of a few tens of thousands of evaluations for the edge and the maxima; not timed), never per frame. **Cost to know:** the graph asks for the state's outline once per new orbital, so the outline's first-use cost (scene 16: worst frame 19.93 ms, deferred) can now fall on an orbital change with the outline Off. Not measured.

## Tests (written first, against a stub that crashed; then the code). Linux GCC Release
References are independent of the lab's `radial()`: the textbook closed forms R10…R32 typed in the test (and checked there to be normalised and orthogonal: worst 2.67e-9 and 9.43e-10 with Simpson h = 0.01), Simpson quadrature, exact peaks, the 1s closed-form 90 % radius, and the dots.
- P(r) against the closed forms, 6 shells × Z 1, 2, 26: worst relative difference 1.98e-14 (guard 1e-12).
- Area = 1, 10 shells × Z 1, 2, 26 (Simpson, 20,000 intervals): worst 2.2069e-8 (guard 2.23e-8). Stored 401-point curve (trapezoid) against Simpson of the exact P: 3.8113e-7 (guard 3.85e-7). Right edge holds 0.999: worst 9.2e-9 (guard 9.30e-9).
- Peaks: 1s at a0/Z and every l = n−1 at n²a0/Z: measured error 0 (guard 1e-12, rounding level). The highest of the 401 points is within 0.47 of a step of the peak.
- ⟨r⟩ by quadrature of rP against a0(3n² − l(l+1))/2Z: worst 4.5654e-10 (guard 4.62e-10); the graph's mean is `meanRadius`; the readout's "theory" text equals the graph's ⟨r⟩ for 30 cases.
- Maxima: n − l for all ten shells (exact roots and on the stored curve), n − l − 1 nodes where P falls to ≈ 0.
- 90 % mark = the outline's own rmax / Z for all 30 real orbitals and for the complex states of 2p, 3p, 3d, 4d, 4f (to 1e-12); 1s against the closed-form 90 % radius 2.661160 a0: outline 2.670809, 0.05966 of a grid step off (guard 0.0603).
- Dots: Kolmogorov distance between 12,000 dots' distances and the graph's cumulative curve, 60 seeded labs (15 states incl. m = ±, Z = 1, 3, 26): mean 0.0407, worst 0.105906 (guard 0.1070). Z = 3 and 26 give exactly the Z = 1 distances (same walker). The spread is the Metropolis chain's correlation (like the sampled ⟨r⟩ test), not an offset: no shell stands out.
- Guards are measured + 1 % except two at rounding level (closed form, peaks), stated in the test.
- **Proof it catches mistakes** (temporary, reverted, full atom run): forgetting the q^l factor → 7 failed (closed forms, area, ⟨r⟩, peak, dots …); dropping the factor Z → 5 failed; 90 % mark not divided by Z → 2 failed.
- Counts: orbit 1,540; **atom 18,299** (was 18,278: +21); app 1,515; nbody 37; jpl 55; moon 24; moon_real 59. All seven pass. No old test or saved reference changed.

## Sources
About "Where the facts come from" already lists Griffiths and Bethe & Salpeter for "the average distance of the electron, the radial probability": **no new entry**. The closed forms R10…R32 were typed from the textbook table from memory of its form and verified numerically (normalised, orthogonal) rather than read from a copy; the 1s share 1 − e^(−2q)(1 + 2q + 2q²) is derived by parts (its derivative is 4q²e^(−2q)).

## Not compiled, not seen, not tested
- `atom_panel.cpp` (the card: `radialPicture`, `radialCard`) is not built here (no imgui in the cloud). I compiled only the new functions against hand-made stand-ins of the ImGui types (`-Wall -Wextra`, syntax and types only): clean. Windows CI builds the app. **Never seen on a screen:** layout, theme colours, text sizes, overlap of ticks and legend, the 11 pt text at large text sizes, whether the font draws "a₀" and "Å" (the code avoids ⟨ ⟩, which no other screen uses). OWNER-TODO has the check list.
- No speed or Windows behaviour claimed. The comparison of the graph with the picture's outline is by numbers only.

## Windows CI result
Run 37192475980 on commit ec4f629 (Native Windows build, windows-latest, Visual Studio 64-bit): **success**. Configure, Build (the whole app, so `atom_panel.cpp` with the new card **compiled on MSVC**) and "Physics unit tests" (all seven programs) passed. I did not read the printed counts from the log, so "18,299 on Windows" is not claimed. Compiling is not seeing: the card has still never been looked at on a screen (OWNER-TODO).
