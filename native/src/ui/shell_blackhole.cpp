// 0.55.0: the Black Hole Lab's shell: its chip (what is on screen, and the one line that says what kind of picture it is), its Controls panel (the preset,
// the mass, the camera, the light), its readout card and its bottom bar. Every number shown is the physics module's (blackhole/lab.h readout); this file
// only formats them.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "blackhole/gas.h"
#include "blackhole/kerr.h"
#include "blackhole/lab.h"
#include "blackhole/light.h"
#include "blackhole/presets.h"
#include "blackhole/spin_sky.h"
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
// a time: "42 s", "3.5 min", "2.1 h", "4.7 days", "12 years" (a year of 365.25 days)
std::string timeText(double sec) {
    if (sec < 120) return sig(sec, 3) + " s";
    if (sec < 7200) return sig(sec / 60, 3) + " min";
    if (sec < 2 * 86400) return sig(sec / 3600, 3) + " h";
    if (sec < 2 * 365.25 * 86400) return sig(sec / 86400, 3) + " days";
    return sig(sec / (365.25 * 86400), 3) + " years";
}
std::string distText(double d) { return sig(d, d < 10 ? 3 : 4) + " GM/c²"; }
// 0.72.0: a clock's reading: "340 µs", "12.3 ms", "42.5 s", "12 min 03 s", "3 h 07 min", "12.3 days", "4.21 years"
std::string clockText(double sec) {
    char b[64];
    if (sec < 1e-3) return sig(sec * 1e6, 3) + " µs";
    if (sec < 1) return sig(sec * 1e3, 3) + " ms";
    if (sec < 60) { std::snprintf(b, sizeof b, "%.1f s", sec); return b; }
    if (sec < 3600) { const int m = static_cast<int>(sec / 60); std::snprintf(b, sizeof b, "%d min %02d s", m, static_cast<int>(sec - 60.0 * m)); return b; }
    if (sec < 86400) { const int h = static_cast<int>(sec / 3600); std::snprintf(b, sizeof b, "%d h %02d min", h, static_cast<int>((sec - 3600.0 * h) / 60)); return b; }
    if (sec < 365.25 * 86400) return sig(sec / 86400, 3) + " days";
    return sig(sec / (365.25 * 86400), 3) + " years";
}
// 0.72.0: a span of time for the rate's words ("0.12 ms", "3.5 min", "2.1 h")
std::string spanText(double sec) {
    if (sec < 1e-3) return sig(sec * 1e6, 2) + " µs";
    if (sec < 1) return sig(sec * 1e3, 2) + " ms";
    return timeText(sec);
}
// 0.72.0: the moving gas's rate in words: "1 s here = 10 min there" (or "real time")
std::string rateText(double rate) {
    if (std::fabs(rate - 1) < 1e-9) return "real time: 1 s here = 1 s there";
    return "1 s here = " + spanText(rate) + " there";
}

// the preset's one sentence (owner's decision of 15 Nov 2026: mass and distance only; spin not measured; its real gas is not shown)
std::string presetSentence(const std::string& key) {
    if (key == "sgra") return "The black hole at the centre of our galaxy. Its real gas is a faint, very hot fog that shines mostly in radio light, not a bright disc, and is not shown here. "
                              "Spin not measured: the telescope's models disfavour a hole that does not spin, and the two that pass all its tests spin at 0.5 and 0.94 (EHT Sgr A* paper I). "
                              "It starts at spin 0; the Spin control is yours.";
    if (key == "m87") return "The black hole at the centre of the galaxy M87, pictured by the Event Horizon Telescope in 2019. Its real gas is a faint, very hot fog, not a bright disc, and is not shown here. "
                             "Spin not measured: models with no spin are disfavoured by the power of its jet, and the spin points away from Earth (EHT M87* paper I, section 6). It starts at spin 0; the Spin control is yours.";
    if (key == blackhole::kFeedingKey) return "A model, not one particular object: a black hole of 100 million Suns feeding from a thin, bright disc of gas. Such discs exist round quasars and round small black holes that feed from a companion star. "
                                              "It starts at spin 0; the Spin control is yours.";
    return "";
}
// 0.55.1: the label on a real hole with the disc switched on (owner's decision of 15 Nov 2026)
const char* kWhatIf = "What a bright disc would look like here; this black hole does not have one.";
std::string tempText(double K) {
    if (K >= 1e6) return sig(K / 1e6, 3) + " million K";
    return fmt::commas(std::round(K / (K >= 1e5 ? 1000 : 100)) * (K >= 1e5 ? 1000 : 100)) + " K";
}
// 0.55.4: a very large or very small factor as "2.95 × 10¹¹"
std::string powText(double v) {
    if (!(v > 0) || (v >= 1e-3 && v < 1e6)) return sig(v, 3);
    int e = static_cast<int>(std::floor(std::log10(v)));
    double m = v / std::pow(10.0, e);
    if (std::round(m * 100) >= 1000) { m /= 10; e++; }
    return sig(m, 3) + " × 10" + fmt::superscript(e);
}
std::string stopsText(double v) {                                      // 0.70.0: "+1.5 stops", "0 stops"
    char b[40];
    if (std::fabs(v) < 1e-9) return "0 stops";
    std::snprintf(b, sizeof b, "%+.3g stop%s", v, std::fabs(std::fabs(v) - 1) < 1e-9 ? "" : "s");
    return b;
}
std::string timesText(double v) { if (v > 0 && v < 1e-3) return powText(v); return v >= 100 ? fmt::commas(std::round(v)) : sig(v, v >= 1 ? 3 : 2); }

// 0.55.3: the Film look's labels, each from the module (blackhole/looks.h: James et al. 2015 and the place of each choice); the UI holds no number of its own
std::string filmSpinText() { return sig(blackhole::filmChoices().spin, 2) + ", the spin the film drew: its story needs nearly 1, slowed so the picture would not confuse (James et al. 2015, section 4.1.1)"; }
std::string filmDiscText() {
    const blackhole::FilmChoices& c = blackhole::filmChoices();
    return fmt::commas(c.discT) + " K all over (one temperature), thin, not feeding; from " + sig(c.rIn, 3) + " to " + sig(c.rOut, 4) + " GM/c², the edges of the paper's disc figure (Figure 13)";
}
const char* kFilmShifts = "None: no colour shift and no brightness shift from the gas's speed and gravity (the film's choice, Figure 15a)";
std::string filmColourText() { return "The paper's white balance: a " + fmt::commas(blackhole::filmChoices().whiteT) + " K black body drawn white (appendix A.6), by the app's CIE 1931 eye in place of the film stock's colours"; }
const char* kFilmGlow = "Our own glow, in place of the film's lens flare: a simple blur of the disc's bright parts, laid over the sky (the film's lens data is not published)";
const char* kFilmHonest = "A copy of the film's stated choices, not of its pictures: the artists' textures, the real lens flare and the film's colour grading are not published and are not here.";
const char* kRealCaption = "A calculated model: the standard thin disc. Real for quasars and for small black holes feeding from a star. A real disc is blinding: Glare draws the veil an average eye would see. "
                           "The picture uses a camera's response curve (Reinhard et al. 2002), our display choice; the colours and the light's ratios are the physics.";
