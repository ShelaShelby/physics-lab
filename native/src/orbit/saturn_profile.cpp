// 0.51.2: Saturn's rings from Cassini's profile (saturn_profile.h).
#include "orbit/saturn_profile.h"
#include "orbit/saturn_rings.h"

#include <algorithm>
#include <cmath>

namespace orbit::rings {

namespace {
#include "orbit/saturn_profile.inc"
// the edges of bin i, km: half way to the next bin's middle (the bins are 10 km to within 0.04 km), so the bins tile the profile exactly
double edgeLo(int i) { return i > 0 ? 0.5 * (kSatProfileKm[i - 1] + kSatProfileKm[i]) : kSatProfileKm[0] - 0.5 * (kSatProfileKm[1] - kSatProfileKm[0]); }
double edgeHi(int i) { return i + 1 < kSatProfileN ? edgeLo(i + 1) : kSatProfileKm[i] + 0.5 * (kSatProfileKm[i] - kSatProfileKm[i - 1]); }
}  // namespace

int saturnProfileCount() { return kSatProfileN; }
double saturnProfileKm(int i) { return kSatProfileKm[i]; }
double saturnProfileTau(int i) { return kSatProfileTau[i]; }

double saturnAlbedoAt(double rKm) {
    for (const Ring& g : kRings) if (rKm >= g.innerKm && rKm < g.outerKm) return 0.5 * (g.albedoLo + g.albedoHi);
    if (rKm < kRings[0].innerKm) return 0.5 * (kRings[0].albedoLo + kRings[0].albedoHi);
    return kFRingAlbedo;
}

std::vector<ProfileGroup> saturnProfileGroups(double wKm) {
    std::vector<ProfileGroup> out;
    const int n = kSatProfileN;
    int i = 0;
    while (i < n) {
        const double r0 = edgeLo(i);
        int j = i;
        while (j < n && (j == i || edgeHi(j) - r0 <= wKm)) j++;
        out.push_back({i, j - i, r0, edgeHi(j - 1)});
        i = j;
    }
    return out;
}

ProfileBin saturnGroupMeans(int i0, int n, double mu, double mu0, bool sunlitFace) {
    double sb = 0, ss = 0;
    for (int j = i0; j < i0 + n; j++) {
        const double tau = kSatProfileTau[j];
        sb += blocked(tau, mu);
        ss += saturnAlbedoAt(kSatProfileKm[j]) / 0.5 * shine(tau, mu, mu0, sunlitFace);
    }
    return {edgeLo(i0), edgeHi(i0 + n - 1), sb / n, ss / n};
}

void saturnAlbedoRegions(int bounds[4], double albedo[5]) {
    for (int k = 0; k < 4; k++) {
        const double edge = k == 0 ? kRings[1].innerKm : k == 1 ? kRings[2].innerKm : k == 2 ? kRings[3].innerKm : kRings[3].outerKm;
        int j = 0;
        while (j < kSatProfileN && kSatProfileKm[j] < edge) j++;
        bounds[k] = j;
    }
    albedo[0] = 0.5 * (kRings[0].albedoLo + kRings[0].albedoHi); albedo[1] = 0.5 * (kRings[1].albedoLo + kRings[1].albedoHi);
    albedo[2] = 0.5 * (kRings[2].albedoLo + kRings[2].albedoHi); albedo[3] = 0.5 * (kRings[3].albedoLo + kRings[3].albedoHi); albedo[4] = kFRingAlbedo;
}

std::vector<ProfileBin> saturnProfileBins(double wKm, double mu, double mu0, bool sunlitFace, double tol) {
    std::vector<ProfileBin> out;
    const int n = kSatProfileN;
    int i = 0;
    while (i < n) {
        const double r0 = edgeLo(i);
        double sb = 0, ss = 0, wsum = 0;
        int j = i;
        while (j < n && (j == i || edgeHi(j) - r0 <= wKm)) {
            const double w = edgeHi(j) - edgeLo(j), tau = kSatProfileTau[j];
            sb += w * blocked(tau, mu);
            ss += w * saturnAlbedoAt(kSatProfileKm[j]) / 0.5 * shine(tau, mu, mu0, sunlitFace);
            wsum += w; j++;
        }
        const ProfileBin b{r0, edgeHi(j - 1), sb / wsum, ss / wsum};
        if (!out.empty() && std::fabs(out.back().blocked - b.blocked) < tol && std::fabs(out.back().shine - b.shine) < tol) {   // merge: the area-weighted mean of the two
            ProfileBin& p = out.back();
            const double wa = p.outerKm - p.innerKm, wb = b.outerKm - b.innerKm;
            p.blocked = (p.blocked * wa + b.blocked * wb) / (wa + wb); p.shine = (p.shine * wa + b.shine * wb) / (wa + wb); p.outerKm = b.outerKm;
        } else out.push_back(b);
        i = j;
    }
    return out;
}

int saturnShadowBands(double mu0, double eqKm, int n, float* out) {
    mu0 = std::max(mu0, 1e-3);
    const double lo = kRings[0].innerKm, hi = kRings[3].outerKm, step = (hi - lo) / n;
    for (int k = 0; k < n; k++) {
        const double a = lo + k * step, b = a + step;
        double st = 0, wsum = 0;
        for (int i = 0; i < kSatProfileN; i++) {
            const double r = kSatProfileKm[i];
            if (r < a || r >= b) continue;
            const double w = edgeHi(i) - edgeLo(i);
            st += w * std::exp(-kSatProfileTau[i] / mu0); wsum += w;
        }
        const double T = wsum > 0 ? st / wsum : 1.0;
        out[3 * k] = static_cast<float>(a / eqKm); out[3 * k + 1] = static_cast<float>(b / eqKm);
        out[3 * k + 2] = static_cast<float>(T > 0 ? -mu0 * std::log(T) : 50.0);
    }
    return n;
}

}  // namespace orbit::rings
