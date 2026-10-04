#include "atom_learn.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#include "orbit/format.h"

namespace atom {

#include "atom_learn_data.inc"

namespace {

using std::string;
string str(long n) { return std::to_string(n); }
double jsRound(double x) { const double r = std::floor(x + 0.5); return r == 0 && x < 0 ? -0.0 : r; }
string lower(string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); }); return s; }
string pl(long n, const string& w) { return str(n) + " " + w + (n == 1 ? "" : "s"); }
string factCard(const string& a, const string& b, const string& c) {   // the web's card(a, b, c): one fact box
    return "<div class=\"lrnF\"><div class=\"lrnFa\">" + a + "</div><div class=\"lrnFb\">" + b + "</div><div class=\"lrnFc\">" + c + "</div></div>";
}
// a row of a log-scale list: name and value, and a bar
string barRow(const string& rn, double widthPct, const string& col) {
    return "<div class=\"lrnRow\"><div class=\"lrnRn\">" + rn + "</div><div class=\"lrnTr\"><div class=\"lrnBar\" style=\"width:" + fmt::jsString(widthPct) + "%;background:" + col + "\"></div></div></div>";
}
// the nuclear radius (m) and the lowest orbit (m) the guides use
double guideR(int A) { return A == 1 ? 0.84e-15 : 1.2e-15 * std::pow(A, 1.0 / 3); }
// the masses: CODATA 2022 (NIST), the values and pages are in atom_core.h (kUKg, kMpKg, kMnKg, kMeKg in kg; kMpU, kMnU, kMeInU in u; kUMeV = 1 u in MeV/c²)
const double kM_U = kUKg, kM_P = kMpKg, kM_N = kMnKg, kM_E = kMeKg;   // kg
const double kU_P = kMpU, kU_N = kMnU, kU_E = kMeInU, kU_MEV = kUMeV;   // u; 1 u = 931.494 MeV/c²
// the mass of the hydrogen-1 ATOM in u: 1.00782503223 is the value this lab already had (AME2020, the mass evaluation behind the binding table); it is NOT a CODATA constant and its source page was not re-read in 0.10.6 stage 2
const double kU_H = 1.00782503223;

}  // namespace

const char* const kLookIntro = "One page, top to bottom. Tap a heading below to jump to it. Everything here is about the atom you have open right now: change it below and the text changes with it.";
const char* const kTimelineIntro = "How our picture of the atom changed in 2,400 years, and how we learned to see, split and move atoms. Scroll through 22 steps.";
const char* const kScaleObjects[5] = {"a pea", "a marble", "a tennis ball", "a football", "a beach ball"};

const std::vector<TimelineStep>& timelineSteps() {
    static std::vector<TimelineStep> v(std::begin(kTl), std::end(kTl));
    return v;
}

