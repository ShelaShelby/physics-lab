// The nucleus's shape from MEASURED electric quadrupole moments (0.10.9 stage 2): the IAEA Nuclear Data Section's recommended values (data/quadrupole, baked in at build time as quadrupole_data.inc by
// tools/make_quadrupole_data.py; the README there has the URL, the date and what the file says about citing it). No drawing here, so it can be tested.
//
// THE TRAP: the table gives the SPECTROSCOPIC quadrupole moment Q, what a laboratory measures with the nucleus in its spin state I. The shape parameter beta2 describes the INTRINSIC moment Q0, in the nucleus' own
// frame. For a rotating, permanently deformed nucleus in strong coupling with K = I (the spin along the symmetry axis) the two are related by the standard relation
//        Q = Q0 [3 K^2 - I (I + 1)] / [(I + 1) (2 I + 3)]  =  Q0 I (2 I - 1) / [(I + 1) (2 I + 3)]      (K = I)
// (Bohr & Mottelson, Nuclear Structure vol. II; Krane, Introductory Nuclear Physics: written from knowledge, not looked up in the session that wrote it). It is a MODEL ASSUMPTION, not a measurement: it is poor for light
// nuclei and for nuclei near closed shells, where a few nucleons carry the moment and there is no deformed core. For I = 0 and I = 1/2 it gives Q = 0 for any Q0: such a nucleus has no spectroscopic moment whatever
// its shape, so "no measured moment" never means "round" there (the lab says so on the card).
// From Q0 to beta2, and the body that is drawn: a UNIFORMLY CHARGED body (an assumption) with the surface r(theta) = R_s (1 + beta2 Y20(theta)), Y20 = sqrt(5/(16 pi)) (3 cos^2 theta - 1) (the lab's old drawing used the
// same form). Per proton Q0 / Z = <3 z^2 - r^2>, so with the protons' mean square radius <r^2> = rms^2 - 0.84^2 (the nucleon's own size is isotropic and adds nothing to a quadrupole moment)
//        <3 z^2 - r^2> / <r^2> = N5 / G5,      G5 = <(1 + beta2 Y20)^5>,   N5 = <(1 + beta2 Y20)^5 (3 cos^2 theta - 1)>      (averages over the sphere; polynomials in cos theta, summed exactly with the moments m_j)
// and beta2 is the solution of  N5(beta2) / G5(beta2) = Q0 / (Z <r^2>). For small beta2 this is the first-order relation Q0 = 3 Z R^2 beta2 / sqrt(5 pi) with R^2 = (5/3) <r^2>; the exact one is lower by about 6 % at 0.3.
// The VOLUME is kept: R_s = R_V F^(-1/3) with F = <(1 + beta2 Y20)^3>, so the body has the volume 4 pi/3 R_V^3 of the sphere of radius R_V for every beta2, and R_V = R_pt F^(5/6) / sqrt(G5) makes its mean square radius
// the measured one (R_pt^2 = (5/3) <r^2>). The nucleon cloud has a diffuse edge, so its own quadrupole differs slightly from the sharp-edged body's (not corrected).
// LIMIT (the lab's own, no source): the conversion is not used where it needs |beta2| > 0.6, as no ground state is believed to be that deformed and a first-order relation means nothing there (the moment is real; the
// card says so and the lab keeps its B(E2) value or estimate).
#pragma once

#include <string>
#include <vector>

namespace atom {

struct QuadRow {
    short Z, A, spin2;         // proton number, mass number (the file's second column), 2 x spin (-1: not one clean value)
    char spinFirm, kind, stable;   // spin is one clean value (the parity may be tentative); S signed, U unsigned (the file gives no sign), T tentative sign, X not a single clean number; the file's half-life says stable
    double Qb, uncB;           // the spectroscopic quadrupole moment and its uncertainty in barns (NaN for X)
    const char *raw, *spin, *method, *nsr;   // the file's own value text and spin text, the method code, the NSR reference key
};
const QuadRow* quadRows(int& count);                         // the ground-state rows of the file, sorted by Z then A
const QuadRow* quadRow(int Z, int N);                        // the ground-state row of the isotope, or nullptr
void quadFileCounts(int& fileRows, int& isomerRows);         // rows in the file, and how many are isomers (not in the table)

constexpr double kBarnFm2 = 100;                             // 1 b = 10^-28 m^2 = 100 fm^2, exactly
constexpr double kShapeBetaLimit = 0.6;                      // the largest |beta2| the conversion is trusted for (the lab's own limit)

double intrinsicQ0(double Qs, int spin2);                    // strong coupling K = I: Q0 = Qs (I + 1)(2 I + 3) / (I (2 I - 1)); NaN for spin 0 and 1/2 (no conversion exists)
double shapeVolumeFactor(double beta);                       // F = <(1 + beta Y20)^3>
double shapeRadiusFactor(double beta, double u);             // f(u) = (1 + beta Y20(u)) F^(-1/3), u = cos theta: the surface is r = R_V f(u) and keeps the volume 4 pi/3 R_V^3
double shapeMomentRatio(double beta);                        // <3 z^2 - r^2> / <r^2> = N5 / G5 of that body
double volumeEquivalentRadius(double Rpt, double beta);      // R_V = R_pt F^(5/6) / sqrt(G5): the radius of the same-volume sphere when the body has the mean square radius (3/5) R_pt^2
double betaFromRatio(double ratio, bool& ok);                // the beta2 with shapeMomentRatio = ratio, |beta2| <= kShapeBetaLimit; ok = false when none
double pointProtonMs(int Z, int N);                          // <r^2> of the protons' centres: the measured rms charge radius squared minus 0.84^2 (or the rule's ball, sqrt(3/5) x 1.2 A^(1/3), where none), fm^2, at least 0.1

enum class ShapeSource { MeasuredMoment, BE2, Estimate };
enum class MomentUse { Used, NoRow, RowNotUsable, SignNotGiven, SpinNotFirm, TooLarge };   // why a measured moment was or was not used
struct Shape {
    double beta = 0;                                         // beta2: + prolate (stretched), - oblate (squashed)
    ShapeSource src = ShapeSource::Estimate;
    MomentUse moment = MomentUse::NoRow;
    const QuadRow* row = nullptr;
    double beta1 = 0;                                        // when the moment was too large: the first-order |beta2| it would need (for the card)
};
Shape nucShape(int Z, int N);                                // measured moment where it can be used, else the lab's B(E2) value, else its estimate (atom_core's nucBeta)

std::vector<std::string> nuclearShapeLines(int Z, int N);    // the words of the card for this isotope
std::string shapeExplain();
inline constexpr const char* kQuadCredit = "Electric quadrupole moments: the IAEA Nuclear Data Section's recommended values (www-nds.iaea.org/nuclearmoments/elec_mom_recomm.csv, retrieved 2026-10-04): ground-state rows, "
                                           "each naming the INDC report indc-nds-0833 and an NSR reference key. The page that states the terms for citing the table could not be read from the session that built this (a bot challenge), so no further citation is given.";

}  // namespace atom
