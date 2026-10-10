#include "style_draw.h"
#include "app/look_options.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cstdarg>
#include <cstdint>
#include <string>
#include <vector>

#include "app/page_cache.h"
#include "app/shell.h"
#include "fonts.h"
#include "imgui_impl_opengl3.h"
#include "render/gl_loader.h"
#include "imgui_internal.h"
#include "look.h"
#include "reading.h"
#include "shell.h"
#include "shell_test.h"

#ifndef PL_VERSION
#define PL_VERSION "0.14"
#endif

namespace ui {

using app::PanelKind;
using app::Rgb;
using app::WidgetState;

const app::ThemeStyle& style() { return look().c.style; }
bool glossy() { return !app::isPlainWidgetStyle(style().widget); }
bool glassy() { return !app::isPlainPanelStyle(style().panel, style().page); }

namespace {

// the colour as Dear ImGui will draw it (its global alpha applies, so a disabled widget fades like the plain ones)
ImU32 tint(const Rgb& c, float a = 1) { return ImGui::GetColorU32(ImVec4(c.r, c.g, c.b, a)); }
float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
const Rgb kWhite{1, 1, 1};

// Paints the vertices v0..v1 (shapes just added to `dl`) with a vertical gradient: colour c0 / opacity a0 at y0, c1 / a1 at y1 (held outside). Each vertex
// keeps the opacity it had (the soft edge of a rounded shape is a vertex with opacity 0), times the gradient's. The same idea as Dear ImGui's own
// ShadeVertsLinearColorGradientKeepAlpha, with the opacity added.
void shade(ImDrawList* dl, int v0, int v1, float y0, float y1, const Rgb& c0, float a0, const Rgb& c1, float a1) {
    const float inv = y1 > y0 ? 1.f / (y1 - y0) : 0.f, styleA = ImGui::GetStyle().Alpha;
    for (int i = v0; i < v1; i++) {
        ImDrawVert& v = dl->VtxBuffer.Data[i];
        const float t = clamp01((v.pos.y - y0) * inv);
        const int oa = int(v.col >> IM_COL32_A_SHIFT);
        const int na = int(oa * (a0 + (a1 - a0) * t) * styleA + 0.5f);
        const int r = int((c0.r + (c1.r - c0.r) * t) * 255.f + 0.5f), g = int((c0.g + (c1.g - c0.g) * t) * 255.f + 0.5f), b = int((c0.b + (c1.b - c0.b) * t) * 255.f + 0.5f);
        v.col = IM_COL32(r, g, b, std::min(255, na));
    }
}

ImDrawFlags allCorners(ImDrawFlags f) { return (f & ImDrawFlags_RoundCornersMask_) ? (f & ImDrawFlags_RoundCornersMask_) : ImDrawFlags_RoundCornersAll; }
ImDrawFlags onlyTop(ImDrawFlags f) { const ImDrawFlags t = allCorners(f) & ImDrawFlags_RoundCornersTop; return t ? t : ImDrawFlags_RoundCornersNone; }

// the page's position t (0 top .. 1 bottom of the main window) of a screen y
float pageT(float y) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    return vp->Size.y > 0 ? (y - vp->Pos.y) / vp->Size.y : 0.f;
}

// A radial page: rings of triangles, the colour of each ring's stop at its edge. Square-cornered rectangles only (it is clipped to a..b).
void radialPage(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    const app::PageStyle& pg = style().page;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 c(vp->Pos.x + pg.cx * vp->Size.x, vp->Pos.y + pg.cy * vp->Size.y), r(std::max(1.f, pg.rx * vp->Size.x), std::max(1.f, pg.ry * vp->Size.y));
    dl->PushClipRect(a, b, true);
    dl->AddRectFilled(a, b, tint(pg.stop[pg.stops - 1].c));                       // outside the last ring: the last colour
    constexpr int kSeg = 64;
    const ImVec2 uv = ImGui::GetDrawListSharedData()->TexUvWhitePixel;
    for (int k = 0; k < pg.stops; k++) {                                          // ring k: from stop k-1 (inner) to stop k (outer); ring 0 is a disc of stop 0
        const float inner = k == 0 ? 0.f : pg.stop[k - 1].pos, outer = pg.stop[k].pos;
        if (outer <= inner) continue;
        const ImU32 ci = tint(pg.stop[k == 0 ? 0 : k - 1].c), co = tint(pg.stop[k].c);
        dl->PrimReserve(kSeg * 6, (kSeg + 1) * 2);
        const ImDrawIdx base = ImDrawIdx(dl->_VtxCurrentIdx);
        for (int i = 0; i <= kSeg; i++) {
            const float an = 6.2831853f * i / kSeg, cs = std::cos(an), sn = std::sin(an);
            dl->PrimWriteVtx(ImVec2(c.x + cs * r.x * inner, c.y + sn * r.y * inner), uv, ci);
            dl->PrimWriteVtx(ImVec2(c.x + cs * r.x * outer, c.y + sn * r.y * outer), uv, co);
        }
        for (int i = 0; i < kSeg; i++) {
            const ImDrawIdx i0 = ImDrawIdx(base + 2 * i), i1 = ImDrawIdx(i0 + 1), i2 = ImDrawIdx(i0 + 2), i3 = ImDrawIdx(i0 + 3);
            dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i1); dl->PrimWriteIdx(i2);
            dl->PrimWriteIdx(i1); dl->PrimWriteIdx(i3); dl->PrimWriteIdx(i2);
        }
    }
    dl->PopClipRect();
}

