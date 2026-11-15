// Every nucleus at its measured size (0.10.9 stage 1): measured rms charge radii from the IAEA Nuclear Data Section's table of nuclear charge radii, baked in at build time
// (src/atom/radii_data.inc, made by tools/make_radii_data.py from data/radii/charge_radii.csv; the README there has the URL, the date and the credit). No drawing here, so it can be tested.
//
// What the number is: the rms charge radius sqrt(<r^2>) of the PROTONS' electric charge (neutrons carry no charge, so it says nothing direct about where the neutrons are).
// How it becomes a drawn size (an ASSUMPTION, stated in the app): the lab's usual nucleus size is a uniform ball, r = 1.2 A^(1/3) fm. A uniform ball of radius R has rms radius sqrt(3/5) R
// (<r^2> = integral of r^2 over the ball / volume = 3/5 R^2, a calculus result), so a measured rms radius is drawn as the uniform ball of radius R = sqrt(5/3) x rms. Without this step the measurement
// and the rule would be compared in different quantities (the rule's R is an edge, the measurement an rms): the rms of the rule's ball is sqrt(3/5) x 1.2 A^(1/3) = 0.9295 A^(1/3) fm.
// Where there is no value the rule stays, and the card says so. Nothing is interpolated. The PRELIMINARY columns of the file are NOT used (the file marks them preliminary; 48 isotopes have only such a value:
// they keep the rule and the card names the preliminary value). The neutron row (a negative number) is not in the table. Hydrogen-1 (a single proton) keeps the lab's own proton radius (CODATA 2022).
#pragma once

#include <string>
#include <vector>

