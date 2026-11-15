// Many-electron atoms (atom/hf.h). 0.39: Koga et al. Hartree-Fock orbitals; 0.41: Dirac-Hartree-Fock (tools/dhf, data/dhf). Included by test_atom.cpp (uses its checkTrue / checkNum / checkStr).
// References, none of them the app's own earlier output:
//  - L. Visscher and K. G. Dyall, At. Data Nucl. Data Tables 67, 207 (1997), Table IV as published on the authors' page (saved 7 Nov 2026 in the private sources repo; data/dhf/visscher_table4.extract.txt holds its rows, native/data/SOURCES.md): the <r> of
//    every orbital in Dirac-Fock (average level, Gaussian nucleus); read here from the saved page for the closed-shell atoms, whose calculation is the same as the app's;
//  - P. S. Bagus, Y. S. Lee and K. S. Pitzer, report LBL-3787 (1975), submitted to Chem. Phys. Lett. (Tables 1 and 2): gold 5d10 6s, Dirac-Fock <r> 6s 3.0609, 5d3/2 1.5359, 5d5/2 1.6185 bohr,
//    6s binding 0.2917 hartree; Hartree-Fock <r> 6s 3.7006 bohr;
//  - the published nonrelativistic Hartree-Fock orbitals of 0.39 (Koga et al. 1999 and 2000) for the nonrelativistic limit of the closed-shell atoms;
//  - NIST ASD ver. 5.12 as received (data/hf/nist_ie_all.tsv): ground shells and ground level J (H to Hs); Smits et al., Phys. Rep. 1035 (2023), Table 2 (Mt to Og);
//  - quantum mechanics: every orbital is normalised, an (n, l) large component has n - l - 1 radial nodes, the radial density integrates to the number of electrons;
//  - statistics: the Kolmogorov-Smirnov distance of 20,000 independent samples from their distribution is below 1.63 / sqrt(N) with probability 99 % (a fixed seed here).
#include "atom/hf.h"
#include <algorithm>
#include <fstream>
// the elements whose computed ground J differs from the reference (filled from the runs of 0.41; each is explained in tools/dhf and REPORT.md)
#define DHF_J_EXCEPTIONS std::vector<int>({})
#include <sstream>

static double hfQuad(const std::function<double(double)>& f, double rmax) {      // Simpson on r = rmax t^2 (dense near the nucleus), 20,000 panels
    const int M = 20000;
    double s = 0;
    for (int i = 0; i <= M; i++) {
        const double t = double(i) / M, r = rmax * t * t, w = (i == 0 || i == M) ? 1 : (i % 2 ? 4 : 2);
        s += w * f(r) * 2 * rmax * t;
    }
    return s / (3.0 * M);
}

// Table IV of Visscher & Dyall as saved: Z -> (configuration text, {"1s", "2p-", "2p", ...} -> <r> for the Gaussian nucleus, the last column)
static std::map<int, std::pair<std::string, std::map<std::string, double>>> visscherTable4() {
    std::map<int, std::pair<std::string, std::map<std::string, double>>> out;
    std::ifstream f(std::string(PL_DATA_DIR) + "/dhf/visscher_table4.extract.txt");
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()), txt;
    all.erase(std::remove(all.begin(), all.end(), '\r'), all.end());   // (a Windows checkout has CRLF line ends)
    bool tag = false;
    for (char c : all) { if (c == '<') tag = true; else if (c == '>') tag = false; else if (!tag) txt += c; }
    std::istringstream in(txt);
    std::string line;
    int Z = 0;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::vector<std::string> w; std::string x;
        while (ls >> x) w.push_back(x);
        if (w.size() < 5) continue;
        const auto isOrb = [](const std::string& o) { return o.size() >= 2 && std::isdigit(static_cast<unsigned char>(o[0])) && std::strchr("spdf", o[1]) && (o.size() == 2 || o == std::string(o.substr(0, 2)) + "-"); };
        const std::string& orb = w[w.size() - 5];
        if (!isOrb(orb) || w.back().find('E') == std::string::npos) continue;
        if (w.size() >= 8 && std::isupper(static_cast<unsigned char>(w[0][0])) && std::isdigit(static_cast<unsigned char>(w[1][0]))) {
            Z = std::atoi(w[1].c_str());
            std::string cfg; for (std::size_t k = 2; k + 5 < w.size(); k++) cfg += (cfg.empty() ? "" : " ") + w[k];
            out[Z].first = cfg;
        }
        if (Z) out[Z].second[orb] = std::atof(w.back().c_str());
    }
    return out;
}

