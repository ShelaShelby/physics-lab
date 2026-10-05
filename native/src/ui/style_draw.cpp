#include "style_draw.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "imgui_internal.h"
#include "look.h"

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

// A soft shadow: layers of rounded rectangles, each a little bigger and fainter, behind the window (on the background list, so under every window).
void shadowBehind(ImVec2 a, ImVec2 b, float rounding) {
    const app::PanelStyle& p = style().panel;
    if (p.shadowAlpha <= 0 || p.shadowBlur <= 0) return;
    const float s = ImGui::GetFontSize() / 16;                                    // the offsets and blur are in points at the normal text size
    const float blur = p.shadowBlur * s;
    const int layers = std::max(4, std::min(14, int(blur / 1.5f)));
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    const float perLayer = 1.f - std::pow(1.f - p.shadowAlpha, 1.f / layers);     // the layers stack up to the shadow's own opacity in the middle
    for (int i = layers; i >= 1; i--) {
        const float grow = blur * i / layers;
        bg->AddRectFilled(ImVec2(a.x - grow + p.shadowDx * s, a.y - grow + p.shadowDy * s), ImVec2(b.x + grow + p.shadowDx * s, b.y + grow + p.shadowDy * s),
                          tint(p.shadow, perLayer), rounding + grow);
    }
}

// a frame for a button-like widget at a..b whose base colour is `base` (from Dear ImGui's style), with the border
void widgetFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& base, WidgetState st, float rounding, bool border) {
    if (base.w > 0) faceRect(dl, a, b, Rgb{base.x, base.y, base.z}, base.w, st, rounding);
    const app::WidgetStyle& w = style().widget;
    if (base.w > 0 && w.shineLineAlpha > 0 && st != WidgetState::Pressed && b.y - a.y > 8) {
        const float inset = std::min(rounding * 0.6f + 1, (b.x - a.x) * 0.25f);
        dl->AddLine(ImVec2(a.x + inset, a.y + 1.5f), ImVec2(b.x - inset, a.y + 1.5f), tint(kWhite, w.shineLineAlpha * base.w), 1.f);
    }
    const float bs = ImGui::GetStyle().FrameBorderSize;
    if (border && bs > 0) dl->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, bs);
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

// ---- panels ----
void panelDecor(PanelKind kind) {
    if (!glassy()) return;
    const app::PanelStyle& p = style().panel;
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    const float bs = w->WindowBorderSize;
    const ImVec2 a(w->Pos.x + bs, w->Pos.y + (w->TitleBarHeight > 0 ? w->TitleBarHeight : bs)), b(w->Pos.x + w->Size.x - bs, w->Pos.y + w->Size.y - bs);
    if (b.x <= a.x || b.y <= a.y) return;
    const float r = std::max(0.f, w->WindowRounding - bs);
    const ImDrawFlags flags = w->TitleBarHeight > 0 ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersAll;
    ImDrawList* dl = w->DrawList;
    // the page gradient of a docked panel goes on the background layer (under every window, the windows themselves are see-through), so a floating window's
    // shadow shows through the glass; a floating window paints its own, so it covers what is behind it
    pageGradient(kind == PanelKind::Docked ? ImGui::GetBackgroundDrawList() : dl, a, b, r, flags);
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
    if (kind == PanelKind::Floating) shadowBehind(w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y), w->WindowRounding);
}

void pushFloatStyle() { ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, app::scaledRadius(style(), style().panel.floatRounding) * ImGui::GetFontSize() / 16); }
void popFloatStyle() { ImGui::PopStyleVar(); }

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
bool Button(const char* label, const ImVec2& size_arg) {
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

bool Checkbox(const char* label, bool* v) {
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

bool SliderFloat(const char* label, float* v, float lo, float hi, const char* format) {
    return glossy() ? sliderScalar(label, ImGuiDataType_Float, v, &lo, &hi, format) : ImGui::SliderFloat(label, v, lo, hi, format);
}
bool SliderInt(const char* label, int* v, int lo, int hi, const char* format) {
    return glossy() ? sliderScalar(label, ImGuiDataType_S32, v, &lo, &hi, format) : ImGui::SliderInt(label, v, lo, hi, format);
}

}  // namespace ui
