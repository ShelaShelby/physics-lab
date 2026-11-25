// Black Hole Lab (0.54.3): the non-spinning hole in closed form: light paths, the shadow, the bent sky and the images of point stars. Physics only, no graphics,
// double precision. Units: G = c = 1, lengths in M. The ray integrator (geodesic.h) at a = 0 is tested against everything here.
//
// THE LIGHT PATH. A ray has impact parameter b = L/E; it bends in one plane. Its swept angle phi obeys (du/dphi)^2 = 2M u^3 - u^2 + 1/b^2, u = 1/r
// (J.-P. Luminet, A&A 75, 228 (1979), (3), read from the page image), i.e. Delta phi = Int b dr / R(r)^(1/2), R(r) = r^4 - r (r - 2M) b^2 (R. Berens, D. O. Chang,
// T. Gravely, A. Lupsasca, PASP 138, 114501 (2026), arXiv:2603.05810, (9), (12b)).
//   Critical impact parameter b_c = 3 sqrt(3) M (S. E. Gralla, A. Lupsasca, PRD 101, 044031 (2020), arXiv:1910.12873, (71); Luminet p. 229 "b_c = 3 sqrt(3) M").
//   b > b_c: a periastron P, b^2 = P^3 / (P - 2M) (Luminet (5)); P is the largest root of r^3 - b^2 r + 2M b^2 (Berens et al. (13), (14d)).
//     With Q^2 = (P - 2M)(P + 6M), modulus k^2 = (Q - P + 6M) / 2Q, sin^2 zeta_inf = (Q - P + 2M) / (Q - P + 6M),
//     sin^2 zeta_r = (Q - P + 2M + 4MP/r) / (Q - P + 6M) (Luminet (6), (12)):
//       from infinity to the periastron   phi_inf = 2 (P/Q)^(1/2) {K(k) - F(zeta_inf, k)}             (6); deflection mu = 2 phi_inf - pi (p. 229)
//       from infinity to r (no periastron) gamma(r) = 2 (P/Q)^(1/2) {F(zeta_r, k) - F(zeta_inf, k)}       (11)
//       from r through the periastron to infinity   2 (P/Q)^(1/2) {2K(k) - F(zeta_inf, k) - F(zeta_r, k)}  (the "2n pi - gamma" form under (13), n = 1)
//   b < b_c: no turning point outside the horizon; R has the real roots r1 < r2 = 0 and the complex pair r3 = conj(r4) = b1 -+ i a1. Gralla & Lupsasca,
//     PRD 101, 044032 (2020), arXiv:1910.12881, App. B case (3) (read from the PDF): x3(r) = [A (r - r1) - B (r - r2)] / [A (r - r1) + B (r - r2)] (B55),
//     A = (a1^2 + (b1 - r2)^2)^(1/2), B = (a1^2 + (b1 - r1)^2)^(1/2) (B56), parameter k3 = [(A + B)^2 - r21^2] / 4AB (B59),
//     Int dr / R^(1/2) = F(arccos x3(r) | k3) / (AB)^(1/2) (B67), (B71), so the ray from r out to infinity sweeps b [F3(inf) - F3(r)].
//   Near b_c from above: b = 5.19695 M + 3.4823 M e^(-mu) (Luminet (7)); far out mu -> 4M/P (p. 229). (5.19695 is Luminet's printed decimal; 3 sqrt(3) =
//   5.19615: the paper's own decimal disagrees with its own 3 sqrt(3) in the fourth place, REPORT 0.54.3. The code uses 3 sqrt(3).)
//
// THE CAMERA hovers at radius rO and looks at angle psi from the outward radial direction (psi = pi: straight at the hole). Its ray has
// b = rO sin(psi) / (1 - 2M/rO)^(1/2) (O. James et al., CQG 32, 065001 (2015), (A.11) at a = 0, theta = pi/2: b = p_phi = E_F varpi n_phi, E_F = 1/alpha).
//   Shadow: psi > pi - alpha_sh, sin alpha_sh = 3 sqrt(3) M (1 - 2M/rO)^(1/2) / rO, alpha_sh > pi/2 for rO < 3M (Synge; V. Perlick, O. Yu. Tsupko,
//   Phys. Rep. 947, 1 (2022), arXiv:2105.07101, (6), (7)); the Zeldovich-Novikov form (3) of the same cone is used as a check.
//   The ray seen at psi came from the far-sky direction  cos(dphi) e_r + sin(dphi) t, where e_r points away from the hole and t is the unit vector
//   of the look direction across e_r; dphi is the swept angle above (flat space: dphi = psi).
//
// THE SKY MAP (plan section 6, way (c); A. Riazuelo, IJMPD 28, 1950042 (2019), section IV: one deflection function serves every pixel):
//   LensTable holds dphi against x = (pi - alpha_sh) - psi, at n points evenly spaced in ln x from 1e-9 to pi - alpha_sh, and interpolates with a
//   4-point cubic in ln x; near the shadow's edge dphi = -ln x + const (+ -2 ln x at rO = 3M) to within O(x), so below the first point it continues
//   along the line through the first two. Lensing keeps surface brightness, so the map's light is used unchanged; the camera's blueshift
//   (kerr.h hoverShift) is returned apart. 0.84.0: the app applies it to the disc and the stars (lab.h heldCameraShift), not to the map (no colour temperature).
//   LensView: pixel (X, Y) of a W x H picture -> look direction by the night sky's own rule (orbit/sky.h skyDirection: the angle from straight ahead is the
//   distance from the middle, 100 degrees at the corner) -> far-sky direction or shadow. A camera at infinite distance (rO = infinity, i.e. the hole's
//   mass is zero) returns the look direction itself: exactly the unbent sky.
//
// POINT STARS (Riazuelo section V): a star in direction s, at angle chi from e_r, has images where dphi(psi) = chi + 2 pi j (on its side of the hole) and
//   2 pi (j+1) - chi (the other side), j = 0, 1, ... (Riazuelo (44)), found by inverting the closed form (bisection in ln x to rounding). Each image's
//   magnification is the ratio of solid angles f = Omega / Omega* (Riazuelo (51)); with the map's rotation symmetry about e_r,
//   Omega / Omega* = sin(psi) / (sin(chi) |d dphi / d psi|). Its light is the star's times f (magnitude m - 2.5 log10 f, Pogson's rule as orbit/sky.h).
//   Images are kept from the brightest down until one is fainter than magLimit (the primary always): each further one is about e^(-pi) (1/23) as bright.
#pragma once

