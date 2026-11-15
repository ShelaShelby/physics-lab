// 0.27: where a pixel's line of sight meets a body drawn as an ellipsoid (orthographic view, as the planets are drawn).
// The C++ twin of the planet shader's shape step (render/shaders.h, kPlanetFS): the same formulas, in double, so the tests can
// check the drawn outline and surface against the measured radii (tests/test_orbit.cpp, "real shapes").
//
// The body frame: x along its 0° meridian, y 90° east, z its north pole. Radii 1, sy, sz along them (in units of the
// radius along the meridian, which is the drawn radius r). K turns screen directions (right, up, towards you) into the
// body frame (row-major, as Renderer2D::planet takes it). (u, v): the pixel's offset from the middle in units of r,
// u to the right, v DOWN (the shader's vUV).
#pragma once

#include <cmath>

namespace render {

struct EllipsoidHit {
    bool hit = false;
    double d = 2;                 // 1 on the outline, under 1 inside (for a ball: the distance from the middle)
    double b[3] = {0, 0, 0};      // the point on the body, body frame
    double n[3] = {0, 0, 1};      // its unit surface normal, screen frame (right, up, towards you)
    double latRad = 0, lonRad = 0;   // planetocentric latitude and east longitude of that point
};

inline EllipsoidHit ellipsoidHit(const float K[9], double sy, double sz, double u, double v) {
    EllipsoidHit h;
    const double S[3] = {1.0, 1.0 / (sy * sy), 1.0 / (sz * sz)};
    double P0[3], D[3];
    for (int i = 0; i < 3; i++) {
        P0[i] = K[3 * i] * u + K[3 * i + 1] * (-v);   // K * (u, -v, 0)
        D[i] = K[3 * i + 2];                          // K * (0, 0, 1)
    }
    double qa = 0, qb = 0, qc = -1;
    for (int i = 0; i < 3; i++) { qa += D[i] * S[i] * D[i]; qb += P0[i] * S[i] * D[i]; qc += P0[i] * S[i] * P0[i]; }
    h.d = std::sqrt(std::fmax(0.0, 1.0 + qc - qb * qb / qa));
    if (h.d > 1.0) return h;
    h.hit = true;
    const double t = (-qb + std::sqrt(std::fmax(0.0, qb * qb - qa * qc))) / qa;
    double nb[3];
    for (int i = 0; i < 3; i++) { h.b[i] = P0[i] + t * D[i]; nb[i] = h.b[i] * S[i]; }
    double len = 0;
    for (int j = 0; j < 3; j++) {   // transpose(K) * nb
        h.n[j] = K[j] * nb[0] + K[3 + j] * nb[1] + K[6 + j] * nb[2];
        len += h.n[j] * h.n[j];
    }
    len = std::sqrt(len);
    for (double& x : h.n) x /= len;
    h.latRad = std::atan2(h.b[2], std::hypot(h.b[0], h.b[1]));
    h.lonRad = std::atan2(h.b[1], h.b[0]);
    return h;
}

}  // namespace render
