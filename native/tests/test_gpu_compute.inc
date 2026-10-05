// 0.13 stage 2: the superposition and the jump on the GPU (included by test_atom.cpp after test_gpu_colours.inc). This file: the INDEPENDENT physics check, which does not depend on
// any GPU code and was written before it; the comparison of the GPU code with the CPU reference is in tests/gl_compare.cpp (it needs an OpenGL 4.3 context: Mesa's llvmpipe).
// REFERENCE: the textbook hydrogen-like wave functions, typed here (not the lab's):
//   psi(2p, m = 0, z)  = Z^(3/2) / (4 sqrt(2 pi)) * rho exp(-rho/2) * z/r                      rho = Z r / a0, lengths in a0, psi in a0^(-3/2)
//   psi(2p, x)         = the same with x/r
//   psi(2p, m = +1)    = -Z^(3/2) / (8 sqrt(pi)) * rho exp(-rho/2) * (x + i y)/r              (Condon-Shortley sign)
//   psi(3d, z^2)       = Z^(3/2) / (81 sqrt(6 pi)) * rho^2 exp(-rho/3) * (3 z^2/r^2 - 1)
// (Griffiths, Introduction to Quantum Mechanics, ch. 4; Bethe and Salpeter). FOUND: the lab's states are normalised by a numerical sum (outline.cpp, orbitalNorm: a midpoint rule with 400 points in
// cos(theta)), which leaves the lab's amplitude too big by 3.1e-6 for 2p and 1.6e-5 for 3d z2 (the textbook value is exactly 1; 1s is exact). It is not changed in this stage (the physics must not
// change); the tolerances below are those measured sizes, rounded up, and say so (density 1e-4, phase 2e-5 rad, norm 5e-5). Constants (CODATA 2022, typed): a0 = 5.29177210544e-11 m, Hartree energy 27.211386245981 eV,
// m_p/m_e = 1836.152673426, h = 6.62607015e-34 J s and e = 1.602176634e-19 C (both exact). The beat period of two levels is h / (E_B - E_A), E_n = -Ry (M/(M+1)) Z^2 / n^2.

static std::complex<double> tbPsi(int which, int Z, double x, double y, double z) {   // which: 0 = 2pz, 1 = 2px, 2 = 2p m=+1, 3 = 3d z2; x, y, z in Bohr radii
    const double pi = 3.14159265358979323846, r = std::sqrt(x * x + y * y + z * z), rho = Z * r, Z32 = std::pow(double(Z), 1.5);
    switch (which) {
        case 0: return Z32 / (4 * std::sqrt(2 * pi)) * rho * std::exp(-rho / 2) * z / r;
        case 1: return Z32 / (4 * std::sqrt(2 * pi)) * rho * std::exp(-rho / 2) * x / r;
        case 2: return -Z32 / (8 * std::sqrt(pi)) * rho * std::exp(-rho / 2) * std::complex<double>(x, y) / r;
        default: return Z32 / (81 * std::sqrt(6 * pi)) * rho * rho * std::exp(-rho / 3) * (3 * z * z / (r * r) - 1);
    }
}
static int tbId(int which) { return which == 0 ? orbIndex("2p|z") : which == 1 ? orbIndex("2p|x") : which == 2 ? complexState(orbIndex("2p"), 1) : orbIndex("3d|z2"); }
static int tbN(int which) { return which == 3 ? 3 : 2; }

static void gcpGaussLegendre(int n, std::vector<double>& x, std::vector<double>& w) {   // nodes and weights on [-1, 1] by Newton's method
    x.resize(n); w.resize(n);
    for (int i = 0; i < n; i++) {
        double z = std::cos(3.14159265358979323846 * (i + 0.75) / (n + 0.5)), dp = 1;
        for (int it = 0; it < 100; it++) {
            double p1 = 1, p2 = 0;
            for (int j = 1; j <= n; j++) { const double p3 = p2; p2 = p1; p1 = ((2 * j - 1) * z * p2 - (j - 1) * p3) / j; }
            dp = n * (z * p1 - p2) / (z * z - 1);
            const double dz = p1 / dp; z -= dz;
            if (std::fabs(dz) < 1e-15) break;
        }
        x[i] = z; w[i] = 2 / ((1 - z * z) * dp * dp);
    }
}