// 0.55.4 (blackhole/light.h): the two controls of real light, and their honest limits
const char* kGlareWhat = "Glare happens in the eye, not at the hole: a very bright light scatters inside the eye and lays a veil of light round itself. Drawn by the CIE's general "
                         "disability glare equation for an average eye (35 years, brown eyes), from 0.1° to 100° round every bright point; it moves light, it does not make any.";
const char* kGlareLimits = "A model of an average eye: a camera's lens would show a different veil. Not drawn: the eye's own pattern within 0.1°, its adaptation, afterimages, "
                           "coloured rings and streaks.";
const char* kTrueWhat = "The stars on the disc's own scale: each star's measured brightness (V magnitude) against the Sun's, its light spread over its dot. Black space here "
                        "because the sky cannot compete with the disc, not because there are no stars.";
const char* kTrueMap = "The sky map is not drawn: its brightness is not calibrated, and its own pixels are fainter than the catalogue stars it contains.";

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
    const blackhole::Settings v = blackhole::shown(app.blackhole);         // 0.55.3: what the picture draws (the Film look's spin and disc)
    const blackhole::Readout r = blackhole::readout(v, true);
    c.title = "Black hole  ·  " + (r.hasPreset ? r.presetName : massText(r.massSun));
    const std::string spin = v.spin > 0 ? "spin " + sig(v.spin, 3) : std::string("no spin");
    c.sub = "A calculated model: " + spin + ", no gas  ·  our sky, seen from Earth";   // the lab's one line of what kind of picture this is (the rest: the guide)
    if (v.disc && v.bend)
        c.sub = r.realPreset ? std::string(kWhatIf) : "A calculated model: " + spin + "; the standard thin disc, real for quasars and small black holes feeding from a star; dimmed" +
                (blackhole::allLightOn(v) ? ", all light (not an eye's picture)" : "") + (blackhole::glareOn(v) ? ", an average eye's glare" : "") + (blackhole::trueSkyOn(v) ? "  ·  the stars at their true brightness beside it" : "  ·  our sky, seen from Earth");
    if (v.look == blackhole::Look::Film)
        c.sub = "The film's choices (James et al. 2015): " + spin + ", one temperature, no shifts, our own glow  ·  a copy of stated choices, not of the film's pictures";
    c.play = false;
    auditNote("bhchip " + c.sub);
    return c;
}

std::string blackholeSubject(const AppState& app) {
    const blackhole::Readout r = blackhole::readout(blackhole::shown(app.blackhole), true);
    return r.hasPreset ? r.presetName : "Black hole";
}

