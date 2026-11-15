// 0.15 stage 5b: the Atom Lab's element chip and its dropdown's controls (included by test_atom.cpp). WRITTEN BEFORE the new functions in atom/atom_tools.* existed.
// References: the words of the chip are the spec's ("U · Uranium-238 · 92 protons · 146 neutrons", singular for 1); the usual isotope of an element is its most abundant one, whose mass
// number is textbook (H-1, He-4, C-12, O-16, Fe-56, Pb-208, U-238); the old controls are called directly: the table cell and the Protons slider = setParam("Z") (which sets the neutrons to the
// usual count), the Neutrons slider = setParam("N"), the guide window's "usual" button = setParam("N", defaultN(Z)), the combo = setParam of Z and N.

static AtomLab chLab(int Z, int N) { AtomLab L; L.setParam("Z", double(Z)); L.setParam("N", double(N)); return L; }

static void testAtomChip() {
    {   // the words
        const atom::ChipText u = atom::elementChipText(92, 146);
        checkTrue("chip: uranium-238 title", u.title == "U · Uranium-238");
        checkTrue("chip: uranium-238 second line", u.sub == "92 protons · 146 neutrons");
        const atom::ChipText h = atom::elementChipText(1, 0);
        checkTrue("chip: hydrogen-1 says 1 proton, 0 neutrons", h.title == "H · Hydrogen-1" && h.sub == "1 proton · 0 neutrons");
        checkTrue("chip: deuterium says 1 neutron", atom::elementChipText(1, 1).sub == "1 proton · 1 neutron");
        checkTrue("chip: helium-3 says 2 protons, 1 neutron", atom::elementChipText(2, 1).sub == "2 protons · 1 neutron");
        checkTrue("chip: carbon-12 plural both", atom::elementChipText(6, 6).title == "C · Carbon-12" && atom::elementChipText(6, 6).sub == "6 protons · 6 neutrons");
    }
    {   // the usual isotope (textbook mass numbers)
        struct U { int Z, A; };
        const U usual[] = {{1, 1}, {2, 4}, {6, 12}, {8, 16}, {26, 56}, {82, 208}, {92, 238}};
        for (const U& u : usual) { AtomLab L; atom::pickElement(L, u.Z); checkTrue(std::string("chip: the usual isotope of Z=") + std::to_string(u.Z) + " has mass number " + std::to_string(u.A), L.p.Z + L.p.N == u.A); }
    }
    {   // every element can be picked, and gives what the old table and Protons slider gave
        int bad = 0;
        for (int Z = 1; Z <= 118; Z++) {
            AtomLab L = chLab(1, 0), ref = chLab(1, 0);
            atom::pickElement(L, Z); ref.setParam("Z", double(Z));
            if (L.p.Z != Z || L.p.N != ref.p.N || L.p.N != defaultN(Z)) bad++;
        }
        checkTrue("chip: all 118 elements can be picked, same Z and N as the old table", bad == 0);
    }
    {   // the Protons stepper (the old slider's - and +)
        AtomLab L = chLab(6, 6), ref = chLab(6, 6);
        atom::stepProtons(L, 1); ref.setParam("Z", 7.0);
        checkTrue("chip: Protons + gives Z+1 and its usual neutrons", L.p.Z == 7 && L.p.N == ref.p.N && L.p.N == defaultN(7));
        atom::stepProtons(L, -1);
        checkTrue("chip: Protons - goes back", L.p.Z == 6 && L.p.N == defaultN(6));
        AtomLab lo = chLab(1, 0); atom::stepProtons(lo, -1);
        checkTrue("chip: Protons - stops at 1", lo.p.Z == 1);
        AtomLab hi = chLab(118, defaultN(118)); atom::stepProtons(hi, 1);
        checkTrue("chip: Protons + stops at 118", hi.p.Z == 118);
    }
    {   // the Neutrons stepper and "usual"
        AtomLab L = chLab(6, 6);
        atom::stepNeutrons(L, 1);
        checkTrue("chip: Neutrons + gives one more (carbon-13)", L.p.Z == 6 && L.p.N == 7);
        atom::stepNeutrons(L, -1); atom::stepNeutrons(L, -1);
        checkTrue("chip: Neutrons - twice gives carbon-11", L.p.N == 5);
        atom::usualNeutrons(L);
        checkTrue("chip: usual gives carbon-12 again", L.p.N == 6 && L.p.N == defaultN(6));
        AtomLab lo = chLab(1, 0); atom::stepNeutrons(lo, -1);
        checkTrue("chip: Neutrons - stops at the slider's low end", lo.p.N == nRange(1).first);
        AtomLab hi = chLab(6, nRange(6).second); atom::stepNeutrons(hi, 1);
        checkTrue("chip: Neutrons + stops at the slider's high end", hi.p.N == nRange(6).second);
        AtomLab far = chLab(6, nRange(6).second); atom::usualNeutrons(far);
        checkTrue("chip: usual from the far end gives the usual count", far.p.N == defaultN(6));
    }
    {   // the famous isotopes: each one sets exactly its Z and N
        const auto& fam = famous();
        checkTrue("chip: the famous list is not empty", !fam.empty());
        int bad = 0;
        for (int i = 0; i < static_cast<int>(fam.size()); i++) {
            AtomLab L; atom::pickFamous(L, i);
            if (L.p.Z != fam[i].Z || L.p.N != fam[i].N) bad++;
        }
        checkTrue("chip: every famous isotope sets its own Z and N", bad == 0);
        AtomLab L; atom::pickFamous(L, -1); atom::pickFamous(L, 100000);
        checkTrue("chip: a famous index out of range changes nothing", L.p.Z == AtomLab().p.Z && L.p.N == AtomLab().p.N);
    }
    {   // Build another and Reshuffle live in the Nucleus panel, only in the nucleus view, and only when the old panel offered them
        AtomLab L = chLab(6, 6);
        checkTrue("chip: not in the atom view", !atom::toolShown(L, atom::Tool::Reshuffle) && !atom::toolShown(L, atom::Tool::BuildAnother));
        L.s.layer = "Nucleus";
        checkTrue("chip: Reshuffle shown in the nucleus view (12 nucleons)", atom::toolShown(L, atom::Tool::Reshuffle) == L.showReshuffle() && L.showReshuffle());
        checkTrue("chip: Build another follows the old rule (a nucleus held after a decay)", atom::toolShown(L, atom::Tool::BuildAnother) == L.showBuildAnother());
        L.s.decHeld = true;
        checkTrue("chip: Build another shown once a decay is held", atom::toolShown(L, atom::Tool::BuildAnother));
        const int seed = L.s.nucSeed;
        atom::pressTool(L, atom::Tool::Reshuffle);
        checkTrue("chip: Reshuffle = the old button (a new seed)", L.s.nucSeed == seed + 1);
        L.s.decKey = "held";
        atom::pressTool(L, atom::Tool::BuildAnother);
        checkTrue("chip: Build another = the old button (the held decay is let go)", L.s.decKey.empty());
        AtomLab one = chLab(1, 0); one.s.layer = "Nucleon";
        checkTrue("chip: no Reshuffle for a nucleus of 1 nucleon (the old rule: more than 3)", !atom::toolShown(one, atom::Tool::Reshuffle));
        AtomLab cl = chLab(6, 6); cl.s.layer = "Nucleus"; cl.setParam("view", std::string("Cloud"));
        checkTrue("chip: not shown in a cloud view", !atom::toolShown(cl, atom::Tool::Reshuffle));
    }
}
