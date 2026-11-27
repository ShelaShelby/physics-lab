// 0.57.0: the processor's side of the Atom Lab's two card programs (render/atom_card.h): what a batch of the Slice's attempts asks of the card, and the
// comparison of the card's answers with the processor's own (the exact reference) within the limits that single-precision rounding allows. The limits are
// derived from the arithmetic, attempt by attempt (below), not chosen from the card's results. The same comparison is the app's start-up check and the tests'.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "atom/cloud_sample.h"
#include "render/atom_card_types.h"

namespace atom {

// the card's batch for a prepared plane: `tries` attempts from the generator's state `seed`
// 0.64.0: stride, first: attempt t takes the numbers stride t + first ... (one state: 3, 0; one state of a superposition: 4, 1, the number 4 t is the choice
// of the state, made by the processor: AtomLab::addDotsCardSuper)
render::SliceDotsJob sliceJobOf(const PlaneSampler& ps, std::uint32_t seed, int tries, int stride = 3, int first = 0);
// the generator's state after `numbers` numbers from `seed` (orbit::Mulberry32 adds 0x6D2B79F5 per number)
inline std::uint32_t mulberryAfter(std::uint32_t seed, std::uint64_t numbers) { return seed + std::uint32_t(numbers * 0x6D2B79F5ull); }

// The card's attempts against the processor's, one by one (res: 4 floats per attempt, as the card writes them).
// Per attempt the limit on the point (each coordinate, Bohr radii) follows the rounding of single precision through the card's steps (atom_card.cpp:
// the logarithm's error as GLSL 4.30 section 4.7.1 allows, the Horner sums with their condition number, Newton's stopping step, the square root, the own
// sine and cosine); a kept / rejected answer may differ only where the processor's own margin |r3 M - |P(u)|^2| is within the card's possible error there.
struct SliceCompare {
    long tries = 0, kept = 0, bothKept = 0;
    long flagDiff = 0, flagExplained = 0;     // attempts kept by one and not the other; how many of them the rounding explains
    long pointBad = 0;                        // attempts whose point is farther from the processor's than its limit
    double worstRatio = 0;                    // the largest |difference| / limit over the points
    double worstAbs = 0;                      // the largest |difference| of a kept point, Bohr radii
    bool ok() const { return tries > 0 && pointBad == 0 && flagDiff == flagExplained; }
};
SliceCompare compareSlice(const PlaneSampler& ps, std::uint32_t seed, const std::vector<float>& res, int tries, int stride = 3, int first = 0);
std::string sliceCompareText(const SliceCompare& c);   // one line for the log and the benchmark

// The planes of the start-up check: 4f z3 on the benchmark's planes at uranium, a complex 3d state on a horizontal plane, 6s far out (Z, state id, axis,
// offset in Bohr radii); each is run with the same seed by the card and by the processor
// 0.64.0: and the two states of the benchmark's superposition (mendelevium 2p z + 5f z3) with a superposition's numbering (stride 4, first 1)
struct SliceCheckCase { int Z, id, axis; double off; std::uint32_t seed; int tries; int stride = 3, first = 0; };
std::vector<SliceCheckCase> sliceCheckCases();

// 0.91.0: the Slice of a Dirac state on the card (render/atom_card_types.h DiracSliceJob, shaders.h kDiracSliceCS), its exact reference tryDiracOnPlane.
// tries attempts from `seed` (stride 3 with the keep test, else 2). Empty job (tries 0): the plane holds none of it (the processor takes no number then).
struct DiracCase { int Z = 1, n = 1, l = 0, j2 = 1; bool round = false; int axis = 1; double off = 0; std::uint32_t seed = 0; int tries = 0; };
render::DiracSliceJob diracJobOf(const DiracCase& c);
// the card's attempts against tryDiracOnPlane's with the same numbers, each point within its limit (atom_card.cpp, derived step by step as compareSlice's)
SliceCompare compareDirac(const DiracCase& c, const std::vector<float>& res);
std::vector<DiracCase> diracCheckCases();   // the start-up check's planes: the benchmark's uranium 4f (j = 7/2 lined up), a far cut of hydrogen 3d5/2, U 1s round

// 0.93.0: a parabolic state's cloud on the card (render/atom_card_types.h StarkJob, shaders.h kStarkCS), its exact reference stark::samplePar: `tries`
// points from `seed`, point t takes the numbers 4t .. 4t + 3 and is always kept. The state is (n1, n2, m) of hydrogen.
struct StarkCase { int n = 1, n1 = 0, n2 = 0, m = 0; std::uint32_t seed = 0; int tries = 0; };
render::StarkJob starkJobOf(const StarkCase& c);
// the card's points against samplePar's with the same numbers: the part exactly, each point within its limit (atom_card.cpp, derived step by step)
SliceCompare compareStark(const StarkCase& c, const std::vector<float>& res);
std::vector<StarkCase> starkCheckCases();   // the start-up check's states: (4, 0, 0) and (1, 2, -1) of n = 5, (0, 0, 3) of n = 4

// The jump's check: a pattern of fixed groups (a few not shown) for jn dots and the shares (old, new); the processor's answer is AtomLab::jumpGroups's rule,
// a dot shown when its fixed random number is below its population's share (atom_sim.cpp jumpUniform), which the card must give exactly
double jumpUniformOf(int pop, long i);   // atom_sim.cpp: a jump's dot's fixed random number, (z >> 11) / 2^53 of splitmix64(i + (pop << 40))
struct JumpCheckCase { long jn; double shareNew; };
std::vector<JumpCheckCase> jumpCheckCases();
void jumpCheckFixed(long jn, std::vector<unsigned char>& fixedGroups);
void jumpCheckExpected(long jn, double shareNew, const std::vector<unsigned char>& fixedGroups, std::vector<unsigned char>& expected);

}  // namespace atom