#include <vector>

#include "orbit/vec3.h"

namespace blackhole {

using orbit::Vec3;

double criticalImpact();                                    // 3 sqrt(3)
double periastron(double b);                                // b > b_c: the turning radius (largest root of r^3 - b^2 r + 2 b^2)
double deflectionFar(double b);                             // mu(b) = 2 phi_inf - pi for a far observer (b > b_c), Luminet (6)
double cameraImpact(double rO, double psi);                 // b of the ray seen at psi
double shadowAngle(double rO);                              // alpha_sh, Perlick & Tsupko (7)
double shadowAngleZN(double rO);                            // the same cone from Zeldovich & Novikov's |tan| form, Perlick & Tsupko (3) (a check)
struct Bend { bool captured = true; double dphi = 0; };
Bend bendFromCamera(double rO, double psi);                 // the closed form for a hovering camera at rO > 2M
Bend bendFromCameraEdge(double rO, double x);               // the same at psi = (pi - alpha_sh) - x, written so that it keeps its digits close to the edge
Vec3 bentDirection(const Vec3& er, const Vec3& look, double cosPsi, double dphi);   // cos(dphi) e_r + sin(dphi) t

class LensTable {
public:
    explicit LensTable(double rO, int n = 1024);            // rO = infinity: no hole (dphi = psi)
    double rO() const { return rO_; }
    double edge() const { return edge_; }                   // pi - alpha_sh: the largest psi that is not shadow (pi for no hole)
    int size() const { return static_cast<int>(v_.size()); }
    bool lookup(double psi, double& dphi) const;            // false: shadow
    bool lookupF(float psi, float& dphi) const;             // the same in single precision throughout (the GPU's precision; a measurement)
    // 0.55.0: by x = edge - psi itself (no rounding of psi near the edge), and the slope d dphi / d ln x of the same cubic; false: shadow (x <= 0)
    bool lookupX(double x, double& dphi, double* slopeLnX = nullptr) const;
    // what a graphics card needs to evaluate the same table: the values at s_i = s0 + i ds, s = ln x (empty for no hole)
    const std::vector<double>& values() const { return v_; }
    double s0() const { return s0_; }
    double ds() const { return ds_; }
private:
    double rO_, edge_, s0_ = 0, ds_ = 0;
    std::vector<double> v_;
};

class LensView {
public:
    // hole: unit direction from the camera to the hole (the night sky's ecliptic frame); the view by the yaw and pitch of orbit/sky.h skyScreen
    LensView(const LensTable& t, const Vec3& hole, double W, double H, double ca, double sa, double cb, double sb);
    int pixel(double X, double Y, Vec3& dir) const;         // 0: sky (dir = where on the far sky), 1: shadow, -1: off the night sky's map
    double shift() const;                                   // the hovering camera's f_c / f' (brightness factor its 4th power, Riazuelo (52), (53)); 1 for no hole
private:
    const LensTable& T_;
    Vec3 h_;
    double W_, H_, ca_, sa_, cb_, sb_;
};

struct StarImage { Vec3 dir; double magnification = 0, mag = 0, psi = 0; int order = 0; };

// 0.55.0, THE LAB'S VIEW: the hole stays at the middle (cx, cy) of the picture (pixels, y down); K pixels per radian; the night sky's projection (the angle from the
// middle is the distance from it: orbit/sky.h skyScreen) to any angle up to pi; the yaw and pitch (ca, sa, cb, sb) of skyScreen, so the middle looks along
// (-sb sa, -sb ca, -cb). With cx = W/2, cy = H/2, K = hypot(W, H)/2/1.75 it is exactly skyScreen / skyDirection (tested).
struct HoleView { double cx = 0, cy = 0, K = 1, ca = 1, sa = 0, cb = 1, sb = 0; };
Vec3 viewMiddle(const HoleView& v);                                         // the direction at the middle: from the camera to the hole
bool viewDirection(const HoleView& v, double X, double Y, Vec3& dir);       // false beyond pi from the middle
bool viewScreen(const HoleView& v, const Vec3& dir, double& X, double& Y);  // false for the direction straight behind
// Pixel (X, Y): 0 sky (dir = where on the far sky the light came from), 1 shadow, -1 beyond pi. The angle from the edge is taken from the pixel's own
// distance to the middle, x = th - alpha_sh with th = distance / K (no rounding of an angle near pi): the double-precision reference of the lab's picture.
int lensPixel(const LensTable& t, const HoleView& v, double X, double Y, Vec3& dir);

// 0.55.0: the images of point stars, fast enough for every frame while the view turns (the table is filled once per camera distance): the same images as
// starImages (Riazuelo (44): dphi(psi) = chi + 2 pi j and 2 pi (j+1) - chi), found by inverting the table's cubic instead of the closed form, and their
// magnification Omega / Omega* = sin(psi) / (sin(chi) |d dphi / d psi|) (Riazuelo (51)) from the cubic's slope. Tested against starImages.
int starImagesFast(const LensTable& t, const Vec3& hole, const Vec3& s, double mag, double magLimit, std::vector<StarImage>& out);
// 0.55.0: the hovering camera's frequency factor f_c / f' (kerr.h hoverShift, a = 0), the factor on the light's brightness, its fourth power (as
// LensView::shift: Riazuelo (52), (53)), and the camera's clock rate against a clock far away, the lapse alpha = (1 - 2M/r)^(1/2) of J15 (A.2)
// (kerr.h kerrMetric at a = 0): while one hour passes on a clock far away, alpha hours pass on the camera's.
double hoverBlueshift(double rO);
double hoverBrightening(double rO);
double hoverClockRate(double rO);
// The share of the camera's whole sky the shadow covers: a cap of angular radius alpha_sh, (1 - cos alpha_sh) / 2.
double shadowSkyFraction(double rO);
constexpr double kImageMagLimit = 6.6;                      // = orbit/sky.h kStarFaintMag: the catalogue's faint end
// The images of a star in direction s (unit). Appends to out; returns how many. Stars within 1e-9 rad of the line through the hole (an Einstein ring) give none.
int starImages(const LensTable& t, const Vec3& hole, const Vec3& s, double mag, double magLimit, std::vector<StarImage>& out);
// d dphi / d psi from the closed form, and the magnification of the image at psi of a source at chi (for tests)
double bendSlope(double rO, double psi);
double magnification(double rO, double psi, double dphi);

}  // namespace blackhole
