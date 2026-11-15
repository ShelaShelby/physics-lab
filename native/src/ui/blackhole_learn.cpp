#include "blackhole_learn.h"

#include <cstdlib>

#include "imgui.h"
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
        p("A black hole that does not spin and has no gas around it, worked out by the laws of general relativity. You hover at the distance shown on the card, "
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
        p("<b>Spin and gas.</b> Real black holes spin and have gas around them. Neither is in the lab yet.") +
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
