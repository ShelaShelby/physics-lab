#include "ui/fps_counter.h"

#include "app/fps.h"
#include "ui/fonts.h"
#include "ui/look.h"

namespace ui {

// CSS: #fps{top:9px;right:12px;font:600 11px monospace;color:var(--muted);background:card at 70%;padding:2px 5px;border-radius:5px}
void drawFpsCounter(ImDrawList* dl, float pictureWidth, float pictureTop, float scale, double fps) {
    const std::string t = app::fpsText(fps);
    if (t.empty()) return;
    const Look& L = look();
    const float s = scale * L.textScale;
    ImFont* f = fontFamily("mono").bold;
    if (!f) f = fonts().bold;
    const float fs = 11 * s;
    const ImVec2 ts = f->CalcTextSizeA(fs, 1e9f, 0, t.c_str());
    const float w = ts.x + 10 * s, h = fs + 4 * s;
    const ImVec2 p1(pictureWidth - 12 * s, pictureTop + 9 * s + h), p0(p1.x - w, pictureTop + 9 * s);
    dl->AddRectFilled(p0, p1, col(L.c.card, 0.7f), corner(5 * s));
    dl->AddText(f, fs, ImVec2(p0.x + 5 * s, p0.y + 2 * s), col(L.c.muted), t.c_str());
}

}  // namespace ui