// ---- 0.14 stage 2: switches for the automatic picture checks (tests/check_y2k_pictures.py): PL_FX=off paints no page textures, PL_DECOR=off no brackets, HUD rules or label ----
bool envOff(const char* name) { const char* e = std::getenv(name); return e && !std::strcmp(e, "off"); }
float g_ptsOverride = 0;                                                             // set while the page cache texture is drawn (it is drawn with the text size of the frame that asked)
float pts() { return g_ptsOverride > 0 ? g_ptsOverride : ImGui::GetFontSize() / 16; }                                    // points to pixels, as the other helpers
uint32_t hash2(int x, int y, uint32_t seed) {                                         // a fixed pseudo-random number for a place: the grain does not shimmer
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// The textures of the page (theme_types.h PageFx) inside a..b, on `dl`. Each pattern's own opacity is at most its number in PageFx, so the contrast tests' extreme holds
// (layers that can stack are drawn with an opacity such that the stack is still at most the number).
void drawPageFx(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    const app::PageFx& fx = style().page.fx;
    if (app::pageFxTotal(fx) <= 0 && fx.streak <= 0) return;
    static const bool off = envOff("PL_FX");
    if (off) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = pts(), W = vp->Size.x, H = vp->Size.y;
    dl->PushClipRect(a, b, true);
    // soft clouds (Aero): puffs of circles, two layers each; about 7 layers pile up where puffs overlap (a cloud only lightens a page under dark text, so more would only help)
    if (fx.clouds > 0) {
        struct Puff { float dx, dy, r; };
        static const Puff puff[7] = {{-1.2f, 0.2f, 0.7f}, {-0.5f, -0.2f, 0.95f}, {0.3f, -0.35f, 1.1f}, {1.1f, -0.1f, 0.9f}, {1.7f, 0.25f, 0.65f}, {0.2f, 0.3f, 1.0f}, {-0.3f, 0.35f, 0.8f}};
        struct Cloud { float x, y, size; };
        static const Cloud cloud[5] = {{0.16f, 0.09f, 1.0f}, {0.62f, 0.05f, 0.8f}, {0.9f, 0.25f, 0.9f}, {0.3f, 0.55f, 0.7f}, {0.78f, 0.8f, 0.75f}};
        const float ac = 1.f - std::pow(1.f - fx.clouds, 1.f / 7.f);
        for (const Cloud& c : cloud) {
            const float unit = 0.075f * H * c.size;
            const ImVec2 o(vp->Pos.x + c.x * W, vp->Pos.y + c.y * H);
            if (o.x + 3 * unit < a.x || o.x - 3 * unit > b.x || o.y + 2 * unit < a.y || o.y - 2 * unit > b.y) continue;
            for (const Puff& p : puff) {
                const ImVec2 pc(o.x + p.dx * unit, o.y + p.dy * unit);
                dl->AddCircleFilled(pc, p.r * unit * 1.3f, tint(fx.tone, ac * 0.6f), 24);
                dl->AddCircleFilled(pc, p.r * unit, tint(fx.tone, ac), 24);
            }
        }
    }
    // glass bubbles: a faint disc, a ring and a small highlight (two layers at most on top of each other)
    if (fx.bubbles > 0) {
        struct Bubble { float x, y, r; };
        static const Bubble bub[8] = {{0.05f, 0.30f, 0.035f}, {0.12f, 0.62f, 0.02f}, {0.25f, 0.88f, 0.045f}, {0.46f, 0.18f, 0.02f}, {0.58f, 0.72f, 0.03f}, {0.7f, 0.4f, 0.05f}, {0.9f, 0.58f, 0.025f}, {0.95f, 0.9f, 0.04f}};
        const float ab = 1.f - std::pow(1.f - fx.bubbles, 0.5f);
        for (const Bubble& bb : bub) {
            const ImVec2 c(vp->Pos.x + bb.x * W, vp->Pos.y + bb.y * H);
            const float r = bb.r * H;
            if (c.x + r < a.x || c.x - r > b.x || c.y + r < a.y || c.y - r > b.y) continue;
            dl->AddCircleFilled(c, r, tint(fx.tone, ab * 0.5f), 32);
            dl->AddCircle(c, r, tint(fx.tone, ab), 32, std::max(1.f, 1.5f * s));
            dl->AddCircleFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.38f), r * 0.2f, tint(fx.tone, ab), 12);
        }
    }
    // a light streak (Y2K Night): a swoosh from the bottom left to the top right, soft edges from six layers
    if (fx.streak > 0) {
        const float al = 1.f - std::pow(1.f - fx.streak, 1.f / 6.f);
        const ImVec2 p0(vp->Pos.x - 0.05f * W, vp->Pos.y + 0.95f * H), p1(vp->Pos.x + 0.35f * W, vp->Pos.y + 0.78f * H), p2(vp->Pos.x + 0.55f * W, vp->Pos.y + 0.35f * H), p3(vp->Pos.x + 1.05f * W, vp->Pos.y + 0.12f * H);
        static const float width[6] = {44, 28, 17, 9, 4, 1.5f};
        for (float wd : width) dl->AddBezierCubic(p0, p1, p2, p3, tint(fx.streakColour, al), wd * s, 48);
    }
    // thin grid lines
    if (fx.grid > 0) {
        const float step = std::max(8.f, fx.gridStep * s);
        const ImU32 c = tint(fx.tone, fx.grid);
        for (float x = vp->Pos.x + std::ceil((a.x - vp->Pos.x) / step) * step; x < b.x; x += step) dl->AddLine(ImVec2(std::floor(x), a.y), ImVec2(std::floor(x), b.y), c, 1.f);
        for (float y = vp->Pos.y + std::ceil((a.y - vp->Pos.y) / step) * step; y < b.y; y += step) dl->AddLine(ImVec2(a.x, std::floor(y)), ImVec2(b.x, std::floor(y)), c, 1.f);
    }
    // halftone dots, big in the top right corner of the window and fading away from it (a magazine print)
    if (fx.halftone > 0) {
        const float step = std::max(4.f, fx.halftoneStep * s), maxR = step * 0.42f, rowH = step * 0.87f;
        const ImVec2 corner(vp->Pos.x + W, vp->Pos.y);
        const float reach = 0.35f * std::sqrt(W * W + H * H);
        const ImU32 c = tint(fx.tone, fx.halftone);
        const int j0 = std::max(0, int((a.y - vp->Pos.y - maxR) / rowH)), j1 = int((b.y - vp->Pos.y + maxR) / rowH) + 1;
        for (int j = j0; j <= j1; j++) {
            const float y = vp->Pos.y + j * rowH;
            if (y < corner.y || y - corner.y > reach) continue;
            const float off = (j & 1) ? step * 0.5f : 0.f;
            const int i0 = int((a.x - vp->Pos.x - maxR - off) / step), i1 = int((b.x - vp->Pos.x + maxR - off) / step) + 1;
            for (int i = std::max(0, i0); i <= i1; i++) {
                const float x = vp->Pos.x + i * step + off, d = std::sqrt((x - corner.x) * (x - corner.x) + (y - corner.y) * (y - corner.y)), f = 1.f - d / reach;
                if (f <= 0.08f || x < a.x - maxR || x > b.x + maxR) continue;
                dl->AddCircleFilled(ImVec2(x, y), maxR * f, c, 6);
            }
        }
    }
    // film grain: tiny specks at fixed places, each at its own opacity up to the grain number
    if (fx.grain > 0) {
        const int cell = std::max(3, int(5 * s));
        const float sz = std::max(1.f, s);
        const int cx0 = int(std::floor(a.x / cell)), cx1 = int(std::ceil(b.x / cell)), cy0 = int(std::floor(a.y / cell)), cy1 = int(std::ceil(b.y / cell));
        for (int cy = cy0; cy < cy1; cy++)
            for (int cx = cx0; cx < cx1; cx++) {
                const uint32_t h = hash2(cx, cy, 1);
                if ((h & 255) > 80) continue;                                         // about 3 cells in 10
                const float al = fx.grain * (0.35f + 0.65f * float((h >> 8) & 255) / 255.f);
                const ImVec2 p(float(cx * cell + int((h >> 16) & 3)), float(cy * cell + int((h >> 18) & 3)));
                dl->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), tint(fx.tone, al));
            }
    }
    dl->PopClipRect();
}

