// Black Hole Lab (0.55.4): real light in the Real look: one true exposure for the stars and the disc, and the glare an eye would see. Physics only, no
// graphics (the view, blackhole_view.cpp, and the renderer's composite use it; the panel reads its numbers). Every number below is from a saved source
// (the private sources repo; the lines used are copied exactly into the extracts named, which tests/test_blackhole.cpp testTrueLight and testGlare read).
//
// ONE TRUE EXPOSURE (part B). The disc's light is already on one scale: luminance Y (CIE 1931 ybar) in units of the Sun's surface, the Sun's surface taken as a
//   black body at its effective temperature (disc.h blackBodyLinear, visibleVsSun; orbit/sun_facts.h kSunTeffK). The bridge for a star:
//   - its catalogue magnitude is the V magnitude: the HYG README (data/stars/hyg_readme.extract.txt), field 12 "mag: The star's apparent visual magnitude"
//     and field 25 "... adjusted to the V magnitude scale to match the "mag" field" (the sky's catalogue is HYG v4.1, orbit/sky.h).
//   - the Sun's own apparent magnitude in the same band: Johnson V, Vega system, -26.76: C. N. A. Willmer, ApJS 236, 47 (2018), arXiv:1804.07788, Table 3
//     ("Magnitudes of the Sun", row "Johnson V", column (5) "App (Vega)"; its absolute magnitude, column (2), 4.81, is the abstract's M_V = 4.81; the two differ by
//     the distance modulus of 1 AU, 5 log10(10 pc / 1 AU) = 31.57, a check the test makes; data/photometry/willmer2018_arxiv1804.07788.extract.txt).
//   - Pogson's scale (orbit/sky.h: IAU 2015 Resolution B2, eq. 4): a star of magnitude m gives 10^(-0.4 (m - m_Sun)) of the Sun's light in the band.
//   - the Sun's light at 1 AU = its surface brightness x the solid angle of its disc, Omega_Sun = pi (R_Sun / 1 AU)^2 (a disc of uniform brightness: the Sun's
//     limb darkening is not in the app, ROADMAP; R_Sun = 695,700 km, IAU 2015 B3, and the IAU au, orbit/units.h).
//   So a star's light, in units of (the Sun's surface luminance x 1 sr):  E(m) = Omega_Sun x 10^(-0.4 (m - m_Sun)).  A "star" of the Sun's magnitude spread
//   over the Sun's disc is the Sun's surface (the identity the test checks). Approximations, stated in the app: the V band is taken as the eye's visible light
//   (V and the CIE photopic curve are close but not equal; the exact conversion depends on the spectrum: Bara & Bao-Varela 2023, section 2, below); the Sun's
//   surface as a black body at its effective temperature.
//   A STAR IS A POINT; a screen shows areas. Its light is spread over the dot the night sky's one rule draws (orbit/sky.h starDrawLight: a disc of radius
//   kStarMinRadius points, growing when its opacity would pass 1), and the dot's luminance is that light over the dot's solid angle. The rule lives in one
//   place: starTrueLight below (its light in units of the screen's white x one square point) into orbit::starDrawLight. A screen point's solid angle comes from
//   the picture's projection (the equidistant map of schwarzschild.h viewDirection: sin(th) / (th K^2) sr per square point, K points per radian).
//   THE SCREEN'S FAINTEST STEP: an 8-bit sRGB screen shows linear light L as code round(255 x 12.92 L) near black (the linear segment of IEC 61966-2-1,
//   the app's encoding everywhere): a channel is lit when L >= 0.5 / (255 x 12.92) of the screen's white.
//   THE SKY MAP (ESA's Gaia map) has no calibrated brightness: its pixels are a picture, not measured surface brightness. Under the true exposure it is not drawn:
//   its own pixels are fainter than the catalogue stars it contains, and those are already far below what the screen can show (the readout's factor).
//
// THE EYE'S GLARE (part C). A very bright source scatters light inside the eye and lays a veil round itself. The model: the CIE general disability glare equation
//   (J. J. Vos, T. J. T. P. van den Berg, "Report on disability glare", CIE collection 1999, 135, equation 11), as printed in two saved papers that agree:
//   S. Bara, C. Bao-Varela, IJSL (2023), arXiv:2212.09103, Eqs. (1), (2) (data/blackhole/glare/bara2023_arxiv2212.09103.extract.txt) and T. J. T. P. van den
//   Berg, L. Franssen, B. Kruijt, J. E. Coppens, Z. Med. Phys. 23, 6 (2013), p. 15 (data/blackhole/glare/vandenberg2013_straylight_review.extract.txt):
//     Psi(theta) = L_eq / E_gl = 10 / theta^3 + (5 / theta^2 + 0.1 p / theta) [1 + (A / 62.5)^4] + 2.5 x 10^-3 p    [sr^-1]
//   theta the angle from the source in DEGREES, valid from 0.1 to 100 degrees (Bara: "theta ranging from 0.1 deg to the limit of the visual field (~100 deg)"),
//   A the age in years, p the iris pigmentation: 0 dark eyes, 0.5 brown, 1.0 blue-green. E_gl: the illuminance the source makes at the eye; L_eq: the luminance of
//   the veil it lays at angle theta. The app's eye: A = 35 years, p = 0.5 (brown): the "average observer of age 35 yr and pigmentation factor p = 0.5" of
//   Bara & Bao-Varela's Figure 2.
//   CONSERVATION. The whole PSF is normalised to 1 ("its integral across the visual field is unity", Bara section 2; "At 90 degrees the integrated value is 1 by
//   definition", van den Berg et al., Figure 4's caption). The general equation gives the part from 0.1 to 100 degrees: its integral s (glareShare, by
//   quadrature) is the share of each source's light the eye spreads; the rest, 1 - s, lies within 0.1 degrees of the source and stays where it is drawn. So glare
//   MOVES light: each source keeps 1 - s of its light and lays s of it round itself by Psi. (The Film look's glow, looks.h, only adds.)
//   LEFT OUT (the app says so): the structure within 0.1 degrees ("strongly determined by the particular features of the ... eye aberration pattern of each
//   person", Bara section 4); the eye's adaptation and afterimages; the coloured rings and the streaks (the ciliary corona) some eyes see; the change of
//   scattering with the light's colour (the model is one curve for all colours, applied to each channel alike). It is an average eye's; a camera's lens would
//   show a different veil.
//   ON THE PICTURE: each pixel's angle comes from the picture's projection; the light is averaged over blocks of about kGlareBlockDeg (at least one pixel) and
//   spread by Psi between the blocks, near blocks at the finest level and far ones on coarser grids (each pair of blocks once, at the finest level where the two
//   are more than kGlareNear blocks apart while their parents are not), then added back bilinearly between the blocks' centres. The weight of a pair: Psi at the
//   exact angle between the two centres, times the ratio of Psi's mean over the two blocks to its value between their centres on a flat patch (the blocks'
//   own size); for blocks within kGlareNear of each other (the block itself too: the light the model puts between 0.1 degrees and the block's edge stays in it)
//   Psi's mean over the two blocks on a flat patch.
#pragma once