static void testGpuComputePhysics() {
    const double a0 = 5.29177210544e-11, pi = 3.14159265358979323846;
    checkNum("physics: the lab's Bohr radius is CODATA 2022's 5.29177210544e-11 m", kA0, a0, 1e-15);
    struct Pair { int A, B; const char* name; };
    const Pair pairs[] = {{0, 1, "2p z + 2p x (orthogonal, one energy)"}, {1, 2, "2p x + 2p m+1 (overlapping, one energy)"}, {0, 3, "2p z + 3d z2 (orthogonal, two energies: it beats)"}};
    const double pts[][3] = {{1.3, -0.7, 2.1}, {0.4, 1.9, -1.2}, {-2.2, 0.5, 0.9}, {3.0, 0.2, -2.5}, {-0.6, -1.1, -0.3}};      // in Bohr radii
    for (const int Z : {1, 2}) {
        for (const Pair& pr : pairs) {
            for (const double mix : {50.0, 30.0}) {
                const double a = std::sqrt(1 - mix / 100), b = std::sqrt(mix / 100);
                const Superposition s{tbId(pr.A), tbId(pr.B), a, b};
                // <A|B> by the textbook angular integral: 0 for different n or orthogonal angular parts; for 2p x and 2p m=+1 it is -1/sqrt(2) (worked out by hand: int sin^2 cos(phi) e^(i phi) = pi * 4/3)
                const std::complex<double> S = (pr.A == 1 && pr.B == 2) ? std::complex<double>(-1 / std::sqrt(2.0), 0) : std::complex<double>(0, 0);
                double worstDens = 0, worstPhase = 0;
                for (const double theta : {0.0, pi / 2, pi, 1.3})
                    for (const auto& p : pts) {
                        const double x = p[0] * a0 / a0, y = p[1], z = p[2];       // metres / a0 -> Bohr radii (the lab takes Bohr radii)
                        std::complex<double> psi = a * tbPsi(pr.A, Z, x, y, z) + b * tbPsi(pr.B, Z, x, y, z) * std::exp(std::complex<double>(0, -theta));
                        if (S != 0.0) psi /= std::sqrt(a * a + b * b + 2 * a * b * (S * std::exp(std::complex<double>(0, -theta))).real());
                        const std::complex<double> lab = psiSuper(s, Z, theta, x, y, z);
                        worstDens = std::max(worstDens, std::fabs(std::norm(lab) - std::norm(psi)) / std::max(std::norm(psi), 1e-300));
                        if (std::norm(psi) > 1e-6 * std::max(1.0, double(Z * Z * Z))) {   // the phase only where the wave is not nearly zero
                            double d = std::fabs(std::arg(lab) - std::arg(psi)); d = std::min(d, 2 * pi - d);
                            worstPhase = std::max(worstPhase, d);
                        }
                    }
                const std::string tag = std::string("physics: ") + pr.name + ", Z = " + std::to_string(Z) + ", " + std::to_string(int(mix)) + " % B";
                checkTrue(tag + ": density and phase at 20 points equal the textbook formulas (density 1e-4 relative, phase 2e-5 rad: the lab\'s normalisation sum, see the top of this file; measured " + std::to_string(worstDens) + ", " + std::to_string(worstPhase) + ")", worstDens < 1e-4 && worstPhase < 2e-5);
            }
        }
    }
    // the energies and the beat period: h / (E_B - E_A), the textbook way
    const double hartree = 27.211386245981, mpMe = 1836.152673426, h = 6.62607015e-34, e = 1.602176634e-19;
    for (const int Z : {1, 2}) {
        const double ry = hartree / 2 * mpMe / (mpMe + 1);                                     // hydrogen-like ion of the proton's mass (N = 0): Z = 1 only is exactly the proton; Z = 2 uses the lab's own mass, below
        if (Z != 1) continue;
        for (const Pair& pr : pairs) {
            const Superposition s{tbId(pr.A), tbId(pr.B), std::sqrt(0.5), std::sqrt(0.5)};
            const double dE = -ry * Z * Z / (double(tbN(pr.B)) * tbN(pr.B)) + ry * Z * Z / (double(tbN(pr.A)) * tbN(pr.A));    // eV, E_B - E_A
            checkNum(std::string("physics: beat energy E_B - E_A of ") + pr.name + " against Ry (CODATA 2022) with the proton's reduced mass (1e-9)", beatEV(1, 0, s), dE, 1e-9, 1e-30);
            if (dE == 0) { checkTrue(std::string("physics: no beat when the energies are equal: ") + pr.name, std::isinf(beatPeriodS(1, 0, s))); continue; }
            const double T = h / (std::fabs(dE) * e);
            checkNum(std::string("physics: beat period h / dE of ") + pr.name + " (1e-9)", beatPeriodS(1, 0, s), T, 1e-9);
            // at a quarter, a half and the whole period the lab's psiSuperT has the beat phase pi/2, pi, 2 pi (and its sign is the sign of E_B - E_A)
            const double x = 1.3, y = -0.7, z = 2.1;
            bool ok = true;
            for (const double q : {0.25, 0.5, 1.0}) {
                const std::complex<double> got = psiSuperT(s, 1, 0, q * T, x, y, z), want = psiSuper(s, 1, 2 * pi * q * (dE > 0 ? 1 : -1), x, y, z);
                ok = ok && std::abs(got - want) < 1e-7 * (std::abs(want) + 1e-3);
            }
            checkTrue(std::string("physics: psiSuperT at T/4, T/2, T is the wave at beat phase pi/2, pi, 2 pi: ") + pr.name, ok);
        }
    }
    // normalisation: the integral of |psiSuper|^2 over space is 1 for every pair and every beat phase (the renormalisation of 0.10.6 for the overlapping pair), by product quadrature
    std::vector<double> gx, gw;
    gcpGaussLegendre(24, gx, gw);
    for (const Pair& pr : pairs) {
        for (const double theta : {0.0, 1.3}) {
            const Superposition s{tbId(pr.A), tbId(pr.B), std::sqrt(0.7), std::sqrt(0.3)};
            double sum = 0;
            const double dr = 0.05; const int nr = 1600, nphi = 32;
            for (int i = 0; i < nr; i++) {
                const double r = (i + 0.5) * dr;
                for (int j = 0; j < 24; j++) {
                    const double ct = gx[std::size_t(j)], st = std::sqrt(1 - ct * ct);
                    for (int k = 0; k < nphi; k++) {
                        const double ph = 2 * pi * (k + 0.5) / nphi;
                        sum += std::norm(psiSuper(s, 1, theta, r * st * std::cos(ph), r * st * std::sin(ph), r * ct)) * r * r * dr * gw[std::size_t(j)] * (2 * pi / nphi);
                    }
                }
            }
            checkNum(std::string("physics: normalisation of ") + pr.name + " at beat phase " + std::to_string(theta).substr(0, 4) + " (quadrature; 5e-5 because of the lab\'s normalisation sum, measured 1.4e-5)", sum, 1.0, 5e-5);
        }
    }
}