// ---------------------------------------------------------------- "What am I even looking at?"
std::vector<LearnSection> lookSections(const AtomParams& p, const std::string& live) {
    const int Z = p.Z, N = p.N, A = Z + N;
    const string NM = name(Z), nm = lower(NM), nuc = nm + "-" + str(A), ion = ionLabel(Z), view = p.view;
    const double R = A == 1 ? 0.84e-15 : 1.2e-15 * std::pow(A, 1.0 / 3), r1 = kA0 / Z;
    std::vector<LearnSection> D;
    D.push_back({"big", "🔎 The big picture",
        "\n<p class=\"lrnP\">Everything around you is made of <b>atoms</b>. This lab shows you one, enlarged hundreds of millions of times so you can see how it is built.</p>"
        "\n<p class=\"lrnP\">An atom has two main parts: a tiny, very heavy <b>nucleus</b> in the middle, and light <b>electrons</b> around it. The nucleus holds almost all of the atom’s weight. The electrons fill almost all of its size, and in between is mostly empty space.</p>"
        "\n<div class=\"lrnCard\"><div class=\"lrnBig\">You have <b>" + NM + "-" + str(A) + "</b> open.</div><div class=\"lrnSm\">Its nucleus has <b>" + pl(Z, "proton") + "</b> and <b>" +
        (N == 0 ? string("no neutrons") : pl(N, "neutron")) + "</b>. The number of protons decides which element it is: " + str(Z) + " " + (Z == 1 ? "proton always means" : "protons always means") + " " + nm +
        ". Protons plus neutrons make " + str(A) + ", which is the number in the name.</div></div>"
        "\n<p class=\"lrnP\">Real life: this nucleus is <b>" + isoInfo(Z, N) + "</b>.</p>"});
    D.push_back({"nuc", "🔴 The nucleus: protons and neutrons",
        "\n<p class=\"lrnP\">The nucleus is made of two kinds of particles, together called <b>nucleons</b>:</p>"
        "\n<p class=\"lrnP\">• <b>Protons</b> have a positive electric charge (+1).<br>• <b>Neutrons</b> have no electric charge (0) and are a tiny bit heavier than protons.</p>"
        "\n<p class=\"lrnP\">Different numbers of neutrons make different versions of the same element, called <b>isotopes</b>. That is why carbon can be carbon-12, carbon-13 or carbon-14.</p>"
        "\n<p class=\"lrnP\"><b>Why doesn’t it fly apart?</b> Protons all have a positive charge, and the same charges push each other away. What holds the nucleus together is the <b>strong nuclear force</b>. At the tiny distances inside a nucleus it is far stronger than the electric push, but it only reaches across about the width of a proton or two.</p>"
        "\n<p class=\"lrnP lrnMuted\">In the Nucleus zoom, the reddish glowing blobs are protons and the grey-white ones are neutrons. The colours and glow are just there to help you tell them apart.</p>"});
    D.push_back({"quark", "🧩 Inside a proton or neutron: quarks",
        "\n<p class=\"lrnP\">Protons and neutrons are not the smallest pieces. Each one is made of three <b>quarks</b>, held together by the strong force. Particles called <b>gluons</b> carry that force.</p>"
        "\n<p class=\"lrnP\">Only two kinds of quark are needed to build protons and neutrons. An <b>up</b> quark has a charge of +⅔ and a <b>down</b> quark has a charge of −⅓:</p>"
        "\n<div class=\"lrnCard\"><div class=\"lrnBig\">Proton = up + up + down</div><div class=\"lrnSm\">+⅔ + ⅔ − ⅓ = <b>+1</b></div><div class=\"lrnBig\" style=\"margin-top:8px\">Neutron = up + down + down</div><div class=\"lrnSm\">+⅔ − ⅓ − ⅓ = <b>0</b></div></div>"
        "\n<p class=\"lrnP\">Surprise: the three quarks weigh only about <b>1%</b> of a proton. The other 99% or so is the energy of the strong force holding them together.</p>"
        "\n<p class=\"lrnP lrnMuted\">The small coloured dots in the deepest zoom are the quarks. Their red, green and blue are not real colours. “Colour charge” is just the name physicists gave to the strong force’s kind of charge. Real nucleons also contain a fizz of gluons and short-lived quark pairs, which you can switch on under Nucleon detail.</p>"});
    D.push_back({"elec", "⚡ The electron",
        "\n<p class=\"lrnP\">The <b>electron</b> has a negative charge (−1), exactly as big as a proton’s charge but the opposite sign. That is why an atom with as many electrons as protons has no charge overall. An electron is also very light: a proton weighs about 1,836 times more.</p>"
        "\n<p class=\"lrnP\">Opposite charges attract, so the nucleus pulls on the electron and keeps it around. But the electron can only have certain amounts of energy, called <b>energy levels</b> (n = 1, 2, 3 …). Level n = 1 is the lowest, so the electron cannot sink any further.</p>"
        "\n<p class=\"lrnP\">Give an electron the right amount of energy, for example from a particle of light (a <b>photon</b>), and it jumps up a level. When it drops back down it gives that energy out as light. You can try this in the Photons card.</p>"
        "\n<p class=\"lrnP lrnMuted\">As far as anyone has measured, an electron has no size at all. It is a point.</p>"});
    D.push_back({"one", "☝ Why only one electron?",
        "\n<p class=\"lrnP\">A real, neutral atom has as many electrons as protons. " + NM + " has <b>" + str(Z) + "</b>. But this lab draws just <b>one</b>.</p>"
        "\n<p class=\"lrnP\">" + (Z == 1 ? string("For hydrogen that is exactly right: a real hydrogen atom has one proton and one electron, so what you see is a real atom.")
                                      : "That means what you see is not a normal " + nm + " atom. It is a " + nm + " nucleus with a single electron, called <b>" + ion + "</b>: a " + nm + " atom that has lost " + str(Z - 1) + " of its " + str(Z) + " electrons.") + "</p>"
        "\n<p class=\"lrnP\"><b>Why?</b> With one electron the physics can be worked out exactly, on paper. With two or more, the electrons also push on each other, and the equations have no exact solution. So the lab shows the case that can be drawn correctly.</p>"
        "\n<p class=\"lrnP\">The more protons, the stronger the pull, so the electron sits <b>closer</b> and is held <b>tighter</b>. For this " + (Z == 1 ? "atom" : "nucleus") + " the lowest orbit is about <b>" + lrnSci(r1 * 1e12) + " pm</b> from the nucleus (a pm is a trillionth of a metre).</p>"});
    string vt;
    if (view == "Orbit")
        vt = (p.ostyle == "Bohr 1913" ? string("<div class=\"lrnCard\"><div class=\"lrnBig\">You are looking at the <b>1913 picture</b>.</div><div class=\"lrnSm\">The nucleus is just one small ball with a plus sign, and the electron (minus sign) sits on one of the fixed circles n = 1, 2, 3, 4. That is how Niels Bohr drew it. Neutrons were not found until 1932, so they are not there. Use ↓ Emit photon and ↑ Absorb photon to see a jump: the electron leaps from one circle to another and one photon is given out or taken in. The nucleus ball is not to scale.</div></div>") : string()) +
             "<p class=\"lrnP\">This is the <b>Bohr model</b>, made by Niels Bohr in 1913. The electron goes round the nucleus on a circle, like a planet round a star. The dotted trail behind it shows where it has just been.</p>"
             "\n<p class=\"lrnP\"><b>It is a simplification.</b> The Bohr model gets the <b>energies</b> of a one-electron atom right, and that is why it correctly predicts the light hydrogen gives out. But real electrons do not move along neat circles. The Cloud views show the modern picture.</p>"
             "\n<p class=\"lrnP lrnMuted\">A real electron goes round far too fast to see, so the lab slows time down by a huge factor. The exact number is written under the picture.</p>"
             "\n<p class=\"lrnP lrnMuted\">Drag the picture to turn the atom and look at the orbit from another angle.</p>";
    else if (view == "Cloud")
        vt = "<p class=\"lrnP\">This is the modern <b>quantum</b> picture (Erwin Schrödinger, 1926). An electron does not have an exact position or path. Instead you get a <b>probability cloud</b>: it is denser where the electron is more likely to be found.</p>"
             "\n<p class=\"lrnP\">Each dot is one place the electron could be found. Measure where an electron is, again and again, and the spots slowly build up this shape. So the cloud is a <b>map of chances</b>, not a picture of a path.</p>"
             "\n<p class=\"lrnP\">The name, like <b>" + p.orb + "</b>, has two parts. The number is the energy level (n). The letter is the shape: <b>s</b> is a sphere, <b>p</b> is a dumbbell, and <b>d</b> has more lobes and rings, and <b>f</b> has even more. A small letter after it, like <b>z</b> in 2p<sub>z</sub>, says which way the shape points. The two colours are the two signs of the electron’s wave: where they meet, the wave is zero and the electron is never found (a node). This is the <b>side view</b>: drag to move around.</p>";
    else if (view == "Cloud 3D")
        vt = "<p class=\"lrnP\">This is the same modern <b>quantum</b> picture as the Cloud view: a <b>probability cloud</b> that is denser where the electron is more likely to be found. Each dot is one place it could be found, and the cloud is a map of chances, not a path.</p>"
             "\n<p class=\"lrnP\">Here you can <b>drag to turn it</b> and see it from every side. The name, like <b>" + p.orb + "</b>, has two parts: the number is the energy level (n), and the letter is the shape (<b>s</b> sphere, <b>p</b> dumbbell, <b>d</b> lobes and rings, <b>f</b> even more lobes; the small letter after it says which way it points). The two colours are the + and − signs of the electron’s wave.</p>";
    else if (view == "Slice")
        vt = "<p class=\"lrnP\">This is the same quantum cloud, but <b>cut in half</b>, like slicing a fruit and looking at the flat cut. The brightness shows how likely the electron is to be found at each spot on that cut.</p>"
             "\n<p class=\"lrnP\">Blue and orange are the two <b>signs</b> (+ and −) of the electron’s wave, called ψ. Where they meet, the wave passes through zero. That is called a <b>node</b>, and the electron is never found exactly there.</p>"
             "\n<p class=\"lrnP\">Drag up or down on the picture, or use the cut controls, to move the cut through the cloud. The name, like <b>" + p.orb + "</b>, has the energy level (n) as a number and the shape as a letter.</p>";
    D.push_back({"pic", "🖼 The picture on screen: " + view, vt});
    string rd;
    if (view == "Orbit")
        rd = "<li><b>" + NM + "-" + str(A) + " (" + ion + ")</b>: the atom, with its one electron. " +
             (Z == 1 ? string() : ion + " means " + nm + " with " + str(Z - 1) + " electron" + (Z == 2 ? "" : "s") + " missing, so it has a charge of +" + str(Z - 1) + ".") +
             "</li><li><b>n=…</b>: the energy level. 1 is the lowest.</li><li><b>r</b>: how far the electron is from the nucleus. Å is an ångström (0.1 nanometre) and pm is a picometre (0.01 Å).</li><li><b>E</b>: the electron’s energy, in <b>eV</b> (electronvolts, a handy tiny unit). It is negative because the electron is <i>held</i>. The number tells you how much energy it takes to pull it away completely.</li><li><b>period</b>: the time for one trip round. <b>as</b> means attoseconds: a billionth of a billionth of a second.</li>";
    else
        rd = "<li><b>" + NM + "-" + str(A) + " (" + ion + ")</b>: the atom, with its one electron.</li><li><b>" + p.orb + " orbital</b>: which cloud shape is shown.</li><li><b>E</b>: the electron’s energy in <b>eV</b> (electronvolts). It is negative because the electron is <i>held</i>: the number is how much energy it takes to pull it away completely.</li><li><b>avg r</b>: the average distance from the nucleus. <b>sampled</b> is measured from the dots on screen, and <b>theory</b> is what the equations say it should be. They differ a little because the dots are random.</li>" +
             (view == "Slice" ? string("<li><b>cut plane</b>: where the knife went through the cloud.</li>") : string());
    D.push_back({"num", "🔢 Reading the numbers at the top",
        (live.empty() ? string() : "<div class=\"lrnCard\"><div class=\"lrnSm\">Right now the lab says:</div><div class=\"lrnBig\" style=\"font-size:14px\">" + live + "</div></div>") + "<ul class=\"lrnUl\">" + rd + "</ul>"});
    D.push_back({"scale", "📏 What is to scale, and what is not",
        "\n<p class=\"lrnP\"><b>To scale:</b> the lab draws the electron’s orbit and the nucleus at the <i>same</i> scale. That is why the nucleus is only a dot in the middle. For " + nuc + " the nucleus is about <b>" + lrnSci(2 * R * 1e15) +
        " fm</b> across (1 fm is a millionth of a billionth of a metre), and the lowest orbit is about <b>" + lrnSci(r1 / R) + " times</b> bigger than the nucleus’s radius. Zoom in to find it: use the Nucleus and Nucleon buttons.</p>"
        "\n<p class=\"lrnP\"><b>Made up to be seen:</b> the colours, the glow and the sparkle. Real protons and neutrons have no colour. Light waves are far too long to show anything that small. So the honest picture would be <span class=\"monoKey\" role=\"button\" tabindex=\"0\" id=\"monoKey\">monochrome</span>: just shades of grey.</p>"
        "\n<p class=\"lrnP\"><b>Simplified:</b> the electron drawn as a little ball (as far as anyone can tell, it is really a point), the neat circle in the Orbit view, and only one electron.</p>"
        "\n<p class=\"lrnP\"><b>Slowed down:</b> everything happens far too fast to watch, so the lab slows time.</p>"});
    return D;
}

