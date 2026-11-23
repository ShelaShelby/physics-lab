// 0.77.0 (outside review item 4): a far marker (a star, a cluster, the galactic centre, a Voyager) is beyond every body of the Solar System, so whatever is drawn
// in front of it on the screen hides it: a ball (the Sun, a planet, a moon) hides it, a ring dims it by the drawn ring's own transmission at that point.
// The ring's transmission is 1 minus its drawn opacity there (ring_bands.h ringBandValue: for Saturn the mean over Cassini's 10 km measurements of
// 1 - exp(-tau / mu) at that point's own slant mu, the same law as the rings' shadow on the ball). The test is on the screen, against what is drawn
// (a ball drawn wider than real at a far zoom hides what its drawn disc covers). tests/test_orbit.cpp checks it.
#pragma once

#include <cmath>
#include <vector>
#include "ring_bands.h"
#include "ring_draw.h"
#include "render/ellipsoid.h"

namespace orbit {

struct FarBall { double x = 0, y = 0, r = 0; bool hasK = false; float K[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; double sz = 1; };   // sz: polar over equatorial radius (render/ellipsoid.h)
struct FarRings { RingFrame f; std::vector<RingBand> bands; };
struct FarOccluders { std::vector<FarBall> balls; std::vector<FarRings> rings; };

// The share of a far marker's light at the screen point (x, y) that reaches you: 0 behind a ball, the product of the rings' transmissions on the way, else 1.
inline double farSeen(const FarOccluders& o, double x, double y) {
    for (const FarBall& b : o.balls) {
        if (b.r <= 0) continue;
        if (b.hasK) { if (render::ellipsoidHit(b.K, 1.0, b.sz, (x - b.x) / b.r, (y - b.y) / b.r).hit) return 0; }
        else if (std::hypot(x - b.x, y - b.y) <= b.r) return 0;
    }
    double t = 1;
    for (const FarRings& r : o.rings) {
        double q[3];
        if (!ringPointAtPixel(r.f, x, y, q)) continue;
        const double rho = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
        for (const RingBand& b : r.bands)
            if (rho >= b.a0 && rho <= b.a1) { t *= 1.0 - ringBandValue(r.f, b, ringMu(r.f, q)).alpha; break; }
    }
    return t;
}

}  // namespace orbit