// Soft layers round a rectangle (the shadow and the outer glow of a window): each a little bigger and fainter, together `alpha` in the middle.
void softLayers(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, const Rgb& c, float alpha, float blur, float dx, float dy) {
    const int layers = std::max(4, std::min(14, int(blur / 1.5f)));
    const float perLayer = 1.f - std::pow(1.f - alpha, 1.f / layers);
    for (int i = layers; i >= 1; i--) {
        const float grow = blur * i / layers;
        dl->AddRectFilled(ImVec2(a.x - grow + dx, a.y - grow + dy), ImVec2(b.x + grow + dx, b.y + grow + dy), tint(c, perLayer), rounding + grow);
    }
}

// Corner brackets, thin HUD rules and the tiny spec label on a window or panel (Y2K): in the 3 to 7 point band along the edge, where the windows have their padding,
// and the label in the title bar, to the left of the close button, where there is room. Never over content (tests/check_y2k_pictures.py).
void techDecor(ImGuiWindow* w, ImDrawList* dl, bool floating) {
    const app::PanelStyle& p = style().panel;
    if (p.bracketAlpha <= 0 && p.hudAlpha <= 0) return;
    static const bool off = envOff("PL_DECOR");
    if (off) return;
    const float s = pts(), in = 3 * s, arm = p.bracketSize * s;
    const ImVec2 a = w->Pos, b(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y);
    if (b.x - a.x < 4 * arm || b.y - a.y < 4 * arm) return;
    const ImU32 cb = tint(p.hud, p.bracketAlpha), ch = tint(p.hud, p.hudAlpha);
    const ImVec2 q0(a.x + in, a.y + in), q1(b.x - in - 1, b.y - in - 1);
    dl->AddLine(q0, ImVec2(q0.x + arm, q0.y), cb); dl->AddLine(q0, ImVec2(q0.x, q0.y + arm), cb);
    dl->AddLine(ImVec2(q1.x, q0.y), ImVec2(q1.x - arm, q0.y), cb); dl->AddLine(ImVec2(q1.x, q0.y), ImVec2(q1.x, q0.y + arm), cb);
    dl->AddLine(ImVec2(q0.x, q1.y), ImVec2(q0.x + arm, q1.y), cb); dl->AddLine(ImVec2(q0.x, q1.y), ImVec2(q0.x, q1.y - arm), cb);
    dl->AddLine(q1, ImVec2(q1.x - arm, q1.y), cb); dl->AddLine(q1, ImVec2(q1.x, q1.y - arm), cb);
    const float gap = arm + 5 * s;                                                   // HUD rules between the brackets, and a ruler of ticks along the left of the bottom rule
    dl->AddLine(ImVec2(q0.x + gap, q0.y), ImVec2(q1.x - gap, q0.y), ch);
    dl->AddLine(ImVec2(q0.x + gap, q1.y), ImVec2(q1.x - gap, q1.y), ch);
    const float tickEnd = q0.x + gap + 0.35f * (q1.x - q0.x - 2 * gap);
    for (float x = q0.x + gap; x < tickEnd; x += 10 * s) dl->AddLine(ImVec2(std::floor(x), q1.y), ImVec2(std::floor(x), q1.y - 3 * s), ch);
    if (floating && !p.spec.empty() && w->TitleBarHeight > 0) {                      // the tiny spec label, monospace, in the title bar
        const std::string label = p.spec + " " + PL_VERSION;
        ImFont* f = fontFamily("mono").regular;
        if (!f) return;
        const float size = std::max(8.f, 8.5f * s);
        const ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0, label.c_str());
        const ImRect tb = w->TitleBarRect();
        const float x = tb.Max.x - ts.x - (w->HasCloseButton ? ImGui::GetFrameHeight() : 0.f) - 12 * s;
        const float titleEnd = tb.Min.x + ImGui::GetStyle().FramePadding.x + ImGui::CalcTextSize(w->Name, ImGui::FindRenderedTextEnd(w->Name)).x * (style().titleFont.empty() ? 1.f : 1.25f);
        if (x > titleEnd + 16 * s) dl->AddText(f, size, ImVec2(x, tb.Min.y + (tb.GetHeight() - ts.y) * 0.5f), tint(p.hud), label.c_str());
    }
}

// The title bar of a floating window: Dear ImGui drew it (flat, in the colour title.top) and then its title; this paints a gradient over the flat bar, and puts a
// shine band, a bright line along the top and the coloured line along the bottom between the bar and the title, so the title is not washed out.
// The bar is found by its colour (a vertex whose colour is the title colour, inside the bar's rectangle).
void titleDecor(ImGuiWindow* w) {
    const app::TitleStyle& t = style().panel.title;
    if (!t.set || w->TitleBarHeight <= 0) return;
    ImDrawList* dl = w->DrawList;
    const ImRect tb = w->TitleBarRect();
    const ImU32 mark = ImGui::GetColorU32(ImGuiCol_TitleBgActive) & 0x00FFFFFFu;
    std::vector<char> isBar(dl->VtxBuffer.Size, 0);
    const float h = std::max(1.f, tb.GetHeight());
    for (int i = 0; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer.Data[i];
        if ((v.col & 0x00FFFFFFu) != mark || v.pos.y < tb.Min.y - 1 || v.pos.y > tb.Max.y + 1 || v.pos.x < tb.Min.x - 1 || v.pos.x > tb.Max.x + 1) continue;
        isBar[i] = 1;
        const Rgb c = app::mixRgb(t.top, t.bottom, clamp01((v.pos.y - tb.Min.y) / h));
        v.col = (v.col & IM_COL32_A_MASK) | (uint32_t(c.r * 255.f + 0.5f) << IM_COL32_R_SHIFT) | (uint32_t(c.g * 255.f + 0.5f) << IM_COL32_G_SHIFT) | (uint32_t(c.b * 255.f + 0.5f) << IM_COL32_B_SHIFT);
    }
    int barEnd = -1;                                                                  // where the bar's triangles end in the index list
    for (int i = 0; i + 2 < dl->IdxBuffer.Size; i += 3)
        if (isBar[dl->IdxBuffer.Data[i]] && isBar[dl->IdxBuffer.Data[i + 1]] && isBar[dl->IdxBuffer.Data[i + 2]]) barEnd = i + 3;
    const int i0 = dl->IdxBuffer.Size;
    const float r = w->WindowRounding, s = pts();
    if (t.shine > 0) dl->AddRectFilled(tb.Min, ImVec2(tb.Max.x, tb.Min.y + h * t.shineHeight), tint(kWhite, t.shine), r, ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(tb.Min.x + r * 0.7f + 1, tb.Min.y + 1.5f), ImVec2(tb.Max.x - r * 0.7f - 1, tb.Min.y + 1.5f), tint(kWhite, 0.55f), 1.f);
    if (t.glowAlpha > 0) dl->AddLine(ImVec2(tb.Min.x + 1, tb.Max.y - 1.5f * s), ImVec2(tb.Max.x - 1, tb.Max.y - 1.5f * s), tint(t.glow, t.glowAlpha), std::max(1.5f, 1.5f * s));
    const int i1 = dl->IdxBuffer.Size;
    const bool oneCmd = dl->CmdBuffer.Size == 1 || (dl->CmdBuffer.Size == 2 && dl->CmdBuffer.back().ElemCount == 0);
    static const bool logWindows = std::getenv("PL_LOG_WINDOWS") != nullptr;
    if (logWindows) std::fprintf(stderr, "PLTITLE %s bar_end=%d one_cmd=%d\n", w->Name, barEnd, oneCmd ? 1 : 0);
    if (barEnd >= 0 && oneCmd && i1 > i0) std::rotate(dl->IdxBuffer.Data + barEnd, dl->IdxBuffer.Data + i0, dl->IdxBuffer.Data + i1);   // under the title text
}