#include <vector>

namespace blackhole {

// ---- part B: one true exposure ----
constexpr double kSunV = -26.76;           // the Sun's apparent V magnitude (Vega system): Willmer 2018 Table 3, Johnson V, column (5)
constexpr double kSunAbsV = 4.81;          // its absolute V magnitude: Table 3 column (2), the abstract's M_V = 4.81
constexpr double kScreenStep = 0.5 / (255 * 12.92);   // the faintest linear light (over the screen's white) an 8-bit sRGB screen shows (IEC 61966-2-1)
double sunSolidAngle();                    // pi (R_Sun / 1 AU)^2, sr
double starLight(double magV);             // E(m) = Omega_Sun 10^(-0.4 (m - m_Sun)): the Sun's surface luminance x sr
double starSurface(double magV, double omegaSr);   // the same spread over omegaSr: luminance in units of the Sun's surface
double starDotArea();                      // the smallest drawn dot (orbit/sky.h kStarMinRadius), square points
// a star's light in units of (the screen's white x one square point) at an exposure `white` (the screen's white in units of the Sun's surface), where one
// square point is ptSr steradians: the input of orbit::starDrawLight
double starTrueLight(double magV, double white, double ptSr);
bool starVisible(double magV, double white, double ptSr);   // its dot's light reaches the screen's faintest step
// the solid angle of one square point at (X, Y) of the picture (equidistant map, K points per radian, middle at (cx, cy))
double pointSolidAngle(double K, double cx, double cy, double X, double Y);
// the readout's numbers for the brightest catalogue star (brightMag) beside the disc (discY: the disc's brightest part, Sun's surface units), at exposure white,
// at the middle of the picture (ptSr = 1 / K^2)
struct TrueSkyNumbers {
    double starY = 0;          // the star's dot's luminance (Sun's surface units)
    double fainter = 0;        // discY / starY: how many times the star is fainter than the disc's brightest part
    double belowStep = 0;      // kScreenStep x white / starY: how many times below the screen's faintest step (< 1: it shows)
    double showsAtWhite = 0;   // the exposure (screen white, Sun's surface units) at which it would first show
};
TrueSkyNumbers trueSkyNumbers(double brightMag, double discY, double white, double ptSr);

// ---- part C: the eye's glare ----
constexpr double kGlareMinDeg = 0.1, kGlareMaxDeg = 100;   // the general equation's range (Bara section 2; van den Berg et al. 2013)
constexpr double kEyeAge = 35, kEyePigment = 0.5;          // the app's eye: Bara & Bao-Varela's Figure 2 (35 years, brown eyes)
constexpr double kGlareBlockDeg = 0.2;                     // the finest block (the app's choice, stated): about two of the model's inner limit
constexpr int kGlareNear = 2;                              // blocks closer than this (Chebyshev) are handled one level finer
double glarePsf(double thetaDeg, double age = kEyeAge, double p = kEyePigment);   // Psi, sr^-1; 0 outside [0.1, 100] degrees
double glareShare(double age = kEyeAge, double p = kEyePigment);                  // s = Int_{0.1 deg}^{100 deg} Psi 2 pi sin(theta) dtheta (cached for the app's eye)
double glareShareGauss(double age, double p);                                     // the same by Gauss-Legendre in theta (a second quadrature, for the test)

// The picture: W x H pixels, the middle at (cx, cy), K pixels per radian (the equidistant map). The finest grid: blocks of b pixels, nx x ny of them.
struct GlareGrid { int W = 0, H = 0, b = 1, nx = 0, ny = 0; double cx = 0, cy = 0, K = 1; };
GlareGrid glareGrid(int W, int H, double cx, double cy, double K);
double glareBlockSolidAngle(const GlareGrid& g, int i, int j);   // the solid angle of block (i, j) (its pixels inside the picture), sr
// src: nx x ny x 3, each block's mean linear light (units of the screen's white). veil: nx x ny x 3, the veil's luminance at each block's centre (same units).
// The light each source keeps is (1 - glareShare()) of its own (applied by the caller). Sums (for the tests): the light spread (s x Sum E), the light the veil
// lays on the picture (Sum veil x Omega), both in units of white x sr.
struct GlareSums { double spread = 0, laid = 0; double ms = 0; };
void glareVeil(const GlareGrid& g, const std::vector<float>& src, std::vector<float>& veil, GlareSums* sums = nullptr, double age = kEyeAge, double p = kEyePigment);
// The reference: every pair of blocks directly (slow; small pictures only)
void glareVeilDirect(const GlareGrid& g, const std::vector<float>& src, std::vector<float>& veil, double age = kEyeAge, double p = kEyePigment);

}  // namespace blackhole
