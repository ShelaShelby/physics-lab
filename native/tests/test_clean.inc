// 0.12 stage 2: the accuracy clean-up (included by test_atom.cpp after test_chain.inc). Each test says what the app said BEFORE (recorded from the 0.12 stage 1 build) and checks what it says NOW against the data.
// REFERENCES (none is the lab's own output): (1) counts made by a separate Python script that reads only data/nubase/nubase_4.mas20.txt (NUBASE2020, sha256 1585a5ee...): 3,557 nuclides + the neutron; half-life
//  kinds M 2,831 (plain), S 253, E 369 (extrapolated, "#"), "-" 84 (none), B 11 (limit), A 6 ("~"), P 3 ("p-unst"); (2) the half-life texts of that file for 22 superheavy nuclides, typed here; (3) the same script's count
//  of nuclides with a decay mode marked "?" that the lab's odds leave out (644; 46 of them "X=?"); (4) the shape rule's distance to the nearest magic number worked out again here by hand.
// Guards: the half-life comparison allows 0.5 %: the lab writes three digits (1.02e2 for NUBASE's 102.5 s), which can be 0.5 % off; the measured worst case is 0.488 % (Sc-50).

static std::string clNubaseKind(int Z, int N) { const NubaseRow* nb = nubase(Z, N); return nb ? std::string(1, nb->kind) : "!"; }

