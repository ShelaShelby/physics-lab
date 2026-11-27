// 0.92.0: the porkchop grid's inputs as the card takes them, and the card's answer against the processor's (orbit/lambert.h). Header only: used by ui/porkchop_runner.cpp (the start-up
// check) and tests/vk_compare.cpp.
#pragma once

#include <cmath>
#include <vector>

#include "orbit/lambert.h"
#include "render/porkchop_card.h"

namespace orbit {
namespace lambert {

inline render::PorkchopJob gpuJobOf(const PorkchopInput& in) {
    render::PorkchopJob j;
    j.nDep = in.s.nDep; j.nArr = in.s.nArr;
    j.off = float(in.s.arrJD0 - in.s.depJD0);                 // days: the first arrival's date minus the first departure's (exact in single precision for the whole-day starts the panel makes)
    j.depStep = float(in.s.depStep); j.arrStep = float(in.s.arrStep);
    j.tu = float(in.timeUnitDays); j.vk = float(in.velUnitKmS);
    const auto pack = [](const std::vector<double>& v, std::vector<float>& out) {
        out.assign(v.size() / 6 * 8, 0.0f);
        for (std::size_t k = 0; k < v.size() / 6; k++) for (int c = 0; c < 6; c++) out[k * 8 + (c < 3 ? c : c + 1)] = float(v[k * 6 + std::size_t(c)]);
    };
    pack(in.dep, j.dep); pack(in.arr, j.arr);
    return j;
}

// The card's grid (2 floats a cell: C3, v_inf; negative: no transfer) as a PorkchopGrid (NaN where there is none)
inline PorkchopGrid gridOfCard(const PorkchopInput& in, const std::vector<float>& res) {
    PorkchopGrid g;
    g.nDep = in.s.nDep; g.nArr = in.s.nArr;
    const std::size_t n = std::size_t(g.nDep) * std::size_t(g.nArr);
    g.c3.assign(n, std::nanf("")); g.vinf = g.c3;
    for (std::size_t k = 0; k < n && 2 * k + 1 < res.size(); k++) if (res[2 * k] >= 0) { g.c3[k] = res[2 * k]; g.vinf[k] = res[2 * k + 1]; }
    return g;
}

}  // namespace lambert
}  // namespace orbit