// ---------------------------------------------------------------- "Scale: how empty is an atom?"
std::string scaleHtml(const AtomParams& p, const std::string& tab, int obj) {
    string out;
    if (tab == "ten") {
        struct Ten { const char* n; double m; const char* c; };
        static const Ten ten[] = {{"Human hair, width", 7e-5, "#c9a06a"}, {"Red blood cell", 8e-6, "#d24a4a"}, {"Bacterium", 2e-6, "#5aa05a"}, {"Virus", 1e-7, "#8a6ad0"},
                                  {"DNA, width of the helix", 2e-9, "#4a8ad2"}, {"Hydrogen atom", 1.06e-10, "#4aa8c0"}, {"Uranium-238 nucleus", 1.5e-14, "#d27a2a"},
                                  {"Proton", 1.7e-15, "#d24a4a"}, {"Quark: smaller than", 1e-18, "#e0b020"}};
        out += "<p class=\"lrnP\">Each step down the list is far smaller than the one above. The bars are on a <b>log scale</b>: every tick to the left is 10 times smaller.</p>";
        const double lo = -18.5, hi = -3.8;   // log10 of the bar's ends, m
        for (const Ten& t : ten) out += barRow(string(t.n) + "<br><small>" + lrnSci(t.m) + " m</small>", std::max(2.0, 100 * (std::log10(t.m) - lo) / (hi - lo)), t.c);
        out += "<p class=\"lrnP\">From a hair to a quark is a factor of about <b>100 trillion</b> (10¹⁴).</p>";
        return out;
    }
    const int Z = p.Z, A = p.Z + p.N;
    const double R = guideR(A), r1 = kA0 / Z;
    const string nm = string(name(Z)) + "-" + str(A);
    out += "<p class=\"lrnP\">Atoms are almost entirely empty space. Here is <b>" + nm + "</b>, the one you have open in the lab. Pick an object and imagine the nucleus were that size.</p>";
    out += "[[chips]]";
    static const double objD[5] = {0.007, 0.015, 0.067, 0.22, 0.5};   // m across
    struct Land { const char* n; double m; };
    static const Land land[] = {{"a bus", 12}, {"a football pitch", 105}, {"the Eiffel Tower", 330}, {"a 1 km walk", 1000}, {"a 5 km run", 5000}, {"a 10 km drive", 10000},
                                {"a marathon", 42195}, {"the sea crossing from Tallinn to Helsinki", 80000}, {"the road from Tallinn to Tartu", 186000}, {"the distance to the Moon", 384400000}};
    const double od = objD[obj], sc = od / (2 * R), er = r1 * sc;    // blow everything up by sc: the electron's orbit is er away
    const Land* best = &land[0];
    for (const Land& l : land) if (std::fabs(std::log(l.m / er)) < std::fabs(std::log(best->m / er))) best = &l;
    const double q = er / best->m;
    const string qt = q >= 0.95 && q <= 1.05 ? "about the same as" : "about " + lrnSci(q) + " times";
    out += "<div class=\"lrnCard\"><div class=\"lrnBig\">If the nucleus were <b>" + string(kScaleObjects[obj]) + "</b> (" + lrnLen(od) + " across), the electron's orbit would be <b>" + lrnLen(er) +
           "</b> away.</div><div class=\"lrnSm\">That is " + qt + " " + best->n + ". The whole atom would be " + lrnLen(2 * er) + " wide.</div></div>";
    const double vf = std::pow(R / r1, 3), nmass = Z * 1.007276 + p.N * 1.008665, em = Z * 0.000549, pct = 100 * nmass / (nmass + em);
    out += "<div class=\"lrnFacts\">" +
           factCard("Atom : nucleus width", lrnSci(r1 / R) + " : 1", "The electron in the 1s orbit is " + lrnSci(r1 / R) + " nucleus-radii away.") +
           factCard("Filled with matter", "1 part in " + lrnSci(1 / vf), "Volume of the nucleus compared to the volume of the orbit. The rest is empty space.") +
           factCard("Mass in the nucleus", fmt::fixed(pct, pct > 99.99 ? 3 : 2) + " %", "A neutral " + string(name(Z)) + " atom has " + str(Z) + " electron" + (Z > 1 ? "s" : "") + ", and together they weigh almost nothing next to the nucleus.") +
           "</div>";
    out += "<p class=\"lrnP lrnMuted\">Uses the real size of the nucleus (" + lrnSci(R * 1e15) + " femtometres) and the lab's 1s orbit (" + lrnSci(r1 * 1e12) +
           " picometres). In bigger atoms the outer electrons sit further out, so the real empty space is even larger. Change the element in the lab and reopen this panel to compare.</p>";
    return out;
}