// What is behind a floating window, on the background list (so under every window): its soft shadow, its outer glow, and the faint mirrored shine under it. Returns how far it reaches outside.
// hug (the new UI, 0.15 stage 5c.1): the owner saw the glow of the small Legend pill spill over its neighbours ("something behind the Legend"); there the shadow and the glow are a soft edge
// of at most app::shell::kHaloMaxPx round the same rounded rectangle, with no offset and no mirrored shine. The old app (hug false) is unchanged.
float shadowBehind(ImVec2 a, ImVec2 b, float rounding, bool hug) {
    const app::PanelStyle& p = style().panel;
    const float s = ImGui::GetFontSize() / 16;                                    // the offsets and blur are in points at the normal text size
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    float reach = 0;
    if (hug) {
        const float edge = static_cast<float>(app::shell::kHaloMaxPx);
        if (p.shadowAlpha > 0 && p.shadowBlur > 0) { softLayers(bg, a, b, rounding, p.shadow, p.shadowAlpha, edge, 0, 0); reach = edge; }
        if (p.glowAlpha > 0 && p.glowBlur > 0) { softLayers(bg, a, b, rounding, p.glow, p.glowAlpha, edge, 0, 0); reach = edge; }
        return reach;
    }
    if (p.shadowAlpha > 0 && p.shadowBlur > 0) { softLayers(bg, a, b, rounding, p.shadow, p.shadowAlpha, p.shadowBlur * s, p.shadowDx * s, p.shadowDy * s); reach = std::max(reach, (p.shadowBlur + std::max(std::fabs(p.shadowDx), std::fabs(p.shadowDy))) * s); }
    if (p.glowAlpha > 0 && p.glowBlur > 0) { softLayers(bg, a, b, rounding, p.glow, p.glowAlpha, p.glowBlur * s, 0, 0); reach = std::max(reach, p.glowBlur * s); }
    if (p.reflectAlpha > 0 && p.reflectHeight > 0) {                             // the glossy floor: the window's own shine, mirrored below it and fading out
        const float h = (b.y - a.y) * p.reflectHeight, inset = std::min(rounding, (b.x - a.x) * 0.2f);
        const ImU32 c0 = tint(kWhite, p.reflectAlpha), c1 = tint(kWhite, 0);
        bg->AddRectFilledMultiColor(ImVec2(a.x + inset, b.y + 2 * s), ImVec2(b.x - inset, b.y + 2 * s + h), c0, c0, c1, c1);
        reach = std::max(reach, 2 * s + h);
    }
    return reach;
}

// a frame for a button-like widget at a..b whose base colour is `base` (from Dear ImGui's style), with the border
void widgetFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& base, WidgetState st, float rounding, bool border) {
    const app::WidgetStyle& w = style().widget;
    const float s = pts();
    const Rgb baseRgb{base.x, base.y, base.z};
    const bool chosen = w.glowAlpha > 0 && base.w > 0 && std::fabs(base.x - look().c.accentFill.r) + std::fabs(base.y - look().c.accentFill.g) + std::fabs(base.z - look().c.accentFill.b) < 0.004f;
    if (chosen && st != WidgetState::Pressed)                                      // a soft glow round the chosen button / tab
        for (int i = 3; i >= 1; i--) dl->AddRect(ImVec2(a.x - i * s, a.y - i * s), ImVec2(b.x + i * s, b.y + i * s), tint(w.glow, w.glowAlpha * (4 - i) / 6.f * base.w), rounding + i * s, 0, std::max(1.f, s));
    if (base.w > 0) faceRect(dl, a, b, baseRgb, base.w, st, rounding);
    if (base.w > 0 && w.shineLineAlpha > 0 && st != WidgetState::Pressed && b.y - a.y > 8) {
        const float inset = std::min(rounding * 0.6f + 1, (b.x - a.x) * 0.25f);
        dl->AddLine(ImVec2(a.x + inset, a.y + 1.5f), ImVec2(b.x - inset, a.y + 1.5f), tint(kWhite, w.shineLineAlpha * base.w), 1.f);
    }
    const float bs = ImGui::GetStyle().FrameBorderSize;
    const bool rim = base.w > 0 && (w.rimDark > 0 || w.rimLight > 0);
    if (base.w > 0 && st == WidgetState::Hover && b.y - a.y > 8) {
        if (w.hoverGlow > 0) dl->AddRect(ImVec2(a.x + 1.5f, a.y + 1.5f), ImVec2(b.x - 1.5f, b.y - 1.5f), tint(kWhite, w.hoverGlow * base.w), std::max(0.f, rounding - 1.5f));   // light just inside the rim
        if (w.glint > 0 && b.x - a.x > 24) {                                          // a short slanted streak of light across the face
            dl->PushClipRect(a, b, true);
            const float wd = (b.x - a.x) * 0.1f, x0 = a.x + (b.x - a.x) * 0.3f, sk = (b.y - a.y) * 0.5f;
            dl->AddQuadFilled(ImVec2(x0, b.y), ImVec2(x0 + wd, b.y), ImVec2(x0 + wd + sk, a.y), ImVec2(x0 + sk, a.y), tint(kWhite, w.glint * base.w));
            dl->PopClipRect();
        }
    }
    if (base.w > 0 && w.brush > 0 && b.y - a.y > 8) {                                // brushed metal: fine horizontal lines
        const float inset = std::min(rounding * 0.5f + 1, 6.f);
        for (float y = a.y + 3; y < b.y - 2; y += 3) {
            const float k = 0.4f + 0.6f * float(hash2(int(y), int(a.x), 7) & 255) / 255.f;
            dl->AddLine(ImVec2(a.x + inset, std::floor(y)), ImVec2(b.x - inset, std::floor(y)), tint(kWhite, w.brush * k * base.w), 1.f);
        }
    }
    if (rim) {                                                                      // a thin rim in the face's lower colour (darker or icy) instead of the plain border
        const app::Gloss g = app::glossColours(w, baseRgb, st);
        Rgb rc = app::mixRgb(g.bot1, Rgb{0, 0, 0}, w.rimDark);
        rc = app::mixRgb(rc, kWhite, w.rimLight);
        dl->AddRect(a, b, tint(rc, base.w), rounding, 0, 1.f);
    } else if (border && bs > 0) dl->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, bs);
}

}  // namespace

