// 0.94.0: the Sun's surface (render/sun_surface.h, orbit/sun_photo.h): dark sunspots, granulation, NASA SDO/HMI's photo of the day.
// Written before the code: none of it builds on 0.93.0 (render/sun_surface.h and orbit/sun_photo.h are new).
//  (a) the numbers in the code are the extracts' (Maltby et al. 1986 Table 3, Mathew et al. 2007 Table 2, Jha et al. 2019, Mathew et al. 2009 Table 2, Hathaway et al. 2015);
//  (b) a spot's mean brightness is the umbra's and penumbra's measured contrasts weighted by the measured area split (an independent sum over the cap);
//  (c) granulation: its spatial spectrum peaks at the measured 1.2 Mm, its rms contrast at the disc centre is the source's per channel, the disc's mean stays
//      Neckel & Labs's, and patterns one lifetime (10 minutes) apart share nothing;
//  (d) the photo: Helioviewer's reply and header (the extracts) are read; the photo's frame agrees with the Sun's IAU frame (orbit/spin.cpp); with PL_SOURCES_DIR,
//      the saved picture of 10 May 2024: its edge profile is Neckel & Labs's at 617.3 nm (the grey level is linear), and its darkest spot's centre lands on NOAA's
//      region 3664 of that day (S19W34, Carrington longitude 348);
//  (e) the loader: no network in tests; missing and bad files give the drawn surface.
#include <complex>
#include <filesystem>
#include <random>
#include <thread>
#include "render/sun_surface.h"
#include "orbit/sun_photo.h"

namespace t094 {
using t085::block;
using t085::readFile;
// a number from a string such as "0.8561" or "−0.0016" (the Unicode minus of the papers' text)
double num(std::string s) {
    const std::string minus = "\xE2\x88\x92";
    for (std::size_t i; (i = s.find(minus)) != std::string::npos;) s.replace(i, minus.size(), "-");
    return std::stod(s);
}
std::string noCr(std::string s) { while (!s.empty() && s.back() == '\r') s.pop_back(); return s; }   // an extract checked out with CRLF (Windows)
std::vector<std::string> words(const std::string& l) { std::istringstream in(l); std::vector<std::string> w; std::string x; while (in >> x) w.push_back(x); return w; }
// radix-2 FFT in place (n a power of two)
void fft(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; i++) { std::size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * 3.14159265358979323846 / static_cast<double>(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (std::size_t j = 0; j < len / 2; j++) { const auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
        }
    }
}
}  // namespace t094

