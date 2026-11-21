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

namespace ui {

namespace {
std::string p(const std::string& s) { return "<p class=\"lrnP\">" + s + "</p>"; }
std::string muted(const std::string& s) { return "<p class=\"lrnP lrnMuted\">" + s + "</p>"; }
}  // namespace

std::vector<BhLearnSection> blackholeLearnSections() {
    std::vector<BhLearnSection> v;
    v.push_back({"big", "🔎 What is on the picture?",
        p("A black hole worked out by the laws of general relativity, not spinning unless you set its spin (Controls › Spin), with no gas around it unless you switch on the model gas disc. You hover at the distance shown on the card, "
          "looking straight at it. Behind it is our own sky as seen from Earth: ESA's Gaia map of the Milky Way and the 2,874 brightest stars of the app's catalogue.") +
        p("The black disc is the hole's <b>shadow</b>: every direction in it leads to the horizon, so no light comes from there. Round it the hole bends the light of the "
          "sky: the bright circle is the <b>Einstein ring</b>, the sky straight behind the hole spread into a ring, and inside it, hugging the shadow, the whole sky "
          "appears again and again, thinner each time.")});
    v.push_back({"exact", "✅ What is exact",
        p("<b>The shadow.</b> Its edge is Synge's formula for a hovering observer (Perlick and Tsupko 2022, eq. 7), tested for every pixel.") +
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
          "it, a tilted disc and a warped one are not in the lab. Your own shift of colour and brightness is still a number on the card, not drawn.")});
    v.push_back({"disc", "🌀 The gas disc (a model)",
        p("Switch on <b>Gas disc (model)</b> to see the standard thin disc of gas (Novikov and Thorne; Page and Thorne 1974): gas going round the hole on circular orbits, "
          "heated by its own friction as it slowly spirals in, and shining from both faces. Such discs exist round quasars and round small black holes that feed from "
          "a companion star (Abramowicz and Fragile 2013; Yuan and Narayan 2014).") +
        p("<b>What is calculated.</b> How bright each ring of the disc is (Page and Thorne's formula: nothing at the inner edge, brightest at 9.55 GM/c², falling off far "
          "out), its temperature (each face a black body), the light's shift on the way to you from the gas's speed and from gravity (Luminet 1979), and where every "
          "pixel's light path meets the disc: over the hole, under it (the disc's underside, bent into view) and in thin rings closer to the shadow. A black body seen "
          "shifted is still a black body, at the shifted temperature (Riazuelo 2019); with spin the shift is Cunningham 1975's; its colour and visible brightness come from the CIE 1931 eye, the same rule as "
          "the stars' colours. The side coming towards you is brighter. The picture is checked pixel by pixel against the double-precision reference.") +
        p("<b>The inner edge</b> is the innermost stable orbit, 6 GM/c² without spin (2.32 at spin 0.9): inside it the gas falls in and gives no light here. The <b>outer edge</b> is your choice: the "
          "model does not set it. The <b>feeding rate</b> is shown as the disc's light over the Eddington limit, and only between 1.5 % and 30 %, where the thin-disc "
          "model applies (Yuan and Narayan 2014; Abramowicz and Fragile 2013).") +
        p("<b>Blinding.</b> A real disc like this is blue-white and far brighter than the Sun's surface everywhere: the card says how many times, in visible light. "
          "The picture dims it (the exposure; automatic draws the brightest part at full brightness) so that the sky stays visible. A real disc this bright would "
          "drown out every star.") +
        p("<b>Sky beside the disc: True brightness</b> (the Real look) puts the stars on the disc's own scale: each catalogue star's measured V magnitude against the "
          "Sun's (" + fmt::precision(blackhole::kSunV, 4) + ", Willmer 2018), the Sun's surface as the yardstick, its light spread over the dot the app draws. Beside a bright disc the sky is then black: "
          "the sky cannot compete with the disc, not because there are no stars. Turn the exposure far down and the stars come back while the disc burns out. "
          "The sky map is not drawn then: it has no calibrated brightness, and its own pixels are fainter than the stars it contains. Approximate: the V band taken as "
          "the eye's visible light, the Sun's surface as a black body at " + fmt::commas(sunfacts::kSunTeffK) + " K, the Sun's disc evenly bright.") +
        p("<b>Glare: as an eye sees it.</b> A very bright light scatters inside the eye and lays a veil of light round itself: this happens in the eye, not at the "
          "hole. It is drawn by the CIE's general disability glare equation (Vos and van den Berg, CIE 1999; as printed by van den Berg et al. 2013 and Bará and "
          "Bao-Varela 2023) for an average eye of 35 years with brown eyes, from 0.1° to 100° from each bright point. It moves light, it does not make any: the eye "
          "keeps " + fmt::precision(100 * (1 - blackhole::glareShare()), 3) + " % of each point's light within 0.1° and spreads the rest. Not drawn: the structure "
          "within 0.1° (each eye's own), the eye's adaptation, afterimages, coloured rings and streaks, and the change of scatter with colour. A camera's lens would "
          "show a different veil.") +
        p("<b>The model's limits.</b> Flat, thin, opaque, each face a perfect black body, steady. No turbulence, hot spots, jets or wind; no light from inside the inner "
          "edge; no light of the disc falling back on itself; each face shines the same in every direction. The gas orbits with the hole's spin, in its plane. The disc lies in a plane the lab chose (parallel to the Earth's orbit, the gas going round as the planets do): its direction against the sky "
          "is not something measured. Sagittarius A* and M87* do not have such a disc; on them the lab labels it as a what-if.") +
        p("<b>Not drawn:</b> your own hovering shifts all the light you see, sky and disc alike, bluer and brighter; the card gives that as a number, as for the sky.")});
    {   // 0.55.3: the Film look, one row per choice (blackhole/looks.h filmRows: the physics, the film's choice, the paper's reason, where in the paper)
        std::string h = p("Controls › Look › <b>Film</b> draws the choices the makers of the film Interstellar made for its black hole, as James, von Tunzelmann, Franklin and "
                          "Thorne (2015) state them, on the same calculated hole and the same light paths as the Real look. One row per choice:");
        for (const blackhole::FilmRow& r : blackhole::filmRows())
            h += "<p class=\"lrnP\"><b>" + r.physics + "</b><br>The film: " + r.choice + ".<br>Why: " + r.reason + ".<br><small>" + r.where + "</small></p>";
        h += p("<b>Not in the app:</b> the artists' textures, the real lens flare (the measured blur of the IMAX lenses) and the film's colour grading are not published. "
               "The glow is the app's own, a simple blur of the disc's bright parts (the sky and the stars are not part of it). The paper gives no mass for the film's black hole.") +
             p("<b>The Real look is still a model</b>, not a photograph: the standard thin disc, with every shift it calculates. A real disc would be blinding; Glare "
               "draws the veil an average eye would see.");
        v.push_back({"film", "🎬 What the film changed", h});
    }
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
        p("<b>You hover.</b> Staying still that close to a black hole would take a powerful rocket. Your clock runs slow, and the starlight reaches you bluer and "
          "brighter: the card gives these as numbers, but the picture does not draw the change of colour or brightness.") +
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
