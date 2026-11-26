// 0.86.0: a star's true light on a camera's exposure. ONE formula for both labs: the Black Hole Lab's true exposure (blackhole/light.h, 0.55.4, which says where
// each number comes from and uses these) and the Orbit Lab's stars with Lighting: True sunlight (orbit/true_light.h, orbit_view.cpp drawSky).
//   - a star's catalogue magnitude is its V magnitude (HYG v4.1 README, fields 12 and 25: data/stars/hyg_readme.extract.txt).
//   - the Sun's apparent V magnitude: -26.76, Johnson V, Vega system: C. N. A. Willmer, ApJS 236, 47 (2018), arXiv:1804.07788, Table 3, row "Johnson V",
//     column (5) "App (Vega)" (data/photometry/willmer2018_arxiv1804.07788.extract.txt; tests/test_blackhole.cpp testTrueLight reads it).
//   - Pogson's scale (IAU 2015 Resolution B2, eq. 4; orbit/sky.h): a star of magnitude m gives 10^(-0.4 (m - m_Sun)) of the Sun's light in the band.
//   - the Sun's light at 1 au = its surface luminance x the solid angle of its disc, Omega_Sun = pi (R_Sun / 1 au)^2 (R_Sun 695,700 km, IAU 2015 B3; the IAU au).
//   So a star's light, in units of (the Sun's surface luminance x 1 sr): E(m) = Omega_Sun x 10^(-0.4 (m - m_Sun)). On a picture whose screen white is `white`
//   (Sun's surface units) and where one square point is ptSr steradians, its light in units of (the screen's white x one square point) is E(m) / (white ptSr):
//   the input of orbit/sky.h starDrawLight (a dot of radius kStarMinRadius points, growing above opacity 1).
// THE ORBIT LAB'S SCREEN WHITE (0.86.0; REPORT 0.86.0, the feasibility step). A body's linear colour is its map scaled so that the ball seen fully lit has its
//   measured geometric albedo (render/planet_maps.h mapAlbedo), times the light factor (render/shading.h: cos i) and (d_E / d)^2 (orbit/true_light.h). So linear 1
//   on the screen is a white Lambert surface (albedo 1) lit face-on by the Sun at the exposure distance d_E. Its luminance is E_Sun(d_E) / pi, and
//   E_Sun(d_E) = L_Sun's surface x Omega_Sun(1 au) x (1 au / d_E)^2. In units of the Sun's surface luminance: white(d_E) = (Omega_Sun / pi) / (d_E / 1 au)^2
//   = (R_Sun / 1 au)^2 / (d_E / 1 au)^2 (2.16e-5 at 1 au). Approximations (said in the app): the V band taken as the eye's visible light, as in the Black Hole Lab;
//   the geometric albedo's band is V too (Mallama et al. 2017, the NSSDC fact sheets).
// THE SCREEN'S FAINTEST STEP: an 8-bit sRGB screen shows linear light L as code round(255 x 12.92 L) near black (IEC 61966-2-1's linear segment): a channel is
//   lit when L >= 0.5 / (255 x 12.92) of the screen's white. A star is drawn if and only if its dot's opacity reaches it (starShown).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "orbit/sky.h"
#include "orbit/true_light.h"
#include "orbit/units.h"