// ---------------------------------------------------------------- "Weight: what does an atom weigh?"
std::string massHtml(const AtomParams& p, const std::string& tab) {
    const int Z = p.Z, N = p.N, A = Z + N;
    const string nm = string(name(Z)) + "-" + str(A);
    const Binding bd = massBinding(Z, N);
    const double parts = Z * kU_P + N * kU_N + Z * kU_E;                   // u: every part on its own
    const double atom = Z * kU_H + N * kU_N - bd.B / kU_MEV;               // u: the real atom (lighter by its binding energy, E = mc²)
    const double atomKg = atom * kM_U, miss = parts - atom;
    string out;
    if (tab == "parts") {
        out += "<p class=\"lrnP\">What does an atom weigh, and how much of that is each part? Here is <b>" + nm + "</b>, the one you have open in the lab. One atom weighs <b>" + lrnSci3(atomKg) +
               " kg</b>. That is " + lrnSci3(atom) + " atomic mass units (u), where 1 u is about the weight of one proton.</p>";
        struct Row { const char* n; int c; double m; const char* col; };
        const Row rows[3] = {{"Protons", Z, kM_P, "#d24a4a"}, {"Neutrons", N, kM_N, "#8a8a99"}, {"Electrons", Z, kM_E, "#4aa8c0"}};
        const double tot = Z * kM_P + N * kM_N + Z * kM_E;
        out += "<div class=\"lrnStack\">";
        for (const Row& r : rows) if (r.c) out += "<i style=\"background:" + string(r.col) + ";width:" + fmt::jsString(std::max(0.4, 100 * r.c * r.m / tot)) + "%\"></i>";
        out += "</div><div class=\"lrnFacts\">";
        for (const Row& r : rows)
            out += factCard("<span class=\"lrnDot\" style=\"background:" + string(r.col) + "\"></span>" + r.n, str(r.c) + " × " + lrnSci3(r.m, 5) + " kg",
                            r.c ? "Together " + lrnSci3(100 * r.c * r.m / tot) + " % of the weight." : string("This atom has none."));
        out += "</div>";
        const double pe = kM_P / kM_E;
        out += "<div class=\"lrnCard\"><div class=\"lrnBig\">A proton is <b>" + fmt::jsLocale(jsRound(pe)) + " times</b> heavier than an electron.</div><div class=\"lrnSm\">If a proton were a 1 kg bag of flour, an electron would weigh only <b>" +
               fmt::fixed(1000 / pe, 2) + " g</b>, about half a paperclip. That is why the electrons in " + nm + " (" + lrnSci3(100 * Z * kM_E / tot) + " % of its weight) hardly count, and why nearly all of an atom’s weight sits in the tiny nucleus.</div></div>";
        out += "<p class=\"lrnP\">The neutron is a tiny bit heavier than the proton: <b>0.14 %</b> more (1.293 MeV/c²). That small gap matters: it is why a neutron alone falls apart into a proton, an electron and a neutrino after about 15 minutes.</p>";
    } else if (tab == "miss") {
        if (A < 2) return "<div class=\"lrnCard\"><div class=\"lrnBig\">Hydrogen-1 is one proton and one electron, so there is nothing to bind.</div><div class=\"lrnSm\">Pick a bigger nucleus, like helium-4 or iron-56, and open this panel again.</div></div>";
        out += "<p class=\"lrnP\">Here is a strange fact. Weigh every part of <b>" + nm + "</b> on its own (" + str(Z) + " proton" + (Z > 1 ? "s" : "") + ", " + str(N) + " neutron" + (N == 1 ? "" : "s") + ", " +
               str(Z) + " electron" + (Z > 1 ? "s" : "") + "), add them up, and you get <b>more</b> than the real atom weighs.</p>";
        const double be = bd.B / A, mev = miss * kU_MEV;
        out += "<div class=\"lrnCard\"><div class=\"lrnBig\">The parts add up to <b>" + lrnSci3(parts) + " u</b>, but the atom weighs <b>" + lrnSci3(atom) + " u</b>.<br>Missing: <b>" + lrnSci3(miss) + " u</b> (" +
               lrnSci3(100 * miss / parts) + " % of the weight).</div><div class=\"lrnSm\">Where did it go? Einstein’s <b>E = mc²</b>: when the nucleons snapped together, that mass left as energy (" + lrnSci3(mev) +
               " MeV), and holding it back out again costs the same energy. It is called the <b>binding energy</b>. A nucleus weighs less than its parts because it is <i>held together</i>.</div></div>";
        out += "<div class=\"lrnFacts\">" + factCard("Binding energy per nucleon", fmt::fixed(be, 2) + " MeV", "The energy needed to pull out one proton or neutron on average.") +
               factCard("Compared with chemistry", lrnSci3(1e6 * be / 5, 2) + "× a chemical bond", "Per particle. A typical chemical bond is about 5 eV, so the forces inside a nucleus are millions of times stronger.") + "</div>";
        out += "<div class=\"lrnTr\" style=\"height:16px\"><div class=\"lrnBar\" style=\"width:" + fmt::jsString(std::max(1.0, std::min(100.0, 100 * be / 9))) + "%;background:#4aa8c0\"></div></div>";
        out += "<p class=\"lrnP lrnMuted\">" + nm + ": " + fmt::fixed(be, 2) + " MeV per nucleon on a bar that ends at 9. The most tightly bound nuclei are around iron and nickel (about 8.8). Lighter nuclei can release energy by <b>fusing</b> (that is how the Sun shines) and heavier ones by <b>splitting</b> (fission). That is also why a star gets no more energy from fusing atoms past iron." +
               (bd.real ? string() : string(" <b>Nobody has measured this isotope’s mass, so the number is an estimate from a formula.</b>")) + "</p>";
    } else if (tab == "count") {
        out += "<p class=\"lrnP\">One <b>" + nm + "</b> atom weighs " + lrnSci3(atomKg) + " kg. So how many of them make up everyday things?</p>";
        out += "<div class=\"lrnFacts\">";
        const std::pair<const char*, double> things[3] = {{"A grain of rice (25 mg)", 25e-6}, {"A paperclip (1 g)", 1e-3}, {"A person (70 kg)", 70}};
        for (const auto& [n, kg] : things) out += factCard(n, lrnSci3(kg / atomKg) + " atoms", "That many " + nm + " atoms weigh as much.");
        out += "</div>";
        out += "<div class=\"lrnCard\"><div class=\"lrnBig\">One gram of " + nm + " holds about <b>" + lrnSci3(1e-3 / atomKg) + "</b> atoms.</div><div class=\"lrnSm\">Count one atom every second and you would need about " +
               lrnSci3(1e-3 / atomKg / 3.156e7) + " years. The Universe is 13.8 billion years old (1.4×10¹⁰ years).</div></div>";
    } else if (tab == "proton") {
        const double qm = (2 * Z + N) * 2.16 + (Z + 2 * N) * 4.67, nmev = Z * 938.272 + N * 939.565, pq = 2 * 2.16 + 4.67;   // MeV/c²
        out += "<p class=\"lrnP\">A proton is made of three quarks (up, up, down). You might think its weight is the weight of those three quarks. It is not.</p>";
        out += "<div class=\"lrnFacts\">" + factCard("Proton weight", "938.3 MeV/c²", "Measured.") +
               factCard("Its 3 quarks added up", fmt::fixed(pq, 1) + " MeV/c²", "Up ≈ 2.2 and down ≈ 4.7 MeV/c², about " + fmt::fixed(100 * pq / 938.272, 1) + " % of the proton.") + "</div>";
        out += "<div class=\"lrnCard\"><div class=\"lrnBig\">About <b>99 %</b> of a proton’s weight is not quarks at all.</div><div class=\"lrnSm\">It is the <b>energy</b> of the gluons and of the quarks rushing around inside, and by E = mc² energy has weight. The Higgs field explains the small quark masses (and the whole electron), but most of the weight of everything you can touch comes from the strong force.</div></div>";
        out += "<div class=\"lrnCard\"><div class=\"lrnBig\">In <b>" + nm + "</b> the quarks add up to only <b>" + fmt::fixed(100 * qm / nmev, 1) + " %</b> of the nucleons’ weight.</div><div class=\"lrnSm\">" +
               fmt::jsLocale(2.0 * Z + N) + " up quarks and " + fmt::jsLocale(Z + 2.0 * N) + " down quarks, weighing " + lrnSci3(qm) + " MeV/c² together, out of " + lrnSci3(nmev) + " MeV/c².</div></div>";
        out += "<p class=\"lrnP lrnMuted\">Quark masses are approximate (2.16 and 4.67 MeV/c², from the Particle Data Group) and depend on how you define them.</p>";
    } else {
        out += "<p class=\"lrnP\">Weights from an electron to the Sun, on a <b>log scale</b>: each tick to the left is 10 times lighter. <b>" + nm + "</b> is highlighted.</p>";
        struct M { string n; double m; const char* c; };
        std::vector<M> list = {{"Electron", kM_E, "#4aa8c0"}, {"Proton", kM_P, "#d24a4a"}, {"Water molecule", 2.99e-26, "#5a8ad0"}, {"Bacterium (about 1 picogram)", 1e-15, "#5aa05a"},
                               {"Grain of salt (about 0.06 mg)", 6e-8, "#c9c9c9"}, {"Paperclip (1 g)", 1e-3, "#a0a0b0"}, {"Apple (about 200 g)", 0.2, "#d2503a"}, {"A person (70 kg)", 70, "#c9a06a"},
                               {"Earth", 5.97e24, "#3a7ad2"}, {"The Sun", 1.99e30, "#e0b020"}, {nm + " atom (yours)", atomKg, "#e0b020"}};
        std::stable_sort(list.begin(), list.end(), [](const M& a, const M& b) { return a.m < b.m; });
        const double lo = -31.5, hi = 30.6;
        for (const M& e : list) {
            const string n = e.n.find("(yours)") != string::npos ? "<b>" + e.n + "</b>" : e.n;
            out += barRow(n + "<br><small>" + lrnSci3(e.m) + " kg</small>", std::max(2.0, 100 * (std::log10(e.m) - lo) / (hi - lo)), e.c);
        }
        out += "<p class=\"lrnP\">From an electron to the Sun is a factor of about <b>2×10⁶⁰</b>.</p>";
    }
    return out;
}