static void testSun094() {
    using namespace t094;
    const double pi = 3.14159265358979323846;
    const auto say = [](bool ok, const std::string& w) { check(ok, w); std::printf("   %s\n", w.c_str()); };   // a check whose numbers are printed too (REPORT.md)
    // ---- (a) the sources' numbers ----
    {
        const auto t3 = block("sun/spots/maltby_1986_table3.extract.txt", "Table 3");
        int rows = 0;
        for (const auto& l : t3) {
            const auto w = words(l);
            if (w.size() < 7 || w[1] != "|") continue;
            double um = 0;
            try { um = std::stod(w[0]); } catch (...) { continue; }
            for (int i = 0; i < render::kMaltbyN; i++)
                if (std::fabs(render::kMaltbyUm[i] - um) < 1e-9) { check(std::fabs(render::kMaltbyMid[i] - std::stod(w[4])) < 1e-12, "sun094 (a): Maltby 1986 Table 3, middle phase at " + w[0] + " um: " + w[4]); rows++; }
        }
        check(rows == render::kMaltbyN, "sun094 (a): all 10 rows of Maltby's Table 3 are the code's (" + std::to_string(rows) + ")");
        const auto m7 = block("sun/spots/mathew_2007_table2.extract.txt", "Table 2");
        bool pen = false;
        for (const auto& l : m7) if (l.find("penumbral mean intensity") != std::string::npos) { const auto w = words(l); pen = w.size() >= 6 && num(w[4]) == render::kPenConst && num(w[5]) == render::kPenGrad; }
        check(pen, "sun094 (a): Mathew 2007 Table 2, the penumbra's fit 0.8561 - 0.0016 r is the code's");
        bool w6768 = false;
        for (const auto& l : block("sun/spots/mathew_2007_table2.extract.txt", "section 2")) w6768 = w6768 || l.find("6768 A\xCC\x8A mid-photospheric absorption line") != std::string::npos;
        check(w6768, "sun094 (a): Mathew 2007's continuum is next to the Ni I 6768 A line");
        std::string jha;
        for (const char* b : {"section 4.1", "section 4.3"}) for (const auto& l : block("sun/spots/jha_2019_ratio.extract.txt", b)) jha += noCr(l) + "\n";   // (Windows: CRLF)
        check(jha.find("settle down to a value of \xE2\x89\x88" "6") != std::string::npos && render::kRatioBig == 6, "sun094 (a): Jha et al. 2019, q about 6 above 100 millionths");
        check(jha.find("remains constant\nat \xE2\x89\x88" "4.5") != std::string::npos && render::kRatioSmall == 4.5 && jha.find("area <100") != std::string::npos, "sun094 (a): Jha et al. 2019, about 4.5 below 100 millionths");
        int g = 0;
        for (const auto& l : block("sun/granulation/mathew_2009_table2.extract.txt", "Table 2"))
            for (int i = 0; i < render::kGranCN; i++) {
                const char* tag = i == 0 ? "4504 (BC)" : i == 1 ? "5550 (GC)" : "6684 (RC)";
                const std::size_t at = l.find(tag);
                if (at == std::string::npos) continue;
                const auto w = words(l.substr(at));   // 4504 (BC) obs-rms obs-C dec-rms dec-C
                if (w.size() >= 6 && std::fabs(std::stod(w[4]) - render::kGranCRms[i]) < 1e-12 && std::fabs(std::stod(w[0]) / 10 - render::kGranCNm[i]) < 1e-9) g++;
            }
        check(g == 3, "sun094 (a): Mathew, Zakharov & Solanki 2009 Table 2, the deconvolved rms at 450.4, 555.0, 668.4 nm are the code's (" + std::to_string(g) + " of 3)");
        std::string h;
        for (const char* b : {"abstract", "introduction"}) for (const auto& l : block("sun/granulation/hathaway_2015.extract.txt", b)) h += noCr(l) + "\n";
        check(h.find("(wavelength of about 1200 km), which is typical of") != std::string::npos && render::kGranCellMm == 1.2, "sun094 (a): Hathaway et al. 2015, the granular peak at about 1200 km");
        check(h.find("about 10 minutes (Bray et al. 1984)") != std::string::npos && render::kGranLifeMin == 10, "sun094 (a): Hathaway et al. 2015, granules live about 10 minutes");
    }
    // ---- (b) the spots ----
    {
        const auto& U = render::umbraRgb();
        say(U[0] > U[1] && U[1] > U[2] && U[2] > 0.008 - 1e-12 && U[0] < 0.215, "sun094 (b): the umbra is darker in blue than in red, within Maltby's visible values: " + std::to_string(U[0]) + " " + std::to_string(U[1]) + " " + std::to_string(U[2]));
        // a constant against wavelength stays that constant in every channel (the weights are normalised)
        const auto one = render::perChannel([](double) { return 0.37; });
        check(std::fabs(one[0] - 0.37) < 1e-12 && std::fabs(one[1] - 0.37) < 1e-12 && std::fabs(one[2] - 0.37) < 1e-12, "sun094 (b): a constant ratio is the same in every channel");
        check(std::fabs(render::umbraAt(579) - 0.066) < 1e-12 && std::fabs(render::umbraAt(624) - 0.078) < 1e-12 && render::umbraAt(380) == 0.008, "sun094 (b): Maltby's ratio between the rows, linear in wavelength");
        check(std::fabs(render::penumbraAt(20) - (0.8561 - 0.032)) < 1e-12 && render::penumbraAt(5) == render::penumbraAt(10) && render::penumbraAt(60) == render::penumbraAt(30), "sun094 (b): the penumbra's fit, held outside 10-30 arcsec");
        // the mean over a spot's cap, summed on a fine grid of the ball at the disc centre, against (A_u U + A_p P) / A from the sources
        for (const double area : {50.0, 300.0, 1090.0}) {
            const double v[3] = {0, 0, 1};
            const double rho = std::acos(1 - area * 1e-6);
            const render::SunSpotDraw s = render::spotDraw(v, rho, area);
            const double q = area >= 100 ? 6.0 : 4.5, P = 0.8561 - 0.0016 * std::clamp(695.7 * std::sin(rho) / 149597.8707 * 206264.80624709636, 10.0, 30.0);
            check(std::fabs(s.pen - P) < 1e-12, "sun094 (b): the penumbra factor for " + std::to_string(int(area)) + " millionths (" + std::to_string(s.pen) + ")");
            double sum[3] = {}, wsum = 0;
            const int N = 600;
            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++) {
                    const double x = (2 * (i + 0.5) / N - 1) * rho * 1.02, y = (2 * (j + 0.5) / N - 1) * rho * 1.02;
                    const double z2 = 1 - x * x - y * y;
                    const double b[3] = {x, y, std::sqrt(z2)};
                    if (std::acos(b[2]) > rho) continue;
                    const double w = 1 / b[2];   // the area on the ball of a grid cell of the plane
                    double f[3], cover;
                    render::spotsFactor(b, 1e-9, &s, 1, U, f, cover);
                    for (int c = 0; c < 3; c++) sum[c] += w * f[c];
                    wsum += w;
                }
            for (int c = 0; c < 3; c++) {
                const double want = (U[std::size_t(c)] + q * P) / (1 + q), got = sum[c] / wsum;
                say(std::fabs(got - want) < 2e-3, "sun094 (b): a spot of " + std::to_string(int(area)) + " millionths, channel " + std::to_string(c) + ": mean " + std::to_string(got) + " (sources: " + std::to_string(want) + ")");
            }
        }
    }
    // ---- (c) granulation ----
    {
        // the pattern's own mean and rms (kGranMean, kGranRms), on random points of the ball, two times
        std::mt19937_64 rng(94);
        std::normal_distribution<double> nd;
        for (const double f : {0.0, 0.5}) {
            double s = 0, s2 = 0;
            const int n = 400000;
            for (int i = 0; i < n; i++) {
                double b[3] = {nd(rng), nd(rng), nd(rng)};
                const double l = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
                for (double& x : b) x /= l;
                render::GranTime t; t.k = static_cast<std::uint32_t>(i % 29); t.f = f;
                const double g = render::granAt(b, t);
                s += g; s2 += g * g;
            }
            const double m = s / n, r = std::sqrt(s2 / n - m * m);
            say(std::fabs(m) < 0.005 && std::fabs(r - 1) < 0.005, "sun094 (c): the pattern has zero mean and unit rms at f = " + std::to_string(f) + ": " + std::to_string(m) + ", " + std::to_string(r));
        }
        // the spatial spectrum on a patch at the disc centre, 512 x 512 points over 48 Mm, 8 patterns: the power per ring of wavenumber (smoothed over +-6 rings,
        // +-15 % at the peak: the pattern's peak is broad) peaks at the measured 1.2 Mm
        {
            const int N = 512;
            const double L = 48.0;   // Mm
            std::vector<double> ring(N, 0.0);
            std::vector<std::complex<double>> a(std::size_t(N) * N), row(N);
            for (int pat = 0; pat < 8; pat++) {
                const render::GranTime t{12345u + 2u * static_cast<std::uint32_t>(pat), 0.0};
                for (int j = 0; j < N; j++)
                    for (int i = 0; i < N; i++) {
                        const double x = (i * L / N) / render::kSunRadiusMm, y = (j * L / N) / render::kSunRadiusMm, z = std::sqrt(1 - x * x - y * y);
                        const double b[3] = {x, y, z};
                        a[std::size_t(j) * N + std::size_t(i)] = render::granAt(b, t);
                    }
                for (int j = 0; j < N; j++) { for (int i = 0; i < N; i++) row[std::size_t(i)] = a[std::size_t(j) * N + std::size_t(i)]; fft(row); for (int i = 0; i < N; i++) a[std::size_t(j) * N + std::size_t(i)] = row[std::size_t(i)]; }
                for (int i = 0; i < N; i++) { for (int j = 0; j < N; j++) row[std::size_t(j)] = a[std::size_t(j) * N + std::size_t(i)]; fft(row); for (int j = 0; j < N; j++) a[std::size_t(j) * N + std::size_t(i)] = row[std::size_t(j)]; }
                for (int j = 0; j < N; j++)
                    for (int i = 0; i < N; i++) {
                        const int kx = i <= N / 2 ? i : i - N, ky = j <= N / 2 ? j : j - N;
                        const int k = static_cast<int>(std::lround(std::hypot(kx, ky)));
                        if (k > 0 && k < N) ring[std::size_t(k)] += std::norm(a[std::size_t(j) * N + std::size_t(i)]);
                    }
            }
            int best = 0; double bestP = 0;
            for (int k = 8; k < N / 2 - 8; k++) { double p = 0; for (int d = -6; d <= 6; d++) p += ring[std::size_t(k + d)]; if (p > bestP) { bestP = p; best = k; } }
            const double peakMm = L / best;
            say(std::fabs(peakMm - 1.2) < 0.12, "sun094 (c): the granulation's spectrum peaks at " + std::to_string(peakMm) + " Mm (Hathaway et al. 2015: about 1.2 Mm; 10 %)");
        }
        // the rms contrast at the disc centre, per channel, against the source's weighted per channel (granContrastRgb), and that against the table
        {
            const auto& C = render::granContrastRgb();
            // (a linear-sRGB channel's CIE weights are negative at some wavelengths, so a channel's weighted mean can pass the table's end a little: 0.002 allowed)
            say(C[2] > C[1] && C[1] > C[0] && C[0] >= 0.111 - 0.002 && C[2] <= 0.215 + 0.002, "sun094 (c): the contrast is larger in blue, within the measured 0.111-0.215: " + std::to_string(C[0]) + " " + std::to_string(C[1]) + " " + std::to_string(C[2]));
            const render::SunLimb& lim = render::sunLimb();
            double s[3] = {}, s2[3] = {};
            int n = 0;
            const render::GranTime t{777u, 0.3};
            for (int j = 0; j < 300; j++)
                for (int i = 0; i < 300; i++) {
                    const double x = (i - 150) * 0.15 / render::kSunRadiusMm, y = (j - 150) * 0.15 / render::kSunRadiusMm;   // 0.15 Mm apart, 45 Mm across
                    const double b[3] = {x, y, std::sqrt(1 - x * x - y * y)};
                    const double g = render::granAt(b, t);
                    for (int c = 0; c < 3; c++) { const double I = lim.centre[c] * render::limbP(lim.poly[c], b[2]) * (1 + g * C[std::size_t(c)]); s[c] += I; s2[c] += I * I; }
                    n++;
                }
            for (int c = 0; c < 3; c++) {
                const double m = s[c] / n, r = std::sqrt(s2[c] / n - m * m) / m;
                say(std::fabs(r / C[std::size_t(c)] - 1) < 0.03, "sun094 (c): the rms contrast at the disc centre, channel " + std::to_string(c) + ": " + std::to_string(r) + " (source weighted: " + std::to_string(C[std::size_t(c)]) + ")");
            }
        }
        // the grain adds no light: the disc's mean with the grain equals Neckel & Labs's F/I times the centre (Equation 6), per channel
        {
            const render::SunLimb& lim = render::sunLimb();
            const auto& C = render::granContrastRgb();
            const render::GranTime t{4242u, 0.7};
            double sum[3] = {}, ref[3] = {};
            const int N = 1400;
            for (int j = 0; j < N; j++)
                for (int i = 0; i < N; i++) {
                    const double x = 2 * (i + 0.5) / N - 1, y = 2 * (j + 0.5) / N - 1;
                    if (x * x + y * y >= 1) continue;
                    const double b[3] = {x, y, std::sqrt(1 - x * x - y * y)};
                    const double g = render::granAt(b, t);
                    for (int c = 0; c < 3; c++) { const double I = render::limbP(lim.poly[c], b[2]); sum[c] += I * (1 + g * C[std::size_t(c)]); ref[c] += I; }
                }
            for (int c = 0; c < 3; c++) {
                const double area = pi * 0.25 * N * N;   // the grid cells inside the unit disc, (N/2)^2 pi
                const double got = sum[c] / area, want = render::limbFlux(lim.poly[c]);
                say(std::fabs(got / want - 1) < 2e-3 && std::fabs(sum[c] / ref[c] - 1) < 1e-3, "sun094 (c): with the grain the disc's mean is Neckel & Labs's, channel " + std::to_string(c) + ": " + std::to_string(got) + " (" + std::to_string(want) + ")");
            }
        }
        // time: the same pattern at the same time, still close 2.5 minutes later, nothing shared 10 minutes later (one lifetime)
        {
            const double jd0 = 2460440.5 + 0.123456;
            const auto corr = [&](double dtMin) {
                double sa = 0, sb = 0, sab = 0, saa = 0, sbb = 0;
                std::mt19937_64 r(5);
                std::normal_distribution<double> nd2;
                const int n = 60000;
                for (int i = 0; i < n; i++) {
                    double b[3] = {nd2(r), nd2(r), nd2(r)};
                    const double l = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
                    for (double& x : b) x /= l;
                    const double ga = render::granAt(b, render::granTime(jd0)), gb = render::granAt(b, render::granTime(jd0 + dtMin / 1440));
                    sa += ga; sb += gb; sab += ga * gb; saa += ga * ga; sbb += gb * gb;
                }
                const double ma = sa / n, mb = sb / n;
                return (sab / n - ma * mb) / std::sqrt((saa / n - ma * ma) * (sbb / n - mb * mb));
            };
            const double c0 = corr(0), c25 = corr(2.5), c10 = corr(10), c15 = corr(15);
            say(c0 > 0.9999 && c25 > 0.4 && std::fabs(c10) < 0.02 && std::fabs(c15) < 0.02, "sun094 (c): the cells change at their lifetime: correlation " + std::to_string(c0) + " now, " + std::to_string(c25) + " after 2.5 min, " +
                  std::to_string(c10) + " after 10 min, " + std::to_string(c15) + " after 15 min");
        }
        check(render::granFade(1.0 / 3) == 1 && render::granFade(1.0 / 1.5) == 0 && render::granFade(1.0 / 2.25) > 0.4 && render::granFade(1.0 / 2.25) < 0.6, "sun094 (c): the grain fades from 3 to 1.5 pixels a cell");
        const std::string fs = render::kPlanetFS;
        char cs[64];
        std::snprintf(cs, sizeof cs, "(%.5f - granRaw(q, k + 1u)) / %.5f", render::kGranMean, render::kGranRms);
        check(fs.find(cs) != std::string::npos && fs.find("granRaw(q, k + 1u)") != std::string::npos && fs.find("uint pcgHash(uint v)") != std::string::npos,
              "sun094 (c): kPlanetFS draws the same pattern with the same mean and rms (" + std::string(cs) + ")");
    }
    // ---- (d) the photo ----
    {
        orbit::SunPhotoMeta m;
        std::string why, json, xml;
        for (const auto& l : block("sun/hmi/hmi_ic_closest_20240510.extract.txt", "reply")) json += noCr(l);
        for (const auto& l : block("sun/hmi/hmi_ic_156593381_header.extract.txt", "header")) xml += noCr(l) + "\n";
        check(orbit::parseClosestReply(json, m, why) && m.id == 156593381 && m.date == "2024-05-09 23:59:50", "sun094 (d): Helioviewer's reply is read (" + why + ")");
        check(std::fabs(m.jd - (t085::jdOf(2024, 5, 9) + (23 + 59.0 / 60 + 50.0 / 3600) / 24)) < 1e-9, "sun094 (d): the reply's time as a Julian day");
        check(orbit::parseHeaderXml(xml, m, why) && m.crlnObs == 313.56503 && m.crltObs == -3.2012870 && m.crota2 == 0 && m.wavelnth == 6173 && m.rsunObs == 950.44952 && m.cdelt1 == 0.60000002,
              "sun094 (d): the header's keys (" + why + ")");
        check(orbit::photoDayKey(t085::jdOf(2024, 5, 9.9)) == "2024-05-10" && orbit::photoDayKey(t085::jdOf(2024, 5, 10.4)) == "2024-05-10" && orbit::photoDayKey(t085::jdOf(2024, 5, 10.6)) == "2024-05-11",
              "sun094 (d): the photo of the day nearest 00:00 UT");
        // the photo's frame against the Sun's IAU frame (orbit/spin.cpp, whose longitudes are Carrington's): the observer's place in that frame at the photo's time
        {
            orbit::SpinFrame F; orbit::spinFrame("Sun", m.jd, F);
            const double lam = (t085::sunLongitudeJ2000(m.jd) + 180) * pi / 180;
            const orbit::Vec3 e{std::cos(lam), std::sin(lam), 0};
            const double L = std::atan2(orbit::dot(e, F.B), orbit::dot(e, F.A)) * 180 / pi, B = std::asin(orbit::dot(e, F.P)) * 180 / pi;
            const double dL = std::remainder(L - m.crlnObs, 360.0), dB = B - m.crltObs;
            say(std::fabs(dL) < 0.1 && std::fabs(dB) < 0.05, "sun094 (d): the photo's observer in the Sun's IAU frame: longitude " + std::to_string(L) + " (CRLN_OBS 313.565, off " + std::to_string(dL) +
                  "), latitude " + std::to_string(B) + " (CRLT_OBS -3.201, off " + std::to_string(dB) + ")");
        }
        // with the sources checkout: the saved picture itself
        if (const char* src = std::getenv("PL_SOURCES_DIR")) {
            const std::string png = readFile(std::string(src) + "/data/sun/hmi/hmi_ic_156593381_s8.png");
            const orbit::SunPhoto p = orbit::loadSunPhoto(std::vector<std::uint8_t>(png.begin(), png.end()), readFile(std::string(src) + "/data/sun/hmi/hmi_ic_156593381_header.xml"),
                                                          readFile(std::string(src) + "/data/sun/hmi/hmi_ic_closest_20240510.json"));
            check(p.ok && p.w == 512 && p.scale == 8 && std::fabs(p.cx - 255.5) < 1e-9 && std::fabs(p.cy - 255.5) < 1e-9, "sun094 (d): the saved picture is read (" + p.why + ")");
            if (p.ok) {
                // the grey level is linear: the picture's limb fit over its centre is Neckel & Labs's at 617.3 nm
                double A[6]; render::nl94At(617.3, A);
                double worst = 0;
                for (double mu = 0.4; mu <= 1.0001; mu += 0.05) worst = std::max(worst, std::fabs(orbit::photoFitAt(p, mu) / orbit::photoFitAt(p, 1) - render::limbP(A, mu)));
                say(worst < 0.01, "sun094 (d): the picture's edge darkening is Neckel & Labs's at 617.3 nm (largest difference " + std::to_string(worst) + " for mu 0.4-1): its grey is linear");
                // the largest group: the centre of its spots (umbrae and penumbrae: below 0.8 of the fitted surface, weighted by 1 - rel) within 30 pixels (0.15 of the
                // radius) of the darkest pixel, mapped back to Carrington's longitude and latitude. NOAA gives a group's place in whole degrees; within 1.5 degrees.
                int bi = 0, bj = 0; double bmin = 9;
                std::vector<double> rel(std::size_t(p.w) * p.h, 1.0);
                for (int j = 0; j < p.h; j++)
                    for (int i = 0; i < p.w; i++) {
                        const double x = (i - p.cx) / p.rpx, y = (p.cy - j) / p.rpx, r2 = x * x + y * y;
                        if (r2 > 0.9) continue;
                        const double mu = std::sqrt(1 - r2);
                        const double v = p.grey[std::size_t(j) * p.w + std::size_t(i)] / orbit::photoFitAt(p, mu);
                        rel[std::size_t(j) * p.w + std::size_t(i)] = v;
                        if (v < bmin) { bmin = v; bi = i; bj = j; }
                    }
                double sw = 0, sx = 0, sy = 0;
                for (int j = bj - 30; j <= bj + 30; j++)
                    for (int i = bi - 30; i <= bi + 30; i++) {
                        if (i < 0 || j < 0 || i >= p.w || j >= p.h) continue;
                        const double v = rel[std::size_t(j) * p.w + std::size_t(i)];
                        if (v < 0.8) { sw += 1 - v; sx += (1 - v) * i; sy += (1 - v) * j; }
                    }
                const double x = (sx / sw - p.cx) / p.rpx, y = (p.cy - sy / sw) / p.rpx, z = std::sqrt(std::max(0.0, 1 - x * x - y * y));
                double b[3];
                for (int k = 0; k < 3; k++) b[k] = x * p.W[k] + y * p.N[k] + z * p.O[k];
                const double lon = std::fmod(std::atan2(b[1], b[0]) * 180 / pi + 360, 360.0), lat = std::asin(b[2]) * 180 / pi;
                // NOAA SRS of 10 May 2024 (native/data/sun/spots/noaa_srs_20240510.extract.txt): 3664 S19W34, Lo 348, the largest region (1090 millionths)
                bool srs = false;
                for (const auto& l : block("sun/spots/noaa_srs_20240510.extract.txt", "part I")) srs = srs || l.rfind("3664 S19W34   348  1090", 0) == 0;
                check(srs, "sun094 (d): the SRS extract has region 3664 at S19W34, Lo 348, 1090 millionths");
                say(std::fabs(std::remainder(lon - 348, 360.0)) < 1.5 && std::fabs(lat + 19) < 1.5, "sun094 (d): the photo's largest group is at Carrington " + std::to_string(lon) + ", latitude " + std::to_string(lat) +
                      " (NOAA SRS region 3664: 348, -19; within 1.5 degrees)");
                // photoAt at that place gives the dark spot, and the disc centre about 1
                double rv, wv;
                orbit::photoAt(p, b, rv, wv);
                check(rv < 0.95 && wv == 1, "sun094 (d): photoAt there is darker than the surface (" + std::to_string(rv) + ")");
                orbit::photoAt(p, p.O, rv, wv);
                check(std::fabs(rv - 1) < 0.05 && wv == 1, "sun094 (d): photoAt at the photo's disc centre is about 1 (" + std::to_string(rv) + ")");
                const double back[3] = {-p.O[0], -p.O[1], -p.O[2]};
                orbit::photoAt(p, back, rv, wv);
                check(wv == 0, "sun094 (d): the far side is not in the photo");
            }
        } else {
            std::printf("sun094 (d): PL_SOURCES_DIR not set: the saved HMI picture's checks (linear grey, the spot on NOAA's place) are skipped\n");
        }
    }
    // ---- (e) the loader: no network in tests; missing and bad files give the drawn surface ----
    {
        const orbit::SunPhoto bad = orbit::loadSunPhoto(std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}, "<NAXIS1>4096", "{}");
        check(!bad.ok && !bad.why.empty(), "sun094 (e): a bad file is refused (" + bad.why + ")");
        namespace fs = std::filesystem;
        const fs::path dir = fs::temp_directory_path() / "pl_sun094_cache";
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
#ifdef _WIN32
        _putenv("PL_SUNPHOTO_OFFLINE=1");
