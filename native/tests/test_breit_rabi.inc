// 0.93.0: the ground state's hyperfine levels in a magnetic field (src/atom/breit_rabi.h), included by test_atom.cpp. Every reference is independent of the
// formula under test: the whole matrix diagonalised by Jacobi rotations, Steck's eq. 23 (g_F), eq. 25 (the strong-field limit), the published A_hfs (the
// zero-field splitting) and Steck's printed clock-transition shifts (Table 6 of each note, rev. 2.3.4).
#include "../src/atom/breit_rabi.h"

static std::string brNum(double v) { char b[40]; std::snprintf(b, sizeof b, "%.6g", v); return b; }

static void testBreitRabi() {
    namespace br = atom::breitrabi;
    checkTrue("breit-rabi: five ground states (H-1, Na-23, Rb-85, Rb-87, Cs-133)", br::groundStates().size() == 5 && br::find(55, 78) && br::find(37, 50) && br::find(37, 48) &&
              br::find(11, 12) && br::find(1, 0) && !br::find(1, 1));
    // mu_B / h: CODATA 2022 1.399 624 491 71(44)e10 Hz/T (Table XXXIII of Mohr et al.; the same value from mu_B in J/T and the exact h)
    checkTrue("breit-rabi: mu_B / h = 1.399 624 491 71e10 Hz/T (CODATA 2022)", std::fabs(br::muBHzPerT() / 1.39962449171e10 - 1) <= 5e-11);
    for (const br::GroundState& g : br::groundStates()) {
        const std::string nm = g.name;
        const double dE = br::splittingHz(g), I = g.twoI / 2.0;
        // (1) the formula against the whole matrix and the m_F blocks, from x = 1e-4 to 1e4
        double worst = 0, worstBlock = 0;
        bool sizes = true;
        for (double x : {0.0, 1e-4, 0.01, 0.3, 1.0, 2.7, 10.0, 1e3, 1e4}) {
            const double B = br::bOfX(g, x);
            const std::vector<br::Sub> f = br::formula(g, B), d = br::diagonalise(g, B);
            const std::vector<double> full = br::fullMatrix(g, B);
            sizes = sizes && f.size() == std::size_t(2 * (g.twoI + 1)) && d.size() == f.size() && full.size() == f.size();
            if (!sizes) break;
            std::vector<double> fs;
            for (std::size_t i = 0; i < f.size(); i++) {
                fs.push_back(f[i].eHz);
                worstBlock = std::max(worstBlock, std::fabs(f[i].eHz - d[i].eHz) / (dE * (1 + x)));
                sizes = sizes && f[i].twoF == d[i].twoF && f[i].twoM == d[i].twoM;
            }
            std::sort(fs.begin(), fs.end());
            for (std::size_t i = 0; i < fs.size(); i++) worst = std::max(worst, std::fabs(fs[i] - full[i]) / (dE * (1 + x)));
        }
        checkTrue("breit-rabi: " + nm + ": the formula = the whole matrix's eigenvalues (x = 0 to 1e4; worst " + brNum(worst) + " of dE (1 + x))", sizes && worst <= 1e-12);
        checkTrue("breit-rabi: " + nm + ": the formula = each m_F block's eigenvalues, with the same (F, m_F) labels (worst " + brNum(worstBlock) + ")", sizes && worstBlock <= 1e-12);
        // (2) the zero-field splitting: two levels, F = I + 1/2 (2I + 2 states) above F = I - 1/2 (2I states) by A_hfs (I + 1/2)
        const std::vector<double> z = br::fullMatrix(g, 0);
        const double lo = z.front(), hi = z.back();
        int nHi = 0, nLo = 0;
        for (double e : z) { if (std::fabs(e - hi) <= 1e-9 * dE) nHi++; if (std::fabs(e - lo) <= 1e-9 * dE) nLo++; }
        checkTrue("breit-rabi: " + nm + ": at B = 0 two levels, 2I + 2 and 2I states, A_hfs (I + 1/2) apart (" + brNum(hi - lo) + " Hz)",
                  nHi == g.twoI + 2 && nLo == g.twoI && std::fabs((hi - lo) / (g.aHz * (I + 0.5)) - 1) <= 1e-12);
        // (3) the low-field g_F: the slope (E(B) - E(-B)) / (2 mu_B B m_F) of the whole matrix's levels at x = 1e-4 against Steck eq. 23 (the even part cancels)
        bool slope = true;
        double worstG = 0;
        const double B = br::bOfX(g, 1e-4);
        const std::vector<br::Sub> p = br::diagonalise(g, B), m = br::diagonalise(g, -B);
        for (std::size_t i = 0; i < p.size(); i++) {
            if (p[i].twoM == 0) continue;
            std::size_t j = 0;
            while (j < m.size() && !(m[j].twoF == p[i].twoF && m[j].twoM == p[i].twoM)) j++;
            if (j == m.size()) { slope = false; break; }
            const double gGot = (p[i].eHz - m[j].eHz) / (2 * br::muBHzPerT() * B * (p[i].twoM / 2.0));
            worstG = std::max(worstG, std::fabs(gGot - br::gF(g, p[i].twoF)));
        }
        checkTrue("breit-rabi: " + nm + ": the low-field slopes are g_F m_F mu_B (Steck eq. 23; worst " + brNum(worstG) + ")", slope && worstG <= 1e-7);
        // the textbook g_F values: g_F = +-g_J / (2I + 1) to within the nuclear term (Steck eq. 23's approximate form)
        checkTrue("breit-rabi: " + nm + ": g_F(I + 1/2) = g_J / (2I + 1) + g_I 2I / (2I + 1) and g_F(I - 1/2) = -g_J / (2I + 1) + g_I (2I + 2) / (2I + 1) (F = 0 has none)",
                  std::fabs(br::gF(g, g.twoI + 1) - (g.gJ + g.gI * g.twoI) / (g.twoI + 1)) <= 1e-15 &&
                  (g.twoI == 1 || std::fabs(br::gF(g, g.twoI - 1) - (-g.gJ + g.gI * (g.twoI + 2)) / (g.twoI + 1)) <= 1e-15));
        // (4) the strong-field limit (Steck eq. 25, B_hfs = 0 for J = 1/2): E -> A m_I m_J + mu_B (g_J m_J + g_I m_I) B, the rest at most dE / (4 x)
        bool high = true;
        double worstH = 0;
        for (double x : {1e3, 1e4}) {
            const double Bh = br::bOfX(g, x);
            const std::vector<double> e = br::fullMatrix(g, Bh);
            std::vector<double> want;
            for (int twoJ : {1, -1})
                for (int twoMI = -g.twoI; twoMI <= g.twoI; twoMI += 2)
                    want.push_back(g.aHz * (twoMI / 2.0) * (twoJ / 2.0) + br::muBHzPerT() * Bh * (g.gJ * twoJ / 2.0 + g.gI * twoMI / 2.0));
            std::sort(want.begin(), want.end());
            // the zero of energy: the model's centre of gravity is 0 at B = 0, the same as eq. 25's (the trace of A I.J is 0)
            for (std::size_t i = 0; i < e.size(); i++) {
                const double err = std::fabs(e[i] - want[i]);
                high = high && err <= dE / (4 * x) * 1.0001 + 1e-13 * dE * x;
                worstH = std::max(worstH, err / dE * x);
            }
        }
        checkTrue("breit-rabi: " + nm + ": at x = 1e3 and 1e4 the levels are Steck eq. 25's within dE / (4x) (worst " + brNum(worstH) + " dE / x)", high);
    }
    // (5) the clock transition's shift over B^2 against Steck's printed values (Table 6, Hz/G^2; 1 G = 1e-4 T), within half the last printed digit
    struct Clock { int Z, N; double hzPerG2, half; };
    for (const Clock& c : {Clock{11, 12, 2218.3, 0.05}, Clock{37, 48, 1293.98, 0.005}, Clock{37, 50, 575.15, 0.005}, Clock{55, 78, 427.45, 0.005}}) {
        const br::GroundState* g = br::find(c.Z, c.N);
        const double got = g ? br::clockShiftHzPerT2(*g) * 1e-8 : 0;
        // and the same shift from the diagonalised levels at a small field (x = 1e-3): (E(F+, 0) - E(F-, 0)) - dE over B^2
        double fromLevels = 0;
        if (g) {
            const double B = br::bOfX(*g, 1e-3);
            double up = 0, lo = 0;
            for (const br::Sub& s : br::diagonalise(*g, B)) if (s.twoM == 0) (s.twoF == g->twoI + 1 ? up : lo) = s.eHz;
            fromLevels = (up - lo - br::splittingHz(*g)) / (B * B) * 1e-8;
        }
        checkTrue(std::string("breit-rabi: ") + (g ? g->name : "?") + ": the clock shift " + brNum(got) + " Hz/G^2 = Steck's " + brNum(c.hzPerG2) +
                  " to its last digit, and the levels' own shift at x = 1e-3 agrees to 1e-5", g && std::fabs(got - c.hzPerG2) <= c.half && std::fabs(fromLevels / got - 1) <= 1e-5);
    }
    // (6) hydrogen's g_J: 2.002 283 8 from CODATA's formula (the Dirac value and the radiative term's binding factor); the 21 cm line's frequency is Martin and Wiese's
    const br::GroundState* h = br::find(1, 0);
    checkTrue("breit-rabi: hydrogen's g_J = |g_D| + (|g_e| - 2)(1 + alpha^2 / 6) = " + brNum(br::gJHydrogen()) + ", below |g_e| by about |g_e| alpha^2 / 3",
              std::fabs((-atom::kGe - br::gJHydrogen()) / (-atom::kGe * atom::kAlpha * atom::kAlpha / 3) - 1) <= 0.01);
    checkTrue("breit-rabi: hydrogen's zero-field splitting is 1420.405 751 766 7 MHz (Martin and Wiese)", h && std::fabs(br::splittingHz(*h) - 1420.4057517667e6) <= 1e-4);
}