void blackholeCard(AppState& app, Panel& p) {
    const blackhole::Settings sv = blackhole::shown(app.blackhole);
    const bool film = sv.look == blackhole::Look::Film;
    const blackhole::Readout r = blackhole::readout(sv, true);
    pStat(p, "Mass", massText(r.massSun) + (r.realPreset ? " (measured)" : r.hasPreset ? " (the model's)" : " (your choice)"));
    if (film) pStat(p, "Spin", sig(r.spin, 3) + " of the largest possible (the film's, Film look)");
    else if (r.spin > 0) pStat(p, "Spin", sig(r.spin, 3) + " of the largest possible (your choice; not measured for any real hole here)");
    pStat(p, "Horizon radius", sig(r.horizonM, 3) + " GM/c²  ·  " + kmText(r.horizonKm) + "  ·  " + auText(r.horizonAU));
    pStat(p, "Your distance", distText(r.dist) + "  ·  " + kmText(r.distKm) + "  ·  " + auText(r.distAU));
    char b[200];
    if (r.spin > 0) std::snprintf(b, sizeof b, "%.4g° wide, %.4g° high (%.3g %% of your sky)", r.shadowWideDeg, r.shadowHighDeg, 100 * r.skyShare);
    else std::snprintf(b, sizeof b, "%.4g° across (%.3g %% of your sky)", r.shadowDeg, 100 * r.skyShare);
    pStat(p, "Shadow on your sky", b);
    if (r.spin > 0) {                                                       // 0.55.2: the dragged space (J15 (A.2)), the disc's edge and efficiency at this spin
        pStat(p, "Dragged round", "the space here turns round the axis and carries you: once every " + timeText(r.turnSeconds) + " of far-away time");
        pStat(p, "Innermost stable orbit", sig(r.rIn, 3) + " GM/c²  ·  " + kmText(r.rInKm) + "  ·  gas falling past it turns " + sig(100 * r.efficiency, 3) + " % of its mass into light");
    }
    std::snprintf(b, sizeof b, "%.4g × the rate of a clock far away", r.clockRate);
    pStat(p, "Your clock", b);
    std::snprintf(b, sizeof b, "%.4g × bluer, %.4g × brighter (not drawn)", r.blueshift, r.brightening);
    pStat(p, "Starlight reaching you", b);
    if (r.spin == 0 && r.dist < r.photonOrbitM) pCaption(p, "You are inside the photon orbit (" + sig(r.photonOrbitM, 2) + " GM/c²): the shadow covers more than half your sky.");
    if (r.spin > 0 && r.skyShare > 0.5) pCaption(p, "The shadow covers more than half your sky.");
    if (r.disc) {                                                           // 0.55.1: the disc's numbers (blackhole/lab.h readout, the view's discLook)
        const blackhole::DiscLook& L = app.blackholeView.stats().look;
        if (r.realPreset && !film) pCaption(p, kWhatIf, false);
        if (film) {                                                         // 0.55.3: the film's disc (blackhole/looks.h)
            pStat(p, "The film's disc", filmDiscText());
            pStat(p, "Shifts", kFilmShifts);
        } else {
            pStat(p, "Disc's inner edge", sig(r.rIn, 3) + " GM/c²  ·  " + kmText(r.rInKm) + " (nothing shines inside it)");
            pStat(p, "Light from the infalling gas", sig(100 * r.efficiency, 3) + " % of its mass (the rest falls in)");
            pStat(p, "Hottest ring", tempText(r.tHot) + " at " + sig(r.rHot, 3) + " GM/c²  ·  " + kmText(r.rHotKm));
        }
        // 0.55.9: the camera can stand exactly in the disc's plane (the tilt reaches 90 degrees); the model's disc has no thickness
        if (std::fabs(sv.pitch - 1.5707963267948966) < 1e-9) pCaption(p, "You are exactly in the disc's plane: a disc with no thickness seen exactly edge on is a line (the model's disc is infinitely thin).");
        if (!app.blackhole.bend) pCaption(p, "Gravity bends light: Off: the disc is not drawn (it needs the hole).");
        else if (L.any) {
            pStat(p, "Brightest part", timesText(L.brightY) + (blackhole::allLightOn(sv) ? " × the Sun's surface in all light (all wavelengths)" : " × the Sun's surface in visible light"));
            const std::string how = app.blackhole.expAuto ? " (automatic)" : " (your choice: " + stopsText(app.blackhole.expEV) + ")";
            if (sv.look == blackhole::Look::Real && L.k > 0)                // 0.70.0: the camera curve (blackhole/tone.h): what middle grey stands for
                pStat(p, "Exposure", std::string("middle grey = ") + timesText(blackhole::kToneKey / L.k) + " × the Sun's surface" + how);
            else pStat(p, "Exposure", std::string("screen white = ") + timesText(L.white) + " × the Sun's surface" + how);
        } else pCaption(p, "The disc is out of the picture from here.");
        if (L.any && blackhole::trueSkyOn(app.blackhole)) {                 // 0.55.4: the true exposure's numbers (blackhole/light.h, the view's trueNumbers)
            const blackhole::ViewStats& st = app.blackholeView.stats();
            const blackhole::TrueSkyNumbers& t = st.trueSky;
            pStat(p, "Brightest star", powText(t.fainter) + " × fainter than the disc's brightest part (V " + sig(st.brightMag, 3) + ", its light spread over its dot)");
            pStat(p, "On the screen", t.belowStep > 1 ? powText(t.belowStep) + " × below the faintest step it can show; it would first show " + stopsText(std::log2(t.belowStep)) + " brighter than now"
                                                      : std::string("bright enough to show at this exposure"));
            char tq[300];
            std::snprintf(tq, sizeof tq, "bhtrue fainter=%.6g below=%.6g showsat=%.6g starY=%.6g brightmag=%.6g shown=%d hidden=%d", t.fainter, t.belowStep, t.showsAtWhite, t.starY, st.brightMag, st.trueShown, st.trueHidden);
            auditNote(tq);
        }
        if (blackhole::glareOn(app.blackhole)) {
            char gq[120];
            std::snprintf(gq, sizeof gq, "bhglare share=%.9g", blackhole::glareShare());
            auditNote(gq);
        }
        char d[260];
        std::snprintf(d, sizeof d, "bhdisc on=1 feed=%.6g outer=%.6g rin=%.6g thot=%.6g rhot=%.6g bright=%.6g white=%.6g any=%d auto=%d edgein=%.6g edgeout=%.6g filmt=%.6g", r.feed, r.rOut, r.rIn, r.tHot, r.rHot, L.brightY, L.white,
                      L.any ? 1 : 0, app.blackhole.expAuto ? 1 : 0, r.discEdgeIn, r.discEdgeOut, r.filmT);
        auditNote(d);
    } else auditNote("bhdisc on=0");
    if (r.hasPreset && r.realPreset) {
        if (r.spin > 0) std::snprintf(b, sizeof b, "the shadow %.3g × %.3g µas (wide × high, the spin axis tilted as from your place); the telescope's ring %.3g ± %.2g µas", r.earthWideMuas, r.earthHighMuas, r.ringMuas, r.ringErr);
        else std::snprintf(b, sizeof b, "the shadow %.3g µas across; the telescope's ring %.3g ± %.2g µas", r.earthShadowMuas, r.ringMuas, r.ringErr);
        pStat(p, "Seen from Earth", b);
        pCaption(p, "The ring is what the Event Horizon Telescope measured at 1.3 mm: light of hot gas round the hole. It need not lie exactly on the shadow's edge (EHT M87* paper I, section 6).");
    }
    {   char q[300];
        std::snprintf(q, sizeof q, "bhspin spin=%.9g horizon=%.9g rin=%.9g eff=%.9g omega=%.9g turn=%.9g wide=%.9g high=%.9g share=%.9g", r.spin, r.horizonM, r.rIn, r.efficiency, r.omega, r.turnSeconds,
                      r.shadowWideDeg, r.shadowHighDeg, r.skyShare);
        auditNote(q); }
    if (app.blackholeView.stats().behind) pCaption(p, "Working out the picture: the one shown is the last finished.", false);   // 0.55.5 (BlackHoleView::async)
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
    char b[640];
    const blackhole::Settings v = blackhole::shown(s);
    std::snprintf(b, sizeof b, "blackhole preset=%s dist=%.6g fov=%.6g yaw=%.6f pitch=%.6f bend=%d sky=%s disc=%d feed=%.6g outer=%.6g exp=%s spin=%.9g fine=%d look=%s shownspin=%.9g showndisc=%d skybeside=%s glare=%s trueon=%d glareon=%d behind=%d light=%s gasclump=%d gaspattern=%d", s.preset.empty() ? "none" : s.preset.c_str(), s.dist, s.fovDeg,
                  s.yaw, s.pitch, s.bend ? 1 : 0, s.skyOurs ? "ours" : "black", s.disc ? 1 : 0, s.feed, s.outer, s.expAuto ? "auto" : fmt::precision(s.expEV, 6).c_str(), s.spin,
                  app.blackholeView.stats().spinFine ? 1 : 0, blackhole::lookKey(s.look), v.spin, v.disc ? 1 : 0, s.trueSky ? "true" : "shown", s.glare ? "eye" : "off",
                  blackhole::trueSkyOn(s) ? 1 : 0, blackhole::glareOn(s) ? 1 : 0, app.blackholeView.stats().behind ? 1 : 0, s.allLight ? "all" : "eye",
                  s.gasClump ? 1 : 0, s.gasPattern ? 1 : 0);   // (0.77.0: the moving gas's two models, for check_blackhole.py realest)
    auditNote(b);
    return r;
}