// ---- the logic around the kernels, with a stand-in for the GPU (so it runs on every machine, also without OpenGL) ----
namespace {
struct GcFake : render::IGroupCompute {
    bool answer = true;                                  // what the calls return (false: pretend the kernel could not run)
    int superCalls = 0, jumpCalls = 0;
    const float* ampsA = nullptr; const float* ampsB = nullptr; long count = 0, dirtyLo = -1, dirtyHi = -1; std::uint64_t epoch = 0;
    render::SuperGroupParams sp; render::JumpGroupParams jp;
    bool ok() const override { return true; }
    bool superGroups(const float* amps, long n, std::uint64_t ep, long lo, long hi, const render::SuperGroupParams& p, unsigned char* out) override {
        superCalls++; ampsA = amps; count = n; epoch = ep; dirtyLo = lo; dirtyHi = hi; sp = p;
        if (!answer) return false;
        for (long i = 0; i < n; i++) out[i] = (unsigned char)(i % 7 == 0 ? 255 : i % 24);
        return true;
    }
    bool jumpGroups(const float* a, const float* b, long jn, std::uint64_t ep, const render::JumpGroupParams& p, unsigned char* out) override {
        jumpCalls++; ampsA = a; ampsB = b; count = jn; epoch = ep; jp = p;
        if (!answer) return false;
        for (long i = 0; i < 2 * jn; i++) out[i] = (unsigned char)(i % 5 == 0 ? 255 : i % 24);
        return true;
    }
};
}

