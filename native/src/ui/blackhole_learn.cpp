#include "blackhole_learn.h"

#include <cstdlib>

#include "blackhole/light.h"
#include "blackhole/looks.h"
#include "imgui.h"
#include "orbit/format.h"
#include "orbit/sun_facts.h"
#include "html_view.h"
#include "orbit_panel.h"
#include "rich_text.h"
#include "ui/reading.h"

#include "atom/atom_core.h"
#include "blackhole/disc.h"

namespace {
// 0.86.0 (Part D): the electron-scattering opacity of ionised hydrogen, sigma_T / m_p, m^2/kg: sigma_T = (8 pi / 3) r_e^2, r_e = alpha^2 a_0 (CODATA 2022, atom_core.h)
double kappaHydrogen() { const double re = atom::kAlpha * atom::kAlpha * atom::kA0; return 8 * 3.14159265358979323846 / 3 * re * re / atom::kMpKg; }
}  // namespace

namespace ui {

namespace {
std::string p(const std::string& s) { return "<p class=\"lrnP\">" + s + "</p>"; }
std::string muted(const std::string& s) { return "<p class=\"lrnP lrnMuted\">" + s + "</p>"; }
}  // namespace

std::vector<BhLearnSection> blackholeLearnSections() {
    std::vector<BhLearnSection> v;
    v.push_back({"big", "🔎 What is on the picture?",
        p("A black hole worked out by the laws of general relativity, not spinning unless you set its spin (Controls › Spin), with no gas around it unless you switch on the model gas disc. You are held still at the distance shown on the card "
          "(round a spinning hole you are carried round by the turning space), looking straight at it. Behind it is our own sky as seen from Earth: ESA's Gaia map of the Milky Way and the 2,874 brightest stars of the app's catalogue.") +
        p("The black disc is the hole's <b>shadow</b>: every direction in it leads to the horizon, so no light comes from there. Round it the hole bends the light of the "
          "sky: the bright circle is the <b>Einstein ring</b>, the sky straight behind the hole spread into a ring, and inside it, hugging the shadow, the whole sky "
          "appears again and again, thinner each time.")});
    v.push_back({"exact", "✅ What is exact",
        p("<b>The shadow.</b> Its edge is Synge's formula for an observer held still (Perlick and Tsupko 2022, eq. 7), tested for every pixel.") +
        p("<b>The bending.</b> Every pixel's light path is the exact closed form of the non-spinning hole (Luminet 1979; Gralla and Lupsasca 2020), worked out "
          "in double precision and checked against a step-by-step ray integrator built from James et al. 2015. The picture looks it up in a table of 1,024 points "
          "for your distance; the table agrees with the closed form to better than 2 × 10⁻⁷ rad.") +
        p("<b>The doubled stars.</b> Every star has a first image outside the Einstein ring, a second one on the other side of the hole inside it, and fainter ones "
          "closer to the shadow (Riazuelo 2019). Each image is drawn as a point at its own position and magnified brightness, never as a stretched blob.")});
    v.push_back({"spin", "🌪️ Spin",
        p("Set <b>Spin</b> to give the hole angular momentum, as a share of the largest a black hole can have. Zero is the non-spinning hole, drawn by the closed "
          "forms above. With spin the horizon shrinks (to 1.14 GM/c² at 0.99), the shadow loses its round shape and shifts sideways: seen from the plane of the spin it is "
          "flattened on the side that moves towards you (Perlick and Tsupko 2022; Gralla and Lupsasca 2020), seen along the axis it stays round. The sky near it is "
          "dragged round, and the gas disc reaches further in, gets hotter there and turns more of the infalling mass into light (Bardeen, Press and Teukolsky 1972; "
          "Page and Thorne 1974).") +
        p("<b>How it is worked out.</b> Every light path round a spinning hole has a closed form in elliptic integrals and Jacobi functions (Gralla and "
          "Lupsasca 2020; Berens et al. 2026 use it for every pixel of a far-away camera): the processor works each path out from it in double precision, "
          "with no stepping, into a table for your distance, latitude and spin; the graphics card looks it up. The step-by-step integrator of James et al. 2015 "
          "stays as the reference the tests check the closed forms against. Which directions lead to the horizon: James et al. give a test from the light's "
          "constants alone, and the shadow's edge is drawn from it, within a thousandth of a pixel. On a graphics card with double precision in its shaders the card itself can follow "
          "the paths step by step (in double precision, checked against the processor's own when the lab first draws a spinning hole); otherwise the "
          "processor does it: a coarser table at once while you change a setting, the fine one a moment after you stop. Turning round the "
          "spin axis needs no new table. The stars' images are found by turning the table round (where it sends their light), with their magnified brightness.") +
        p("<b>Its limits.</b> The spin of Sagittarius A* and M87* is not measured: the control is yours and every preset starts at 0. The top, 0.99, is where the "
          "lab's tests stop: closer to the largest spin the light paths near the shadow change too fast for the tables it makes. Close to a spinning hole nothing "
          "can stand still: space is dragged round the axis and the camera, which keeps no angular momentum, is carried with it (the card says how fast). The "
          "picture is one instant; the sky's slow turning with time is not shown. The gas orbits with the spin, in the plane of the spin; a disc orbiting against "
          "it, a tilted disc and a warped one are not in the lab. Your own shift of colour and brightness is drawn ray by ray: light that comes round with the "
          "turning space reaches you less blue than light that comes against it (James and others 2015, eq. A.16).")});
    v.push_back({"disc", "🌀 The gas disc (a model)",
        p("Switch on <b>Gas disc (model)</b> to see the standard thin disc of gas (Novikov and Thorne; Page and Thorne 1974): gas going round the hole on circular orbits, "
          "heated by its own friction as it slowly spirals in, and shining from both faces. Such discs exist round quasars and round small black holes that feed from "
          "a companion star (Abramowicz and Fragile 2013; Yuan and Narayan 2014).") +
        p("<b>What is calculated.</b> How bright each ring of the disc is (Page and Thorne's formula: nothing at the inner edge, brightest at 9.55 GM/c², falling off far "
          "out), its temperature (each face a black body), the light's shift on the way to you from the gas's speed and from gravity (Luminet 1979), and where every "
          "pixel's light path meets the disc: over the hole, under it (the disc's underside, bent into view) and in thin rings closer to the shadow. Seen from near the "
          "disc's plane, those rings are thinner than a pixel: one ring along the shadow's edge then looks like a faint dotted curve that ends on the disc's straight edge. "
          "The curve is real: light that went round the hole once or more before reaching you (it is in the double-precision reference too). Its dotted look is not: it "
          "comes from the pixel grid, because each pixel looks along one ray and only some of them hit the thin ring. A camera that averages over each pixel would show "
          "a smooth, faint line. A black body seen "
          "shifted is still a black body, at the shifted temperature (Riazuelo 2019); with spin the shift is Cunningham 1975's; its colour and visible brightness come from the CIE 1931 eye, the same rule as "
          "the stars' colours. The side coming towards you is brighter. The picture is checked pixel by pixel against the double-precision reference.") +
        p("<b>The inner edge</b> is the innermost stable orbit, 6 GM/c² without spin (2.32 at spin 0.9): inside it the gas falls in and gives no light here. The <b>outer edge</b> is your choice: the "
          "model does not set it. The <b>feeding rate</b> is shown as the disc's light over the Eddington limit, and only between 1.5 % and 30 %, where the thin-disc "
          "model applies (Yuan and Narayan 2014; Abramowicz and Fragile 2013).") +
        p("<b>Blinding.</b> A real disc like this is blue-white and far brighter than the Sun's surface everywhere: the card says how many times, in visible light. "
          "The picture dims it so that the sky stays visible. A real disc this bright would drown out every star. The Real look draws it as a camera would: "
          "automatic exposure puts the disc's average brightness (its log-average) at middle grey and its brightest part at white, through a camera's "
          "response curve that rolls off the highlights (Reinhard et al. 2002), so the bright parts keep their shading and the side coming towards you "
          "stays brighter. The curve is our display choice; the colours and the light's ratios are the physics. Your choice moves the exposure in stops "
          "(each doubles the light) from automatic.") +
        p("<b>Sky beside the disc: True brightness</b> (the Real look) puts the stars on the disc's own scale: each catalogue star's measured V magnitude against the "
          "Sun's (" + fmt::precision(blackhole::kSunV, 4) + ", Willmer 2018), the Sun's surface as the yardstick, its light spread over the dot the app draws. Beside a bright disc the sky is then black: "
          "the sky cannot compete with the disc, not because there are no stars. Turn the exposure up by many stops and the stars come back while the disc burns out. "
          "The sky map is not drawn then: it has no calibrated brightness, and its own pixels are fainter than the stars it contains. Approximate: the V band taken as "
          "the eye's visible light, the Sun's surface as a black body at " + fmt::commas(sunfacts::kSunTeffK) + " K, the Sun's disc evenly bright.") +
        p("<b>Glare: as an eye sees it.</b> A very bright light scatters inside the eye and lays a veil of light round itself: this happens in the eye, not at the "
          "hole. It is drawn by the CIE's general disability glare equation (Vos and van den Berg, CIE 1999; as printed by van den Berg et al. 2013 and Bará and "
          "Bao-Varela 2023) for an average eye of 35 years with brown eyes, from 0.1° to 100° from each bright point. It moves light, it does not make any: the eye "
          "keeps " + fmt::precision(100 * (1 - blackhole::glareShare()), 3) + " % of each point's light within 0.1° and spreads the rest. Not drawn: the structure "
          "within 0.1° (each eye's own), the eye's adaptation, afterimages, coloured rings and streaks, and the change of scatter with colour. A camera's lens would "
          "show a different veil.") +
        p("<b>The model's limits.</b> Flat, thin, opaque, steady. No turbulence, hot spots, jets or wind; no light from inside the inner "
          "edge. Each face's light follows three rules from radiative transfer (below), each an approximation. The gas orbits with the hole's spin, in its plane. The disc lies in a plane the lab chose (parallel to the Earth's orbit, the gas going round as the planets do): its direction against the sky "
          "is not something measured. Sagittarius A* and M87* do not have such a disc; on them the lab labels it as a what-if.") +
        // 0.87.0 (native/PLAN_REVIEW.md R9-R11; blackhole/disc.h THE DISC'S REAL LIGHT, returning.h): the three rules drawn, each with its limits as the plan
        // states them. (0.77.0 to 0.86.0: "The lab draws neither" of the first two.)
        p("<b>The disc's real light</b> (Real). A real hot disc has an atmosphere where electrons scatter the light, and gravity bends some of its light back "
          "onto it. The lab draws three effects of that, each from a paper and each an approximation:") +
        p("<b>Bluer and dimmer: the colour correction.</b> Scattering makes the light come out harder than a black body at the gas's temperature: each ring sends "
          "the light of a black body f times hotter, dimmed f⁴ times, so its total light is unchanged (Done et al. 2012, their equations 1 and 2, used at each "
          "radius). f is 1 below 30,000 K, about 2.7 at 100,000 K and about 1.6 near 10 million K. So a small, hot disc looks a little bluer and dimmer in visible "
          "light; a cool disc does not change. For stellar-mass discs Shimura and Takahara 1995 found f near 1.7 at a tenth of the Eddington rate. Limits: an "
          "approximate rule fitted to radiative-transfer models of discs round holes that do not spin; the card gives f at the hottest ring.") +
        p("<b>Darker towards the edge: limb darkening.</b> The face of an atmosphere that only scatters off electrons is about three times brighter seen "
          "straight on than seen at a grazing angle (Chandrasekhar 1946, Table 2: 1.2628 against 0.4151). Each ray's angle to the disc's face is taken in the "
          "moving gas's own frame. The law is scaled so that each face still sends the thin disc's flux. Limits: grey, pure electron scattering, deep and "
          "plane-parallel; no absorption and not the disc's real layers; the table is the paper's third approximation (it sends 0.991 of the flux, so the lab "
          "scales it up); between its 11 points a cubic (a choice); the polarisation it gives is not drawn; on the thick disc (at every spin) the angle is to the plane, "
          "not to the tilted surface.") +
        p("<b>Light that comes back: returning radiation.</b> Gravity bends some of the disc's light back onto the disc, which takes it in and sends it out "
          "again, so the disc is hotter than its own heat makes it (Cunningham 1976, equations 7a and 8a). The lab follows the light paths from 32 rings, "
          "32,768 paths each, once per spin (on the graphics card when its light paths are on), and checks them against Cunningham's Tables 1 and 2 at spins 0.9 "
          "and 0.99. Without spin the returned light is about 2 % of the disc's own (about 0.5 % in temperature: nothing to see); at spin 0.99 it is about 15 % "
          "(about 4 % in temperature), at every radius. Limits: a thin, steady disc; the returned light taken in and sent out again (Cunningham's assumption); "
          "its push on the gas (his equation 8b) left out; the falling gas inside the innermost orbit dark; the light sent equally in every direction for this "
          "sum (Cunningham: limb darkening changes it by a few percent). Until a new spin's sum is made the disc is drawn without it.") +
        p("<b>Drawn: your own place.</b> Held still near the hole, you see all the light that falls in towards you bluer and brighter, the disc and the stars alike: "
          "each colour temperature times the card's factor, the light by its fourth power (Riazuelo 2019, eqs. 52, 53). Without spin it is one factor for the whole "
          "picture; round a spinning hole each ray has its own. Exact for a camera held still (not falling, not orbiting). The faint sky map is not shifted: it has no "
          "colour temperature. Each star is shifted as a black body at the temperature Ballesteros's (2012) formula gives for its B−V. For blue B and O stars that is "
          "well below their surface temperature (a B0 star: 16,600 K against 31,400 K, Mamajek's table after Pecaut & Mamajek 2013), so their shifted light is not exact: that needs their "
          "measured spectra. The automatic exposure follows, so the picture is not simply brighter: it is bluer, and the card says how much brighter it really is.")});
    {   // 0.55.3: the Film look, one row per choice (blackhole/looks.h filmRows: the physics, the film's choice, the paper's reason, where in the paper)
        std::string h = p("Controls › Look › <b>Film</b> draws the choices the makers of the film Interstellar made for its black hole, as James, von Tunzelmann, Franklin and "
                          "Thorne (2015) state them, on the same calculated hole and the same light paths as the Real look. One row per choice:");
        for (const blackhole::FilmRow& r : blackhole::filmRows())
            h += "<p class=\"lrnP\"><b>" + r.physics + "</b><br>The film: " + r.choice + ".<br>Why: " + r.reason + ".<br><small>" + r.where + "</small></p>";
        h += p("<b>The lab's own, not the film's.</b> The paper calls the film's disc \"physically thin and marginally optically thick\" (section 4.1.1). It does not "
               "publish how much light gets through, nor the disc's texture or edges, so the Film look draws the paper's thin disc opaque, with hard edges at the "
               "paper's radii. The lab adds only the moving gas's streaks and the clump, when they are on (see Moving gas). The film's lens flare (a soft glow) is "
               "not drawn: it is the measured blur of the IMAX lenses, which is not published (section 4.3.1).") +
             p("<b>Not in the app:</b> the artists' textures (fractal noise in the close-ups), the film's volume disc (built in Houdini, about 17 million voxels), the "
               "real lens flare (the measured blur of the IMAX lenses) and the film's colour grading are not published (James et al. 2015, appendix A.6 and section "
               "4.3.1). The paper gives no mass for the film's black hole.") +
             p("<b>The Real look is still a model</b>, not a photograph: the standard thin disc, with every shift it calculates. A real disc would be blinding; Glare "
               "draws the veil an average eye would see.");
        v.push_back({"film", "🎬 What the film changed", h});
    }
    v.push_back({"moving", "⏱️ Moving gas",
        p("With the disc on, the gas moves (Controls › Gas disc: <b>Streaks and clump</b> Off or On, then <b>Pause</b> and the rate; the rest under More settings › Moving gas). "
          "Each ring goes round at its true speed, Ω = 1 / (r^3/2 + a) in the hole's units (Bardeen, Press and Teukolsky 1972): near the hole more than half "
          "the speed of light. One turn of the innermost gas takes about half an hour round Sagittarius A*, about a month round M87* and a few thousandths "
          "of a second round a hole of 10 Suns; the rate sets how fast the film runs, from real time to much faster or slower.") +
        p("<b>What is real.</b> The speeds; the clocks drawn on the picture: far away (the hole's own time), on the clump (a clock riding with the gas runs slow, "
          "by α (1 − V²)^1/2, Cunningham 1975) and yours when it differs by more than 1 %; the bending of the light and its shifts of colour and brightness; the "
          "time the light takes to reach you, worked out exactly for every pixel (Gralla and Lupsasca 2020): each part of the picture shows the gas as it was when "
          "its light left, so the far side and the second image under the hole are older than the near side; and the shearing of the gas into trailing spirals, "
          "because the inner rings go round faster.") +
        p("<b>What is a model.</b> No telescope has seen the face of such a disc. The bright clump is the model astronomers fit to the flares of Sagittarius A* "
          "(Broderick and Loeb 2006; GRAVITY Collaboration 2018; Wielgus et al. 2022): its place, size and brightness are the lab's choices, and it keeps its "
          "shape as in that model, where real gas would shear it into an arc. The streaks follow computer simulations of magnetised discs: magnetic "
          "turbulence makes long thin filaments, tilted about 15° from the orbit and trailing (Guan et al. 2009), about 25° long (Schnittman, Krolik and Hawley 2006) "
          "and 0.18 of that across (Guan et al.); the lab draws them as a random field with that shape (Lee and Gammie 2021), renewed every 0.3 of an orbit "
          "(Schnittman et al.). Where each streak is, is drawn at random; their strength is set so that the ring from 6 to 10 GM/c² flickers by the 43.8 % of "
          "Schnittman et al.'s Table 2. In the Film look both are added by the lab: the film's own disc texture is not published and James et al. 2015 does not "
          "say how it moved.") +
        p("<b>The disc's thickness</b> (Real). The disc is drawn at its true thickness at every spin: its surface sits at the height h where the pressure "
          "of its own light (near the hole) and of its gas (far out) holds it up against the hole's pull, from the vertical balance of Zhu et al. 2012 (their "
          "appendix B, one zone in height). At a tenth of the Eddington limit it is at most about 1.5 % of its radius thick, near 20 GM/c²: the bright disc of a feeding "
          "hole really is nearly flat, which is why it shows only as a slightly fuller inner rim. It thins to nothing at the innermost stable orbit, where the gas "
          "stops making heat, instead of ending at a sharp line. Limits: one zone in height; the surface drawn opaque at h; the gas at the surface moving as in the "
          "plane; its darkening towards the edge taken against the plane, not the tilted surface; no light from inside the innermost orbit (simulations put about 4 % of the light there). Round a spinning "
          "hole the thickness takes the spin's own term in the hole's pull (Abramowicz and Fragile 2013, eq. 92, the form that gives their vertical "
          "frequency, eq. 40). There each ray's meeting with the surface is found from its exact path in closed form near where it crosses the plane "
          "(Gralla and Lupsasca 2020); its colour is worked out at the distance from the axis where it meets the surface. The direct image and the one of the "
          "disc's underside meet the surface; the thin rings of light that circled the hole more than once are drawn where they cross the plane, at every spin. "
          "A ray that grazes the surface without crossing the plane near it is not drawn on it.") +
        p("<b>Two opacities, each from its own source.</b> The Eddington limit (the feeding rate's yardstick) uses the electron-scattering opacity " +
          fmt::precision(blackhole::kKappaEs * 10, 2) + " cm²/g, the value Yuan and Narayan 2014 give as the usual one: it is that of ionised pure hydrogen, "
          "σ_T / m_p = " + fmt::precision(kappaHydrogen() * 10, 4) + " cm²/g (CODATA 2022), rounded. The thickness uses Zhu et al.'s own " +
          fmt::precision(blackhole::kZhuKappa * 10, 4) + " cm²/g, for their gas of 70 % hydrogen and 30 % helium by mass (electron scattering in such a gas "
          "is (1 + X) / 2 of hydrogen's, X = 0.7: " + fmt::precision(kappaHydrogen() * 10 * 0.85, 4) + " cm²/g). Each number follows the paper whose formula "
          "it enters, so the two are not made equal.") +
        p("<b>How it is drawn.</b> The graphics card works out, for every pixel, where on the disc its light left and when, from the same closed forms, then the "
          "surface's height there, then the clump and the streaks; streaks finer than a pixel are drawn as their average over the pixel. The processor keeps the same steps in double "
          "precision: the lab checks the card against it when it first draws, and uses it where the card cannot.")});
    v.push_back({"soft", "〰️ What is soft",
        p("<b>The sky map.</b> Gaia's map has about 0.09° per map pixel. Near the Einstein ring the hole magnifies the sky many times, so the map's own blur "
          "shows there: that is the map's sharpness, not the physics. Close to the shadow's edge the whole sky is squeezed into rings thinner than a pixel; the "
          "picture shows their average colour.") +
        p("<b>The graphics card</b> draws in single precision, from the table the processor fills in double precision. Measured against the double-precision reference "
          "(1280 × 800, three distances, REPORT of 0.55.0): the shadow's edge lies within a millionth of a pixel of where it should be, and every pixel's bent "
          "direction is right to about 5 millionths of a radian (a map pixel is 1,600 millionths), even right at the edge.") +
        p("<b>Very bright images.</b> A star exactly behind the hole would become a ring of light, and its images near the ring are magnified without limit. "
          "A drawn dot cannot show that: no image is drawn brighter than the brightest star of the sky seen without the hole (Sirius).")});
    v.push_back({"limits", "⚠️ What the lab leaves out",
        p("<b>Spin</b> up to 0.99 of the largest (see Spin above). Their gas: the lab has the standard thin disc as a model (above), nothing else.") +
        p("<b>The sky is ours.</b> It is the sky seen from Earth, placed behind the hole. Nobody has a map of the sky as it looks from the centre of our galaxy or "
          "from M87: one would have to be invented.") +
        p("<b>You are held still.</b> Staying still that close to a black hole would take a powerful rocket. Without spin you hover; round a spinning hole you are "
          "held at your distance and carried round by the turning space (an observer with no angular momentum). Your clock runs slow, and the light reaches you "
          "bluer and brighter: the picture draws it. Dragging takes you round the hole "
          "as the Orbit Lab's camera goes round a planet, and you glide a little when you let go, but at each place you are held still again: you are not falling or "
          "orbiting. A camera that orbits or falls in sees a different picture (its own motion shifts and crowds the light); the lab does not draw that.") +
        p("<b>Sagittarius A* and M87*.</b> Only their mass and distance are measured (GRAVITY Collaboration 2022; the Event Horizon Telescope 2019 and 2022). "
          "Their spin is not measured. Their real gas is a faint, very hot fog that shines mostly in radio light, not a bright disc. The famous telescope "
          "pictures are radio maps at 1.3 mm drawn in chosen colours, and their bright ring need not lie exactly on the shadow's edge.")});
    v.push_back({"unit", "📏 The hole's own unit",
        p("Distances are in <b>GM/c²</b>: the hole's mass in units of length (G the constant of gravity, c the speed of light). The horizon of a non-spinning hole "
          "is at 2 of these, the photon orbit, where light can circle the hole, at 3. In this unit every non-spinning black hole looks the same, so the picture does "
          "not change with the mass; only the sizes in kilometres do.") +
        muted("Every paper named here is listed with its address in About › Where the facts come from.")});
    return v;
}

void blackholeLearnWindow(AppState& app) {
    if (app.bhLearn != 1) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x * 0.6f, 44 * ImGui::GetFontSize()), vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.4f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (pageBegin("What is real in this picture?###bhlearn", &open)) {
        if (app.bhLearnJump == "top") { ImGui::SetScrollY(0); app.bhLearnJump.clear(); }
        if (const char* sc = std::getenv("PL_LEARN_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
        ImGui::PushTextWrapPos(0);
        const auto secs = blackholeLearnSections();
        for (std::size_t i = 0; i < secs.size(); i++) {
            if (i) { ImGui::Spacing(); ImGui::Separator(); }
            richHeading(secs[i].title);
            htmlView(secs[i].html);
        }
        ImGui::PopTextWrapPos();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) open = false;
    }
    pageEnd();
    if (!open) app.bhLearn = 0;
}

}  // namespace ui
