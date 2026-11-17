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
render::SliceDotsJob sliceJobOf(const PlaneSampler& ps, std::uint32_t seed, int tries);
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
SliceCompare compareSlice(const PlaneSampler& ps, std::uint32_t seed, const std::vector<float>& res, int tries);
std::string sliceCompareText(const SliceCompare& c);   // one line for the log and the benchmark

// The planes of the start-up check: 4f z3 on the benchmark's planes at uranium, a complex 3d state on a horizontal plane, 6s far out (Z, state id, axis,
// offset in Bohr radii); each is run with the same seed by the card and by the processor
struct SliceCheckCase { int Z, id, axis; double off; std::uint32_t seed; int tries; };
std::vector<SliceCheckCase> sliceCheckCases();

// The jump's check: a pattern of fixed groups (a few not shown) for jn dots and the shares (old, new); the processor's answer is AtomLab::jumpGroups's rule,
// a dot shown when its fixed random number is below its population's share (atom_sim.cpp jumpUniform), which the card must give exactly
double jumpUniformOf(int pop, long i);   // atom_sim.cpp: a jump's dot's fixed random number, (z >> 11) / 2^53 of splitmix64(i + (pop << 40))
struct JumpCheckCase { long jn; double shareNew; };
std::vector<JumpCheckCase> jumpCheckCases();
void jumpCheckFixed(long jn, std::vector<unsigned char>& fixedGroups);
void jumpCheckExpected(long jn, double shareNew, const std::vector<unsigned char>& fixedGroups, std::vector<unsigned char>& expected);

}  // namespace atom