// ---- the page ----
void pageGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, ImDrawFlags flags) {
    const app::PageStyle& pg = style().page;
    if (pg.kind == 0 || pg.stops < 2) return;
    if (pg.kind == 2) {
        if (rounding <= 0) radialPage(dl, a, b);
        else dl->AddRectFilled(a, b, tint(app::pageGradientAt(pg, 0.5f)), rounding, flags);   // a rounded window: its middle colour (the radial page is only exact for square panels)
        return;
    }
    const app::Rgb top = app::pageGradientAt(pg, pageT(a.y)), bottom = app::pageGradientAt(pg, pageT(b.y));
    if (rounding > 0) {                                                            // rounded corners: one shape, shaded from its top colour to its bottom colour
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding, flags);
        shade(dl, v0, dl->VtxBuffer.Size, a.y, b.y, top, 1, bottom, 1);
        return;
    }
    float ys[5] = {a.y};                                                           // square corners: one strip between each pair of stops
    int n = 1;
    for (int i = 0; i < pg.stops; i++) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float y = vp->Pos.y + pg.stop[i].pos * vp->Size.y;
        if (y > a.y + 0.5f && y < b.y - 0.5f && n < 4) ys[n++] = y;
    }
    ys[n++] = b.y;
    for (int i = 0; i + 1 < n; i++) {
        const ImU32 c0 = tint(app::pageGradientAt(pg, pageT(ys[i]))), c1 = tint(app::pageGradientAt(pg, pageT(ys[i + 1])));
        dl->AddRectFilledMultiColor(ImVec2(a.x, ys[i]), ImVec2(b.x, ys[i + 1]), c0, c0, c1, c1);
    }
}

// ---- 0.15 stage 2, C2: the page effects cache ----
// The page (gradient + textures) of the whole window is drawn ONCE into a texture, with the very same calls as before into a private draw list that Dear ImGui's own OpenGL
// backend renders; every frame a docked panel then draws its piece of that texture (one quad) instead of thousands of vertices. Rebuilt only when the key changes
// (app/page_cache.h): the window size, the theme, the text size. PL_PAGE_CACHE=off draws the old way (its picture guard, check_page_cache.py, is archived in native/archive/themes/tests: no theme has page textures now).
namespace {
struct PageTexture { unsigned fbo = 0, tex = 0; int w = 0, h = 0; };
PageTexture g_pageTex;
app::PageCacheTracker g_pageCacheTracker;

bool buildPageTexture(const ImGuiViewport* vp, int W, int H) {
    using namespace plgl;
    if (!g_pageTex.fbo) glGenFramebuffers(1, &g_pageTex.fbo);
    if (!g_pageTex.tex) glGenTextures(1, &g_pageTex.tex);
    GLint prevFbo = 0, prevTex = 0, prevVp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glGetIntegerv(GL_VIEWPORT, prevVp);
    if (g_pageTex.w != W || g_pageTex.h != H) {                                                  // a new size: a new storage
        glBindTexture(GL_TEXTURE_2D, g_pageTex.tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);                       // 1 texel = 1 pixel: a panel's piece is copied, not filtered
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        g_pageTex.w = W; g_pageTex.h = H;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_pageTex.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_pageTex.tex, 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (ok) {
        glViewport(0, 0, W, H);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList dl(ImGui::GetDrawListSharedData());
        dl._ResetForNewFrame();
        dl.PushTexture(io.Fonts->TexRef);
        dl.PushClipRectFullScreen();
        const ImVec2 a = vp->Pos, b(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
        const float before = g_ptsOverride;
        g_ptsOverride = ImGui::GetFontSize() / 16;
        pageGradient(&dl, a, b, 0, 0);
        drawPageFx(&dl, a, b);
        g_ptsOverride = before;
        ImDrawData dd;
        dd.Valid = true;
        dd.CmdListsCount = 1;
        dd.TotalVtxCount = dl.VtxBuffer.Size;
        dd.TotalIdxCount = dl.IdxBuffer.Size;
        dd.CmdLists.push_back(&dl);
        dd.DisplayPos = vp->Pos;
        dd.DisplaySize = vp->Size;
        dd.FramebufferScale = io.DisplayFramebufferScale;
        dd.OwnerViewport = const_cast<ImGuiViewport*>(vp);
        ImGui_ImplOpenGL3_RenderDrawData(&dd);
        dd.CmdLists.clear();                                                                      // (dl is on this stack, not Dear ImGui's)
    }
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex));
    return ok;
}
}  // namespace

bool pageCacheDraw(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    static const bool off = envOff("PL_PAGE_CACHE");
    static const bool log = std::getenv("PL_LOG_WINDOWS") != nullptr;
    if (off || !app::pageNeedsCache(style().page)) return false;
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->TexRef.GetTexID() == 0) return false;                                           // the very first frame: the font texture is not uploaded yet, draw the old way
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const int W = int(vp->Size.x * io.DisplayFramebufferScale.x + 0.5f), H = int(vp->Size.y * io.DisplayFramebufferScale.y + 0.5f);
    if (W < 1 || H < 1) return false;
    app::PageCacheKey key;
    key.theme = look().c.id; key.w = W; key.h = H; key.fontPx100 = int(ImGui::GetFontSize() * 100 + 0.5f);
    if (g_pageCacheTracker.update(key)) {
        if (!buildPageTexture(vp, W, H)) { g_pageCacheTracker.invalidate(); return false; }
        if (log) std::fprintf(stderr, "PLPAGECACHE rebuild %d %s %dx%d\n", g_pageCacheTracker.rebuilds(), key.theme.c_str(), W, H);
    }
    const float u0 = (a.x - vp->Pos.x) / vp->Size.x, u1 = (b.x - vp->Pos.x) / vp->Size.x;
    const float v0 = 1.f - (a.y - vp->Pos.y) / vp->Size.y, v1 = 1.f - (b.y - vp->Pos.y) / vp->Size.y;    // a texture drawn into a framebuffer has its top at v = 1
    dl->AddImage(ImTextureRef(static_cast<ImTextureID>(g_pageTex.tex)), a, b, ImVec2(u0, v0), ImVec2(u1, v1));
    return true;
}