static void testHartreeFock() {
    using namespace atom::hf;
    // the tables: every element He..Og, relativistic and nonrelativistic; nothing for H (one electron) or beyond Og
    int have = 0, countBad = 0, nodeBad = 0, normBad = 0;
    std::string worstNorm;
    for (int Z = 2; Z <= 118; Z++)
        for (bool rel : {true, false}) {
            const Atom* a = find(Z, 0, rel);
            if (!a) continue;
            have++;
            if (a->electrons() != Z) countBad++;
            for (const Subshell& s : a->sub) {                    // nodes of the large component (the larger j part), inside the part that holds all but 1e-6 of it
                // Skipped as rounding: the innermost part holding less than 1e-12 of the probability (like the outer 1e-6 below) and values below 1e-9 of the function's largest.
                // 0.41: inside the nucleus of oganesson the Gaussian sums of the d and f orbitals cancel to about 1e-13 in P (up to 1e-6 of the largest in R = P / r near r = 1e-5)
                // with random signs; the fixed cut of 1e-14 this test had first counted those as nodes. A physical node is a change of sign between values of order 1.
                double vmax = 0;
                for (int i = 1; i < 100000; i += 7) vmax = std::max(vmax, std::fabs(radialR(s, 1e-5 * std::pow(250.0 / 1e-5, double(i) / 100000))));
                int nodes = 0; double last = 0;
                for (int i = 1; i < 100000; i++) {
                    const double r = 1e-5 * std::pow(250.0 / 1e-5, double(i) / 100000);
                    const double cdf = radialCdf(s, r);
                    if (cdf > 1 - 1e-6) break;
                    if (cdf < 1e-12) continue;
                    const double v = radialR(s, r);
                    if (std::fabs(v) < 1e-9 * vmax) continue;
                    if (last != 0 && (v > 0) != (last > 0)) nodes++;
                    last = v;
                }
                if (nodes != s.n - s.l - 1) { nodeBad++; worstNorm = std::to_string(Z) + " " + std::to_string(s.n) + "spdf"[s.l] + " nodes " + std::to_string(nodes); }
            }
            if (Z % 9 == 0 || Z == 79 || Z == 118) {              // quadrature of the exact density (independent of the tables) on a sample of atoms
                for (const Subshell& s : a->sub) {
                    const double nrm = hfQuad([&](double r) { return radialDensity(s, r); }, 200.0);
                    if (std::fabs(nrm - 1) > 1e-6) { normBad++; worstNorm = std::to_string(Z) + " " + std::to_string(s.n) + "spdf"[s.l] + " norm " + std::to_string(nrm); }
                }
                const double ne = hfQuad([&](double r) { return totalRadial(*a, r); }, 200.0);
                if (std::fabs(ne - Z) > 1e-5 * Z) countBad++;
            }
        }
    checkTrue("DHF: every element He..Og has its relativistic and nonrelativistic atom (234), none for H or a charge", have == 234 && !find(1, 0) && !find(119, 0) && !find(26, 1));
    checkTrue("DHF: every atom holds Z electrons (and its radial density integrates to Z)", countBad == 0);
    checkTrue("DHF: every orbital is normalised (quadrature, 1e-6) " + worstNorm, normBad == 0);
    checkTrue("DHF: every (n, l) large component has n - l - 1 radial nodes " + worstNorm, nodeBad == 0);
    // Visscher & Dyall Table IV: the closed-shell atoms (their configuration is NIST's and the average level is the state itself): every relativistic orbital's <r>
    {
        const auto vt = visscherTable4();
        int compared = 0, bad = 0, inTable = 0; double worst = 0; std::string worstAt;
        for (int Z : {2, 4, 10, 12, 18, 20, 30, 36, 38, 48, 54, 56, 70, 80, 86, 88, 102}) {
            const Atom* a = find(Z, 0);
            auto it = vt.find(Z);
            if (!a || it == vt.end()) { bad++; continue; }
            inTable += int(it->second.second.size());
            for (const Subshell& s : a->sub)
                for (const Part& p : s.parts) {
                    std::string key = std::to_string(s.n) + "spdf"[s.l] + (s.l > 0 && p.j2 == 2 * s.l - 1 ? "-" : "");
                    auto v = it->second.second.find(key);
                    if (v == it->second.second.end()) { bad++; continue; }
                    const double d = std::fabs(partMeanRadius(p, s.l) / v->second - 1);
                    compared++;
                    if (d > worst) { worst = d; worstAt = std::to_string(Z) + " " + key; }
                }
        }
        // every orbital of the table is compared (230: counted from the saved Table IV; the first version of this test asked for more than 300, a wrong guess of the count)
        char b[96]; std::snprintf(b, sizeof b, "%d orbitals, worst %.1e at %s", compared, worst, worstAt.c_str());
        std::printf("DHF vs Visscher & Dyall Table IV (closed shells): %s\n", b);
        // the app's c is CODATA 2022 (theirs 137.0359895) and its nuclear mass numbers are the app's default isotopes: both change <r> by far less than 1e-5
        checkTrue(std::string("DHF: <r> of every orbital of the 17 closed-shell atoms He..No = Visscher & Dyall 1997 within 2e-5 (") + b + ")", bad == 0 && compared == inTable && inTable == 230 && worst < 2e-5);
    }
    // gold, 5d10 6s (one relativistic configuration): Bagus, Lee and Pitzer 1975
    {
        const Atom* au = find(79, 0); const Atom* aun = find(79, 0, false);
        checkTrue("DHF: gold is in the tables", au && aun);
        if (au && aun) {
        const Subshell& s6 = au->sub[std::size_t(subshellIndex(*au, 6, 0))];
        const Subshell& d5 = au->sub[std::size_t(subshellIndex(*au, 5, 2))];
        checkNum("DHF: gold 6s <r> = 3.0609 bohr (Bagus, Lee & Pitzer 1975, Dirac-Fock)", meanRadius(s6), 3.0609, 0, 1e-4);
        checkNum("DHF: gold 5d3/2 <r> = 1.5359 bohr (ibid.)", partMeanRadius(d5.parts[0], 2), 1.5359, 0, 1e-4);
        checkNum("DHF: gold 5d5/2 <r> = 1.6185 bohr (ibid.)", partMeanRadius(d5.parts[1], 2), 1.6185, 0, 1e-4);
        checkNum("DHF: gold 6s binding 0.2917 hartree (ibid.)", -s6.eps, 0.2917, 0, 1e-4);
        const Subshell& n6 = aun->sub[std::size_t(subshellIndex(*aun, 6, 0))];
        checkNum("DHF: gold, nonrelativistic limit: 6s <r> = 3.7006 bohr (ibid., Hartree-Fock)", meanRadius(n6), 3.7006, 0, 5e-4);
        std::printf("DHF: gold 6s <r> relativistic / nonrelativistic = %.4f\n", meanRadius(s6) / meanRadius(n6));
        }
    }
    // the nonrelativistic limit against the published Hartree-Fock orbitals (Koga et al.): the closed-shell atoms, every orbital
    {
        int compared = 0, subshells = 0; double worst = 0; std::string at;
        // Koga et al. use a point nucleus, the app a finite (Gaussian) one. For the heaviest that alone moves an s orbital by about 1e-4 (0.41: Hg 1s 0.9e-4, Rn 1.1e-4, Ra 1.2e-4,
        // No 1.7e-4; the outer s orbitals 0.4-0.7e-4). So this check covers He..Yb; Hg, Rn, Ra and No are checked below with tools/dhf's point-nucleus run, and printed.
        for (int Z : {2, 4, 10, 12, 18, 20, 30, 36, 38, 46, 48, 54, 56, 70}) {
            const Atom* a = find(Z, 0, false);
            if (!a) { subshells++; continue; }
            subshells += int(a->sub.size());
            for (const Subshell& s : a->sub) {
                const double k = kogaMeanRadius(Z, s.n, s.l);
                if (k <= 0) continue;
                const double d = std::fabs(meanRadius(s) / k - 1);
                compared++;
                if (d > worst) { worst = d; at = std::to_string(Z) + " " + std::to_string(s.n) + "spdf"[s.l]; }
            }
        }
        {   // printed: the finite-nucleus shift of the s orbitals of mercury, radon, radium and nobelium
            for (int Z : {80, 86, 88, 102}) {
                const Atom* x = find(Z, 0, false);
                for (int n = 1; x && n <= 7; n++) {
                    const int i = subshellIndex(*x, n, 0);
                    if (i < 0) break;
                    const double app = meanRadius(x->sub[std::size_t(i)]), k = kogaMeanRadius(Z, n, 0);
                    std::printf("DHF Z = %d nonrelativistic %ds <r>: %.8f, Koga et al. (point nucleus) %.8f, %+.1e\n", Z, n, app, k, app / k - 1);
                }
            }
        }
        char b[96]; std::snprintf(b, sizeof b, "%d orbitals, worst %.1e at %s", compared, worst, at.c_str());
        std::printf("DHF nonrelativistic limit vs Koga et al. (closed shells): %s\n", b);
        checkTrue(std::string("DHF: the nonrelativistic limit's <r> = the published Hartree-Fock orbitals (Koga et al.) within 1e-4 for the closed-shell atoms (") + b + ")", compared == subshells && worst < 1e-4);   // every orbital of those atoms has its published one
    }
    {   // the same solver with a point nucleus (tools/dhf/run_atom.py point Z, data/dhf/koga_point_check.tsv) against Koga et al. for Hg, Rn, Ra and No: every orbital within 1e-4
        std::ifstream f(std::string(PL_DATA_DIR) + "/dhf/koga_point_check.tsv");
        std::string line; int rows = 0, expect = 0, bad = 0; double worst = 0; std::string at;
        for (int Z : {80, 86, 88, 102}) { const Atom* a = find(Z, 0, false); expect += a ? int(a->sub.size()) : 1000; }
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#' || line[0] == 'Z') continue;
            std::istringstream ls(line); int Z = 0, n = 0, l = 0; double r = 0; std::string conv;
            ls >> Z >> n >> l >> r >> conv;
            const double k = kogaMeanRadius(Z, n, l);
            rows++;
            if (k <= 0 || conv != "True") { bad++; continue; }
            const double d = std::fabs(r / k - 1);
            if (d > worst) { worst = d; at = std::to_string(Z) + " " + std::to_string(n) + "spdf"[l]; }
        }
        char b[96]; std::snprintf(b, sizeof b, "%d orbitals, worst %.1e at %s", rows, worst, at.c_str());
        std::printf("DHF point-nucleus nonrelativistic run vs Koga et al. (Hg, Rn, Ra, No): %s\n", b);
        checkTrue(std::string("DHF: with a point nucleus the solver gives Koga et al.'s orbitals within 1e-4 for Hg, Rn, Ra and No (") + b + ")", rows == expect && bad == 0 && worst < 1e-4);
    }
    // configurations and ground levels: NIST's ground shells for He..Hs; the configuration interaction's ground J against NIST (He..Hs) and Smits et al. (Mt..Og)
    {
        std::vector<int> wrongConf, wrongJ;
        for (int Z = 2; Z <= 108; Z++) {
            const Atom* a = find(Z, 0); const NistIe* n = nistIe(Z, 0);
            if (!a || !n) { wrongConf.push_back(Z); continue; }
            if (!configMatchesNist(*a, *n)) wrongConf.push_back(Z);
            if (std::fabs(a->J2 / 2.0 - levelJ(n->level)) > 1e-9) wrongJ.push_back(Z);
        }
        const double smitsJ[10] = {4.5, 4, 2.5, 0, 0.5, 0, 1.5, 2, 1.5, 0};   // Smits et al. 2023, Table 2: Mt 4F9/2, Ds 3F4, Rg 2D5/2, Cn 1S0, Nh 2P1/2, Fl 1S0, Mc 2P3/2, Lv 3P2, Ts 2P3/2, Og 1S0
        for (int Z = 109; Z <= 118; Z++) { const Atom* a = find(Z, 0); if (!a || std::fabs(a->J2 / 2.0 - smitsJ[Z - 109]) > 1e-9) wrongJ.push_back(Z); }
        std::string wj; for (int z : wrongJ) wj += " " + std::to_string(z);
        std::printf("DHF: ground J different from the reference for Z =%s\n", wj.c_str());
        checkTrue("DHF: every configuration is NIST's ground shells (He..Hs)", wrongConf.empty());
        checkTrue("DHF: the configuration interaction's ground level J = NIST / Smits et al. for every element" + wj, wrongJ == DHF_J_EXCEPTIONS);
    }
    {   // sampleRadius inverts a table of the distribution function by cubic Hermite interpolation: the radius it returns for a random number u has the distribution function u to 1e-5
        double worstInv = 0;
        for (int Z = 2; Z <= 118; Z += 3) {
            const Atom* a = find(Z, 0);
            if (!a) continue;
            for (const Subshell& s : a->sub) {
                orbit::Mulberry32 rng(unsigned(1000 * Z + 10 * s.n + s.l));
                for (int i = 0; i < 300; i++) {
                    orbit::Mulberry32 copy = rng;
                    const double u = copy.next(), x = sampleRadius(s, rng);
                    worstInv = std::max(worstInv, std::fabs(radialCdf(s, x) - u));
                }
            }
        }
        char wb[32]; std::snprintf(wb, sizeof wb, "%.2e", worstInv);
        checkTrue(std::string("DHF: the sampled radius has the distribution function to 1e-5 (worst ") + wb + ")", worstInv < 1e-5);
    }
    // iron: the file's configuration, and NIST's values as received
    const Atom* fe = find(26, 0);
    checkStr("HF: iron's configuration", fe ? configText(*fe) : "", "1s2 2s2 2p6 3s2 3p6 3d6 4s2");
    const NistIe* nfe = nistIe(26, 0);
    checkStr("NIST: Fe I ionization energy (eV)", nfe ? nfe->ie : "", "7.9024681");
    checkStr("NIST: Fe I ground shells", nfe ? shellsText(nfe->shells) : "", "[Ar] 3d6 4s2");
    checkStr("NIST: Fe I is measured (no brackets)", nfe ? nistKind(*nfe) : "", "measured");
    const NistIe* npr = nistIe(59, 0);
    checkStr("NIST: Pr I is semi-empirical (square brackets)", npr ? nistKind(*npr) : "", "semi-empirical");
    // sampling: the radii of one orbital follow its distribution (Kolmogorov-Smirnov), and the whole atom's dots share out by occupation
    if (fe) {
        const int i3d = subshellIndex(*fe, 3, 2), i1s = subshellIndex(*fe, 1, 0);
        for (int idx : {i1s, i3d, subshellIndex(*fe, 4, 0)}) {
            orbit::Mulberry32 rng(12345u + unsigned(idx));
            const int N = 20000;
            std::vector<double> rs(N);
            for (double& v : rs) v = sampleRadius(fe->sub[std::size_t(idx)], rng);
            std::sort(rs.begin(), rs.end());
            double ks = 0;
            for (int i = 0; i < N; i++) {
                const double F = radialCdf(fe->sub[std::size_t(idx)], rs[std::size_t(i)]);
                ks = std::max({ks, std::fabs(F - double(i) / N), std::fabs(F - double(i + 1) / N)});
            }
            checkTrue("HF: Fe " + std::to_string(fe->sub[std::size_t(idx)].n) + "spdf"[fe->sub[std::size_t(idx)].l] + " sampled radii pass Kolmogorov-Smirnov (D = " + std::to_string(ks) + ")",
                      ks < 1.63 / std::sqrt(double(N)));
        }
        orbit::Mulberry32 rng(777u);
        const int N = 26000;
        int in3d = 0;
        double zz = 0, rr = 0;
        for (int i = 0; i < N; i++) {
            double x, y, z; int sh;
            sampleAtom(*fe, rng, x, y, z, sh);
            if (sh == i3d) in3d++;
            zz += z * z; rr += x * x + y * y + z * z;
        }
        checkNum("HF: Fe whole-atom dots in 3d = 6/26 of all", double(in3d) / N, 6.0 / 26, 0, 4 * std::sqrt(6.0 / 26 * 20.0 / 26 / N));
        checkNum("HF: Fe whole-atom dots are spherical: <z^2> = <r^2>/3", zz / rr, 1.0 / 3, 0.03);
    }
    // the lab: when the many-electron cloud is on, and the enlarged-shells radius keeps the order of the shells and the true outer edge (atom_sim.cpp, drawnRadius)
    {
        atom::AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        lab.setParam("view", std::string("Cloud 3D")); lab.setParam("Z", 26.0); lab.setParam("elec", std::string("All"));
        checkTrue("lab: iron, Cloud 3D, Electrons All shows every electron", lab.manyOn() && lab.manySub() < 0);
        checkTrue("lab: no superposition with many electrons", (lab.setParam("super", std::string("On")), !lab.superOn()));
        lab.setParam("super", std::string("Off"));
        bool mono = true;
        double last = -1;
        for (double r = 0; r < 20; r += 0.01) { const double d = lab.drawnRadius(r); if (!(d > last)) mono = false; last = d; }
        checkTrue("lab: the enlarged radius only grows (the shells keep their order)", mono);
        const atom::hf::Subshell& s4 = fe->sub[std::size_t(subshellIndex(*fe, 4, 0))];
        double lo = 0, hi = 200;
        for (int it = 0; it < 60; it++) { const double mid = 0.5 * (lo + hi); (radialCdf(s4, mid) < 0.9 ? lo : hi) = mid; }
        checkNum("lab: the enlarged radius keeps the outer edge (the 90 % radius of 4s) true", lab.drawnRadius(hi), hi, 1e-9);
        lab.setParam("rscale", std::string("True scale"));
        checkNum("lab: True scale draws every radius as it is", lab.drawnRadius(1.234), 1.234, 0);
        lab.setParam("show", std::string("One orbital")); lab.setParam("orb", std::string("3d"));
        checkTrue("lab: One orbital 3d of iron is its 3d subshell", lab.manySub() == subshellIndex(*fe, 3, 2));
        lab.setParam("orb", std::string("4p"));
        checkTrue("lab: an orbital iron does not fill (4p) falls back to the whole atom", lab.manySub() < 0);
        lab.setParam("show", std::string("Whole atom"));
        lab.s.layer = "Nucleus";                                  // 0.39.1: zoomed to the nucleus, the many-electron readout keeps the nucleus lines (it returned before them in 0.39)
        { bool shape = false; for (const std::string& l : lab.info()) if (l.rfind("shape:", 0) == 0) shape = true;
          checkTrue("lab: Electrons All, zoomed to the nucleus: the readout still gives the nucleus's shape", shape); }
        lab.s.layer = "Atom";
        lab.setParam("view", std::string("Slice"));              // 0.39.1: every view shows every electron (0.39: "the Slice view stays one-electron")
        checkTrue("lab: the Slice view shows every electron too", lab.manyOn() && lab.manySub() < 0);
        lab.setParam("view", std::string("Orbit"));
        {
            const std::vector<atom::AtomLab::Ring> g = lab.manyRings();
            int sum = 0; for (const auto& q : g) sum += q.electrons;
            const double k = meanRadius(fe->sub[std::size_t(subshellIndex(*fe, 1, 0))]);
            checkTrue("lab: Orbit view, iron: the rings are K 2, L 8, M 14, N 2 (26 electrons), K at the 1s average distance", lab.manyOn() && g.size() == 4 && g[0].electrons == 2 && g[1].electrons == 8 &&
                      g[2].electrons == 14 && g[3].electrons == 2 && sum == 26 && std::fabs(g[0].r - k) < 1e-12);
            const auto& m = fe->sub;   // the M ring: 3s2 3p6 3d6, weighted by the electrons
            const double mr = (2 * meanRadius(m[std::size_t(subshellIndex(*fe, 3, 0))]) + 6 * meanRadius(m[std::size_t(subshellIndex(*fe, 3, 1))]) + 6 * meanRadius(m[std::size_t(subshellIndex(*fe, 3, 2))])) / 14;
            checkNum("lab: the M ring of iron is at the electron-weighted average distance of 3s, 3p, 3d", g[2].r, mr, 1e-12);
            // 0.50 (review item 6): the rings stand still: two frames 1 s apart draw every electron at the same place (the turning was the view's one non-physical motion)
            std::vector<double> a0;
            for (const auto& q : g) for (int k = 0; k < q.electrons; k++) a0.push_back(lab.ringElectronAngle(q, k));
            const bool rm = lab.reducedMotion; lab.reducedMotion = false;
            for (int f = 0; f < 240; f++) lab.step(1.0 / 240);
            lab.reducedMotion = rm;
            double moved = 0; std::size_t i = 0;
            for (const auto& q : lab.manyRings()) for (int k = 0; k < q.electrons; k++) moved = std::max(moved, std::fabs(lab.ringElectronAngle(q, k) - a0[i++]));
            checkTrue("lab: Orbit view, every electron: two frames 1 s apart draw the same picture (no electron moved; largest move " + std::to_string(moved) + " rad)", i == a0.size() && moved == 0);
        }
        lab.setParam("view", std::string("Cloud")); lab.setParam("zoom", 2.0);
        {   // 0.39.1: zoomed in, a new element keeps the scale on screen (the counter used the one-electron Zprev / Z also for neutral atoms, whose size hardly changes with Z)
            const double m = lab.fitSpan(), before = lab.pxPerM(m) * std::pow(10, lab.p.zoom);
            lab.setParam("Z", 92.0);
            checkNum("lab: Electrons All, zoomed in, iron -> uranium: the points per metre stay the same", lab.pxPerM(m) * std::pow(10, lab.p.zoom) / before, 1, 1e-9);
        }
        lab.setParam("Z", 26.0); lab.setParam("elec", std::string("One")); lab.setParam("zoom", 3.0);
        {   // 0.39.1: the same when Electrons changes (iron: the 1s ion's cloud against the whole atom, about 50 times), so a view zoomed to the nucleus stays there
            const double m = lab.fitSpan(), before = lab.pxPerM(m) * std::pow(10, lab.p.zoom), s0 = lab.sizeM();
            lab.setParam("elec", std::string("All"));
            std::printf("HF lab: iron, zoom 3, Electrons One -> All: fitted size x%.1f, zoom %.3f\n", lab.sizeM() / s0, lab.p.zoom);
            checkNum("lab: zoomed in, Electrons One -> All keeps the points per metre", lab.pxPerM(m) * std::pow(10, lab.p.zoom) / before, 1, 1e-9);
        }
        lab.setParam("Z", 1.0);
        checkTrue("lab: hydrogen has one electron anyway (no many-electron table)", !lab.manyOn());
        lab.setParam("Z", 118.0);
        checkTrue("lab: oganesson shows every electron too (0.41: Dirac-Hartree-Fock for He..Og; 0.39 ended at lawrencium)", lab.manyOn());
    }
    // 0.39.1: One orbital with a complex orientation (iron 2p, m = +1): each dot carries the wave's phase, m phi + a constant (2p has no radial node and no theta sign change),
    // not only 0 or pi (0.39 took the sign of the real part)
    {
        atom::AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.paused = false; lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("Z", 26.0); lab.setParam("elec", std::string("All"));
        lab.setParam("show", std::string("One orbital")); lab.setParam("orb", std::string("2p")); lab.setParam("pAx", std::string("m+1"));
        for (int i = 0; i < 150; i++) lab.step(1.0 / 240);
        const long cnt = std::min<long>(lab.s.k, long(lab.s.ph.size()));
        const auto wrap = [](double d) { d = std::fmod(d, 2 * kPi); if (d > kPi) d -= 2 * kPi; if (d < -kPi) d += 2 * kPi; return d; };
        double c0 = 0, worst = 0;
        for (long i = 0; i < cnt; i++) {
            const double phi = std::atan2(lab.s.pts[3 * i + 1], lab.s.pts[3 * i]), c = lab.s.ph[i] - phi;
            if (i == 0) c0 = c; else worst = std::max(worst, std::fabs(wrap(c - c0)));
        }
        std::printf("HF lab: iron 2p m = +1, One orbital: %ld dots, phase - phi varies by %.1e rad\n", cnt, worst);
        checkTrue("lab: iron 2p m = +1 (One orbital): every dot's phase is phi + one constant (within 1e-4 rad)", lab.manyOn() && lab.manySub() >= 0 && cnt > 1000 && worst < 1e-4);
    }
    // 0.39.1, the Slice view: the whole atom on a plane. hf::planeOf's mass (closed form) against a direct quadrature of the density sum occ R^2 / 4 pi over the plane, and the sampled
    // distance from the nucleus against the same quadrature (independent: Simpson's rule on R(r) itself, not the closed-form integrals)
    for (double off : {0.0, 0.3, 1.5}) {
        if (!fe) break;
        const auto rho = [&](double sv) { const double r = std::sqrt(sv * sv + off * off); double d = 0; for (const Subshell& sh : fe->sub) if (sh.occ) { const double R = radialR(sh, r); d += sh.occ * R * R; } return d / (4 * kPi); };
        const int M = 40000; const double tMax = std::sqrt(80.0), h = tMax / M;
        double mass = 0, mr = 0;
        for (int i = 0; i <= M; i++) {                                  // s = t^2 (fine near the nucleus, where the K shell is), ds = 2 t dt
            const double t = i * h, sv = t * t, wgt = (i == 0 || i == M ? 1 : i % 2 ? 4 : 2) * h / 3, f = rho(sv) * 2 * kPi * sv * 2 * t;
            mass += wgt * f; mr += wgt * f * std::sqrt(sv * sv + off * off);
        }
        const Plane pl = planeOf(*fe, off);
        checkNum("HF plane: iron, the whole atom's electrons per unit area summed over the plane at " + std::to_string(off) + " a0 = the quadrature", pl.total, mass, 1e-6 * mass);
        orbit::Mulberry32 rng(4242);
        const int N = 200000;
        double s1 = 0, s2 = 0; bool on = true;
        for (int k = 0; k < N; k++) { double rad; int sh; if (!samplePlane(*fe, pl, -1, rng, rad, sh) || rad < off) on = false; s1 += rad; s2 += rad * rad; }
        const double mean = s1 / N, sd = std::sqrt(std::max(0.0, s2 / N - mean * mean));
        std::printf("HF plane: iron at %.1f a0: mass %.6f (quadrature %.6f), mean distance of the dots %.5f a0 (quadrature %.5f, 4 sigma = %.5f)\n", off, pl.total, mass, mean, mr / mass, 4 * sd / std::sqrt(double(N)));
        checkNum("HF plane: iron at " + std::to_string(off) + " a0: the dots' average distance from the nucleus = the quadrature (4 sigma)", mean, mr / mass, 0, 4 * sd / std::sqrt(double(N)));   // (0.41: 4 sigma as an absolute bound; until 0.40 it was passed as a relative one by mistake)
        checkTrue("HF plane: every dot's distance is at least the plane's", on);
    }
    // and in the lab: Slice with Electrons All puts every dot on the cut plane (the enlarged shells spread them within the plane only)
    {
        atom::AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.paused = false; lab.restart();
        lab.setParam("view", std::string("Slice")); lab.setParam("Z", 26.0); lab.setParam("elec", std::string("All")); lab.setParam("cutAxis", std::string("Horizontal")); lab.setParam("cut", 0.2);
        for (int i = 0; i < 40; i++) lab.step(1.0 / 240);
        const long cnt = std::min<long>(lab.s.k, long(lab.s.ph.size()));
        const double off = lab.cutOffsetM() / kA0;
        double worst = 0;
        for (long i = 0; i < cnt; i++) worst = std::max(worst, std::fabs(lab.s.pts[3 * i + 2] / kA0 - off));
        std::printf("HF lab: Slice, iron, cut 0.2: %ld dots, plane at %.4f a0, farthest dot off it %.2e a0\n", cnt, off, worst);
        checkTrue("lab: Slice, iron, Electrons All: every dot lies on the cut plane (within float rounding)", lab.manyOn() && cnt > 1000 && off > 0 && worst < 1e-5 * (1 + off));
    }
}
