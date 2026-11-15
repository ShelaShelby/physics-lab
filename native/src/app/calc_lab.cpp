// The calculator's lab formulas (0.49): see calc_lab.h. Every formula is a textbook one, written out in the line itself.
#include "calc_lab.h"

#include <cstdio>

namespace calc {

std::string num(double v) {
    char b[48];
    std::snprintf(b, sizeof b, "%.10g", v);
    return b;
}

static std::string q(double v, const char* unit) { return "(" + num(v) + " " + unit + ")"; }   // "(1.495978707e+11 m)"

std::vector<Recipe> orbitRecipes(const std::string& name, double r, double v, double radius, double mass, double a) {
    std::vector<Recipe> o;
    if (r > 0) o.push_back({"Time light takes from the Sun to " + name, q(r, "m") + " / c to min", "distance now, divided by the speed of light"});
    if (v > 0) o.push_back({name + "'s speed round the Sun", q(v, "m/s") + " to km/s", ""});
    if (a > 0) o.push_back({"Its year from Kepler's third law", "2 π sqrt(" + q(a, "m") + "^3 / GMsun) to day",
                            "T = 2π √(a³ / GM☉): the Sun's pull only (its own mass and the other planets left out)"});
    if (r > 0) o.push_back({"Speed needed to escape the Sun from here", "sqrt(2 GMsun / " + q(r, "m") + ") to km/s", "v = √(2GM☉ / r)"});
    if (mass > 0 && radius > 0) {
        o.push_back({"Escape speed from " + name + "'s surface", "sqrt(2 G " + q(mass, "kg") + " / " + q(radius, "m") + ") to km/s",
                     "v = √(2GM / R), with the equator's radius; air and spin left out"});
        o.push_back({"Gravity at " + name + "'s surface", "G " + q(mass, "kg") + " / " + q(radius, "m") + "^2 to m/s^2",
                     "g = GM / R², with the equator's radius; the spin's push left out"});
    }
    return o;
}

std::vector<Recipe> atomRecipes(const std::string& name, int Z, double nucleusKg, int n, double bindingMeV, int A) {
    std::vector<Recipe> o;
    if (Z < 1) return o;
    if (n < 1) n = 1;
    const std::string z = std::to_string(Z), ns = std::to_string(n);
    if (nucleusKg > 0) {
        const std::string M = q(nucleusKg, "kg"), red = M + " / (" + M + " + me)";   // the reduced-mass factor M / (M + me)
        o.push_back({"Energy of level n = " + ns + " (Bohr, one electron)", "-Ry * " + z + "^2 / " + ns + "^2 * " + red + " to eV",
                     "E = −Ry Z² / n² × M / (M + me): no fine structure, one electron"});
        const int hi = n >= 2 ? n : 2, lo = hi - 1;
        o.push_back({"Light from the jump " + std::to_string(hi) + " → " + std::to_string(lo) + " (Bohr)",
                     "hP c / (Ry * " + z + "^2 * (1/" + std::to_string(lo) + "^2 - 1/" + std::to_string(hi) + "^2) * " + red + ") to nm",
                     "λ = hc / ΔE, vacuum wavelength; no fine structure or Lamb shift"});
        o.push_back({"Size of orbit n = " + ns + " (Bohr radius)", "a0 * " + ns + "^2 / " + z + " * (" + M + " + me) / " + M + " to pm",
                     "r = a0 n² / Z × (M + me) / M"});
        o.push_back({"Mass of this nucleus", M + " to u", ""});
    }
    o.push_back({"Electron speed on orbit n = " + ns + " (Bohr)", "α c * " + z + " / " + ns + " to km/s",
                 "v = Zαc / n: non-relativistic, so only good while it is well below c"});
    // ---- 0.50.1: beyond Bohr (tests/test_calc_qm.inc checks each against the Atom Lab's own functions and NIST) ----
    // the Dirac level (Sommerfeld's formula, the exact Dirac energy of a point nucleus of infinite mass), j = 1/2: n - δ with δ = (j + 1/2) - √((j + 1/2)² - (Zα)²)
    o.push_back({"Dirac energy of level n = " + ns + ", j = 1/2 (point nucleus)",
                 "me c^2 * ((1 + (" + z + " α / (" + ns + " - 1 + sqrt(1^2 - (" + z + " α)^2)))^2)^(-1/2) - 1) to eV",
                 "E = mc² [(1 + (Zα / (n - δ))²)^(-1/2) - 1], δ = (j + ½) - √((j + ½)² - (Zα)²); here j + ½ = 1. Infinitely heavy point nucleus, no QED. "
                 "For other j change both 1s. Written this way it keeps about 11 digits for hydrogen."});
    {   // 0.50.2: the fine-structure gap np1/2 - np3/2 as the exact difference of the two Dirac levels above (0.50.1 had the leading order in (Zα)²: -29 % at Z = 92).
        // Each level is written E = -mc² x² / (1 + x² + √(1 + x²)), x = Zα / (n - δ), the same as mc² ((1 + x²)^(-1/2) - 1) without its cancellation, so the
        // difference keeps about 11 digits for hydrogen (tests: against atom::diracInfiniteEV for Z = 1, 30, 92).
        const std::string nf = std::to_string(n >= 2 ? n : 2);
        const auto x = [&](const char* k) { return std::string("(") + z + " α / (" + nf + " - " + k + " + sqrt(" + k + "^2 - (" + z + " α)^2)))"; };
        const auto level = [&](const char* k) { const std::string xx = x(k); return xx + "^2 / (1 + " + xx + "^2 + sqrt(1 + " + xx + "^2))"; };
        o.push_back({"Fine-structure gap " + nf + "p1/2 - " + nf + "p3/2 (Dirac, point nucleus)", "me c^2 * (" + level("1") + " - " + level("2") + ") / hP to GHz",
                     "ΔE = E(j = 3/2) - E(j = 1/2) from the Dirac formula above (j + ½ = 2 and 1), each written -mc² x²/(1 + x² + √(1 + x²)), x = Zα/(n - δ), so the "
                     "difference keeps its digits. Infinitely heavy point nucleus, no QED. Hydrogen's measured gap, 10.969 GHz (NIST ASD level energies), is 0.2 % "
                     "larger: the Lamb shift (QED)."});
    }
    // the Zeeman shift of 2p3/2, mJ = 3/2, in a weak field B = 1 T: μB gJ mJ B. 0.50.2: Landé's gJ with the measured electron g-factor (was g = 2):
    // gJ = gL (j(j+1) - s(s+1) + l(l+1)) / (2j(j+1)) + gS (j(j+1) + s(s+1) - l(l+1)) / (2j(j+1)) with gL = 1, gS = |ge|, i.e.
    // gJ = 1 + (|ge| - 1)(j(j+1) - l(l+1) + s(s+1)) / (2j(j+1)) (the usual Landé formula, e.g. Bransden & Joachain, "Physics of Atoms and Molecules"; not opened here)
    o.push_back({"Zeeman shift of 2p3/2, mJ = 3/2, in 1 T (Landé gJ)",
                 "μB * (1 + (abs(ge) - 1) * (1.5*(1.5+1) - 1*(1+1) + 0.5*(0.5+1)) / (2*1.5*(1.5+1))) * 1.5 * 1 T to μeV",
                 "ΔE = μB gJ mJ B, gJ = 1 + (|ge| - 1)(j(j+1) - l(l+1) + s(s+1)) / (2j(j+1)) = 1.33411 here (4/3 with g = 2), ge the measured electron g-factor "
                 "(CODATA 2022). Weak field (B well below the fine-structure gap); gL = 1 (the nucleus's motion, about me/M, left out)."});
    if (nucleusKg > 0) {
        const std::string M = q(nucleusKg, "kg");
        // <r> = a0 / (2Z) (3n² - l(l+1)) with the reduced mass (a0 → a0 me / μ = a0 (M + me) / M), here l = 0
        o.push_back({"Mean distance <r> of level n = " + ns + ", l = 0", "a0 / (2*" + z + ") * (3*" + ns + "^2 - 0*(0+1)) * (" + M + " + me) / " + M + " to pm",
                     "<r> = a0 / (2Z) (3n² - l(l+1)) × (M + me) / M: the non-relativistic one-electron atom with the reduced mass; for another l change both 0s"});
        // the 2p → 1s rate: A = (2/3)^8 α^5 me c² / ħ for hydrogen with an infinitely heavy nucleus, × Z⁴, × μ/me (A goes as the reduced mass: ω ∝ μ, the dipole ∝ 1/μ)
        o.push_back({"Rate of the jump 2p → 1s (Einstein A)", "(2/3)^8 α^5 me c^2 / ħ * " + z + "^4 * " + M + " / (" + M + " + me) to 1/s",
                     "A = (2/3)⁸ α⁵ mc²/ħ Z⁴ μ/me: the dipole rate of the non-relativistic atom (a rate, so no 2π). Hydrogen: NIST ASD gives 6.2648 × 10⁸ 1/s; "
                     "the lifetime of 2p is 1/A = 1.6 ns."});
    }
    if (Z == 1 && A == 1) {
        // Fermi's formula for hydrogen's ground-state hyperfine splitting (the 21-cm line): ΔE = (4/3) α⁴ me c² gp (me/mp) (1 + ae) (mr/me)³
        o.push_back({"Hyperfine splitting of 1s (Fermi, the 21-cm line)", "(4/3) α^4 me c^2 gp (me/mp) (1 + ae) (mp / (mp + me))^3 / hP to MHz",
                     "Fermi's contact formula with the electron's anomaly ae and the reduced mass: 1420.49 MHz. The measured line is 1420.405 751 766 7 MHz "
                     "(Eides, Grotch & Shelyuto 2001, eq. 406); left out: Breit's relativistic factor, the proton's size (Zemach) and recoil, and higher QED."});
    }
    // 0.50.2: not for A = 1 (a single nucleon has no binding energy)
    if (bindingMeV >= 0 && A > 1) o.push_back({"Binding energy per nucleon of " + name, q(bindingMeV, "MeV") + " / " + std::to_string(A) + " to MeV", ""});
    return o;
}

std::vector<Recipe> moleculeRecipes(const std::string& name, double massU, double wavenumber) {
    std::vector<Recipe> o;
    if (massU > 0) {
        o.push_back({"Mass of one " + name + " molecule", q(massU, "u") + " to kg", "the most common isotopes"});
        o.push_back({"Molecules in 1 gram", "1 g / " + q(massU, "u"), ""});
        o.push_back({"Mass of one mole", q(massU, "u") + " NA to g/mol", ""});
    }
    if (wavenumber > 0) {
        o.push_back({"Energy of one quantum of this vibration", "hP c " + q(wavenumber, "cm^-1") + " to eV", "E = h c ν̃"});
        o.push_back({"Infrared wavelength it absorbs", "1 / " + q(wavenumber, "cm^-1") + " to μm", "λ = 1 / ν̃"});
    }
    return o;
}

std::vector<Recipe> generalRecipes() {
    return {
        {"Light time from the Moon (mean distance)", "384400 km / c", ""},
        {"Energy of a green photon (500 nm)", "hP c / 500 nm to eV", "E = hc / λ"},
        {"Thermal energy at room temperature", "kB * 293.15 K to eV", "kB T"},
        {"100 km in 2 hours", "100 km / 2 hr", "a number next to a unit belongs to it: (100 km) / (2 hr)"},
    };
}

// 0.50.1: a few quantum-mechanics lines (the panel's "QM examples"); every line is worked out in tests/test_calc_qm.inc
std::vector<Recipe> qmRecipes() {
    return {
        {"Rabi frequency of hydrogen's 1s-2p pair in 100 kV/m (e a0 E / ħ)", "e*a0*1e5 V/m/ħ to MHz", "ħ makes it an angular frequency ω: the note gives f = ω/2π"},
        {"1 eV as a wavenumber", "1 eV to cm^-1", "converted through E = h c ν̃"},
        {"Clebsch-Gordan <1 1; 1/2 -1/2 | 1/2 1/2>", "cg(1, 1, 0.5, -0.5, 0.5, 0.5)", "= √(2/3), Condon-Shortley phase"},
        {"Rotational partition function of CO at 300 K (B = 1.93 cm⁻¹)", "sum((2 j + 1) exp(-j (j + 1) hP c * 1.93 cm^-1 / (kB * 300 K)), j, 0, 100)",
         "Z = Σ (2J+1) e^(-J(J+1) hcB / kT); the index is j because J is the joule"},
        {"Finite square well, z0 = 4: the ground state", "solve(tan(z) - sqrt(4^2/z^2 - 1), z, 0.1, 1.5)", "tan z = √(z0²/z² - 1), z = a √(2mE)/ħ (Griffiths' notation)"},
        // 0.54.2: small matrices
        {"Pauli matrices: σx σy = i σz", "sigmax * sigmay", "sigmax, sigmay, sigmaz (or σx, σy, σz) are built in; sigma alone is the Stefan-Boltzmann constant"},
        {"Two-level system, Δ = 0.3 eV, Ω = 0.4 eV: its energies", "eig([[0.3/2, 0.4/2], [0.4/2, -0.3/2]] eV) to eV",
         "±√(Δ² + Ω²)/2 = ±0.25 eV; eig gives the eigenvalues of a Hermitian matrix, smallest first"},
        {"Two spins: the Kronecker product of σx and σz", "kron(sigmax, sigmaz)", "a 4x4 matrix, the largest size the calculator takes"},
    };
}

}  // namespace calc
