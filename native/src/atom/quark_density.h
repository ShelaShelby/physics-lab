// 0.43: the Accurate quark style's clouds: where a proton's (or neutron's) up and down quarks and its gluons are found, from measured form factors.
// The tables come from native/tools/make_quark_density.py (method and sources: native/data/qcd/densities/README.txt): the transverse densities of
// Miller, PRL 99, 112001 (2007), i.e. seen along the direction of a very fast nucleon's motion (the only frame where a density is model independent).
// The quark clouds are NET densities (quarks minus antiquarks of each flavour): the sea's pairs cancel in them. The gluon cloud is the transverse
// density of the gluons' share of the momentum (the gravitational form factor A_g, lattice tripole). All are normalised to 1, per quark.
#pragma once
#include "quark_density_data.h"
#include <algorithm>
#include <cmath>

namespace atom {

enum class QdKind { U, D, G };   // the proton's u profile, its d profile, the gluons (in a neutron the u and d profiles swap: its d quarks have the proton's u profile)

inline const double* qdRho(QdKind k) { return k == QdKind::U ? kQdRhoU : k == QdKind::D ? kQdRhoD : kQdRhoG; }
inline const double* qdCum(QdKind k) { return k == QdKind::U ? kQdCumU : k == QdKind::D ? kQdCumD : kQdCumG; }

// the density at b (fm), fm^-2 (one quark, or the gluons, normalised to 1); 0 beyond the table's 4 fm. Linear between the 0.01 fm steps.
inline double quarkDensity(QdKind k, double b) {
    if (!(b >= 0)) b = 0;
    const double x = b / kQdStep;
    const int i = int(x);
    if (i >= kQdN - 1) return 0;
    const double f = x - i, *r = qdRho(k);
    return r[i] + f * (r[i + 1] - r[i]);
}

// the radius b (fm) inside which the fraction u (0..1) of the cloud lies: for drawing dots at random with the cloud's own spread (u uniform on 0..1)
inline double quarkDensityRadius(QdKind k, double u) {
    const double* c = qdCum(k);
    u = std::min(1.0, std::max(0.0, u));
    const int i = int(std::upper_bound(c, c + kQdN, u) - c);
    if (i <= 0) return 0;
    if (i >= kQdN) return (kQdN - 1) * kQdStep;
    const double d = c[i] - c[i - 1];
    return ((i - 1) + (d > 0 ? (u - c[i - 1]) / d : 0)) * kQdStep;
}

// <b^2> of the table, fm^2 (trapezoid on 2 pi b^3 rho, inside 4 fm)
inline double quarkDensityMeanB2(QdKind k) {
    const double* r = qdRho(k);
    double s = 0;
    for (int i = 0; i + 1 < kQdN; i++) {
        const double b0 = i * kQdStep, b1 = b0 + kQdStep;
        s += 0.5 * kQdStep * (2 * 3.14159265358979323846 * (b0 * b0 * b0 * r[i] + b1 * b1 * b1 * r[i + 1]));
    }
    return s;
}

}  // namespace atom