// ---- panels ----
float panelDecor(PanelKind kind, bool hug) {
    static const bool logWindows = std::getenv("PL_LOG_WINDOWS") != nullptr;   // for automatic checks: where every painted window is (tests/check_window_pictures.py)
    if (logWindows) {
        const ImGuiWindow* lw = ImGui::GetCurrentWindow();
        std::fprintf(stderr, "PLWIN %s %d %d %d %d %s\n", lw->Name, int(lw->Pos.x), int(lw->Pos.y), int(lw->Size.x), int(lw->Size.y), kind == PanelKind::Docked ? "docked" : "floating");
        if (lw->TitleBarHeight > 0) std::fprintf(stderr, "PLTITLEBAR %s %d %d %d %d %.3f\n", lw->Name, int(lw->Pos.x), int(lw->Pos.y), int(lw->Size.x), int(lw->TitleBarHeight), lw->TitleBarHeight);
    }
    if (!glassy()) return 0;
    const app::PanelStyle& p = style().panel;
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    const float bs = w->WindowBorderSize;
    const ImVec2 a(w->Pos.x + bs, w->Pos.y + (w->TitleBarHeight > 0 ? w->TitleBarHeight : bs)), b(w->Pos.x + w->Size.x - bs - (w->ScrollbarY ? w->ScrollbarSizes.x : 0.f), w->Pos.y + w->Size.y - bs - (w->ScrollbarX ? w->ScrollbarSizes.y : 0.f));   // not over the scroll bars (ImGui painted them already)
    if (b.x <= a.x || b.y <= a.y) return 0;
    const float r = std::max(0.f, w->WindowRounding - bs);
    const ImDrawFlags flags = w->TitleBarHeight > 0 ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersAll;
    ImDrawList* dl = w->DrawList;
    // the page gradient of a docked panel goes on the background layer (under every window, the windows themselves are see-through), so a floating window's
    // shadow shows through the glass; a floating window paints its own, so it covers what is behind it
    // the title bar first (it reorders the window's index list, which is only safe while that is one draw command)
    if (kind == PanelKind::Floating) titleDecor(w);
    if (kind == PanelKind::Docked && pageCacheDraw(ImGui::GetBackgroundDrawList(), a, b)) {}   // 0.15 stage 2: the page and its textures from the cache (one quad)
    else {
        pageGradient(kind == PanelKind::Docked ? ImGui::GetBackgroundDrawList() : dl, a, b, r, flags);
        if (kind == PanelKind::Docked) drawPageFx(ImGui::GetBackgroundDrawList(), a, b);          // textures on the page, under the glass
    }
    dl->PushClipRect(a, b, false);
    const float alpha = kind == PanelKind::Docked ? p.fillAlpha : p.floatAlpha;
    if (alpha < 1) dl->AddRectFilled(a, b, tint(p.fill, alpha), r, flags);
    if (p.shineAlpha > 0 && p.shineHeight > 0) {                                  // a white band at the top that fades out
        const float y1 = a.y + p.shineHeight * (b.y - a.y);
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, ImVec2(b.x, y1), IM_COL32_WHITE, r, onlyTop(flags));
        shade(dl, v0, dl->VtxBuffer.Size, a.y, y1, kWhite, p.shineAlpha, kWhite, 0);
    }
    if (p.shineLineAlpha > 0) dl->AddLine(ImVec2(a.x + r * 0.7f, a.y + 0.5f), ImVec2(b.x - r * 0.7f, a.y + 0.5f), tint(kWhite, p.shineLineAlpha), 1.f);
    dl->PopClipRect();
    if (kind == PanelKind::Floating && p.rimAlpha > 0) {                          // a bright thin rim just inside the edge, so the window reads over the dark picture
        const float in = 1.5f;
        dl->AddRect(ImVec2(w->Pos.x + in, w->Pos.y + in), ImVec2(w->Pos.x + w->Size.x - in, w->Pos.y + w->Size.y - in), tint(p.rim, p.rimAlpha), std::max(0.f, w->WindowRounding - in), 0, 1.f);
    }
    techDecor(w, dl, kind == PanelKind::Floating);
    return kind == PanelKind::Floating ? shadowBehind(w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y), w->WindowRounding, hug) : 0.0f;
}

void logTitleRect(const char* what, ImVec2 a, ImVec2 b) {                          // for the automatic picture checks (PL_LOG_WINDOWS): where a title is drawn
    static const bool logWindows = std::getenv("PL_LOG_WINDOWS") != nullptr;
    if (logWindows) std::fprintf(stderr, "PLTITLETEXT %s %d %d %d %d\n", what, int(a.x), int(a.y), int(b.x - a.x + 1), int(b.y - a.y + 1));
}

void pushDockedBg() { if (glassy()) ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); }
void popDockedBg() { if (glassy()) ImGui::PopStyleColor(); }
// the title bar of a window with a styled bar has its own text colour; and the title (not the content) may be in the theme's display font
static bool titleFontOn() { return !app::titleFontKeyFor(style(), fontFamilyAvailable(style().titleFont)).empty(); }
// 0.15 stage 5k (New UI only; with it Off every floating window is exactly as before): a floating window is never taller than the room under the top bar, and on opening and after the app window
// is resized it is moved inside it (title bar reachable, bottom edge 8 px above the window's bottom edge). A taller window scrolls inside.
static const float kKeepMargin = 8.0f;
static float keepTop() { return std::max(0.0f, shellTopBarH()); }
static void keepInside() {
    if (!readingModeOn()) return;
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID kW = ImGui::GetID("##keepW"), kH = ImGui::GetID("##keepH"), kLeft = ImGui::GetID("##keepLeft");
    int left = st->GetInt(kLeft, 0);
    if (w->Appearing || st->GetFloat(kW, -1) != vp->Size.x || st->GetFloat(kH, -1) != vp->Size.y) left = 5;     // a few frames: an auto-sized window settles over two or three
    st->SetFloat(kW, vp->Size.x); st->SetFloat(kH, vp->Size.y);
    if (left > 0) {
        const float top = keepTop();
        const float nx = std::max(vp->Pos.x, std::min(w->Pos.x, vp->Pos.x + vp->Size.x - w->Size.x));
        const float ny = std::max(vp->Pos.y + top, std::min(w->Pos.y, vp->Pos.y + vp->Size.y - kKeepMargin - w->Size.y));
        if (nx != w->Pos.x || ny != w->Pos.y) ImGui::SetWindowPos(w, ImVec2(nx, ny), ImGuiCond_Always);
        left--;
    }
    st->SetInt(kLeft, left);
}