namespace atom {

struct RadiusRow { short Z, N; double rms, unc, prelim, prelimUnc; };   // fm; NaN where the file leaves the cell empty
const RadiusRow* radiusRows(int& count);                      // the whole table, sorted by Z then N
const RadiusRow* iaeaRadius(int Z, int N);                    // the isotope's row of the IAEA file exactly, or nullptr when the file has none
// The radius the lab uses: the IAEA row, except the deuteron (0.46, review A9: one rule for the proton and the deuteron, CODATA 2022, the source of the proton's 0.84075 fm:
// r_d = 2.127 78(27) fm, data/qcd/nucleon/codata2022_rd.html; the IAEA electron-scattering value 2.1421(88) fm is 1.6 sigma larger and is named on the card).
constexpr double kDeuteronRmsFm = 2.12778, kDeuteronRmsUncFm = 0.00027;
constexpr double kProtonRmsUncFm = 0.00064;   // 0.46.5: CODATA 2022 r_p = 0.840 75(64) fm (data/qcd/nucleon/codata2022_rp.html), the value kProtonRmsFm (atom_core.h) holds
const RadiusRow* chargeRadius(int Z, int N);
bool sizedByMeasurement(int Z, int N);                        // the lab draws it at a measured size: a (non-preliminary) value exists and A > 1

constexpr double kSharpOverRms = 1.2909944487358056;          // sqrt(5/3): the radius of the uniform ball whose rms radius is 1
double ruleRms(int A);                                        // the usual rule's ball (1.2 A^(1/3) fm) expressed as an rms charge radius: sqrt(3/5) x nuclearR(A), fm
double nuclearRadius(int Z, int N);                           // fm: the drawn (uniform-ball) radius: sqrt(5/3) x the measured rms where sizedByMeasurement, else the usual rule nuclearR(A)

// 0.42: what the nucleons' own charge adds to a nucleus' mean-square charge radius (fm^2): <R_p^2> + (N/Z) <R_n^2> + 3/(4 M^2) (Darwin-Foldy), the split of
// Ong, Berengut and Flambaum, PRC 82, 014320 (2010), eq. (8) (their spin-orbit term <r^2>_so is left out: it needs the shell-model orbitals). Filin et al., PRL 124, 082501 (2020), eq. (2), use the same
// split for the deuteron. Values: CODATA 2022 r_p, PDG 2024 <r_n^2>, CODATA 2022 m_p (atom_core.h). Until 0.41 only r_p^2 = 0.84^2 was taken off.
double nucleonSizeMs(int Z, int N);
double pointProtonMsFromCharge(int Z, int N, double rms);      // the protons' point mean square radius for a measured rms charge radius: rms^2 - nucleonSizeMs, at least 0.1 fm^2
// The Woods-Saxon half-density radius (diffuseness a = 0.50 fm since 0.46, kWsPointDiffuseFm) whose point density has the mean square radius ms (fm^2). Solved numerically (bisection). It only STARTS the sampling of a nucleus;
// buildNucleus then scales the snapshot so its protons have the measured size exactly (atom_core.cpp).
// a = 0.54 fm: inside the 0.535-0.549 fm that de Vries, de Jager and de Vries (ADNDT 36, 495 (1987)) fitted to the round nuclei Y-89, Nb-93, La-139, Au-197, Pb-207 and Pb-208 (two-parameter Fermi,
// as quoted by Kitano, Koike and Okada, PRD 66, 096002 (2002), Table 7, and Loizides, Nagle and Steinberg, arXiv:1408.2549 (2015; journal not checked), Table I; the 1987 table itself could not be reached). Other nuclei:
// 0.52-0.64 fm; the large values (Ta-181 0.64) are deformed nuclei, where a round fit takes the shape into its diffuseness. The lab draws the shape separately and keeps one a for every nucleus.
constexpr double kWsDiffuseFm = 0.54;
// 0.46 (review A8): the diffuseness of the POINT nucleons' density (the centres the Nucleons look samples; each nucleon is then drawn with its own size): 0.50 fm, the value L. C. Chamon et al.,
// PRC 66, 014610 (2002), arXiv:nucl-th/0202015 (data/qcd/nucleus), section 2, call "more realistic" for nucleon distributions (0.48 fm from their Dirac-Hartree-Bogoliubov densities, raised by
// the 0.01-0.02 fm their theory under-shoots the measured charge diffuseness, 0.53 fm). Folding: a Fermi edge's derivative has variance pi^2 a^2 / 3, a nucleon of <r^2>_N adds <r^2>_N / 3, so
// a_charge^2 = a_point^2 + <r^2>_N / pi^2: 0.50 fm folds to 0.53-0.54 fm (the review's "3 r_p^2 / pi^2" was off by this factor 3; its 0.47 fm was right).
constexpr double kWsPointDiffuseFm = 0.50;
double wsHalfDensityRadiusForPointMs(double ms);   // (0.46: with the point diffuseness kWsPointDiffuseFm)
// 0.42: the neutron skin r_n - r_p (rms radii of the point neutrons and protons, fm): S = 0.90 I - 0.03 fm, I = (N - Z) / A, the fit to 26 stable nuclei (Ca-40 .. U-238) measured with antiprotonic atoms:
// Trzcinska et al., PRL 87, 082501 (2001), as quoted by Centelles et al., PRL 102, 122502 (2009) and by Klos et al., PRC 76, 014311 (2007) (the 2001 paper itself was not reachable;
// 0.43: Klos et al. add that a later fit of all antiprotonic data allows a slope of 0.9 to 1.3 fm, 0.154 to 0.240 fm for Pb-208; shown in More info). Pb-208: 0.160 fm (PREX-II measured 0.283 +- 0.071,
// Adhikari et al., PRL 126, 172502 (2021): 1.7 sigma apart); Ca-48: 0.120 fm (CREX 0.121 +- 0.026 +- 0.024, Adhikari et al., PRL 129, 042501 (2022)). Beyond the fitted nuclei it is an extrapolation.
// Until 0.41: 1.7 I, unsourced (0.36 fm for Pb-208). Clamped to -0.3 .. 0.6 fm (the neutron slider reaches far-off isotopes). The layout draws it only for A > 16.
double neutronSkinFm(int Z, int N);

std::vector<std::string> nuclearSizeLines(int Z, int N);      // the words of the "Nuclear size (measured)" card for this isotope
std::string chargeRadiusExplain();                            // what a charge radius is, and the limits of this picture
inline constexpr const char* kRadiiCredit = "Nuclear charge radii: the IAEA Nuclear Data Section's table of nuclear charge radii (www-nds.iaea.org/radii/charge_radii.csv, retrieved 2026-10-04), the compilation of I. Angeli and K. P. Marinova, "
                                            "Atomic Data and Nuclear Data Tables 99 (2013) 69, \"Table of experimental nuclear ground state charge radii: An update\" (the file itself carries no citation text and does not say which edition it is).";

}  // namespace atom