// ---------------------------------------------------------------- the panel's texts
std::string isotopeLine(int Z, int N) { return (decInfo(Z, N) ? "☢ " : "● ") + isoLine(Z, N); }

std::string elemCardHtml(int Z) {
    string chips;
    for (const string& c : elementChips(Z)) chips += "<span>" + c + "</span>";
    return "<div class=\"ecard\" style=\"--h:" + str(categoryHue(Z)) + "\"><div class=\"etile\"><small>" + str(Z) + "</small><b>" + symbol(Z) + "</b></div><div class=\"ebody\"><div class=\"ename\">" +
           name(Z) + "</div><div class=\"echips\">" + chips + "</div><div class=\"euse\"><b>Where you meet it:</b> " + everyday(Z) + "</div></div></div>";
}
const char* const kTableNote = "Click an element for its most common isotope (longest-lived if none is stable). Dashed outline = no stable isotope.";

std::string factsHtml(int Z) {
    const Temp& t = tempData(Z);
    if (std::isnan(t.melt) && std::isnan(t.boil)) return factsLinesHtml(Z);
    const double m = t.melt, b = t.boil;
    const char f = t.flag;
    auto sq = [](double k) { return std::sqrt(std::min(k, kTMax) / kTMax); };
    string bar = "<div class=\"tbar\"><i class=\"s\" style=\"width:" + fmt::jsString(100 * sq(f == 's' ? b : std::isnan(m) ? 0 : m)) + "%\"></i>";
    if (!(f == 's' || std::isnan(m))) bar += "<i class=\"l\" style=\"width:" + fmt::jsString(100 * (sq(std::isnan(b) ? kTMax : b) - sq(m))) + "%\"></i>";
    bar += "<i class=\"g\" style=\"flex:1\"></i><em style=\"left:" + fmt::jsString(100 * std::sqrt(kTRoom / kTMax)) + "%\" title=\"room temperature\"></em><u class=\"tnow\"></u></div><div class=\"tsc\"><span>0 K</span><span>room</span><span>" +
           fmt::jsString(kTMax) + " K</span></div>";
    const string notes = factsNotesHtml(Z);
    return factsLinesHtml(Z) + bar + (notes.empty() ? "" : "<div class=\"tnote\">" + notes + "</div>") + "<div class=\"tnote\">" + kFactsFootnote + "</div>";
}

