#include "orbit/ring_bands.h"

#include <algorithm>
#include <cmath>

#include "orbit/saturn_profile.h"
#include "orbit/saturn_rings.h"

namespace orbit {

const std::vector<RingBand>& saturnDrawnBands(double eqKm, double r) {
    static std::vector<RingBand> drawn;
    static double kEq = -1, kR = -1;
    if (eqKm == kEq && r == kR) return drawn;
    kEq = eqKm; kR = r;
    drawn.clear();
    const double kmPx = eqKm / r, span = rings::saturnProfileKm(rings::saturnProfileCount() - 1) - rings::saturnProfileKm(0);
    for (const rings::ProfileGroup& g : rings::saturnProfileGroups(std::max({10.0, kmPx, span / 700}))) {
        const double rin = g.innerKm / eqKm, rout = g.outerKm / eqKm, mid = 0.5 * (rin + rout);
        const double wPx = (rout - rin) * r, drawPx = std::max(wPx, 1.0);
        RingBand b;
        b.a0 = mid - 0.5 * drawPx / r; b.a1 = mid + 0.5 * drawPx / r; b.i0 = g.i0; b.n = g.n; b.cover = wPx / drawPx;
        drawn.push_back(b);
    }
    // 0.75.0: no two drawn bands overlap (the profile's bins could overlap by a few hundredths of a pixel at the planet's own scale; the pinhole camera close by
    // magnifies the near part of the rings up to 100 times, and there the processor's triangles blended both bands twice in the overlap, a darker seam the
    // card's per-pixel bands, one band a pixel, did not draw): each band starts where the one before it ends
    for (std::size_t i = 1; i < drawn.size(); i++) drawn[i].a0 = std::max(drawn[i].a0, drawn[i - 1].a1);
    drawn.erase(std::remove_if(drawn.begin(), drawn.end(), [](const RingBand& b) { return !(b.a1 > b.a0); }), drawn.end());
    return drawn;
}

const std::vector<RingBand>& saturnDrawnBandsFlat(double eqKm, double r, double mu, double mu0, bool sunlitFace) {
    static std::vector<RingBand> drawn;
    drawn.clear();
    const double kmPx = eqKm / r, span = rings::saturnProfileKm(rings::saturnProfileCount() - 1) - rings::saturnProfileKm(0);
    for (const rings::ProfileBin& pb : rings::saturnProfileBins(std::max({10.0, kmPx, span / 700}), mu, mu0, sunlitFace)) {
        const double rin = pb.innerKm / eqKm, rout = pb.outerKm / eqKm, mid = 0.5 * (rin + rout);
        const double wPx = (rout - rin) * r, drawPx = std::max(wPx, 1.0), cover = wPx / drawPx;
        const float alpha = static_cast<float>(pb.blocked * cover);
        const double shine = pb.shine * cover;
        if (alpha < 0.002f && shine < 0.002) continue;
        const float k = static_cast<float>(alpha > 1e-6f ? std::min(1.0, shine / alpha) : 0.0);
        drawn.push_back({mid - 0.5 * drawPx / r, mid + 0.5 * drawPx / r, alpha, k});
    }
    for (std::size_t i = 1; i < drawn.size(); i++) drawn[i].a0 = std::max(drawn[i].a0, drawn[i - 1].a1);
    drawn.erase(std::remove_if(drawn.begin(), drawn.end(), [](const RingBand& b) { return !(b.a1 > b.a0); }), drawn.end());
    return drawn;
}

std::vector<RingBand> giantDrawnBands(const std::vector<rings::Band>& bands, double eqKm, double r) {
    std::vector<RingBand> drawn;
    for (const auto& bd : bands) {
        const double rin = bd.innerKm / eqKm, rout = bd.outerKm / eqKm, mid = 0.5 * (rin + rout);
        const double wPx = (rout - rin) * r, drawPx = std::max(wPx, 1.0), cover = wPx / drawPx;   // a narrow ring is widened to a pixel, its opacity scaled down by the share that is its own
        // a band too faint to show even at the flattest slant is left out (Jupiter's main ring, tau 5e-6); its light is under the albedo (0.03 at most) times its opacity, so nothing either
        if (rings::blocked(bd.tau, kRingMuMin) * cover < 0.002) continue;
        RingBand nb;
        nb.a0 = mid - 0.5 * drawPx / r; nb.a1 = mid + 0.5 * drawPx / r; nb.cover = cover; nb.tauBand = true; nb.tau = bd.tau; nb.albedo = bd.albedo;
        drawn.push_back(nb);
    }
    return drawn;
}

RingValue ringBandValue(const RingFrame& f, const RingBand& b, double mu) {
    if (b.tauBand) {   // 0.75.2: the other giants' rings: the same formulas as the single slant of 0.75.1 (blocked share 1 - exp(-tau / mu), light albedo / 0.5 x the single-scattering shine) at this point's mu
        const float alpha = static_cast<float>(rings::blocked(b.tau, mu) * b.cover);
        const double shine = b.albedo / 0.5 * rings::shine(b.tau, mu, f.mu0, f.sunlit) * b.cover;
        RingValue v;
        v.alpha = alpha; v.faint = alpha < 0.002f && shine < 0.002;
        v.k = static_cast<float>(alpha > 1e-6f ? std::min(1.0, shine / alpha) : 0.0);
        return v;
    }
    const rings::ProfileBin m = rings::saturnGroupMeans(b.i0, b.n, mu, f.mu0, f.sunlit);
    const float alpha = static_cast<float>(m.blocked * b.cover);
    const double shine = m.shine * b.cover;
    RingValue v;
    v.alpha = alpha; v.faint = alpha < 0.002f && shine < 0.002;
    v.k = static_cast<float>(alpha > 1e-6f ? std::min(1.0, shine / alpha) : 0.0);   // drawn colour x opacity = the light
    return v;
}

}  // namespace orbit