namespace orbit {

constexpr double kSunV = -26.76;                       // the Sun's apparent V magnitude (Vega system): Willmer 2018 Table 3, Johnson V, column (5)
constexpr double kSunAbsV = 4.81;                      // its absolute V magnitude: Table 3 column (2), the abstract's M_V = 4.81
constexpr double kScreenStep = 0.5 / (255 * 12.92);   // the faintest linear light (over the screen's white) an 8-bit sRGB screen shows (IEC 61966-2-1)

inline double sunSolidAngle() { const double a = units::kSunRadius / units::kAU; return 3.14159265358979323846 * a * a; }   // pi (R_Sun / 1 au)^2, sr
inline double starLight(double magV) { return sunSolidAngle() * std::pow(10.0, -0.4 * (magV - kSunV)); }                   // E(m), Sun's surface x sr
inline double starTrueLight(double magV, double white, double ptSr) { return starLight(magV) / (white * ptSr); }           // screen white x square point
// the dot's opacity (for a dot no larger than the smallest) reaches the screen's faintest step; a larger dot is at opacity 1
inline bool starShown(const StarDraw& d) { return d.alpha >= kScreenStep; }
// the Orbit Lab's screen white with the exposure set for d_E (au), in units of the Sun's surface luminance (above)
inline double sunlitWhite(double dExposureAU) { return sunSolidAngle() / 3.14159265358979323846 / (dExposureAU * dExposureAU); }
// one square point's solid angle at (X, Y) of a pinhole picture (focal F points, principal point (ppx, ppy)): cos^3(theta) / F^2, tan theta = rho / F
inline double pinholePointSr(double F, double ppx, double ppy, double X, double Y) {
    const double rho2 = (X - ppx) * (X - ppx) + (Y - ppy) * (Y - ppy), c2 = F * F / (F * F + rho2);
    return c2 * std::sqrt(c2) / (F * F);
}
// the same for the equidistant map (orbit/sky.cpp skyScreen, K points per radian, middle (cx, cy)): sin(th) / (th K^2)
inline double mapPointSr(double K, double cx, double cy, double X, double Y) {
    const double th = std::hypot(X - cx, Y - cy) / K;
    return (th > 1e-9 ? std::sin(th) / th : 1.0) / (K * K);
}

// One star's dot in the Orbit Lab's sky (drawSky). Even lighting (trueWhite <= 0): sky.h starDraw, its opacity times the sky's level lv (0.50's rule, unchanged).
// True sunlight (trueWhite > 0, the screen white above): its true light on the exposure (no lv: the exposure sets it), drawn only where starShown.
struct SkyDot { double radius = 0, alpha = 0; bool drawn = false; };
inline SkyDot skyStarDot(double magV, double lv, double trueWhite, double ptSr) {
    SkyDot o;
    if (!(trueWhite > 0)) {
        const StarDraw d = starDraw(magV);
        o.radius = d.radius; o.alpha = d.alpha * lv; o.drawn = true;
        return o;
    }
    const StarDraw d = starDrawLight(starTrueLight(magV, trueWhite, ptSr));
    o.radius = d.radius; o.alpha = d.alpha; o.drawn = starShown(d);
    return o;
}

// 0.86.0 (Part D): outside True sunlight the catalogue's brightness range is squeezed so the faint stars stay visible (sky.h starDraw: drawn light ∝ light^kStarGamma).
// The numbers from the code: the catalogue's brightest and faintest magnitudes (skyData, stars left out of a sky skipped), their light ratio by Pogson and the
// drawn ratio (the dots grow above opacity 1, so the drawn light is exactly kStarFaintLight (F / F(6.6))^0.5 for every star: tests/test_orbit.cpp).
struct StarSqueeze { double magBright = 0, magFaint = 0, lightRatio = 1, drawnRatio = 1; };
inline StarSqueeze starSqueeze() {
    StarSqueeze q;
    q.magBright = 1e9; q.magFaint = -1e9;
    for (const auto& st : skyData().stars) if (st.mag < 30) { q.magBright = std::min(q.magBright, double(st.mag)); q.magFaint = std::max(q.magFaint, double(st.mag)); }
    q.lightRatio = std::pow(10.0, 0.4 * (q.magFaint - q.magBright));
    q.drawnRatio = std::pow(q.lightRatio, kStarGamma);
    return q;
}
// The Sky choice's caption (skyMode: 0 Soft, 1 Full, 2 Off). trueStars: the stars are on the exposure (True sunlight in the Solar System scene).
inline std::string skyCaption(int skyMode, bool trueStars) {
    if (skyMode == 2) return "plain background";
    if (trueStars)
        return "On the camera's exposure (True sunlight): each star by its true light, from its V magnitude against the Sun's, drawn only where it reaches the "
               "faintest step the screen shows. A camera exposed for a sunlit planet near the Sun records no stars, as photos from the Moon show; far from the Sun "
               "more appear. Soft and Full are the same here. The Milky Way's map is not drawn: its brightness is not calibrated.";
    const StarSqueeze q = starSqueeze();
    char b[64];
    std::snprintf(b, sizeof b, "%.1f to %.1f", q.magBright, q.magFaint);
    return std::string(skyMode == 0 ? "Dim close up, brighter as you zoom out." : "Always bright.") + " Not on the planets' exposure: the catalogue's brightness range is "
           "squeezed so the faint stars stay visible. Its stars (magnitude " + b + ") differ " + timesText(q.lightRatio) + " times in light; they are drawn " +
           timesText(q.drawnRatio) + " times apart (the drawn light goes as the square root of the light).";
}

}  // namespace orbit