std::string spectrumNote(int Z, int N, bool dirac) {
    double ha = 0;
    for (const SpecLine& l : specLines(Z, N)) if (l.lo == 2 && l.hi == 3) ha = l.lam;   // (the non-relativistic H-alpha; with the Dirac levels on the sentence says "near", because the line is then five lines a few thousandths of a nm apart)
    const string ion = Z == 1 ? string() : string(symbol(Z)) + (Z - 1 > 1 ? supDigits(Z - 1) : "") + "⁺";
    string t = Z == 1 ? "<b>This is hydrogen’s real spectrum:</b> the light it gives off when its electron drops from a higher level to a lower one. The visible lines are the Balmer series (drops to level 2): Hα " + string(dirac ? "near" : "at") + " " + fmt::fixed(ha, 2) + " nm is the red glow of hydrogen in nebulae."
                      : "<b>The spectrum of " + string(name(Z)) + " with only one electron left (" + ion + ")</b>, the atom on screen. It is real: ions like this glow in very hot plasmas and stars. A normal " + lower(name(Z)) + " atom, with all " + str(Z) + " electrons, has a very different spectrum, which this lab does not model.";
    if (dirac) {   // 0.10.8 stage 1: the Dirac levels are on (dirac.h): fine structure IS drawn; the Lamb shift, the other QED effects and the nucleus's size are not
        t += " Each group is a series of drops to one level; its dashed line is the series limit. Vacuum wavelengths, with the nucleus’s real mass. Line heights are not brightness. "
             "Relativity is ON: the levels and lines are from the Dirac equation (one electron, point nucleus), so each line splits by j, and the split grows with Z. Not included: the Lamb shift, the other QED effects and the size of the nucleus, "
             "so NIST’s real lines still differ slightly. ▼ = lines your photon jumps made; they keep the non-relativistic wavelength.";
        return t;
    }
    t += " Each group is a series of drops to one level; its dashed line is the series limit. ▼ = lines your photon jumps made. Vacuum wavelengths, with the nucleus’s real mass. Line heights are not brightness. Not included: fine structure (relativity splits each line slightly: about 0.005 % for hydrogen, much more for heavy ions)";
    t += Z >= kRelativityZ ? " (for " + string(name(Z)) + " the true lines are shifted and split noticeably, so these are approximate)." : string(".");
    return t;
}

