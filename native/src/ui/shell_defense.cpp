// 0.17 stage 1: the Defense Lab's build bar (bottom centre, where the other labs have their toolbar) and its Controls panel (the planet's seed, new game, save,
// power, the core's inventory, the pollution overlay, how to play, and the game numbers, which are design choices, not measured values).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "defense/defense_lab.h"
#include "imgui.h"
#include "ui/fonts.h"
#include "ui/style_draw.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"

namespace ui::shellimpl {

using namespace sh;
using app::shell::Metrics;
using app::shell::Rect;

namespace {
std::string shortName(defense::Kind k) { return k == defense::Kind::Generator ? "Generator" : k == defense::Kind::Pole ? "Pole" : defense::kindName(k); }
const char* tipOf(defense::Kind k) {
    switch (k) {
    case defense::Kind::Belt: return "Belt: carries items one way. Turn it with R; a belt fed from one side becomes a corner.";
    case defense::Kind::Miner: return "Miner (2 x 2): stand it on ore. It digs the ore under it and puts it on the belt or building in front. Needs power.";
    case defense::Kind::Furnace: return "Furnace (2 x 2): iron ore to iron plate, copper ore to copper plate, 2 stone to a brick. Burns coal. Takes from belts, puts out in front.";
    case defense::Kind::Chest: return "Chest: stores items from belts and gives them to the belt in front.";
    case defense::Kind::Generator: return "Coal generator (2 x 2): burns coal from belts and makes power for the poles around it.";
    case defense::Kind::Pole: return "Power pole: links generators and buildings within its 5 x 5 area; poles up to 7.5 tiles apart are wired together.";
    default: return "";
    }
}
std::string fmtCount(uint64_t v) {
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return s;
}
}  // namespace

Rect defenseBar(AppState& app, const app::shell::Frame& fr, const Metrics& m) {
    defense::DefenseLab& L = app.defense;
    const double W = fr.window.w, H = fr.window.h;
    // a saved planet that could not be read: say so and offer a new game (nothing runs and nothing is saved until then)
    if (!L.loadError.empty()) {
        const double pw = std::min<double>(m.em(28), W - 2 * m.s24);
        const Rect r{(W - pw) / 2, H * 0.3, pw, sh::lastHeight("##defenseError", static_cast<float>(m.em(8)))};
        sh::Panel p;
        if (sh::beginPanel("##defenseError", r, p, sh::Shape::Panel, false)) {
            sh::pTitle(p, "The saved planet could not be read");
            sh::pBody(p, L.loadError);
            if (sh::pButton(p, "defense.newgame.error", "Start a new game", sh::Face::Primary)) L.startNew(defense::seedFromText(L.seedText[0] ? L.seedText : std::to_string(static_cast<unsigned>(ImGui::GetTime() * 1e6)).c_str()));
            sh::endPanel(p);
        }
        auditNote("defense loaderror");
        return r;
    }
    if (!L.loaded) return Rect{0, 0, 0, 0};
    // the bar: one button a building, then Turn and Save
    std::vector<double> bw;
    const auto widthOf = [&](const std::string& s) { return sh::width(Type::Body, false, s) + 2.0 * m.s12; };
    for (defense::Kind k : defense::kBuildBar) bw.push_back(widthOf(shortName(k)));
    const double turnW = widthOf("Turn (R)"), saveW = widthOf("Save");
    double total = 2.0 * m.s8 + turnW + saveW + 2.0 * m.s12;
    for (double w : bw) total += w + m.s4;
    const double freeW = fr.freeW > 0 ? fr.freeW : W;
    const double x = std::max<double>(m.s12, std::min(freeW / 2 - total / 2, freeW - m.s12 - total));
    const Rect r{x, H - m.s12 - m.toolbarH, total, static_cast<double>(m.toolbarH)};
    sh::Panel p;
    if (sh::beginPanel("##defenseBar", r, p, sh::Shape::Pill, false)) {
        float bx = p.origin.x + m.s8;
        const float by = p.origin.y + (m.toolbarH - m.control) / 2.0f;
        for (std::size_t i = 0; i < bw.size(); i++) {
            const defense::Kind k = defense::kBuildBar[i];
            const std::string id = "defense.build." + shortName(k);
            if (sh::button(id.c_str(), ImVec2(bx, by), ImVec2(static_cast<float>(bw[i]), static_cast<float>(m.control)), shortName(k), Face::Quiet, L.tool == k)) L.pick(k);
            if (ImGui::IsItemHovered()) ui::SetTooltip("%s", tipOf(k));
            bx += static_cast<float>(bw[i] + m.s4);
        }
        bx += static_cast<float>(m.s12 - m.s4);
        if (sh::button("defense.turn", ImVec2(bx, by), ImVec2(static_cast<float>(turnW), static_cast<float>(m.control)), "Turn (R)", Face::Quiet)) L.rotate();
        if (ImGui::IsItemHovered()) ui::SetTooltip("Turn the building being placed (R)");
        bx += static_cast<float>(turnW + m.s12);
        if (sh::button("defense.save", ImVec2(bx, by), ImVec2(static_cast<float>(saveW), static_cast<float>(m.control)), "Save", Face::Secondary)) L.save();
        if (ImGui::IsItemHovered()) ui::SetTooltip("Save the planet now (it also saves itself every 60 s and when the app closes)");
        sh::endPanel(p);
    }
    auditNote(std::string("defense tool=") + (L.tool == defense::Kind::None ? "none" : shortName(L.tool)) + " dir=" + std::to_string(L.dir));
    // a short note above the bar (saved, cannot build there ...)
    if (!L.note.empty() && L.clock < L.noteUntil) {
        const double nw = sh::width(Type::Body, false, L.note) + 2.0 * m.s16;
        const Rect nr{(freeW - nw) / 2, r.y - m.s8 - m.control, nw, static_cast<double>(m.control)};
        sh::Panel np;
        if (sh::beginPanel("##defenseNote", nr, np, sh::Shape::Pill, false)) {
            sh::text(np.dl, Type::Body, false, ImVec2(np.origin.x + m.s16, np.origin.y + (m.control - sh::px(Type::Body)) / 2), sh::cText(), L.note);
            sh::endPanel(np);
        }
    }
    return r;
}

void defenseControls(AppState& app, Panel& p, const SectionFn& section) {
    defense::DefenseLab& L = app.defense;
    const Metrics& m = F().m;
    if (!L.loaded) { pCaption(p, "No planet is open."); return; }
    defense::World& w = L.world;
    if (section("planet", "Planet", true)) {
        pStat(p, "Seed", std::to_string(w.seed()));
        const unsigned long long s = w.tick() / defense::kTicksPerSecond;
        char t[48];
        std::snprintf(t, sizeof t, "%llu h %02llu min %02llu s", s / 3600, s / 60 % 60, s % 60);
        pStat(p, "Played", t);
        pHeading(p, "Seed for a new game (a number or any words)");
        ImGui::SetCursorScreenPos(here(p));
        ImGui::SetNextItemWidth(p.w);
        ImGui::InputText("##defense.seed", L.seedText, sizeof L.seedText);
        noteItem("defense.seed");
        p.y += ImGui::GetFrameHeight() + m.s4;
        if (pButton(p, "defense.newgame", "New game with this seed")) L.startNew(defense::seedFromText(L.seedText));
        if (pButton(p, "defense.savenow", "Save now")) L.save();
        pCaption(p, "One planet, one save file, next to the settings file. It saves itself every 60 s and when the app closes; nothing runs while the app is closed. The same seed always makes the same map.");
    }
    if (section("power", "Power", true)) {
        int gens = 0, poles = 0, miners = 0;
        for (const defense::Entity& e : w.entities()) { gens += e.kind == defense::Kind::Generator; poles += e.kind == defense::Kind::Pole; miners += e.kind == defense::Kind::Miner; }
        int64_t d = 0, sup = 0;
        for (const defense::PowerNet& n : w.nets()) { d += n.demandJ; sup += n.supplyJ; }
        char b[64];
        std::snprintf(b, sizeof b, "%d %%", w.totalSatPermille() / 10);
        pStat(p, "Power for what needs it", d == 0 ? "nothing needs power now" : b);
        std::snprintf(b, sizeof b, "%.0f kW / %.0f kW", d * defense::kTicksPerSecond / 1000.0, sup * defense::kTicksPerSecond / 1000.0);
        pStat(p, "Needed / available", b);
        pStat(p, "Generators, poles, miners", std::to_string(gens) + ", " + std::to_string(poles) + ", " + std::to_string(miners));
        pCaption(p, "With too little power, miners dig proportionally slower. A red dot on a miner: no power reaches it.");
    }
    if (section("core", "Core inventory", true)) {
        bool any = false;
        for (int i = 1; i < defense::kItemCount; i++) {
            const uint64_t v = w.core().store[i];
            if (!v) continue;
            any = true;
            pStat(p, defense::itemName(static_cast<defense::Item>(i)), fmtCount(v));
        }
        if (!any) pCaption(p, "Empty. Items belted into the core are counted here.");
    }
    if (section("view", "View", true)) {
        bool ov = L.overlay;
        if (pCheck(p, "defense.overlay", "Pollution overlay", &ov)) L.overlay = ov;
        char b[48];
        std::snprintf(b, sizeof b, "%.2f", w.pollution(defense::World::floorDiv(static_cast<int>(std::floor(w.camX)), defense::kChunk), defense::World::floorDiv(static_cast<int>(std::floor(w.camY)), defense::kChunk)));
        pStat(p, "Pollution in the middle chunk", b);
        pCaption(p, "Miners, furnaces and generators pollute their chunk (32 x 32 tiles); it spreads slowly to the next chunks. It has no effect yet.");
    }
    if (section("help", "How to play", false)) {
        pCaption(p, "Drag to move the map (either button), the wheel zooms. Pick a building on the bar, left click to place it, R to turn it, right click to remove a building (what it held goes to the core). The core cannot be removed.", false);
        pCaption(p, "Start: a miner on iron ore with a belt to a furnace; coal to the furnace; plates on a belt into the core. Miners need power: a coal generator fed with coal, and poles to the miners.", false);
    }
    if (section("numbers", "Game numbers (design, not physics)", false)) {
        pCaption(p, "Every speed, cost, size, ore amount, power and pollution number in the Defense Lab is a game design choice, not a measured value. Only the element names and symbols (Fe, Cu, C) are real, from the Atom Lab's element table.", false);
        pStat(p, "Belt", "1.875 tiles/s, 7.5 items/s");
        pStat(p, "Miner", "1 ore / 2 s, 90 kW");
        pStat(p, "Furnace", "3.2 s an item, 1 coal / 32 s");
        pStat(p, "Coal generator", "900 kW, 4 MJ a coal");
        pStat(p, "Power pole", "5 x 5 area, wires 7.5 tiles");
        pStat(p, "Chest", "400 items");
    }
}

}  // namespace ui::shellimpl