void pushFloatStyle() {
    if (readingModeOn()) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(std::max(100.0f, vp->Size.x - 2 * kKeepMargin), std::max(100.0f, vp->Size.y - keepTop() - 2 * kKeepMargin)));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, look().m1 ? static_cast<float>(app::lookopt::m1().radiusPanel) * ImGui::GetFontSize() / 16 : app::scaledRadius(style(), style().panel.floatRounding) * ImGui::GetFontSize() / 16);   // 5n: M1 windows have the panels' radius
    if (style().panel.title.set) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(style().panel.title.text.r, style().panel.title.text.g, style().panel.title.text.b, 1));
    if (titleFontOn()) {                                                                                  // a wide font: a little smaller so a title still fits ...
        const float before = ImGui::GetFontSize();
        ImGui::PushFont(titleFont(), ImGui::GetStyle().FontSizeBase * titleFontScale());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + (before - ImGui::GetFontSize()) * 0.5f));   // ... and the bar keeps the height it had
    }
}
void popFloatStyle() {
    keepInside();                                   // (called right after Begin: the window is the current one)
    if (titleFontOn()) { ImGui::PopStyleVar(); ImGui::PopFont(); }
    if (style().panel.title.set) ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void cardRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, const Rgb& border, float alpha, bool bordered) {
    const ImU32 line = ImGui::ColorConvertFloat4ToU32(ImVec4(border.r, border.g, border.b, alpha));
    if (!glassy()) {                                                               // plain: exactly the two calls that were written in the pages
        const Rgb& c = look().c.card;
        dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, alpha)), rounding);
        if (bordered) dl->AddRect(a, b, line, rounding);
        return;
    }
    const app::PanelStyle& p = style().panel;
    dl->AddRectFilled(a, b, tint(p.fill, std::min(1.f, p.floatAlpha * 0.8f) * alpha), rounding);          // a card on glass is a little more solid than the glass
    if (p.shineAlpha > 0) {
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, ImVec2(b.x, a.y + (b.y - a.y) * 0.5f), IM_COL32_WHITE, rounding, ImDrawFlags_RoundCornersTop);
        shade(dl, v0, dl->VtxBuffer.Size, a.y, a.y + (b.y - a.y) * 0.5f, kWhite, p.shineAlpha * 0.6f * alpha, kWhite, 0);
    }
    if (bordered) dl->AddRect(a, b, line, rounding);
    if (p.reflectAlpha > 0 && p.reflectHeight > 0) {                                   // the glossy floor: a faint mirrored shine under the card
        const float h = (b.y - a.y) * p.reflectHeight * 0.6f, in = std::min(rounding, (b.x - a.x) * 0.2f);
        const ImU32 c0 = tint(kWhite, p.reflectAlpha * 0.7f * alpha), c1 = tint(kWhite, 0);
        dl->AddRectFilledMultiColor(ImVec2(a.x + in, b.y + 1), ImVec2(b.x - in, b.y + 1 + h), c0, c0, c1, c1);
    }
}

// ---- widget faces ----
void faceRect(ImDrawList* dl, ImVec2 a, ImVec2 b, const Rgb& base, float alpha, WidgetState st, float rounding, ImDrawFlags flags) {
    if (!glossy()) {                                                               // plain: one flat rounded rectangle, the call the pages made themselves
        dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(base.r, base.g, base.b, alpha)), rounding, flags);
        return;
    }
    const app::WidgetStyle& w = style().widget;
    const app::Gloss g = app::glossColours(w, base, st);
    const float h = b.y - a.y, sy = a.y + h * w.split;
    if (w.split <= 0 || w.split >= 1 || h < 6) {                                   // no split: one shape from the top colour to the bottom colour
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding, flags);
        shade(dl, v0, dl->VtxBuffer.Size, a.y, b.y, g.top0, alpha, g.bot1, alpha);
        return;
    }
    int v0 = dl->VtxBuffer.Size;                                                   // the lower half first (the whole shape), then the upper half over it
    dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding, flags);
    shade(dl, v0, dl->VtxBuffer.Size, sy, b.y, g.bot0, alpha, g.bot1, alpha);
    v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, ImVec2(b.x, sy), IM_COL32_WHITE, rounding, onlyTop(flags));
    shade(dl, v0, dl->VtxBuffer.Size, a.y, sy, g.top0, alpha, g.top1, alpha);
}

// ---- widgets ----
static bool buttonImpl(const char* label, const ImVec2& size_arg) {
    if (!glossy()) return ImGui::Button(label, size_arg);
    ImGuiWindow* window = ImGui::GetCurrentWindow();                                // the same steps as ImGui::ButtonEx (imgui_widgets.cpp), with the frame drawn by us
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& st = g.Style;
    const ImGuiID id = window->GetID(label);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + st.FramePadding.x * 2.0f, label_size.y + st.FramePadding.y * 2.0f);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, st.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;
    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);
    const bool down = held && hovered;
    ImVec4 base = st.Colors[down ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button];
    base.w *= st.Alpha;
    ImGui::RenderNavCursor(bb, id);
    widgetFrame(window->DrawList, bb.Min, bb.Max, base, down ? WidgetState::Pressed : hovered ? WidgetState::Hover : WidgetState::Normal, st.FrameRounding, true);
    ImGui::RenderTextClipped(ImVec2(bb.Min.x + st.FramePadding.x, bb.Min.y + st.FramePadding.y), ImVec2(bb.Max.x - st.FramePadding.x, bb.Max.y - st.FramePadding.y),
                             label, label_end, &label_size, st.ButtonTextAlign, &bb);
    return pressed;
}

bool Button(const char* label, const ImVec2& size_arg) { const bool r = buttonImpl(label, size_arg); noteItem(label); return r; }   // (noteItem: the automatic checks find a control by its label; it does nothing otherwise)

static bool checkboxImpl(const char* label, bool* v) {
    if (!glossy()) return ImGui::Checkbox(label, v);
    ImGuiWindow* window = ImGui::GetCurrentWindow();                                // the same steps as ImGui::Checkbox
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& st = g.Style;
    const ImGuiID id = window->GetID(label);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, ImVec2(pos.x + square_sz + (label_size.x > 0.0f ? st.ItemInnerSpacing.x + label_size.x : 0.0f), pos.y + label_size.y + st.FramePadding.y * 2.0f));
    ImGui::ItemSize(total_bb, st.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) { *v = !*v; ImGui::MarkItemEdited(id); }
    const ImRect check_bb(pos, ImVec2(pos.x + square_sz, pos.y + square_sz));
    ImGui::RenderNavCursor(total_bb, id);
    ImVec4 bg = st.Colors[(held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg];
    bg.w *= st.Alpha;
    widgetFrame(window->DrawList, check_bb.Min, check_bb.Max, bg, (held && hovered) ? WidgetState::Pressed : hovered ? WidgetState::Hover : WidgetState::Normal, st.FrameRounding, true);
    if (*v) {
        const float pad = std::max(1.0f, IM_TRUNC(square_sz / 6.0f));
        ImGui::RenderCheckMark(window->DrawList, ImVec2(check_bb.Min.x + pad, check_bb.Min.y + pad), ImGui::GetColorU32(ImGuiCol_CheckMark), square_sz - pad * 2.0f);
    }
    if (label_size.x > 0.0f) ImGui::RenderText(ImVec2(check_bb.Max.x + st.ItemInnerSpacing.x, check_bb.Min.y + st.FramePadding.y), label, label_end, false);
    return pressed;
}