// 0.72.0: the moving gas's clocks, drawn on the picture (top left): the far clock (Boyer-Lindquist t, the hole's time), the clump's own (gas.h gasClockRate at
// its orbit) and the camera's when it differs from the far one by over 1 % (cameraClockRate), all from the far time T (GM/c^3) the app runs
Rect blackholeClocks(AppState& app, const app::shell::Frame& fr, const Metrics& m) {
    const blackhole::Settings& s = app.blackhole;
    const blackhole::Settings v = blackhole::shown(s);
    if (!blackhole::gasOn(s)) { auditNote("bhgas on=0"); return Rect{0, 0, 0, 0}; }
    const double mt = blackhole::massTimeSeconds(v.massSun), T = app.bhGasT, far = T * mt;
    const double rc = blackhole::gasClumpRadius(s), qc = blackhole::gasClockRate(rc, v.spin), qa = blackhole::cameraClockRate(v.spin, v.dist, v.pitch);
    const bool cam = std::fabs(qa - 1) > 0.01;
    const std::string lines[3][2] = {{"Far away", clockText(far)}, {v.gasClump ? "On the clump" : "On the gas at " + sig(rc, 2) + " GM/c²", clockText(far * qc)}, {"Your clock", clockText(far * qa)}};
    const int n = cam ? 3 : 2;
    double lw = 0, vw = 0;
    for (int i = 0; i < n; i++) { lw = std::max(lw, static_cast<double>(sh::width(Type::Body, false, lines[i][0]))); vw = std::max(vw, static_cast<double>(sh::width(Type::Body, false, "000 min 00 s"))); }
    const double w = lw + vw + 3.0 * m.s12, h = n * static_cast<double>(m.row) + 2.0 * m.s8;
    const Rect r{fr.topBar.x + m.s12, fr.topBar.y + fr.topBar.h + m.s12, w, h};
    Panel p;
    if (beginPanel("##blackholeClocks", r, p, Shape::Panel, false)) {
        for (int i = 0; i < n; i++) {
            const float y = p.origin.y + static_cast<float>(i * m.row + (m.row - sh::lineH(Type::Body)) / 2.0);
            sh::text(p.dl, Type::Body, false, ImVec2(p.origin.x, y), cMuted(), lines[i][0]);
            sh::text(p.dl, Type::Body, false, ImVec2(p.origin.x + static_cast<float>(lw + m.s12), y), cText(), lines[i][1]);
        }
        endPanel(p);
    }
    if (ImGui::IsMouseHoveringRect(ImVec2(static_cast<float>(r.x), static_cast<float>(r.y)), ImVec2(static_cast<float>(r.x + r.w), static_cast<float>(r.y + r.h))))
        ui::SetTooltip("Clocks of the moving gas. Far away: the hole's own time. On the clump: a clock riding with the gas at %s GM/c² runs %s × as fast. "
                       "%s", sig(rc, 3).c_str(), sig(qc, 4).c_str(), cam ? "Yours: the hovering camera's clock, slowed by the hole's gravity." : "Yours runs within 1 % of the far one (not shown).");
    char b[300];
    std::snprintf(b, sizeof b, "bhgas on=1 T=%.9g far=%.9g clump=%.9g cam=%.9g qc=%.9g qa=%.9g rate=%.9g run=%d rc=%.6g camshown=%d", T, far, far * qc, far * qa, qc, qa,
                  blackhole::gasRateOf(s), s.gasRun ? 1 : 0, rc, cam ? 1 : 0);
    auditNote(b);
    return r;
}