static void testGpuComputeLogic() {
    // a superposition: what is sent to the GPU, and which path made the groups
    for (const int which : {10, 11, 12}) {                                  // scenes 13, 14, 15
        AtomLab L; benchSetup(L, which, 1040, 713);
        const std::string tag = "scene " + std::to_string(which + 3) + ": ";
        const long cnt = std::min<long>(L.s.k, L.cloudCap());
        std::vector<unsigned char> grp(static_cast<std::size_t>(cnt)), ref(static_cast<std::size_t>(cnt));
        L.s.beatT = 1.0;                                                     // a quarter beat
        const double th = L.beatTheta();
        for (long i = 0; i < cnt; i++) ref[static_cast<std::size_t>(i)] = (unsigned char)L.superGroup(i, th);
        GcFake gc;
        checkTrue(tag + "with a working compute object the GPU path makes the groups (and the fake's bytes are used as they are)", fillGroups(L, &gc, cnt, grp.data()) == GroupsBy::Gpu && gc.superCalls == 1 && gc.jumpCalls == 0 && grp[0] == 255 && grp[1] == 1 && grp[24] == 0);
        const Superposition su = L.superState();
        checkTrue(tag + "it was sent the amplitudes, the epoch and the dot count of the lab", gc.ampsA == L.s.sa.data() && gc.epoch == L.s.saEpoch && gc.count == cnt);
        checkTrue(tag + "the weights are the shares' square roots as floats, and exp(-i theta) as floats", gc.sp.a == float(su.a) && gc.sp.b == float(su.b) && gc.sp.emRe == float(std::cos(th)) && gc.sp.emIm == float(-std::sin(th)) &&
                  std::fabs(double(gc.sp.a) * gc.sp.a + double(gc.sp.b) * gc.sp.b - 1) < 1e-6);
        checkTrue(tag + "the overlap flag is set exactly for the overlapping pair (scene 14)", (gc.sp.overlap == 1) == (which == 11));
        checkTrue(tag + "the beat phase is the beat clock's: quarter beat = pi/2 in size (zero for the pairs without a beat)", which == 12 ? std::fabs(std::fabs(th) - 3.14159265358979323846 / 2) < 1e-9 : th == 0);
        // a failing GPU falls back to the CPU reference; the switch Off never calls it; no compute object (a 3.3 PC) is the CPU path
        GcFake bad; bad.answer = false;
        checkTrue(tag + "a compute object that fails: the CPU result, equal to the reference loop", fillGroups(L, &bad, cnt, grp.data()) == GroupsBy::Cpu && bad.superCalls == 1 && grp == ref);
        GcFake off; L.gpuCompute = false;
        checkTrue(tag + "switch Off: the CPU path, the compute object is not called", fillGroups(L, &off, cnt, grp.data()) == GroupsBy::Cpu && off.superCalls == 0 && grp == ref);
        L.gpuCompute = true;
        checkTrue(tag + "no compute object (OpenGL 3.3): the CPU path", fillGroups(L, nullptr, cnt, grp.data()) == GroupsBy::Cpu && grp == ref);
    }
    // a plain cloud (no jump, no superposition) never goes to the GPU
    {
        AtomLab L; benchSetup(L, 5, 1040, 713);
        std::vector<unsigned char> grp(48000);
        GcFake gc;
        checkTrue("a plain cloud: the CPU groups, the compute object is not called", fillGroups(L, &gc, 48000, grp.data()) == GroupsBy::Cpu && gc.superCalls == 0 && gc.jumpCalls == 0);
    }
    // the dirty range: after a frame of the lab, every amplitude that changed is inside the range the GPU copy will be sent
    {
        AtomLab L; benchSetup(L, 11, 1040, 713);
        const long dots = std::min<long>(L.s.k, L.cloudCap());      // (s.k keeps counting past the cap: the dots go round a ring)
        GcFake gc; std::vector<unsigned char> g(static_cast<std::size_t>(dots));
        L.superGroupsGpu(gc, dots, 0, g.data());
        checkTrue("dirty range: taking it empties it", L.s.saDirtyHi <= L.s.saDirtyLo);
        bool inside = true, any = false;
        for (int frame = 0; frame < 6; frame++) {
            const std::vector<float> before = L.s.sa;
            L.tick(1.0 / 240); L.step(1.0 / 240);
            const long lo = L.s.saDirtyLo, hi = L.s.saDirtyHi;
            for (std::size_t i = 0; i < before.size(); i++) if (before[i] != L.s.sa[i]) { any = true; inside = inside && long(i / 4) >= lo && long(i / 4) < hi; }
            L.superGroupsGpu(gc, dots, 0, g.data());                        // taken (as a frame does)
            inside = inside && gc.dirtyLo == lo && gc.dirtyHi == hi && L.s.saDirtyHi <= L.s.saDirtyLo && gc.epoch == L.s.saEpoch;
        }
        checkTrue("dirty range: in six frames the lab changed some amplitudes, every changed amplitude was inside the range handed to the GPU copy, and the epoch stayed", any && inside);
        const std::uint64_t e0 = L.s.saEpoch;
        L.restart();
        checkTrue("epoch: a restart makes a new epoch (the GPU copy is then sent in full)", L.s.saEpoch != e0);
    }
    // a jump: both populations and the jump's terms
    for (const char* phase : {"Sign", "Wheel"}) {
        AtomLab L; benchSetup(L, 8, 1040, 713);
        L.p.phase = phase;
        for (int f = 0; f < 700 && !(L.s.trans && L.jumpDrawn()); f++) { benchFrame(L, 8, f, f - 200); L.tick(1.0 / 240); L.step(1.0 / 240); }
        checkTrue(std::string("jump (") + phase + "): a jump is in flight and built", L.s.trans && L.jumpDrawn());
        const long jn = L.jumpDots();
        std::vector<unsigned char> grp(static_cast<std::size_t>(2 * jn)), ref(grp.size());
        L.jumpGroups(ref.data());
        GcFake gc;
        checkTrue(std::string("jump (") + phase + "): the GPU path makes the groups, from both populations' amplitudes and this jump's epoch", fillGroups(L, &gc, 2 * jn, grp.data()) == GroupsBy::Gpu && gc.jumpCalls == 1 && gc.superCalls == 0 &&
                  gc.ampsA == L.s.jA.data() && gc.ampsB == L.s.jB.data() && gc.count == jn && gc.epoch == L.s.jumpEpoch && grp[0] == 255 && grp[1] == 1);
        const double u = L.jumpU(), phi = L.jumpPhi();
        const JumpTerms T = jumpTerms(3.14159265358979323846 / 2 * u, phi, u);
        checkTrue(std::string("jump (") + phase + "): the terms sent are jumpTerms() as floats; Wheel says wheel", gc.jp.cth == float(T.cth) && gc.jp.sth == float(T.sth) && gc.jp.lam == float(T.lam) && gc.jp.emRe == float(T.em.real()) && gc.jp.emIm == float(T.em.imag()) &&
                  gc.jp.epRe == float(T.ep.real()) && gc.jp.epIm == float(T.ep.imag()) && (gc.jp.wheel == 1) == (std::string(phase) == "Wheel") && std::fabs(double(gc.jp.cth) * gc.jp.cth - gc.jp.lam) < 1e-6);
        GcFake bad; bad.answer = false;
        checkTrue(std::string("jump (") + phase + "): a failing compute object falls back to the CPU result", fillGroups(L, &bad, 2 * jn, grp.data()) == GroupsBy::Cpu && grp == ref);
        const std::uint64_t e0 = L.s.jumpEpoch;
        for (int f = 0; f < 800 && L.s.trans; f++) { L.tick(1.0 / 240); L.step(1.0 / 240); }      // let this jump finish (it lasts 1.2 s)
        const std::vector<int> next = L.targets(-1);
        checkTrue(std::string("jump (") + phase + "): the jump ended and another can start", !L.s.trans && !next.empty());
        if (!L.s.trans && !next.empty()) { L.startJump("emit", next.front()); checkTrue(std::string("jump (") + phase + "): the next jump has a new epoch (the GPU copy is sent again)", L.s.jumpEpoch != e0); }
    }
}
