// Black Hole Lab (0.70.0): the Real look's tone curve (tone.h: Reinhard, Stark, Shirley & Ferwerda 2002, eqs. (1), (2), (4))
#include "blackhole/tone.h"

#include <algorithm>
#include <cmath>

namespace blackhole {

double logAverage(const std::vector<double>& Y, double delta) {
    if (Y.empty()) return 0;
    double s = 0;
    for (double y : Y) s += std::log(delta + y);
    return std::exp(s / static_cast<double>(Y.size()));
}

double toneCurve(double L, double Lwhite) {
    if (!(L > 0)) return 0;
    const double burn = Lwhite > 0 ? L / (Lwhite * Lwhite) : 0;
    return L * (1 + burn) / (1 + L);
}

ToneExposure toneExposure(double logAvg, double yMax, double stops) {
    ToneExposure e;
    e.logAvg = logAvg; e.yMax = yMax;
    if (!(logAvg > 0) || !(yMax > 0)) return e;
    e.k = kToneKey * std::exp2(stops) / logAvg;
    e.lw = e.k * yMax;
    e.ok = true;
    return e;
}

ToneExposure toneFromSamples(const std::vector<double>& Y, double stops) {
    double m = 0;
    for (double y : Y) m = std::max(m, y);
    return toneExposure(m > 0 ? logAverage(Y, kToneDelta * m) : 0, m, stops);
}

double lumaSrgb(const std::array<double, 3>& c) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }

std::array<double, 3> toneScaled(const std::array<double, 3>& c, double lw) {
    const double y = lumaSrgb(c);
    if (!(y > 0)) return {0, 0, 0};
    const double f = toneCurve(y, lw) / y;
    std::array<double, 3> o{c[0] * f, c[1] * f, c[2] * f};
    const double m = std::max({o[0], o[1], o[2]});
    if (m > 1) for (double& v : o) v /= m;
    return o;
}

std::array<double, 3> toneRGB(const std::array<double, 3>& rgb, const ToneExposure& e) {
    return toneScaled({rgb[0] * e.k, rgb[1] * e.k, rgb[2] * e.k}, e.lw);
}

}  // namespace blackhole