static void testCleanup() {
    const std::vector<ChartTile>& tiles = chartTiles();
    // ---- 1. isoInfo: what each group of the 473 nuclides with no half-life number really is
    {
        // BEFORE (0.12 stage 1): all 473 said "seen only fleetingly in labs: no half-life measured (extremely short-lived or unbound)"; a square with no nuclide said "never observed: it either falls apart instantly or nobody has made it yet"
        std::map<std::string, int> byKind; int fleeting = 0, neverObs = 0, wrongWords = 0;
        for (const ChartTile& t : tiles) {
            const std::string info = isoInfo(t.Z, t.N);
            if (info.find("fleeting") != std::string::npos) fleeting++;
            if (info.find("never observed") != std::string::npos) neverObs++;
            const std::string* c = nucCode(t.Z, t.N);
            if (*c != "?") continue;
            const std::string k = clNubaseKind(t.Z, t.N);
            byKind[k]++;
            const char* want = k == "E" ? "has only an extrapolation from systematics" : k == "B" ? "only a limit is known" : k == "A" ? "half-life about" : k == "P" ? "unstable to emitting a particle" : "no half-life given by NUBASE2020";
            if (info.find(want) == std::string::npos) wrongWords++;
            if (k == "E" && info.find("which the lab does not show") == std::string::npos) wrongWords++;
        }
        checkNum("isoInfo: the 473 nuclides with no half-life number are, by NUBASE2020's kind, 369 extrapolated, 84 none given, 11 only a limit, 6 approximate, 3 p-unst (counts from the file)", byKind["E"] * 1000000 + byKind["-"] * 10000 + byKind["B"] * 100 + byKind["A"] * 10 + byKind["P"], 369 * 1000000 + 84 * 10000 + 11 * 100 + 6 * 10 + 3, 0);
        checkNum("isoInfo: texts that still say 'fleetingly' (before: 473) or 'never observed' (before: every square with no nuclide)", fleeting + neverObs, 0, 0);
        checkNum("isoInfo: a text that does not say what its group is (extrapolation / limit / approximate / p-unst / none given)", wrongWords, 0, 0);
        checkStr("isoInfo F-31 (extrapolated only)", isoInfo(9, 22), "no half-life measured: NUBASE2020 has only an extrapolation from systematics, which the lab does not show");
        checkStr("isoInfo B-16 (only a limit, >4.6 zs)", isoInfo(5, 11), "no half-life measured: only a limit is known (>4.6 zs)");
        checkStr("isoInfo Eu-162 (approximate, ~10 s)", isoInfo(63, 99), "half-life about 10 s (NUBASE2020 gives it only as approximate)");
        checkStr("isoInfo Li-3 (p-unst)", isoInfo(3, 0), "no half-life given: NUBASE2020 says only that it is unstable to emitting a particle");
        checkStr("isoInfo Li-12 (no half-life field)", isoInfo(3, 9), "no half-life given by NUBASE2020");
        checkStr("isoInfo of a square with no nuclide (helium-2)", isoInfo(2, 0), "not in the table: NUBASE2020 lists no such nuclide");
        // the one-line text: BEFORE it said "Fluorine-31: half-life 2 ms" and "Boron-16: half-life 4.6×10⁻²¹ s" (an extrapolation and a LIMIT shown as half-lives) and "Europium-162: half-life 10 s" (approximate shown as plain)
        const std::string f31 = isoLine(9, 22), b16 = isoLine(5, 11), eu = isoLine(63, 99);
        checkTrue("isoLine: F-31 no longer shows 'half-life 2 ms', B-16 no longer shows its limit as 'half-life 4.6×10⁻²¹ s', Eu-162 says 'about' (and 'approximate')", f31.rfind("Fluorine-31: no half-life measured", 0) == 0 && f31.find("half-life 2 ms") == std::string::npos &&
                  b16.find("half-life 4.6") == std::string::npos && b16.find("(>4.6 zs)") != std::string::npos && eu.find("half-life about 10 s") != std::string::npos && eu.find("approximate") != std::string::npos);
    }
    // ---- 2. the decay clock times only what the chain times
    {
        AtomLab lab; lab.p = AtomParams(); lab.paused = false; lab.restart(); lab.nowMs = [] { return 0.0; };
        lab.setParam("view", std::string("Cloud"));
        int untimed = 0, untimedWithNumber = 0, clockTimedUntimed = 0, moved = 0, timed = 0, clockMissedTimed = 0;
        for (const ChartTile& t : tiles) {
            if (t.cls == ChartClass::Stable) continue;
            const ChainTiming tm = chainTiming(t.Z, t.N);
            lab.setParam("Z", double(t.Z)); lab.setParam("N", double(t.N));
            lab.tick(0);
            if (lab.p.Z != t.Z || lab.p.N != t.N) continue;
            if (tm.kind == ChainTime::None) {
                untimed++;
                const auto d = decInfo(t.Z, t.N);
                if (d && d->T) untimedWithNumber++;                         // the lab's decay table holds a number for it: the OLD clock timed these
                if (!std::isinf(lab.s.decAt)) clockTimedUntimed++;
                lab.tick(1e15);
                if (lab.p.Z != t.Z || lab.p.N != t.N || lab.s.decays != 0) moved++;
            } else { timed++; if (!(lab.s.decAt > 0) || std::isinf(lab.s.decAt)) clockMissedTimed++; }
        }
        checkNum("clock: non-stable nuclides with no half-life to use (87 none + 369 extrapolated + 11 limit)", untimed, 467, 0);
        checkNum("clock: of them, those for which the lab's own decay table holds a half-life NUMBER (369 extrapolated + 11 limit): the old clock timed these as if measured", untimedWithNumber, 380, 0);
        checkNum("clock: untimed nuclides whose clock is set to a finite time (before: 380)", clockTimedUntimed, 0, 0);
        checkNum("clock: untimed nuclides that moved or decayed after 10^15 s of ticking", moved, 0, 0);
        checkNum("clock: timed nuclides (2,831 measured + 6 approximate) whose clock has no finite positive time", clockMissedTimed, 0, 0);
        checkNum("clock: timed nuclides seen", timed, 2837, 0);
        // the words (BEFORE for F-31: "☢ unstable: half-life 2 ms · decays by ... · chance it decays in the next minute: certain")
        AtomLab f; f.p = AtomParams(); f.paused = false; f.restart(); f.nowMs = [] { return 0.0; };
        f.setParam("Z", 9.0); f.setParam("N", 22.0); f.setParam("view", std::string("Cloud")); f.tick(0);
        const std::string dl = f.decLine();
        checkTrue("clock words: F-31 says its half-life is only an extrapolation, that the lab does not time it and it will not decay here by itself, and shows no half-life number or chance", dl.find("only an extrapolation") != std::string::npos || dl.find("has only an extrapolation") != std::string::npos);
        checkTrue("clock words (continued)", dl.find("so the lab does not time it and it will not decay here by itself") != std::string::npos && dl.find("chance it decays") == std::string::npos && dl.find("half-life 2 ms") == std::string::npos);
        const auto hist = decayHistory(f.chain, 9, 22);
        checkTrue("decay history of F-31 says the lab does not time it (it used to promise 'When it decays, each step ... is listed here')", hist.size() >= 1 && hist[0].second.find("does not time it") != std::string::npos && hist[0].second.find("When it decays") == std::string::npos);
        AtomLab e; e.p = AtomParams(); e.paused = false; e.restart(); e.nowMs = [] { return 0.0; };
        e.setParam("Z", 63.0); e.setParam("N", 99.0); e.setParam("view", std::string("Cloud")); e.tick(0);
        checkTrue("clock words: Eu-162 (approximate ~10 s) is timed and says 'about'", e.decLine().find("half-life about 10 s") != std::string::npos && !std::isinf(e.s.decAt));
        // the readout for the frame after a decay still belongs to the nucleus whose odds it shows (a bug found while writing this: it used the daughter's half-life)
        AtomLab g; g.p = AtomParams(); g.paused = false; g.restart(); g.nowMs = [] { return 0.0; };
        g.setParam("Z", 98.0); g.setParam("N", 154.0); g.setParam("view", std::string("Cloud")); g.tick(0.016); g.tick(1e12);
        checkTrue("clock words: right after californium-252 decays (to curium-248) the line is still californium-252's: half-life 2.65 years with its odds", g.p.Z == 96 && g.decLine().find("half-life 2.65 years") != std::string::npos);
    }
    // ---- 3. Kr-78 and Xe-124, and the lab's table against NUBASE2020 about which nuclides are stable
    {
        int disagree = 0, stable = 0;
        for (const ChartTile& t : tiles) { const bool nbS = clNubaseKind(t.Z, t.N) == "S", labS = *nucCode(t.Z, t.N) == "S"; if (nbS != labS) disagree++; if (labS) stable++; }
        checkNum("stable: nuclides where the lab's table and NUBASE2020 disagree about stable (before: 2, Kr-78 and Xe-124)", disagree, 0, 0);
        checkNum("stable: stable nuclides (NUBASE2020's 253 'stbl'; before: 251)", stable, 253, 0);
        checkTrue("Kr-78 and Xe-124: stable, no decay entry, class stable, the plateau; the numbers the lab had (9.2e21 and 1.8e22 years) are in neither NUBASE2020 nor the mendeleev package 1.3.0 (read 2026-10-04)",
                  *nucCode(36, 42) == "S" && *nucCode(54, 70) == "S" && !decInfo(36, 42) && !decInfo(54, 70) && chartClassOf(36, 42) == ChartClass::Stable && chartClassOf(54, 70) == ChartClass::Stable &&
                  chartHeightOf(36, 42).kind == HeightKind::StableNoted && chartHeightOf(54, 70).kind == HeightKind::StableNoted && isoInfo(36, 42) == "stable");
        checkTrue("Kr-78 and Xe-124: NUBASE2020's own limits are shown (>110 Ey and >200 Ty), as written", hoverHas(36, 42, ">110 Ey") && hoverHas(54, 70, ">200 Ty"));
        int obs = 0; for (const ChartTile& t : tiles) { const NubaseRow* nb = nubase(t.Z, t.N); if (nb && nb->kind == 'S' && (*nb->lim || *nb->modes) && *nucCode(t.Z, t.N) == "S") obs++; }
        checkNum("stable: observationally stable nuclides (a limit or a '?' entry) that the lab now calls stable: all 71 (69 before)", obs, 71, 0);
    }
    // ---- 4. the unsourced magic numbers 114 and 184
    {
        // BEFORE: kMagicN 184 and kMagicZ 114 sat in the code with no source and no word in the app. The shape rule (Casten's P-factor, distance to the nearest magic number) used them silently.
        const int mz[] = {2, 8, 20, 28, 50, 82}, mn[] = {2, 8, 20, 28, 50, 82, 126};                // the textbook list, typed again
        int affected = 0, mism = 0, noteWrong = 0, doubly = 0, by114 = 0, by184 = 0;
        for (const ChartTile& t : tiles) {
            const int Z = t.Z, N = t.N;
            if (Z == 114 && N == 184) doubly++;
            bool uses = false;
            if (Z + N >= 12 && !nucBE2Known(Z, N)) {
                int dz = 1000, dn = 1000, dz0 = 1000, dn0 = 1000;
                for (int m : mz) { dz = std::min(dz, std::abs(Z - m)); dz0 = dz; }
                for (int m : mn) { dn = std::min(dn, std::abs(N - m)); dn0 = dn; }
                dz = std::min(dz, std::abs(Z - 114)); dn = std::min(dn, std::abs(N - 184));
                uses = dz != dz0 || dn != dn0;
                if (dz != dz0) by114++;
                if (dn != dn0) by184++;
            }
            if (uses) affected++;
            if (uses != nucUsesUnsourcedMagic(Z, N)) mism++;
            const std::vector<std::string> lines = nuclearShapeLines(Z, N);
            bool note = false; for (const std::string& l : lines) if (l.find("Z = 114 or N = 184") != std::string::npos) note = true;
            const bool estimate = nucShape(Z, N).src == ShapeSource::Estimate && !(nucMeasured(Z, N) && !nucBE2Known(Z, N)) && Z + N >= 12;
            if (note != (uses && estimate)) noteWrong++;
        }
        checkNum("magic: nuclides whose shape estimate has Z = 114 or N = 184 as the nearest magic number, worked out again by hand (114 alone: 280, 184 alone: 203, either: 286)", affected, 286, 0);
        checkNum("magic: ... by 114", by114, 280, 0);
        checkNum("magic: ... by 184", by184, 203, 0);
        checkNum("magic: tiles where nucUsesUnsourcedMagic disagrees with the hand count", mism, 0, 0);
        checkNum("magic: tiles whose Shape source lines carry the 'no source' sentence when they should not, or lack it when they should (an estimate that 114 or 184 sets)", noteWrong, 0, 0);
        checkNum("magic: no nuclide is doubly magic by the unsourced numbers (Z = 114 with N = 184 is not a nuclide of the table), so the 'drawn round' rule never uses them", doubly, 0, 0);
        checkTrue("magic: the sentence says the two numbers are the web version's, have no source, are predictions never observed", [&] { for (const ChartTile& t : tiles) if (nucUsesUnsourcedMagic(t.Z, t.N) && nucShape(t.Z, t.N).src == ShapeSource::Estimate) { std::string a; for (const std::string& l : nuclearShapeLines(t.Z, t.N)) a += l; return a.find("web version's, with no source") != std::string::npos && a.find("never observed") != std::string::npos; } return false; }());
    }
    // ---- 5. the "~" half-lives and the stale superheavy values
    {
        // BEFORE: 113 half-lives were written "~x" and shown "about ... superheavy values are approximate, and some of these nuclides are claimed but not confirmed". NUBASE2020 has a plain measured value (with an uncertainty,
        // no "#" and no "~") for every one of them: the lab's table had rounded them to one or two digits and flagged them. 16 more superheavy values differed from NUBASE2020 by up to a factor 2.8 (an older evaluation).
        int tilde = 0, off = 0, compared = 0; double worst = 0;
        for (const ChartTile& t : tiles) {
            const std::string& c = *nucCode(t.Z, t.N);
            if (c[0] == '~') tilde++;
            const NubaseRow* nb = nubase(t.Z, t.N);
            if (!nb || nb->kind != 'M' || c == "U" || c == "?" || c == "S") continue;
            const double sec = nubaseSeconds(t.Z, t.N), lab = std::atof(c.c_str());
            if (!(sec > 0)) { off++; continue; }
            compared++;
            const double rel = std::fabs(lab - sec) / sec;
            worst = std::max(worst, rel);
            if (rel > 0.005) off++;
        }
        checkNum("half-lives: codes written '~' (before: 113)", tilde, 0, 0);
        checkNum("half-lives: plain half-lives compared with NUBASE2020's text (its 2,831 plain values minus the 32 unbound ones = 2,799) and those that differ by more than the guard 0.5 % (before: 16 stale + 113 rounded ones off by up to 4 %, found by the same comparison)", compared * 1000 + off, 2799 * 1000, 0);
        checkTrue("half-lives: the largest difference is the lab's three-digit rounding (measured 0.488 %, Sc-50: 1.02e2 s for 102.5 s)", worst < 0.005 && worst > 0.004);
        checkNum("half-lives: approximate values left = NUBASE2020's own '~' entries (6)", chartHeightCount(HeightKind::Approx), 6, 0);
        // typed from the file (half-life column and unit): the 16 that differed, and six of the rounded ones
        struct Ref { int Z, N; double sec; const char* what; };
        static const Ref ref[] = {{103, 163, 22 * 3600.0, "Lr-266: 22 h"}, {104, 163, 2.5 * 3600, "Rf-267: 2.5 h"}, {105, 163, 29 * 3600.0, "Db-268: 29 h"}, {106, 163, 300, "Sg-269: 5 m"}, {107, 163, 3.8 * 60, "Bh-270: 3.8 m"},
            {108, 161, 15, "Hs-269: 15 s"}, {109, 169, 6, "Mt-278: 6 s"}, {110, 171, 14, "Ds-281: 14 s"}, {111, 171, 130, "Rg-282: 130 s"}, {112, 173, 30, "Cn-285: 30 s"}, {113, 173, 12, "Nh-286: 12 s"},
            {114, 175, 2.1, "Fl-289: 2.1 s"}, {115, 175, 0.84, "Mc-290: 840 ms"}, {116, 177, 0.07, "Lv-293: 70 ms"}, {117, 177, 0.07, "Ts-294: 70 ms"}, {118, 176, 0.0007, "Og-294: 0.7 ms"},
            {103, 149, 0.369, "Lr-252: 369 ms (was ~3.7e-1)"}, {103, 150, 0.632, "Lr-253: 632 ms (was ~6.3e-1)"}, {103, 151, 12.0, "Lr-254: 12.0 s (was ~1.2e1)"}, {116, 176, 0.016, "Lv-292: 16 ms (was ~1.6e-2)"},
            {117, 176, 0.025, "Ts-293: 25 ms (was ~2.5e-2)"}, {118, 177, 0.68, "Og-295: 680 ms (was ~6.8e-1)"}};
        int bad = 0; for (const Ref& r : ref) { const auto d = decInfo(r.Z, r.N); if (!d || !d->T || std::fabs(*d->T - r.sec) > 0.005 * r.sec || chartHeightOf(r.Z, r.N).kind != HeightKind::Measured) { bad++; std::printf("  half-life check %s: got %g\n", r.what, d && d->T ? *d->T : -1.0); } }
        checkNum("half-lives: 22 typed values from NUBASE2020's file (16 stale, 6 rounded) that the lab's table now has, each a measured half-life", bad, 0, 0);
        checkTrue("half-lives: Sg-269 changed from 840 s (14 minutes) to NUBASE2020's 5 minutes, Rf-267 from 48 minutes to 2.5 hours, Og-294 from 690 µs to 0.7 ms", isoInfo(106, 163) == "radioactive (half-life 5 minutes)" && isoInfo(104, 163) == "radioactive (half-life 2.5 hours)" && isoInfo(118, 176) == "radioactive (half-life 700 µs)");
        checkTrue("half-lives: no text still says 'superheavy values are approximate'", [&] { for (const ChartTile& t : tiles) if (isoInfo(t.Z, t.N).find("superheavy values are approximate") != std::string::npos) return false; return true; }());
    }
    // ---- 6. decay modes NUBASE2020 lists without an intensity: the odds are not complete
    {
        // BEFORE: the odds of such a nuclide were shown as if complete (beryllium-11: "beta minus 97 %, beta then alpha 3.3 %", with NUBASE2020's "B-n ?" left unsaid)
        int listed = 0, hoverWith = 0, hoverWithout = 0, unkWith = 0, stableWith = 0, lineWith = 0, eqq = 0;
        for (const ChartTile& t : tiles) {
            const std::vector<std::string> u = decUnquantified(t.Z, t.N);
            const auto d = decInfo(t.Z, t.N);
            bool has = false; for (const std::string& l : chartHover(t.Z, t.N)) if (l.find("with no intensity (marked with a question mark)") != std::string::npos) has = true;
            if (!u.empty()) { listed++; if (has) hoverWith++; if (isoLine(t.Z, t.N).find("odds not complete") != std::string::npos) lineWith++; const NubaseRow* nb = nubase(t.Z, t.N); std::string m = nb ? nb->modes : ""; m.erase(std::remove(m.begin(), m.end(), ' '), m.end()); if (m.find("=?") != std::string::npos) eqq++; }
            else if (has) hoverWithout++;
            if (!u.empty() && d && d->unk) unkWith++;
            if (!u.empty() && *nucCode(t.Z, t.N) == "S") stableWith++;
        }
        checkNum("odds: nuclides with measured odds that leave out a mode NUBASE2020 lists with '?' (counted by a separate Python script from the file: 644)", listed, 644, 0);
        checkNum("odds: ... of them, those with an observed mode of unknown intensity ('X=?') (46)", eqq, 46, 0);
        checkNum("odds: hover boxes that say so, of those 644", hoverWith, 644, 0);
        checkNum("odds: one-line texts that say 'odds not complete', of those 644", lineWith, 644, 0);
        checkNum("odds: hover boxes that say so for a nuclide that has no such mode, for a nuclide whose odds are already marked not measured, for a stable nuclide", hoverWithout + unkWith + stableWith, 0, 0);
        checkStr("odds: beryllium-11's sentence", decUnquantifiedWords(4, 7), "NUBASE2020 also lists β⁻ then neutron for it, with no intensity (marked with a question mark): the odds above leave it out, so they are not complete.");
        checkTrue("odds: titanium-39 (NUBASE2020: B+=100, B+p=93.7, B+2p=?): the lab's odds are beta plus + proton 93.7 % and a 6.3 % remainder; the hover says B+2p is left out", decUnquantified(22, 17) == std::vector<std::string>{"B+2p"} && hoverHas(22, 17, "β⁺ then 2 protons for it, with no intensity"));
        checkTrue("odds: californium-251 and oganesson-294 (NUBASE2020: 'SF ?') say spontaneous fission is left out", decUnquantified(98, 153) == std::vector<std::string>{"SF"} && decUnquantified(118, 176) == std::vector<std::string>{"SF"});
        AtomLab lab; lab.p = AtomParams(); lab.paused = false; lab.restart(); lab.nowMs = [] { return 0.0; };
        lab.setParam("Z", 4.0); lab.setParam("N", 7.0); lab.setParam("view", std::string("Cloud")); lab.tick(0);
        checkTrue("odds: the decay readout of beryllium-11 says the odds are not complete", lab.decLine().find("odds not complete (NUBASE2020 lists modes with no intensity)") != std::string::npos);
        const ChainGraph g = decayChain(92, 146);
        bool inSummary = false; for (const std::string& l : chainSummary(g)) if (l.find("no intensity") != std::string::npos) inSummary = true;
        checkTrue("odds: the chain summary of uranium-238 says some nuclides in the chain have modes with no intensity (NUBASE2020 lists, for example, 'SF ?' for several)", inSummary == (std::any_of(g.nodes.begin(), g.nodes.end(), [](const ChainNode& x) { return x.reach >= 1e-3 && !decUnquantified(x.Z, x.N).empty(); })));
    }
}
