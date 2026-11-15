// 0.55.0: the Black Hole Lab's shell: its chip (what is on screen, and the one line that says what kind of picture it is), its Controls panel (the preset,
// the mass, the camera, the light), its readout card and its bottom bar. Every number shown is the physics module's (blackhole/lab.h readout); this file
// only formats them.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "blackhole/lab.h"
#include "blackhole/presets.h"
#include "imgui.h"
#include "orbit/format.h"
#include "ui/look.h"
#include "ui/realest.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"
#include "ui/style_draw.h"

namespace ui::shellimpl {

using namespace sh;
using app::shell::Metrics;
using app::shell::Rect;

namespace {

std::string sig(double v, int digits) { return fmt::precision(v, digits); }
// a length in km: "12.7 million km", "1.92 billion km", "42,300 km"
std::string kmText(double km) {
    if (km >= 1e12) return sig(km / 1e12, 3) + " trillion km";
    if (km >= 1e9) return sig(km / 1e9, 3) + " billion km";
    if (km >= 1e6) return sig(km / 1e6, 3) + " million km";
    return fmt::commas(km) + " km";
}
std::string auText(double au) { return (au >= 0.01 ? sig(au, 3) : sig(au, 2)) + " AU"; }
std::string massText(double m) {
    if (m >= 1e9) return sig(m / 1e9, 4) + " billion Suns";
    if (m >= 1e6) return sig(m / 1e6, 4) + " million Suns";
    return fmt::commas(m) + (m == 1 ? " Sun" : " Suns");
}
std::string distText(double d) { return sig(d, d < 10 ? 3 : 4) + " GM/c²"; }

// the preset's one sentence (owner's decision of 15 Nov 2026: mass and distance only; spin not measured; its real gas is not shown)
std::string presetSentence(const std::string& key) {
    if (key == "sgra") return "The black hole at the centre of our galaxy. Its real gas is a faint, very hot fog that shines mostly in radio light, not a bright disc, and is not shown here; its spin is not measured (this lab's holes do not spin).";
    if (key == "m87") return "The black hole at the centre of the galaxy M87, pictured by the Event Horizon Telescope in 2019. Its real gas is a faint, very hot fog, not a bright disc, and is not shown here; its spin is not measured (this lab's holes do not spin).";
    if (key == blackhole::kFeedingKey) return "A model, not one particular object: a black hole of 100 million Suns feeding from a thin, bright disc of gas. Such discs exist round quasars and round small black holes that feed from a companion star.";
    return "";
}
// 0.55.1: the label on a real hole with the disc switched on (owner's decision of 15 Nov 2026)
const char* kWhatIf = "What a bright disc would look like here; this black hole does not have one.";
std::string tempText(double K) {
    if (K >= 1e6) return sig(K / 1e6, 3) + " million K";
    return fmt::commas(std::round(K / (K >= 1e5 ? 1000 : 100)) * (K >= 1e5 ? 1000 : 100)) + " K";
}
std::string timesText(double v) { return v >= 100 ? fmt::commas(std::round(v)) : sig(v, v >= 1 ? 3 : 2); }

// a typed number: applied when it reads as a number inside [lo, hi]; the field keeps what was typed
bool typedNumber(const char* id, char* buf, std::size_t n, const char* hint, float w, double lo, double hi, double& out) {
    if (!inputField(id, buf, n, hint, w)) return false;
    char* end = nullptr;
    const double v = std::strtod(buf, &end);
    if (end == buf || !std::isfinite(v) || v < lo || v > hi) return false;
    while (*end == ' ') end++;
    if (*end) return false;
    out = v;
    return true;
}

}  // namespace

ChipInfo blackholeChip(const AppState& app) {
    ChipInfo c;
    const blackhole::Readout r = blackhole::readout(app.blackhole);
    c.title = "Black hole  ·  " + (r.hasPreset ? r.presetName : massText(r.massSun));
    c.sub = "A calculated model: no spin, no gas  ·  our sky, seen from Earth";   // the lab's one line of what kind of picture this is (the rest: the guide)
    if (app.blackhole.disc && app.blackhole.bend)
        c.sub = r.realPreset ? std::string(kWhatIf) : "A calculated model: no spin; a thin gas disc (model), dimmed  ·  our sky, seen from Earth";
    c.play = false;
    return c;
}

std::string blackholeSubject(const AppState& app) {
    const blackhole::Readout r = blackhole::readout(app.blackhole);
    return r.hasPreset ? r.presetName : "Black hole";
}

void blackholeCard(AppState& app, Panel& p) {
    const blackhole::Readout r = blackhole::readout(app.blackhole);
    pStat(p, "Mass", massText(r.massSun) + (r.realPreset ? " (measured)" : r.hasPreset ? " (the model's)" : " (your choice)"));
    pStat(p, "Horizon radius", kmText(r.horizonKm) + "  ·  " + auText(r.horizonAU));
    pStat(p, "Your distance", distText(r.dist) + "  ·  " + kmText(r.distKm) + "  ·  " + auText(r.distAU));
    char b[160];
    std::snprintf(b, sizeof b, "%.4g° across (%.3g %% of your sky)", r.shadowDeg, 100 * r.skyShare);
    pStat(p, "Shadow on your sky", b);
    std::snprintf(b, sizeof b, "%.4g × the rate of a clock far away", r.clockRate);
    pStat(p, "Your clock", b);
    std::snprintf(b, sizeof b, "%.4g × bluer, %.4g × brighter (not drawn)", r.blueshift, r.brightening);
    pStat(p, "Starlight reaching you", b);
    if (r.dist < r.photonOrbitM) pCaption(p, "You are inside the photon orbit (" + sig(r.photonOrbitM, 2) + " GM/c²): the shadow covers more than half your sky.");
    if (r.disc) {                                                           // 0.55.1: the disc's numbers (blackhole/lab.h readout, the view's discLook)
        const blackhole::DiscLook& L = app.blackholeView.stats().look;
        if (r.realPreset) pCaption(p, kWhatIf, false);
        pStat(p, "Disc's inner edge", sig(r.rIn, 3) + " GM/c²  ·  " + kmText(r.rInKm) + " (nothing shines inside it)");
        pStat(p, "Hottest ring", tempText(r.tHot) + " at " + sig(r.rHot, 3) + " GM/c²  ·  " + kmText(r.rHotKm));
        if (!app.blackhole.bend) pCaption(p, "Gravity bends light: Off: the disc is not drawn (it needs the hole).");
        else if (L.any) {
            pStat(p, "Brightest part", timesText(L.brightY) + " × the Sun's surface in visible light");
            pStat(p, "Exposure", std::string("screen white = ") + timesText(L.white) + " × the Sun's surface" + (app.blackhole.expAuto ? " (automatic)" : " (your choice)"));
        } else pCaption(p, "The disc is out of the picture from here.");
        char d[260];
        std::snprintf(d, sizeof d, "bhdisc on=1 feed=%.6g outer=%.6g rin=%.6g thot=%.6g rhot=%.6g bright=%.6g white=%.6g any=%d auto=%d", r.feed, r.rOut, r.rIn, r.tHot, r.rHot, L.brightY, L.white,
                      L.any ? 1 : 0, app.blackhole.expAuto ? 1 : 0);
        auditNote(d);
    } else auditNote("bhdisc on=0");
    if (r.hasPreset && r.realPreset) {
        std::snprintf(b, sizeof b, "the shadow %.3g µas across; the telescope's ring %.3g ± %.2g µas", r.earthShadowMuas, r.ringMuas, r.ringErr);
        pStat(p, "Seen from Earth", b);
        pCaption(p, "The ring is what the Event Horizon Telescope measured at 1.3 mm: light of hot gas round the hole. It need not lie exactly on the shadow's edge (EHT M87* paper I, section 6).");
    }
    auditNote("bhcard mass=" + sig(r.massSun, 6) + " dist=" + sig(r.dist, 6) + " shadow=" + sig(r.shadowDeg, 6) + " clock=" + sig(r.clockRate, 6) + " blue=" + sig(r.blueshift, 6) +
              " bright=" + sig(r.brightening, 6) + " horizonkm=" + sig(r.horizonKm, 6) + (r.realPreset ? " preset=" + app.blackhole.preset + " earth=" + sig(r.earthShadowMuas, 6) + " ring=" + sig(r.ringMuas, 4) :
              r.hasPreset ? " preset=" + app.blackhole.preset : std::string(" preset=none")));
}

Rect blackholeBar(AppState& app, const app::shell::Frame& fr, const Metrics& m) {
    blackhole::Settings& s = app.blackhole;
    const double W = fr.window.w, H = fr.window.h;
    const auto widthOf = [&](const std::string& t) { return sh::width(Type::Body, false, t) + 2.0 * m.s12; };
    const char* bendName = "Bending";
    const char* resetName = "Reset view";
    const double bw = widthOf(bendName), rw = widthOf(resetName), total = 2.0 * m.s8 + bw + m.s12 + rw;
    const double freeW = fr.freeW > 0 ? fr.freeW : W;
    const double x = std::max<double>(m.s12, std::min(freeW / 2 - total / 2, freeW - m.s12 - total));
    const Rect r{x, H - m.s12 - m.toolbarH, total, static_cast<double>(m.toolbarH)};
    Panel p;
    if (beginPanel("##blackholeBar", r, p, Shape::Pill, false)) {
        float bx = p.origin.x + m.s8;
        const float by = p.origin.y + (m.toolbarH - m.control) / 2.0f, ch = static_cast<float>(m.control);
        if (toolToggle("blackhole.bar.bend", ImVec2(bx, by), ImVec2(static_cast<float>(bw), ch), bendName, s.bend)) s.bend = !s.bend;
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", s.bend ? "Gravity bends light (on). Click to see the same sky without the hole's bending." : "Gravity bends light: off (the plain sky, to compare). Click to turn it on.");
        bx += static_cast<float>(bw + m.s12);
        if (button("blackhole.bar.reset", ImVec2(bx, by), ImVec2(static_cast<float>(rw), ch), resetName, Face::Quiet)) blackhole::resetView(s);
        if (ImGui::IsItemHovered()) ui::SetTooltip("Back to the first view: distance, field of view and direction (key R)");
        endPanel(p);
    }
    char b[320];
    std::snprintf(b, sizeof b, "blackhole preset=%s dist=%.6g fov=%.6g yaw=%.6f pitch=%.6f bend=%d sky=%s disc=%d feed=%.6g outer=%.6g exp=%s", s.preset.empty() ? "none" : s.preset.c_str(), s.dist, s.fovDeg,
                  s.yaw, s.pitch, s.bend ? 1 : 0, s.skyOurs ? "ours" : "black", s.disc ? 1 : 0, s.feed, s.outer, s.expAuto ? "auto" : fmt::precision(s.expLog, 6).c_str());
    auditNote(b);
    return r;
}

void blackholeControls(AppState& app, Panel& p, const SectionFn& section) {
    blackhole::Settings& s = app.blackhole;
    const Metrics& m = F().m;
    if (section("preset", "Black hole", true)) {
        int n = 0;
        const blackhole::Preset* ps = blackhole::presets(n);
        for (int i = 0; i < n; i++) {
            const std::string id = std::string("blackhole.preset.") + ps[i].key;
            if (pRow(p, id.c_str(), ps[i].name, s.preset == ps[i].key)) blackhole::applyPreset(s, ps[i].key);
        }
        if (pRow(p, "blackhole.preset.feeding", blackhole::kFeedingName, s.preset == blackhole::kFeedingKey)) blackhole::applyPreset(s, blackhole::kFeedingKey);
        if (s.preset == blackhole::kFeedingKey) pCaption(p, presetSentence(s.preset));
        else if (!s.preset.empty()) {
            pCaption(p, presetSentence(s.preset));
            const blackhole::Preset* q = blackhole::preset(s.preset.c_str());
            char b[200];
            std::snprintf(b, sizeof b, "Measured: mass %s (± %s), distance %s pc (± %s). Source: %s.", massText(q->massSun).c_str(), massText(q->massErr).c_str(), fmt::commas(q->distPc).c_str(), fmt::commas(q->distErr).c_str(), q->source);
            pCaption(p, b);
        } else pCaption(p, "Your own mass: no real black hole is chosen.");
    }
    if (section("mass", "Mass", true)) {
        pStat(p, "Mass", massText(s.massSun));
        float t = static_cast<float>(blackhole::massToSlider(s.massSun));
        if (pSlider(p, "blackhole.mass", &t, 0.0f, 1.0f)) {
            const double v = blackhole::sliderToMass(t), q = std::pow(10.0, std::floor(std::log10(v)) - 2);   // three significant digits
            blackhole::setMass(s, std::round(v / q) * q);
        }
        pCaption(p, "Log scale: " + massText(blackhole::kMassMin) + " to " + massText(blackhole::kMassMax) + ". Or type a mass in Suns:");
        static char typed[32] = "";
        ImGui::SetCursorScreenPos(here(p));
        double v;
        if (typedNumber("##blackhole.mass.text", typed, sizeof typed, "e.g. 4.297e6", p.w, blackhole::kMassMin, blackhole::kMassMax, v)) blackhole::setMass(s, v);
        p.y += m.control + m.s4;
        pCaption(p, "The picture does not change with the mass. Every length round a black hole grows in step with its mass (the unit is GM/c²), so at the same distance in "
                    "that unit a light and a heavy hole look exactly alike: only the sizes in km and the numbers on the card change.");
    }
    if (section("camera", "Camera", true)) {
        pStat(p, "Distance", distText(s.dist) + "  ·  " + kmText(blackhole::readout(s).distKm));
        float t = static_cast<float>(blackhole::distToSlider(s.dist));
        if (pSlider(p, "blackhole.dist", &t, 0.0f, 1.0f)) {
            const double v = blackhole::sliderToDist(t), q = std::pow(10.0, std::floor(std::log10(v)) - 2);
            blackhole::setDistance(s, std::round(v / q) * q);
        }
        char hint[96];
        std::snprintf(hint, sizeof hint, "Log scale: %g to %g GM/c². Or type one:", blackhole::kDistMin, blackhole::kDistMax);
        pCaption(p, hint);
        static char typed[32] = "";
        ImGui::SetCursorScreenPos(here(p));
        double v;
        if (typedNumber("##blackhole.dist.text", typed, sizeof typed, "e.g. 6", p.w, blackhole::kDistMin, blackhole::kDistMax, v)) blackhole::setDistance(s, v);
        p.y += m.control + m.s4;
        {   const blackhole::Readout r = blackhole::readout(s);
            pCaption(p, "The horizon is at " + sig(r.horizonM, 3) + " GM/c², the photon orbit at " + sig(r.photonOrbitM, 3) + " (inside it the shadow covers more than half the sky). The closest here, " +
                        sig(blackhole::kDistMin, 3) + ", is the closest the lab's light paths are tested at."); }
        char b[64];
        std::snprintf(b, sizeof b, "%.0f° across the picture's height", s.fovDeg);
        pStat(p, "Field of view", b);
        float f = static_cast<float>(s.fovDeg);
        if (pSlider(p, "blackhole.fov", &f, static_cast<float>(blackhole::kFovMin), static_cast<float>(blackhole::kFovMax))) blackhole::setFov(s, std::round(f));
        {   // the shadow fills the picture (close in, a narrow view): say how to see the sky round it
            const double cx = app.bhFreeW > 0 ? app.bhFreeX + app.bhFreeW / 2 : app.canvasW / 2, cy = app.bhFreeH > 0 ? app.bhFreeY + app.bhFreeH / 2 : app.canvasH / 2;
            const blackhole::HoleView hv = blackhole::holeView(s, app.canvasW, app.canvasH, cx, cy);
            double far = 0;
            for (double x : {0.0, app.canvasW}) for (double y : {0.0, app.canvasH}) far = std::max(far, std::hypot(x - cx, y - cy));
            if (s.bend && blackhole::shadowAngle(s.dist) * hv.K > far) pCaption(p, "The shadow fills the whole picture: widen the field of view to see the sky round it.", false);
        }
        if (pButton(p, "blackhole.reset", "Reset view", Face::Quiet)) blackhole::resetView(s);
        pCaption(p, "Drag to go round the hole (it stays in the middle; the sky behind it changes); the wheel or + − move you closer or farther; R resets the view; 1 2 3 4 switch lab. You hover in place.");
    }
    if (section("light", "Light", true)) {
        pHeading(p, "Gravity bends light");
        int cur = s.bend ? 0 : 1;
        real::markLabels("blackhole", "bend", {"On", "Off"});
        const std::vector<std::string> bn{"blackhole.bend.on", "blackhole.bend.off"};
        if (pSegmented(p, "blackhole.bend", {"On", "Off"}, &cur, &bn)) s.bend = cur == 0;
        if (!s.bend) pCaption(p, "Off: the same sky as if the hole were not there, to compare.");
        pHeading(p, "Sky");
        int sky = s.skyOurs ? 0 : 1;
        real::markLabels("blackhole", "sky", {"Our sky", "Black"});
        const std::vector<std::string> sn{"blackhole.sky.ours", "blackhole.sky.black"};
        if (pSegmented(p, "blackhole.sky", {"Our sky", "Black"}, &sky, &sn)) s.skyOurs = sky == 0;
        pCaption(p, s.skyOurs ? "Our sky is the one we see from Earth (ESA's Gaia map and the brightest stars), placed behind the hole: no map of the sky as seen from the centre of our galaxy or from M87 exists."
                              : s.disc ? "Black: nothing behind the hole and its disc." : "Black: nothing behind the hole. The hole gives no light of its own, so without the gas disc the whole picture is black.");
    }
    if (section("disc", "Gas disc (model)", true)) {                       // 0.55.1
        int on = s.disc ? 1 : 0;
        const std::vector<std::string> dn{"blackhole.disc.off", "blackhole.disc.on"};
        if (pSegmented(p, "blackhole.disc", {"Off", "On"}, &on, &dn)) s.disc = on == 1;
        const blackhole::Readout r = blackhole::readout(s);
        if (s.disc && r.realPreset) pCaption(p, kWhatIf, false);
        if (!s.disc) pCaption(p, "The standard thin disc of gas (Novikov and Thorne; Page and Thorne 1974), as round quasars and round small black holes feeding from a star.");
        else {
            char b[160];
            std::snprintf(b, sizeof b, "%.3g %% of the Eddington limit", 100 * s.feed);
            pStat(p, "Feeding rate", b);
            float f = static_cast<float>(blackhole::feedToSlider(s.feed));
            if (pSlider(p, "blackhole.feed", &f, 0.0f, 1.0f)) {
                const double v = blackhole::sliderToFeed(f), q = std::pow(10.0, std::floor(std::log10(v)) - 1);   // two significant digits
                blackhole::setFeed(s, std::round(v / q) * q);
            }
            pCaption(p, "The light the disc gives, as a share of the Eddington limit (where the light's push balances gravity). Only " + sig(100 * blackhole::kThinMin, 2) + " % to " +
                        sig(100 * blackhole::kThinMax, 2) + " %: below it the gas no longer forms a thin disc (Yuan and Narayan 2014), above it the disc swells (Abramowicz and Fragile 2013).");
            pStat(p, "Outer edge", sig(s.outer, 3) + " GM/c²  ·  " + kmText(s.outer * r.rgM / 1000));
            float o = static_cast<float>(blackhole::outerToSlider(s.outer));
            if (pSlider(p, "blackhole.outer", &o, 0.0f, 1.0f)) {
                const double v = blackhole::sliderToOuter(o), q = std::pow(10.0, std::floor(std::log10(v)) - 1);
                blackhole::setOuter(s, std::round(v / q) * q);
            }
            pCaption(p, "The model does not set an outer edge; most of the light comes from inside 30 GM/c² (Luminet 1979). The inner edge is the innermost stable orbit, " + sig(r.rIn, 2) + " GM/c²: inside it the gas falls in and gives no light here.");
            pHeading(p, "Exposure");
            int ex = s.expAuto ? 0 : 1;
            const std::vector<std::string> en{"blackhole.exp.auto", "blackhole.exp.manual"};
            if (pSegmented(p, "blackhole.exp", {"Automatic", "Your choice"}, &ex, &en)) s.expAuto = ex == 0;
            float e = static_cast<float>(s.expAuto ? std::log10(std::max(1e-9, app.blackholeView.stats().look.white)) : s.expLog);
            e = std::clamp(e, static_cast<float>(blackhole::kExpLogMin), static_cast<float>(blackhole::kExpLogMax));
            if (pSlider(p, "blackhole.exposure", &e, static_cast<float>(blackhole::kExpLogMin), static_cast<float>(blackhole::kExpLogMax))) blackhole::setExposure(s, std::round(e * 20) / 20);
            pCaption(p, "The screen's white stands for this many times the Sun's surface in visible light (" + timesText(std::pow(10.0, e)) + " ×). Automatic: the brightest part of the disc "
                        "in the picture is drawn at full brightness. A real disc is blinding: brighter than the Sun's surface everywhere, and blue-white. Here it is dimmed so the sky "
                        "stays visible; a real disc this bright would drown out every star.");
        }
    }
    if (section("honest", "What is real in this picture", true)) {
        pCaption(p, s.disc ? "A calculated model: a black hole that does not spin, with a model disc of gas: flat, thin, opaque, each face a perfect black body, steady. Real black holes spin; a spinning hole's disc reaches further in."
                           : "A calculated model: a black hole that does not spin and has no gas around it. Real black holes spin and have gas; switch on the gas disc to see the standard model of one.", false);
        pCaption(p, "Exact: the shadow, the bending of the sky, the doubled stars" + std::string(s.disc ? ", the disc's shape, its colour shift and brightness by the model" : "") +
                    ". Soft: the sky map's own sharpness (about 0.09° a map pixel). Not drawn: the camera's own shift of colour and brightness (numbers on the card).", false);
        if (pButton(p, "blackhole.guide", "What is real in this picture")) { app.bhLearn = 1; app.bhLearnJump = "top"; }
    }
}

void blackholeSearch(AppState& app, const SearchAdd& add) {
    add("Guides", "What is real in this picture? (Black Hole Lab)", "bhguide:1", [&app] { app.bhLearn = 1; app.bhLearnJump = "top"; });
    int n = 0;
    const blackhole::Preset* ps = blackhole::presets(n);
    for (int i = 0; i < n; i++) { const std::string k = ps[i].key; add("Black holes", std::string(ps[i].name), "bh:" + k, [&app, k] { blackhole::applyPreset(app.blackhole, k); }); }
    add("Light", "Gravity bends light: on", "bhbend:on", [&app] { app.blackhole.bend = true; });
    add("Light", "Gravity bends light: off (the plain sky)", "bhbend:off", [&app] { app.blackhole.bend = false; });
    add("Light", "Sky: our sky", "bhsky:ours", [&app] { app.blackhole.skyOurs = true; });
    add("Light", "Sky: black", "bhsky:black", [&app] { app.blackhole.skyOurs = false; });
    add("Camera", "Reset view", "bhreset", [&app] { blackhole::resetView(app.blackhole); });
    add("Black holes", std::string(blackhole::kFeedingName), "bh:feeding", [&app] { blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey); });
    add("Gas", "Gas disc (model): on", "bhdisc:on", [&app] { app.blackhole.disc = true; });
    add("Gas", "Gas disc (model): off", "bhdisc:off", [&app] { app.blackhole.disc = false; });
}

}  // namespace ui::shellimpl
