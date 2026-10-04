# Native Physics Lab 0.11 (alpha) — stage 1 of 4: Auto photons say why they cannot jump

Bug fix only, Atom Lab only. `index.html` untouched (sha256 e5501387ae64…). No helper agents. Stages 2-4 (the chart of nuclides) not started. The CMake/app version number is still 0.10.9; the history entry is labelled 0.11.

## The bug
`AtomLab::advance`, the Auto block (atom_sim.cpp, was ~line 605): when `targets(-1)` and `targets(1)` were both empty nothing happened and nothing was said. `targets()` is empty for a superposition and for any complex state (m = ±1, ±2, ±3) outside the Orbit view. The manual Emit/Absorb buttons explained themselves (`manualJump`, three messages); Auto did not. **Reproduced first** by the new test on the unfixed code: no jump (correct) and no message (the bug), 4 states.

## The fix
- New `AtomLab::whyNoJump(dir)` holds the three messages (superposition 5.5 s, complex state 6.5 s, "lowest level / can't emit / highest level" 4.5 s). `manualJump` now calls it: **same words, same times as before** (byte-compared by the test).
- Auto, when both directions are empty, sets `s.msg` from `whyNoJump` **once per reason**: `s.autoSaid` remembers the text given. It is cleared when a jump starts, when Auto is switched off/on (`toggleAuto`), and by `init()` (any change of orbital, orientation, view, element, superposition). So: once when it first finds nothing; again only after a change of situation or a new Auto switch-on. The message lasts as long as the buttons' own (6.5 s) and then is gone; the atom stays still, Auto stays on.
- **Why Auto is not switched off** (a choice, in DECISIONS.md): Auto is the user's setting and today it resumes by itself when a real orientation is picked. **Why not a permanent label:** the brief said once; a persistent "Auto: waiting" label would change `autoText()` and the panel (not compiled here). Owner may overrule.
- No random number is used differently (the wait is still drawn once per look), so saved runs cannot move.

## Other silent dead ends
- `manualJump` returns silently while a jump or photon is under way (`s.trans || s.photon`): **left as is** (the web does the same; the button press is simply ignored, the jump is visibly running).
- Auto does nothing while paused (`advance` runs in `step`): intended (Pause freezes the lab).
- `atom_bench.cpp` tests `targets()` empty and skips: a benchmark, not a user path.
- Examined by the test, all 4 Orbit levels and every real orbital: **0 states with no jump in either direction** and **0 states below n = 4 with nothing to absorb**, so Auto's third message (and the manual "highest level" words) are never wrong for a real state. The third message from Auto is therefore only reachable in principle.

## The complex-state question (not changed; for the owner)
`targets()` refuses every jump from a complex state because "the dipole rule depends on the light's polarisation". **That is over-cautious as a physics statement.** (From knowledge, not looked up; any QM text, e.g. Bransden & Joachain, Sakurai, has the rules.) The electric-dipole rules Δl = ±1, Δm = q with q = 0, ±1 hold for Y_l^m too: q = 0 is light polarised along z, q = ±1 circular light about z. The polarisation fixes the final m' = m + q; it does not decide whether a jump is possible: some polarisation always reaches a level. Rates go as |⟨n'l'm'|r_q|nlm⟩|², which is a Clebsch-Gordan coefficient squared times a radial integral. Spontaneous emission needs no polarised light at all: the vacuum emits into all q, so from a complex state the final m' is **random with the Clebsch-Gordan weights** (for example 2p m = +1 → 1s: only m' = 0 exists, so the probability is 1; the weights for the other cases are not computed here, and a test against tabulated Clebsch-Gordan values must come first).
**Two things the refusal does get right.** (1) Real orbitals (px, pz …) are not eigenstates of L_z either; the lab's `dipoleOK` already allows a jump if ANY of x, y, z light connects them, so for real orbitals the lab already assumes "light of whatever polarisation is needed". A complex state refused for a missing polarisation model is therefore inconsistent with real ones. (2) The jump drawing (`buildJump`) interpolates between REAL orbitals only (comment at atom_sim.cpp:442); a complex final state would need new drawing code. So allowing it is bigger than deleting the `return`.
Options:
1. **Refuse as now.** Honest and simple; leaves Auto and Emit/Absorb idle for the complex choices: 2 of the 5 p, 4 of the 9 d and 6 of the 13 f orientations. Wrong only in saying "depends on the polarisation" as if that made it impossible.
2. **Allow; final m' random, said in the app.** Emit: weight by Clebsch-Gordan² (exact for hydrogenic spontaneous emission; uniform would be wrong). Absorb: unpolarised light, uniform over the reachable m' (a stated assumption). Needs complex-to-complex jump drawing; changes saved references only for states that now jump (none for real ones).
3. **Allow; the user picks the polarisation** (z, σ+, σ-, or unpolarised). Most teaching value (shows Δm = q), most UI and drawing work, needs a new control.
**Recommendation: 2 for Emit** (physically exact without a new control) and the same wording for Absorb; then 3 later as a card if wanted. Rewrite the refusal text at once, whatever is chosen: say "not modelled" instead of implying "impossible". Not on the roadmap as a stage; listed there as waiting for the owner.

## Tests (Linux GCC Release, `-DPL_BUILD_APP=OFF`) — all pass
orbit 1,540; **atom 18,889 (+44)**; app 1,520; nbody 37; jpl 55; moon 24; moon_real 59. Every existing check passes unchanged; dot hashes, web-compared runs and 0.10.6-0.10.9 guards untouched.
New `testAutoSilence`: 4 states (2p m = +1, 3d m = −2, 4f m = +3, superposition): no jump after 4 s, message equals the manual button's text, shown by `info()`, **not issued again over 40 s** (msgUntil unchanged on a fake clock that runs with the steps), expired from `info()`, once more after Auto off/on and then once only. Changes of situation (m +1 → −1, complex → superposition, back to pz where jumps resume). Real orientation, 60 s, Cloud and Orbit (seed 97531): **no message at all, 15 and 18 photons, equal to the numbers measured on the code before the fix** (an own-output guard, labelled; independent bound: at most 120 since the wait is ≥ 0.5 s). The exhaustive state scan above. No `long double`, no byte hashes.

## Perturbations (each reverted; file compared byte for byte)
Message suppressed: 14 failures. Wrong cause (superposition reported as complex): 2. Message at every look: 12. Message on a normal Auto: 15. All caught.

## Not compiled, not seen
`changelog.cpp` (the history entry) is built only in the full app, which the cloud cannot build: not compiled here (plain string literals). The message's layout on screen, and Auto on a real PC, never seen. No Windows behaviour or speed tested here.

## Windows CI result
(filled in after the run — see below)