// 0.71.0 (DECISIONS 21 Nov 2026: one short panel, the rest under one More settings): the short panel holds the controls people use and fits a 1280 x 800 window unscrolled
// (Real with the disc on, and Film: check_blackhole.py:short); every other control and every long caption (each limit and source sentence of 0.70.0's panel, word for word) is in the one section "More settings", closed
// by default and kept in prefs.shellPanels like every section. The closest-to-reality marks are set right before their own control, so a closed section marks nothing else.
void blackholeControls(AppState& app, Panel& p, const SectionFn& section) {
    blackhole::Settings& s = app.blackhole;
    const Metrics& m = F().m;
    const bool film = s.look == blackhole::Look::Film;
    const double shownSpin = blackhole::shown(s).spin;                      // 0.55.3: the Film look shows the film's spin, locked
    if (section("preset", "Black hole", true)) {
        int n = 0;
        const blackhole::Preset* ps = blackhole::presets(n);
        for (int i = 0; i < n; i++) {
            const std::string id = std::string("blackhole.preset.") + ps[i].key;
            if (pRow(p, id.c_str(), ps[i].name, s.preset == ps[i].key)) blackhole::applyPreset(s, ps[i].key);
        }
        if (pRow(p, "blackhole.preset.feeding", blackhole::kFeedingName, s.preset == blackhole::kFeedingKey)) blackhole::applyPreset(s, blackhole::kFeedingKey);
        if (!s.preset.empty() && s.preset != blackhole::kFeedingKey) {
            const blackhole::Preset* q = blackhole::preset(s.preset.c_str());
            char b[200];
            std::snprintf(b, sizeof b, "Measured: mass %s (± %s), distance %s pc (± %s). Source: %s.", massText(q->massSun).c_str(), massText(q->massErr).c_str(), fmt::commas(q->distPc).c_str(), fmt::commas(q->distErr).c_str(), q->source);
            pCaption(p, b);
        }
        int cur = film ? 1 : 0;                                             // 0.55.3 (blackhole/looks.h): the look
        real::markLabels("blackhole", "look", {"Real", "Film"});
        const std::vector<std::string> ln{"blackhole.look.real", "blackhole.look.film"};
        if (pSegmented(p, "blackhole.look", {"Real", "Film"}, &cur, &ln)) blackhole::setLook(s, cur == 1 ? blackhole::Look::Film : blackhole::Look::Real);
        if (s.look == blackhole::Look::Film) auditNote("bhfilm labels=5");
        pStat(p, "Spin", shownSpin > 0 ? sig(shownSpin, 3) + " of the largest possible" + std::string(film ? " (the film's)" : "") : std::string("0: the hole does not spin"));   // 0.55.2
        if (film) ImGui::BeginDisabled();
        float t = static_cast<float>(shownSpin);
        if (pSlider(p, "blackhole.spin", &t, 0.0f, static_cast<float>(blackhole::kSpinTop)) && !film) blackhole::setSpin(s, std::round(t * 100) / 100);
        if (film) ImGui::EndDisabled();
        if (shownSpin > 0 && s.bend && !app.blackholeView.stats().spinFine) pCaption(p, "Working out the light paths: a coarse picture first, the fine one in a moment.", false);
        if (pButton(p, "blackhole.guide", "What is real in this picture")) { app.bhLearn = 1; app.bhLearnJump = "top"; }
    }
    if (section("camera", "Camera", true)) {
        pStat(p, "Distance", distText(s.dist) + "  ·  " + kmText(blackhole::readout(s, true).distKm));
        float t = static_cast<float>(blackhole::distToSlider(s.dist));
        if (pSlider(p, "blackhole.dist", &t, 0.0f, 1.0f)) {
            const double v = blackhole::sliderToDist(t), q = std::pow(10.0, std::floor(std::log10(v)) - 2);
            blackhole::setDistance(s, std::round(v / q) * q);
        }
        {   char b[64];
            std::snprintf(b, sizeof b, "%.0f° across the picture's height", s.fovDeg);
            pStat(p, "Field of view", b);
            float f = static_cast<float>(s.fovDeg);
            if (pSlider(p, "blackhole.fov", &f, static_cast<float>(blackhole::kFovMin), static_cast<float>(blackhole::kFovMax))) blackhole::setFov(s, std::round(f));
        }
        {   // the shadow fills the picture (close in, a narrow view): say how to see the sky round it
            const double cx = app.bhFreeW > 0 ? app.bhFreeX + app.bhFreeW / 2 : app.canvasW / 2, cy = app.bhFreeH > 0 ? app.bhFreeY + app.bhFreeH / 2 : app.canvasH / 2;
            const blackhole::HoleView hv = blackhole::holeView(s, app.canvasW, app.canvasH, cx, cy);
            double far = 0;
            for (double x : {0.0, app.canvasW}) for (double y : {0.0, app.canvasH}) far = std::max(far, std::hypot(x - cx, y - cy));
            if (s.bend && (s.spin > 0 ? blackhole::readout(s, true).shadowHighDeg / 2 * 3.14159265358979 / 180 : blackhole::shadowAngle(s.dist)) * hv.K > far) pCaption(p, "The shadow fills the whole picture: widen the field of view to see the sky round it.", false);
        }
        if (pButton(p, "blackhole.reset", "Reset view", Face::Quiet)) blackhole::resetView(s);
        pCaption(p, "Drag to go round the hole; the wheel or + − move you closer or farther.");
    }
    if (section("disc", "Gas disc (model)", true)) {                       // 0.55.1
        int on = (s.disc || film) ? 1 : 0;
        const std::vector<std::string> dn{"blackhole.disc.off", "blackhole.disc.on"};
        if (film) {                                                         // 0.55.3: the film's disc, locked (why: More settings)
            ImGui::BeginDisabled();
            pSegmented(p, "blackhole.disc", {"Off", "On"}, &on, &dn);
            ImGui::EndDisabled();
        } else if (pSegmented(p, "blackhole.disc", {"Off", "On"}, &on, &dn)) blackhole::setDisc(s, on == 1);
        const blackhole::Readout r = blackhole::readout(s, true);
        if (s.disc && r.realPreset && !film) pCaption(p, kWhatIf, false);
        if (!film && s.disc) {
            char b[160];
            std::snprintf(b, sizeof b, "%.3g %% of the Eddington limit", 100 * s.feed);
            pStat(p, "Feeding rate", b);
            float f = static_cast<float>(blackhole::feedToSlider(s.feed));
            if (pSlider(p, "blackhole.feed", &f, 0.0f, 1.0f)) {
                const double v = blackhole::sliderToFeed(f), q = std::pow(10.0, std::floor(std::log10(v)) - 1);   // two significant digits
                blackhole::setFeed(s, std::round(v / q) * q);
            }
        }
        if (s.disc || film) {                                               // 0.72.0: the moving gas, one row: Moving gas (on / off), Pause / Play, the rate
            const ImVec2 a = here(p);
            const float ch = static_cast<float>(m.control), gap = static_cast<float>(m.s8);
            const char* mv = "Moving gas";
            const char* pl = s.gasRun ? "Pause" : "Play";
            const float mw = static_cast<float>(sh::width(Type::Body, false, mv) + 2.0 * m.s12), pw = static_cast<float>(sh::width(Type::Body, false, "Pause") + 2.0 * m.s12);
            if (toolToggle("blackhole.gas.moving", a, ImVec2(mw, ch), mv, s.gasMoving)) s.gasMoving = !s.gasMoving;
            if (ImGui::IsItemHovered()) ui::SetTooltip("%s", s.gasMoving ? "The gas turns at its true speeds (on). Off: the still disc." : "Off: the still disc. Click to set the gas moving.");
            if (!s.gasMoving) ImGui::BeginDisabled();
            if (button("blackhole.gas.play", ImVec2(a.x + mw + gap, a.y), ImVec2(pw, ch), pl, Face::Quiet)) s.gasRun = !s.gasRun;
            if (ImGui::IsItemHovered()) ui::SetTooltip("%s", s.gasRun ? "Pause the gas (the clocks stop too)" : "Let the gas move again");
            const float sx = a.x + mw + pw + 2 * gap, sw = std::max(40.0f, a.x + p.w - sx);
            float t = static_cast<float>(blackhole::gasRateToSlider(blackhole::gasRateOf(s)));
            if (slider("blackhole.gas.rate", ImVec2(sx, a.y + (ch - static_cast<float>(m.row)) / 2), sw, &t, 0.0f, 1.0f) && s.gasMoving) {
                const double v = blackhole::sliderToGasRate(t), q = std::pow(10.0, std::floor(std::log10(v)) - 1);   // two significant digits
                blackhole::setGasRate(s, std::round(v / q) * q);
            }
            if (ImGui::IsItemHovered()) ui::SetTooltip("How fast the film runs: %s (More settings: real time, the clump, the streaks)", rateText(blackhole::gasRateOf(s)).c_str());
            if (!s.gasMoving) ImGui::EndDisabled();
            p.y += ch + static_cast<float>(m.s4); p.gap = static_cast<float>(m.s4);
        }
    }
    if (section("more", "More settings", false)) {                         // 0.71.0: everything else, with its full explanations, grouped by what the user wants to do
        pHeading(p, "Black hole");
        pCaption(p, s.preset.empty() ? std::string("Your own mass: no real black hole is chosen.") : presetSentence(s.preset));
        pHeading(p, "Look");
        pCaption(p, film ? "Film: the choices the makers of the film Interstellar made for its black hole." : "Real: the standard disc model, with its true shifts of colour and brightness.");
        if (film) {                                                         // 0.55.3: the film's choices, and the paper's view
            pCaption(p, "The choices the makers of the film Interstellar made for its black hole, as James, von Tunzelmann, Franklin and Thorne (2015) state them. " + std::string(kFilmHonest));
            pStat(p, "Spin", filmSpinText());
            pStat(p, "Disc", filmDiscText());
            pStat(p, "Shifts", kFilmShifts);
            pStat(p, "Colour", filmColourText());
            pStat(p, "Glow", kFilmGlow);
            if (pButton(p, "blackhole.paperview", "The paper's view")) blackhole::paperView(s);
            const blackhole::FilmChoices& c = blackhole::filmChoices();
            pCaption(p, "Puts the camera where the paper's disc figure has it: " + sig(c.camR, 3) + " GM/c², " + sig(c.camThetaDeg, 4) + "° from the spin axis (Figure 13). "
                        "The paper gives no field of view; the camera stays yours. The paper gives no mass for the film's black hole, so the lab has no preset for it.");
        } else pCaption(p, kRealCaption);
        pHeading(p, "Mass");
        pStat(p, "Mass", massText(s.massSun));
        {   float t = static_cast<float>(blackhole::massToSlider(s.massSun));
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
        pHeading(p, "Camera");
        {   char hint[96];
            std::snprintf(hint, sizeof hint, "Log scale: %g to %g GM/c². Or type one:", blackhole::kDistMin, blackhole::kDistMax);
            pCaption(p, hint);
            static char typed[32] = "";
            ImGui::SetCursorScreenPos(here(p));
            double v;
            if (typedNumber("##blackhole.dist.text", typed, sizeof typed, "e.g. 6", p.w, blackhole::kDistMin, blackhole::kDistMax, v)) blackhole::setDistance(s, v);
            p.y += m.control + m.s4;
            const blackhole::Readout r = blackhole::readout(s, true);
            if (r.spin > 0) pCaption(p, "The horizon is at " + sig(r.horizonM, 3) + " GM/c² at this spin. The closest here, " + sig(blackhole::kDistMin, 3) + ", is the closest the lab's light paths are tested at.");
            else pCaption(p, "The horizon is at " + sig(r.horizonM, 3) + " GM/c², the photon orbit at " + sig(r.photonOrbitM, 3) + " (inside it the shadow covers more than half the sky). The closest here, " +
                        sig(blackhole::kDistMin, 3) + ", is the closest the lab's light paths are tested at.");
            pCaption(p, "Drag to go round the hole (it stays in the middle; the sky behind it changes); the wheel or + − move you closer or farther; R resets the view; 1 2 3 4 switch lab. You hover in place.");
        }
        pHeading(p, "Spin");
        {   if (film) pCaption(p, "Locked while the Film look is chosen: the film drew its black hole at this spin. The Real look gives your own spin back.", false);
            else pCaption(p, "How fast the hole turns, as a share of the largest spin possible.");
            char hint[160];
            std::snprintf(hint, sizeof hint, "0 to %.2g of the largest spin a black hole can have (its angular momentum over GM²/c). Or type one:", blackhole::kSpinTop);
            pCaption(p, hint);
            if (film) ImGui::BeginDisabled();
            static char typed[32] = "";
            ImGui::SetCursorScreenPos(here(p));
            double v;
            if (typedNumber("##blackhole.spin.text", typed, sizeof typed, "e.g. 0.9", p.w, 0, blackhole::kSpinTop, v) && !film) blackhole::setSpin(s, v);
            p.y += m.control + m.s4;
            if (film) ImGui::EndDisabled();
            char top[400];
            std::snprintf(top, sizeof top, "The spin of the two real holes is not measured: the control is yours, and every preset starts at 0. The top, %.2g, is where this lab's tests "
                                           "of its light paths stop (higher spins need finer tables than it makes). The gas orbits with the spin; a disc orbiting against it, a tilted "
                                           "or a warped disc are not in the lab.", blackhole::kSpinTop);
            pCaption(p, top);
            if (shownSpin > 0) {                                            // 0.58.0: true only inside the ergosphere (BPT 1972 (2.7))
                const double rEq = blackhole::staticLimit(shownSpin, 1.5707963267948966), rPole = blackhole::staticLimit(shownSpin, 0);
                pCaption(p, "Inside the ergosphere, out to " + sig(rEq, 3) + " GM/c² in the disc's plane and " + sig(rPole, 3) + " GM/c² (the horizon) at the poles, nothing can stand still: "
                            "space there drags everything round the axis (Bardeen, Press & Teukolsky 1972). Farther out something could hover without turning, but this lab's camera "
                            "turns with the dragged space wherever it is (a locally non-rotating observer). The picture is one instant; the sky's turning with time is not shown.");
            }
        }
        pHeading(p, "Gas disc");
        {   const blackhole::Readout r = blackhole::readout(s, true);
            if (film) pCaption(p, "Locked while the Film look is chosen: the film's disc is shown. The Real look gives your own disc settings back.", false);
            if (film) {
                pStat(p, "The film's disc", filmDiscText());
                pStat(p, "Feeding rate", "none: the film's disc is not feeding (James et al. 2015, section 4.1.1)");
            } else if (!s.disc) pCaption(p, "The standard thin disc of gas (Novikov and Thorne; Page and Thorne 1974), as round quasars and round small black holes feeding from a star.");
            else {
                pCaption(p, "The light the disc gives, as a share of the Eddington limit (where the light's push balances gravity). Only " + sig(100 * blackhole::kThinMin, 2) + " % to " +
                            sig(100 * blackhole::kThinMax, 2) + " %: below it the gas no longer forms a thin disc (Yuan and Narayan 2014), above it the disc swells (Abramowicz and Fragile 2013).");
                pStat(p, "Outer edge", sig(s.outer, 3) + " GM/c²  ·  " + kmText(s.outer * r.rgM / 1000));
                float o = static_cast<float>(blackhole::outerToSlider(s.outer));
                if (pSlider(p, "blackhole.outer", &o, 0.0f, 1.0f)) {
                    const double v = blackhole::sliderToOuter(o), q = std::pow(10.0, std::floor(std::log10(v)) - 1);
                    blackhole::setOuter(s, std::round(v / q) * q);
                }
                pCaption(p, "The model does not set an outer edge. The default, 2000 GM/c² (a thousand Schwarzschild radii, about 0.01 parsec for 100 million Suns), is where such discs "
                            "are believed to end: farther out the disc's own gravity breaks it up (Goodman 2003). The same physics all the way out: the disc fades by itself as it cools "
                            "outward; most of its light comes from inside 30 GM/c² (Luminet 1979). The inner edge is the innermost stable orbit, " + sig(r.rIn, 2) + " GM/c²: inside it the gas falls in and gives no light here.");
                if (blackhole::heightOn(s.spin)) {                          // 0.79.0: the true thickness (disc.h; Zhu et al. 2012 App. B)
                    const blackhole::DiscPhys dp = blackhole::discPhys(s);
                    double hr = 0, at = 0;
                    for (double q = r.rIn * 1.001; q < std::min(200.0, s.outer); q *= 1.01) { const double t = dp.height(q) / q; if (t > hr) { hr = t; at = q; } }
                    pStat(p, "Thickness", "at most " + sig(100 * hr, 2) + " % of the radius (at " + sig(at, 3) + " GM/c²)  ·  " + sig(dp.height(std::min(2000.0, s.outer)), 2) + " GM/c² at the outer edge");
                    pCaption(p, "The disc is drawn at its true thickness: its surface sits at the height h where the pressure of its light and gas holds it up against the hole's pull "
                                "(Zhu et al. 2012, one zone in height). It thins to nothing at the innermost stable orbit, where the gas stops making heat. Limits: one zone in height, "
                                "the surface drawn opaque at h, no darkening towards its edge, no light from inside the innermost orbit.");
                } else pCaption(p, "Round a spinning hole the disc is drawn in its plane (no thickness): the thickness's spin term is not yet checked against its source.");
            }
            if (s.disc || film) {                                           // 0.72.0: the moving gas (blackhole/gas.h; PLAN_MOVINGGAS.md)
                pHeading(p, "Moving gas");
                pCaption(p, "Real: each ring of gas turns at its true speed (more than half the speed of light near the hole), the clocks (far away, on the clump, yours), "
                            "the bending of the light, its shifts of colour and brightness, the time the light takes to reach you (each part of the picture shows the gas as it was "
                            "when its light left: the second image under the hole is older than the first), and the shearing of the gas into trailing spirals.");
                pCaption(p, "A model: where each streak and the clump are, and how strong. No telescope has seen the face of such a disc.");
                const double rate = blackhole::gasRateOf(s), turn = blackhole::gasPeriod(blackhole::discGeom(blackhole::shown(s)).rIn, blackhole::shown(s).spin) * blackhole::massTimeSeconds(blackhole::shown(s).massSun);
                pStat(p, "Rate", rateText(rate) + (s.gasRateAuto ? " (automatic)" : ""));
                pStat(p, "Innermost turn", spanText(turn) + " far away  ·  " + spanText(turn / rate) + " on the screen");
                if (pButton(p, "blackhole.gas.realtime", "Real time", Face::Quiet)) blackhole::setGasRate(s, 1);
                if (pButton(p, "blackhole.gas.auto", "Automatic: one innermost turn in 20 s", Face::Quiet)) s.gasRateAuto = true;
                real::markCheck("blackhole", "gasclump");   // 0.77.0: a model, best left off
                pCheck(p, "blackhole.gas.clump", "A bright clump (model)", &s.gasClump);
                if (s.gasClump) {
                    const double rc = blackhole::gasClumpRadius(s);
                    pStat(p, "Its orbit", sig(rc, 3) + " GM/c²  ·  one turn " + spanText(blackhole::gasPeriod(rc, blackhole::shown(s).spin) * blackhole::massTimeSeconds(blackhole::shown(s).massSun)) + " far away");
                    float c = static_cast<float>(s.gasClumpR);
                    if (pSlider(p, "blackhole.gas.clumpr", &c, static_cast<float>(blackhole::discGeom(blackhole::shown(s)).rIn), static_cast<float>(blackhole::kGasClumpRMax))) s.gasClumpR = std::round(c * 10) / 10;
                    pCaption(p, "The clump astronomers fit to the flares of Sagittarius A*: a spot of extra light, Gaussian in its own frame with R_S = 1.5 GM/c² (Broderick and Loeb 2006), "
                                "its centre five times as bright as the gas round it (the lab's choice), on a circular orbit; GRAVITY (2018) saw flares circle at 6 to 10 GM/c² "
                                "(33 to 65 minutes), Wielgus et al. (2022) a loop of 74 minutes. It keeps its shape, as in Broderick and Loeb's model: real gas would shear it into an arc.");
                }
                real::markCheck("blackhole", "gaspattern");
                pCheck(p, "blackhole.gas.pattern", "Streaks (model)", &s.gasPattern);
                if (s.gasPattern)
                    pCaption(p, "Long thin streaks as in computer simulations of a magnetised disc. Real (from the simulations): each streak's tilt, 15° from the orbit and "
                                "trailing, and its size, 25° along by 0.18 of that across (Guan et al. 2009; Schnittman, Krolik and Hawley 2006). Each ring carries them round at "
                                "its own speed, so they wind into spirals, and they are renewed every 0.3 of an orbit. A model: where each streak is (drawn at random), and their "
                                "strength, set so that the ring from 6 to 10 GM/c² flickers by the simulation's 43.8 % (Schnittman et al., Table 2). The disc's total light is unchanged.");
                if (film) pCaption(p, "Added by the lab: the film's own disc texture is not published and James et al. 2015 does not say how it moved.");
                pHeading(p, "Exposure");
                int ex = s.expAuto ? 0 : 1;
                const std::vector<std::string> en{"blackhole.exp.auto", "blackhole.exp.manual"};
                if (pSegmented(p, "blackhole.exp", {"Automatic", "Your choice"}, &ex, &en)) s.expAuto = ex == 0;
                float e = static_cast<float>(s.expAuto ? 0.0 : s.expEV);   // 0.70.0: stops brighter (+) or darker (-) than automatic
                if (pSlider(p, "blackhole.exposure", &e, static_cast<float>(blackhole::kEvMin), static_cast<float>(blackhole::kEvMax))) blackhole::setExposure(s, std::round(e * 4) / 4);
                if (!film)
                    pCaption(p, std::string("Your choice: ") + stopsText(s.expAuto ? 0.0 : s.expEV) + " from automatic (each stop doubles the light; it starts from 0 again when the "
                                "preset, the look or the disc changes). Automatic: as a camera meters a scene, the disc's average brightness (its log-average, Reinhard et al. 2002) is "
                                "drawn as middle grey and its brightest part as the screen's white, through a camera's response curve that rolls off the highlights (our display "
                                "choice; the colours and the light's ratios are the physics). A real disc is blinding: brighter than the Sun's surface everywhere, and blue-white. "
                                "Here it is dimmed so the sky stays visible; a real disc this bright would drown out every star.");
                else
                    pCaption(p, std::string("Your choice: ") + stopsText(s.expAuto ? 0.0 : s.expEV) + " from automatic (each stop doubles the light). Automatic: the brightest part "
                                "of the disc in the picture is drawn at full brightness.");
            }
        }
        pHeading(p, "Gravity bends light");
        {   int cur = s.bend ? 0 : 1;
            real::markLabels("blackhole", "bend", {"On", "Off"});
            const std::vector<std::string> bn{"blackhole.bend.on", "blackhole.bend.off"};
            if (pSegmented(p, "blackhole.bend", {"On", "Off"}, &cur, &bn)) s.bend = cur == 0;
            if (!s.bend) pCaption(p, "Off: the same sky as if the hole were not there, to compare.");
        }
        pHeading(p, "Sky");
        {   int sky = s.skyOurs ? 0 : 1;
            real::markLabels("blackhole", "sky", {"Our sky", "Black"});
            const std::vector<std::string> sn{"blackhole.sky.ours", "blackhole.sky.black"};
            if (pSegmented(p, "blackhole.sky", {"Our sky", "Black"}, &sky, &sn)) s.skyOurs = sky == 0;
            pCaption(p, s.skyOurs ? "Our sky is the one we see from Earth (ESA's Gaia map and the brightest stars), placed behind the hole: no map of the sky as seen from the centre of our galaxy or from M87 exists."
                                  : s.disc ? "Black: nothing behind the hole and its disc." : "Black: nothing behind the hole. The hole gives no light of its own, so without the gas disc the whole picture is black.");
        }
        if (s.disc && !film) {                                              // 0.58.0: Light: what an eye sees or all light (blackhole/disc.h allLightLinear)
            pHeading(p, "Light");
            int li = s.allLight ? 1 : 0;
            real::markLabels("blackhole", "light", {"What an eye sees", "All light"});
            const std::vector<std::string> ln{"blackhole.light.eye", "blackhole.light.all"};
            if (pSegmented(p, "blackhole.light", {"What an eye sees", "All light"}, &li, &ln)) s.allLight = li == 1;
            if (!s.allLight) pCaption(p, "What an eye sees: each point's visible light, weighed as the eye weighs colours (the CIE 1931 observer).");
            else {
                pCaption(p, "All light: each point's brightness is the light of all wavelengths together, most of it ultraviolet for this disc, which no eye sees; "
                            "its colour is still the colour the eye would see there. The gas's speed and gravity change the total light as the 4th power of the shift "
                            "(Luminet 1979; Cunningham 1975), much more than they change the visible light, so the side coming towards you outshines the other.");
                pCaption(p, "Not an eye's picture: the eye's glare and the stars' true brightness are left out here.", false);
            }
        }
        if (s.disc && !film && !s.allLight) {                               // 0.55.4: real light (blackhole/light.h), only for the Real look with the disc on
            pHeading(p, "Sky beside the disc");
            int ts = s.trueSky ? 1 : 0;
            real::markLabels("blackhole", "skybeside", {"Shown", "True brightness"});
            const std::vector<std::string> tn{"blackhole.skybeside.shown", "blackhole.skybeside.true"};
            if (pSegmented(p, "blackhole.skybeside", {"Shown", "True brightness"}, &ts, &tn)) s.trueSky = ts == 1;
            if (!s.trueSky) pCaption(p, "Shown: the sky on its own brightness, so you can see it next to the disc (not on the disc's scale).");
            else {
                pCaption(p, kTrueWhat);
                pCaption(p, kTrueMap);
                if (!s.skyOurs) pCaption(p, "Sky: Black is chosen, so there are no stars to draw.", false);
                pCaption(p, "Approximate: the V band taken as the eye's visible light; the Sun's surface as a black body at its temperature, evenly bright (Willmer 2018 for the Sun's magnitude).");
            }
            pHeading(p, "Glare");
            int gl = s.glare ? 1 : 0;
            real::markLabels("blackhole", "glare", {"Off", "As an eye sees it"});
            const std::vector<std::string> gn{"blackhole.glare.off", "blackhole.glare.eye"};
            if (pSegmented(p, "blackhole.glare", {"Off", "As an eye sees it"}, &gl, &gn)) s.glare = gl == 1;
            pCaption(p, kGlareWhat);
            if (s.glare) pCaption(p, "Worked out when the view rests: while you turn or move, the picture waits without it.", false);
            if (s.glare) pCaption(p, std::string(kGlareLimits) + " The eye keeps " + sig(100 * (1 - blackhole::glareShare()), 3) + " % of each point's light within 0.1° and spreads the rest (CIE 1999; van den Berg et al. 2013).");
        }
        pHeading(p, "What is real in this picture");
        if (film) {
            pCaption(p, "The film's choices on a calculated black hole: spin " + sig(blackhole::filmChoices().spin, 2) + ", a thin disc at one temperature, no colour or brightness shift, our own glow. " + std::string(kFilmHonest), false);
            pCaption(p, "Exact: the shadow, the bending of the sky, the doubled stars, the disc's shape. The Real look is still a model, not a photograph.", false);
        } else {
            const std::string kind = s.spin > 0 ? "a black hole spinning at " + sig(s.spin, 3) + " of the largest possible spin (your choice)" : std::string("a black hole that does not spin");
            pCaption(p, s.disc ? "A calculated model: " + kind + ", with a model disc of gas orbiting with the spin: flat, thin, opaque, each face a perfect black body, steady." + (s.spin > 0 ? "" : " Real black holes spin; a spinning hole's disc reaches further in.")
                               : "A calculated model: " + kind + " with no gas around it. Real black holes have gas; switch on the gas disc to see the standard model of one.", false);
            pCaption(p, "Exact: the shadow, the bending of the sky, the doubled stars" + std::string(s.disc ? ", the disc's shape, its colour shift and brightness by the model" : "") +
                        ". Soft: the sky map's own sharpness (about 0.09° a map pixel). Not drawn: the camera's own shift of colour and brightness (numbers on the card).", false);
        }
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
    add("Black holes", "Spin: none (0)", "bhspin:0", [&app] { blackhole::setSpin(app.blackhole, 0); });
    add("Look", "Look: Real (the closest to reality)", "bhlook:real", [&app] { blackhole::setLook(app.blackhole, blackhole::Look::Real); });
    add("Look", "Look: Film (the film's choices, James et al. 2015)", "bhlook:film", [&app] { blackhole::setLook(app.blackhole, blackhole::Look::Film); });
    add("Light", "Sky beside the disc: shown", "bhtrue:off", [&app] { app.blackhole.trueSky = false; });                       // 0.55.4
    add("Light", "Sky beside the disc: true brightness", "bhtrue:on", [&app] { app.blackhole.trueSky = true; });
    add("Light", "Light: what an eye sees", "bhlight:eye", [&app] { app.blackhole.allLight = false; });                       // 0.58.0
    add("Light", "Light: all light", "bhlight:all", [&app] { app.blackhole.allLight = true; });
    add("Light", "Glare: off", "bhglare:off", [&app] { app.blackhole.glare = false; });
    add("Light", "Glare: as an eye sees it", "bhglare:eye", [&app] { app.blackhole.glare = true; });
}

}  // namespace ui::shellimpl