std::string spectrumNote(int Z, int N) { return spectrumNote(Z, N, false); }

std::string tempStateHtml(int Z, double K) {
    const auto st = tempState(Z, K);
    const string kk = K < 1000 ? fmt::fixed(K, K < 100 ? 1 : 0) : fmt::jsLocale(jsRound(K));
    return "<b>" + kk + " K</b> · " + fC(K) + " °C · " + fF(K) + " °F  →  " + name(Z) + " is " + (st ? "<b>" + string(stateIcon(*st)) + " " + *st + "</b>" : string("<b>unknown</b> (never measured)")) +
           (st && *st == "gas" && tempData(Z).flag == 's' ? " (it sublimed)" : "");
}

std::string tempSpeedHtml(int Z, double K) {
    if (K < 0.05) return "At this temperature the atoms have almost no motion.";
    const double mA = (Z + defaultN(Z)) * kUKg, vv = std::sqrt(3 * kBoltzmann * K / mA);   // typical speed: ½mv² = (3/2)kT
    const string sp = vv >= 1000 ? fmt::fixed(vv / 1000, 2) + " km/s" : vv >= 10 ? fmt::jsString(jsRound(vv)) + " m/s" : fmt::fixed(vv, 1) + " m/s";
    const double mph = vv * 2.23694;
    return "Typical atom speed here: ≈ <b>" + (fmt::imperial() ? (mph >= 10 ? fmt::jsLocale(jsRound(mph)) : fmt::fixed(mph, 1)) + " mph" : sp) + "</b>" +
           (fmt::imperial() ? string() : " (" + (fmt::fixed(vv * 3.6, 0) == "0" ? string() : "≈ " + fmt::jsLocale(jsRound(vv * 3.6)) + " km/h") + ")") + ". Hotter = faster.";
}
const char* const kTempNote = "States belong to big crowds of atoms, not to one atom. Solid: atoms locked in place, jiggling. Liquid: atoms touch but slide past each other. Gas: atoms fly freely. Real temperature is just how fast the atoms jiggle.";
const char* const kTempDetailsHtml =
    "<p><b>Temperature is how fast the atoms are jiggling and flying about, on average.</b> Hot means fast, cold means slow. There is no “hotness” stuff inside things: it is only motion. More precisely, it measures the average energy of that random motion.</p>"
    "<p><b>Heat</b> is different: it is energy moving from something hot to something cold, because fast atoms bump into slow ones and speed them up.</p>"
    "<p><b>0 K (−273.15 °C)</b> is the coldest possible temperature, where the atoms have almost no motion left (quantum physics never lets them stop completely). Kelvin starts there on purpose: 600 K has twice the motion energy of 300 K.</p>"
    "<p><b>Melting</b> happens when the jiggling gets strong enough to break the atoms out of their places. <b>Boiling</b> happens when they move so fast that they fly apart from each other.</p>"
    "<p>Drag the slider and watch: the atoms are the same, only their speed changes.</p>";