namespace {
bool sliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format) {
    if (!glossy()) return ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format);
    ImGuiWindow* window = ImGui::GetCurrentWindow();                                // the same steps as ImGui::SliderScalar, the frame and the grab drawn by us
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& st = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = ImGui::CalcItemWidth();
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
    const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + label_size.y + st.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, ImVec2(frame_bb.Max.x + (label_size.x > 0.0f ? st.ItemInnerSpacing.x + label_size.x : 0.0f), frame_bb.Max.y));
    ImGui::ItemSize(total_bb, st.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb, ImGuiItemFlags_Inputable)) return false;
    const bool hovered = ImGui::ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active) {                                                    // Ctrl+click turns the slider into a box to type in
        const bool clicked = hovered && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, id);
        const bool make_active = clicked || g.NavActivateId == id;
        if (make_active && clicked) ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
        if (make_active && ((clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))) temp_input_is_active = true;
        if (make_active) std::memcpy(&g.ActiveIdValueOnActivation, p_data, ImGui::DataTypeGetInfo(data_type)->Size);
        if (make_active && !temp_input_is_active) {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }
    if (temp_input_is_active) return ImGui::TempInputScalar(frame_bb, id, label, data_type, p_data, format, nullptr, nullptr);
    ImVec4 bg = st.Colors[g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg];
    bg.w *= st.Alpha;
    ImGui::RenderNavCursor(frame_bb, id);
    widgetFrame(window->DrawList, frame_bb.Min, frame_bb.Max, bg, WidgetState::Normal, st.FrameRounding, true);
    ImRect grab_bb;                                                                 // the behaviour paints its own grab: make that one invisible, then paint ours
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    const bool value_changed = ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, ImGuiSliderFlags_None, &grab_bb);
    ImGui::PopStyleColor(2);
    if (value_changed) ImGui::MarkItemEdited(id);
    if (grab_bb.Max.x > grab_bb.Min.x) {
        ImVec4 gc = st.Colors[g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab];
        gc.w *= st.Alpha;
        widgetFrame(window->DrawList, grab_bb.Min, grab_bb.Max, gc, g.ActiveId == id ? WidgetState::Pressed : WidgetState::Normal, st.GrabRounding, false);
    }
    char value_buf[64];
    const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, nullptr, ImVec2(0.5f, 0.5f));
    if (label_size.x > 0.0f) ImGui::RenderText(ImVec2(frame_bb.Max.x + st.ItemInnerSpacing.x, frame_bb.Min.y + st.FramePadding.y), label, label_end, false);
    return value_changed;
}
}  // namespace

bool Checkbox(const char* label, bool* v) { const bool r = checkboxImpl(label, v); noteItem(label); return r; }

bool SliderFloat(const char* label, float* v, float lo, float hi, const char* format) {
    const bool r = glossy() ? sliderScalar(label, ImGuiDataType_Float, v, &lo, &hi, format) : ImGui::SliderFloat(label, v, lo, hi, format);
    noteItem(label);
    return r;
}
bool SliderInt(const char* label, int* v, int lo, int hi, const char* format) {
    return glossy() ? sliderScalar(label, ImGuiDataType_S32, v, &lo, &hi, format) : ImGui::SliderInt(label, v, lo, hi, format);
}

// ---- 0.14 stage 2: tooltips, combo boxes and their lists in the theme's style ----
bool BeginTooltip() {
    if (!ImGui::BeginTooltip()) return false;
    panelDecor(PanelKind::Floating);
    return true;
}
void SetTooltip(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (BeginTooltip()) { ImGui::TextV(fmt, args); ImGui::EndTooltip(); }
    va_end(args);
}

bool BeginCombo(const char* label, const char* preview, ImGuiComboFlags flags) {
    if (!glossy() && !glassy()) { const bool o = ImGui::BeginCombo(label, preview, flags); noteItem(label); return o; }
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    const ImGuiStyle& st = ImGui::GetStyle();
    const ImGuiID id = window->GetID(label);
    bool open = false;
    if (glossy()) {                                                                  // the same box as ImGui::BeginCombo's, painted as a jelly / chrome face
        const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
        const float w = (flags & ImGuiComboFlags_NoPreview) ? ImGui::GetFrameHeight() : ImGui::CalcItemWidth();
        const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + label_size.y + st.FramePadding.y * 2.0f));
        const bool popupOpen = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);
        const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(bb.Min, bb.Max);
        ImVec4 base = st.Colors[popupOpen ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button];
        base.w *= st.Alpha;
        widgetFrame(window->DrawList, bb.Min, bb.Max, base, popupOpen ? WidgetState::Pressed : hovered ? WidgetState::Hover : WidgetState::Normal, st.FrameRounding, true);
        for (ImGuiCol c : {ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive, ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive}) ImGui::PushStyleColor(c, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        open = ImGui::BeginCombo(label, preview, flags);                             // Dear ImGui draws only the preview text, the arrow and the list
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(6);
    } else open = ImGui::BeginCombo(label, preview, flags);
    noteItem(label);
    // for the automatic picture checks: PL_OPEN_COMBO=<label> opens that combo box's list once, after a few frames
    static const char* wanted = std::getenv("PL_OPEN_COMBO");
    static int frames = 0, lastFrame = -1;
    if (wanted && std::strcmp(wanted, label) == 0 && ImGui::GetFrameCount() != lastFrame) {
        lastFrame = ImGui::GetFrameCount();
        if (++frames == 6) ImGui::OpenPopupEx(ImHashStr("##ComboPopup", 0, id));
    }
    if (open) panelDecor(PanelKind::Floating);                                      // the list: frosted body, rim, glow
    return open;
}
bool Combo(const char* label, int* current, const char* items_separated_by_zeros) {
    const char* p = items_separated_by_zeros;
    std::vector<const char*> items;
    while (*p) { items.push_back(p); p += std::strlen(p) + 1; }
    const char* preview = *current >= 0 && *current < int(items.size()) ? items[*current] : "";
    bool changed = false;
    if (BeginCombo(label, preview)) {
        for (int i = 0; i < int(items.size()); i++) {
            ImGui::PushID(i);
            const bool sel = i == *current;
            if (ImGui::Selectable(items[i], sel)) { *current = i; changed = true; }
            noteItem(items[i]);
            if (sel) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

}  // namespace ui
