#pragma once
// 0.16 stage 6: where the three valence quarks of a nucleon are, for the "Accurate" quark style. Pure (no drawing, no allocation), so it can be tested.
// Facts and sources: docs/QUARKS.md.
#include "atom_core.h"

namespace atom {

constexpr double kQuarkMeanR = 0.33;   // fm: the mean distance of a quark from the nucleon centre, kept from the old tumbling triangle (measured 0.332 fm over its motion); a choice, NOT a measurement
constexpr double kQuarkCutR = 0.6;     // fm: no quark is drawn farther out than this (the owner's choice)
// the ratio of the mean d to the mean u distance: Miller, PRL 99, 112001 (2007), arXiv:0705.2409, finds the central u-quark charge density about 70 % larger than the d one
// (a transverse density in the infinite-momentum frame; used here as a heuristic, not a 3D snapshot). Per quark, density ∝ 1/a³, so a_d / a_u = 1.7^(1/3).
inline double quarkDUratio() { return std::cbrt(1.7); }

struct QuarkWander { char f[3]; double x[3][3]; };   // flavours (p: u, u, d; n: u, d, d) and places in fm, in the nucleon's own frame (the camera is not used)
QuarkWander accurateQuarks(const Nucleon& q, double t);   // t: seconds (the same for all nucleons; each wanders on its own seeds from q.ph)

}  // namespace atom