std::vector<std::pair<std::string, std::string>> decayHistory(const std::vector<ChainStep>& ch, int Z, int N) {
    std::vector<std::pair<string, string>> out;
    if (ch.size() < 2) {
        out.push_back({"decNone", ch.empty() ? string("No decays yet.")
                                  : decInfo(Z, N) && chainTiming(Z, N).kind == ChainTime::None ? ch[0].nuc + ": it is unstable, but " + chainHalfLifeWords(Z, N) + ", so the lab does not time it and it will not decay here by itself."
                                  : decInfo(Z, N) ? ch[0].nuc + ": no decays yet. When it decays, each step of the chain is listed here with how long it lived."
                                                  : ch[0].nuc + " does not decay (as far as anyone has measured). Build an unstable nucleus to start a decay history."});
        return out;
    }
    out.push_back({"decNuc", ch[0].nuc});
    double tot = 0;
    for (std::size_t i = 1; i < ch.size(); i++) {
        out.push_back({"decStep", "↓ " + ch[i].how + ", after " + fmtLife(ch[i].lived)});
        out.push_back({"decNuc", ch[i].to ? *ch[i].to : ch[i].how.find("split") != string::npos ? string("✂ two smaller nuclei") : string("something this lab cannot build")});
        tot += ch[i].lived;
    }
    out.push_back({"decNone", str(long(ch.size()) - 1) + " decay" + (ch.size() > 2 ? "s" : "") + " in " + fmtLife(tot) + " of real time"});
    return out;
}

}  // namespace atom