#else
        setenv("PL_SUNPHOTO_OFFLINE", "1", 1);
#endif
        orbit::SunPhotoSource& S = orbit::SunPhotoSource::get();
        S.setCacheDir(dir.string());
        check(!S.canDownload(), "sun094 (e): the tests never download (PL_SUNPHOTO_OFFLINE or a build without the download)");
        const auto waitDone = [&](double jd) { for (int i = 0; i < 500; i++) { S.want(jd, true); if (S.lastLoad().done && S.lastLoad().day == orbit::photoDayKey(jd)) break; std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
        const double jd = t085::jdOf(2024, 5, 10);
        waitDone(jd);
        check(S.shown() == nullptr && !S.lastLoad().ok, "sun094 (e): an empty cache and no download: the drawn surface");
        bool says = false;
        for (const auto& l : S.cardLines(jd)) says = says || (l.find("Surface: drawn") == 0);
        check(says, "sun094 (e): the card says the surface is drawn");
        std::ofstream(dir / "2024-05-11.json") << "{\"id\":\"1\",\"date\":\"2024-05-11 00:00:00\",\"name\":\"HMI Int\"}";
        std::ofstream(dir / "2024-05-11.xml") << "<x>";
        std::ofstream(dir / "2024-05-11.png") << "not a png";
        const double jd2 = t085::jdOf(2024, 5, 11);
        waitDone(jd2);
        bool cannot = false;
        for (const auto& l : S.cardLines(jd2)) cannot = cannot || l.find("cannot be read") != std::string::npos;
        check(S.shown() == nullptr && cannot, "sun094 (e): a bad saved photo: the drawn surface, and the card says it cannot be read");
        if (const char* src = std::getenv("PL_SOURCES_DIR")) {   // the saved picture as the cache's day: it is shown
            fs::copy_file(fs::path(src) / "data/sun/hmi/hmi_ic_156593381_s8.png", dir / "2024-05-12.png", fs::copy_options::overwrite_existing, ec);
            fs::copy_file(fs::path(src) / "data/sun/hmi/hmi_ic_156593381_header.xml", dir / "2024-05-12.xml", fs::copy_options::overwrite_existing, ec);
            std::string j = readFile(std::string(src) + "/data/sun/hmi/hmi_ic_closest_20240510.json");
            j.replace(j.find("2024-05-09 23:59:50"), 19, "2024-05-12 00:00:10");   // (its own day is 10 May; the copy stands for 12 May)
            std::ofstream(dir / "2024-05-12.json") << j;
            const double jd3 = t085::jdOf(2024, 5, 12);
            waitDone(jd3);
            check(S.shown() != nullptr && S.lastLoad().ok && !S.lastLoad().fromNet, "sun094 (e): a good saved photo is shown, from the cache");
            bool nasa = false;
            for (const auto& l : S.cardLines(jd3)) nasa = nasa || l.find("Courtesy of NASA/SDO and the AIA, EVE, and HMI science teams") != std::string::npos;
            check(nasa, "sun094 (e): the card names NASA SDO (its credit line)");
        }
        S.dropMemory();
        fs::remove_all(dir, ec);
    }
    // ---- the card ----
    {
        orbit::SpotDay d = orbit::sunspotsAt(t085::jdOf(2024, 5, 10));
        bool dark = false, outl = false;
        for (const auto& l : orbit::spotCardLines(d)) { dark = dark || l.find("Dark core (umbra)") != std::string::npos; outl = outl || l.find("Outline only") != std::string::npos; }
        check(dark && !outl, "sun094: the card says how the spots' darkness is drawn, and no longer \"Outline only\"");
    }
}
