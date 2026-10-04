# Native Physics Lab 0.10.6 (alpha) — stage 8 report: the cloud jump made cheaper

Audit finding 11, confirmed by the owner's benchmark (RX 9060 XT, 255 Hz, 2026-10-04): scene 11 (jump held in flight, Cloud 3D, 48,000 dots) average 29.93 ms, worst 33.28 ms, CPU 29.84 ms, GPU 0.28 ms; scene 12 (jumps one after another) average 8.06 ms, **worst 36.35 ms**; scene 9 (no jump) 3.80 ms. Only the cloud jump path was changed. `index.html` untouched (sha256 e5501387ae64…).
**Not timed on a PC and no speed-up is claimed.** The owner must re-run scenes 11 and 12 (OWNER-TODO.md).

## Which candidates mattered (profiled in this session on a Linux CPU, only to rank them; PC numbers will differ)
Scene 11's setup, per frame, before the change: all dots' colour groups **25.7 ms** (`jumpShown` alone 19.4 ms; `jumpPhi` alone 9.4 ms for 96,000 calls: each call runs `levelEV` twice, which looks up the nucleus mass). Start of a jump **20.3 ms**: normFactor 1.5 (first jump only), old dots' amplitudes 4.7, 800 settling steps 0.2, 48,000 chain steps with their amplitudes 13.4.
| candidate | measured here | decision |
|---|---|---|
| `jumpFractions` + per-dot `jumpU`/`jumpPhi`/cos/sin/exp (atom_sim.cpp:287 area) | **~26 of the 30 ms** per frame | changed |
| start of a jump (48,000 new dots in one frame) | 20 ms once | changed |
| `allPts.assign + insert` (atom_view.cpp:304) | **0.16 ms** | left (not material) |
| `info1`'s 48,000 `hypot` (atom_sim.cpp:791) | **0.3 ms** (the whole `info()` call) | left (not material) |

## What changed
1. **Once per frame, not once per dot** (`atom_core.cpp` `jumpTerms`, a second `jumpFractions`; `atom_sim.cpp` `AtomLab::jumpGroups`): cos θ, sin θ, cos²θ, exp(−iφ), exp(iφu), u and φ (with its `levelEV` lookups) are worked out once for all dots; the arithmetic per dot is written in the same order, so the numbers are the same bits. The view (`atom_view.cpp`, 2 lines) calls `jumpGroups` instead of 96,000 `jumpGroup` calls. `jumpGroup`/`jumpShown`/`jumpPhase` per dot are kept as they were (the reference in the tests).
2. **The start is spread over 4 frames** (`buildJump`, `AtomState::JumpBuild`): the start frame only sets up (a normFactor lookup, allocating the arrays, the 800 settling steps); then each `step()` builds a quarter of the dots (an old dot's amplitudes and one chain step of the new dot with its amplitudes). Why this is exact: during a smooth cloud jump `step()` samples nothing, so the old dots and the random numbers are not touched by anything else; the generator is moved past the 7 numbers per step the old code used (`skipRandom`, exact in constant time because Mulberry32 moves its state by a fixed step per call), the chain uses its own copy; `s.w2` gets the chain's end. Anything that reads the jump (`jumpShown`, `jumpPhase`, `jumpGroups`, the landing) first builds what is left, so a jump ended by reduced motion or one very long frame lands on the same cloud.
3. **Fewer function evaluations** in the chain: psi at the walker's place is carried from step to step (it is the same number the next step starts with and the new dot's amplitude needs; pure function, same bits): 2 instead of 4 evaluations per new dot.
4. **What the user sees:** while the dots are being built (4 frames, about 16 ms at 255 Hz, 67 ms at 60 Hz) the cloud is drawn as it was (`jumpDrawn()`), then the jump, which is under 1 % through at that point. The jump's clock is not held, so it ends at the same moment as before.
Deliberately not changed: phase-colour scan (finding 10), outline (12), superposition paths, the sampling and the random sequence, `walk()`, any other view.

## Proof the output is unchanged
- No saved number moved: every existing test passes unchanged (atom 18,157 old checks included: the web-compared runs, dot hashes, the jump tests, stage 3–6 guards).
- `testJumpSameDots` (new, 24 checks): fingerprints of FNV-1a over the bytes of the new dots, both amplitude arrays and the walker, every dot's colour group at two moments (Sign and Wheel), the cloud after landing (dots, signs, phases, walker, count) and the next random numbers, for 4 seeded jumps (4f→3d Ultra 48,000; 2p absorb →3d; 3d→2p with reduced motion; 2s→1s). The numbers were recorded **from the stage 7 code** before any change, and the test passed on that code first, then after.
- `testJumpBuild` (new, 22 checks): `skipRandom(n)` = n draws (5 sizes); the new `jumpFractions` = the old one bit for bit on 20,000 random cases; `jumpGroups` = `jumpGroup` per dot for every dot, both colour modes, 6 moments; start frame builds nothing and draws nothing; built by 3 to 5 frames alone, equal bit for bit to built at once (Normal and Ultra); asking about a dot of a half-built jump gives the finished one; a 5 s frame lands on the same cloud.
## Tests (Linux GCC Release, Ninja, `PL_BUILD_APP=OFF`): all seven pass
orbit 1,540; **atom 18,203** (was 18,157: +46); app 1,515; nbody 37; jpl 55; moon 24; moon_real 59.

## Indicative numbers from this session (CPU only, not the PC, not a claim)
After the change, same machine and scene: start frame 3.6 ms; the four build frames about 4 ms each; groups per frame 4.5 ms (before: 20.3, one frame; 25.7). The remaining per-dot work (a hash, a complex `arg`, a `cos`) is the next candidate if the PC still shows the jump as slow.

## Not compiled / not tested
`atom_view.cpp` (2 changed lines: `jumpDrawn`, `jumpGroups`) is in the app target, which needs SDL3 and does not build here: never compiled. Not seen on screen: the first 4 frames showing the old cloud. No GPU, Windows or PC timing.
