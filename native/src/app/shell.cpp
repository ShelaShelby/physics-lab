#include "app/shell.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "app/display_scale.h"
#include "app/theme_style.h"

namespace app::shell {

// ---- the type scale ----
const TypeDef& typeDef(Type t) {
    static const TypeDef defs[] = {{"Caption", 13, 12}, {"Body", 15, 14}, {"Title", 17, 15}, {"Key", 28, 22}};   // DESIGN.md section 1
    return defs[static_cast<int>(t)];
}
int textPx(Type t, double dpi, double tf) { return scaledTextPx(typeDef(t).u, typeDef(t).floorPx, dpi, tf); }
float fontBase(Type t, double dpi, double tf) { return static_cast<float>(textPx(t, dpi, tf) / (dpi * tf)); }
int unitPx(double u, double dpi, double tf) { return std::max(1, static_cast<int>(std::lround(u * dpi * tf))); }

int Metrics::em(double n) const { return static_cast<int>(std::lround(n * body)); }

Metrics metrics(double dpi, double tf) {
    Metrics m;
    m.dpi = dpi; m.tf = tf;
    m.caption = textPx(Type::Caption, dpi, tf); m.body = textPx(Type::Body, dpi, tf); m.title = textPx(Type::Title, dpi, tf); m.key = textPx(Type::Key, dpi, tf);
    m.h1 = scaledTextPx(28, 22, dpi, tf); m.h2 = scaledTextPx(20, 16, dpi, tf); m.h3 = scaledTextPx(17, 15, dpi, tf);   // DESIGN.md: reading view H1 28, H2 20, H3 17 (floors chosen here: 22 = the key number's, 16, 15 = Title's)
    m.s4 = unitPx(4, dpi, tf); m.s8 = unitPx(8, dpi, tf); m.s12 = unitPx(12, dpi, tf); m.s16 = unitPx(16, dpi, tf); m.s24 = unitPx(24, dpi, tf);
    m.radiusPanel = unitPx(10, dpi, tf); m.radiusCtl = unitPx(6, dpi, tf);
    m.control = std::max(unitPx(30, dpi, tf), m.body + 2 * m.s4);                // a control is tall enough for its Body text with 4 u above and below
    m.iconBtn = m.control;
    m.toolBtn = std::max(unitPx(32, dpi, tf), m.control);
    m.toolbarH = m.toolBtn + 2 * m.s4;                                           // 32 u buttons in a 40 u pill
    m.chipH = std::max(unitPx(38, dpi, tf), m.title + m.caption + 2 * m.s4);     // two lines: the value in Title, the sub-line in Caption
    m.topBar = std::max(unitPx(44, dpi, tf), m.chipH + m.s4);
    m.check = unitPx(16, dpi, tf);
    m.row = std::max(unitPx(28, dpi, tf), m.body + 2 * m.s4);
    m.caretW = unitPx(16, dpi, tf);
    return m;
}

// ---- rectangles ----
bool within(const Rect& r, const Rect& b, double eps) {
    return r.x >= b.x - eps && r.y >= b.y - eps && r.right() <= b.right() + eps && r.bottom() <= b.bottom() + eps;
}
Rect clampInto(Rect r, const Rect& b) {
    r.w = std::min(r.w, b.w); r.h = std::min(r.h, b.h);
    r.x = std::min(std::max(r.x, b.x), b.right() - r.w);
    r.y = std::min(std::max(r.y, b.y), b.bottom() - r.h);
    return r;
}

// ---- the fixed pieces ----
double dockWidth(const Metrics& m, double winW) { return std::max<double>(m.em(12), std::min<double>(m.em(22), winW * 0.45)); }

Rect dockRect(double winW, double winH, const Metrics& m, double dockW) { return {winW - m.s12 - dockW, m.topBar + static_cast<double>(m.s8), dockW, winH - m.topBar - m.s8 - m.s12}; }

double slideStep(double s, double dt, bool open, bool reduced) {
    if (reduced) return open ? 1.0 : 0.0;
    if (!(dt > 0)) return s;
    const double step = dt / kSlideSeconds;
    return open ? std::min(1.0, s + step) : std::max(0.0, s - step);
}
double slideEase(double t) { t = std::min(1.0, std::max(0.0, t)); return t * t * (3 - 2 * t); }
Rect slidRect(const Rect& dock, double winW, double e) { Rect r = dock; r.x = dock.x + (1 - e) * (winW - dock.x); return r; }
Rect handleRect(const Rect& dock, double winW, const Metrics& m, double e) {
    const double w = m.s24, h = 2.0 * m.s24 + m.s8;
    return {std::min(winW, slidRect(dock, winW, e).x) - w, dock.y + dock.h / 2 - h / 2, w, h};
}

Frame makeFrame(double winW, double winH, const Metrics& m, double toolbarW, double readoutW, double readoutH, double dockW, double anchorW, double sceneCx) {
    Frame f;
    f.window = {0, 0, winW, winH};
    f.topBar = {0, 0, winW, static_cast<double>(m.topBar)};
    if (dockW > 0) f.dock = dockRect(winW, winH, m, dockW);
    f.freeW = dockW > 0 ? f.dock.x : winW;
    const double tw = std::min(toolbarW, f.freeW - 2.0 * m.s12);
    double tx = (f.freeW - tw) / 2;
    if (anchorW > 0) {
        tx = winW / 2 - anchorW / 2;                                              // the fixed left edge (stage 5g)
        if (dockW > 0) tx = std::min(tx, f.dock.x - m.s12 - anchorW);             // only the open Controls panel moves it, to its left
        tx = std::max(tx, static_cast<double>(m.s12));
    }
    if (sceneCx >= 0) tx = std::max(static_cast<double>(m.s12), std::min(sceneCx - tw / 2, f.freeW - m.s12 - tw));
    f.toolbar = {tx, winH - m.s12 - m.toolbarH, tw, static_cast<double>(m.toolbarH)};
    const double rw = std::min(readoutW, f.freeW - 2.0 * m.s12);
    f.readout = {f.freeW - m.s12 - rw, winH - m.s12 - readoutH, rw, readoutH};
    if (f.readout.inflated(m.s8).intersects(f.toolbar)) f.readout.y = f.toolbar.y - m.s8 - readoutH;   // no room beside the toolbar: just above it
    return f;
}

int toolbarVisible(const std::vector<double>& widths, double gap, double moreW, double winW, const Metrics& m) {
    const double avail = winW - 2.0 * m.s12 - 2.0 * m.s8;                    // the pill's own padding
    const int count = static_cast<int>(widths.size());
    for (int n = count; n >= 0; n--) {
        double used = 0;
        for (int i = 0; i < n; i++) used += widths[i] + (i ? gap : 0);
        if (n < count) used += (n ? gap : 0) + moreW;                        // the rest sit behind the "..." button
        if (used <= avail) return n;
    }
    return 0;
}

TopBarPlan planTopBar(double winW, const Metrics& m, const TopBarWidths& w) {
    TopBarPlan p;
    const double rightStart = winW - m.s12 - w.right;
    const double crumbs[3] = {w.crumbFull, w.crumbShort, w.crumbIcon};
    for (int mode = 0; mode < 3; mode++) {
        const double leftEnd = m.s12 + w.menu + m.s8 + crumbs[mode];
        const double lo = leftEnd + m.s16, hi = rightStart - m.s16 - w.chip;
        if (lo <= hi || mode == 2) {
            p.crumbMode = mode;
            p.fits = lo <= hi;
            const double ideal = (winW - w.chip) / 2;
            double cw = w.chip, x;
            if (p.fits) x = std::min(std::max(ideal, lo), hi);
            else { cw = std::max(0.0, rightStart - m.s16 - lo); x = lo; }     // even the icons leave no room: the chip gets what is left
            p.chip = {x, (m.topBar - m.chipH) / 2.0, cw, static_cast<double>(m.chipH)};
            return p;
        }
    }
    return p;
}

// ---- floating panels ----
Rect placePanel(Anchor a, double w, double h, const Frame& f, const Metrics& m, double anchorX) {
    const double W = f.freeW > 0 ? f.freeW : f.window.w, top = f.topBar.bottom();     // (stage 5c: left of the Controls panel when it is open)
    w = std::max(1.0, std::min(w, W - 2.0 * m.s12));
    Rect r{0, 0, w, h};
    switch (a) {
    case Anchor::TopLeft:
        r.x = std::min<double>(m.s24, W - m.s12 - w); r.y = top + m.s12;
        r.h = std::min(h, f.toolbar.y - m.s12 - r.y);
        break;
    case Anchor::TopRight:
        r.x = std::max<double>(m.s12, W - m.s24 - w); r.y = top + m.s12;
        r.h = std::min(h, f.toolbar.y - m.s12 - r.y);
        break;
    case Anchor::UnderChip:
        r.x = std::min(std::max(anchorX - w / 2, static_cast<double>(m.s12)), W - m.s12 - w); r.y = top + m.s8;
        r.h = std::min(h, f.toolbar.y - m.s12 - r.y);
        break;
    case Anchor::AboveToolbar: {
        r.x = std::min(std::max(anchorX - w / 2, static_cast<double>(m.s12)), W - m.s12 - w);
        const double bottom = f.toolbar.y - m.s8, avail = bottom - (top + m.s12);
        r.h = std::min(h, avail);
        r.y = bottom - std::max(1.0, r.h);
        break;
    }
    case Anchor::Centre:
        r.x = (W - w) / 2;
        r.y = top + m.s24;
        // stage 5i: it ends above the bottom toolbar (it was drawn over it by about 27 px at 1280x800); a taller one scrolls inside itself
        r.h = std::min(h, std::min(f.window.h - m.s24, f.toolbar.h > 0 ? f.toolbar.y - m.s8 : f.window.h) - r.y);
        break;
    }
    r.h = std::max(1.0, r.h);
    return clampInto(r, Rect{f.window.x, f.window.y, W, f.window.h});
}

Rect sideStep(Rect r, const Rect& blocker, const Rect& window, double gap, double margin) {
    if (!r.intersects(blocker)) return r;
    const double left = blocker.x - gap - r.w, right = blocker.right() + gap;
    if (left >= window.x + margin) r.x = left;
    else if (right + r.w <= window.right() - margin) r.x = right;
    return r;
}

double collapsedWidth(double titleW, const Metrics& m) { return titleW + m.caretW + 2.0 * m.s12; }

// ---- the centre shift ----
Rect largestFreeRect(const Rect& window, const std::vector<Rect>& occ) {
    std::vector<double> xs{window.x, window.right()}, ys{window.y, window.bottom()};
    for (const Rect& o : occ) {
        xs.push_back(std::min(std::max(o.x, window.x), window.right())); xs.push_back(std::min(std::max(o.right(), window.x), window.right()));
        ys.push_back(std::min(std::max(o.y, window.y), window.bottom())); ys.push_back(std::min(std::max(o.bottom(), window.y), window.bottom()));
    }
    std::sort(xs.begin(), xs.end()); xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end()); ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    Rect best = window;
    double bestArea = -1;
    for (std::size_t i = 0; i < xs.size(); i++)
        for (std::size_t j = i + 1; j < xs.size(); j++)
            for (std::size_t k = 0; k < ys.size(); k++)
                for (std::size_t l = k + 1; l < ys.size(); l++) {
                    const Rect c{xs[i], ys[k], xs[j] - xs[i], ys[l] - ys[k]};
                    if (c.area() <= bestArea) continue;
                    bool free = true;
                    for (const Rect& o : occ) if (c.intersects(o)) { free = false; break; }
                    if (free) { best = c; bestArea = c.area(); }
                }
    return best;
}

bool hiddenBy(Pt c, double radius, const std::vector<Rect>& occ, double margin) {
    for (const Rect& o0 : occ) {
        const Rect o = o0.inflated(margin);
        const double dx = std::max({o.x - c.x, 0.0, c.x - o.right()}), dy = std::max({o.y - c.y, 0.0, c.y - o.bottom()});
        if (dx * dx + dy * dy < radius * radius || (dx == 0 && dy == 0)) return true;
    }
    return false;
}

bool centreShiftTarget(Pt c, double radius, const Rect& window, const std::vector<Rect>& occ, double margin, Pt& target) {
    if (!hiddenBy(c, radius, occ, margin)) return false;
    const Rect r = largestFreeRect(window, occ);
    target = {r.x + r.w / 2, r.y + r.h / 2};
    return true;
}

double easeToward(double cur, double target, double dt, double seconds, bool reduced) {
    if (reduced) return target;
    if (dt <= 0 || seconds <= 0) return dt <= 0 ? cur : target;
    const double step = 1.0 - std::exp(-10.0 * dt / seconds);                 // exponential: about 95 % of the way after `seconds`, never past the target
    const double v = cur + (target - cur) * step;
    return std::fabs(target - v) < 0.05 ? target : v;
}

// ---- glass ----
namespace {
Rgb quantise(const Rgb& c) {                                                // the screen shows 8 bits per channel
    auto q = [](float v) { return static_cast<float>(std::round(std::min(1.0f, std::max(0.0f, v)) * 255.0f) / 255.0); };
    return {q(c.r), q(c.g), q(c.b)};
}
}  // namespace

Rgb worstBehind(const ThemeColors& c) { return relativeLuminance(c.card) > 0.4 ? Rgb{0, 0, 0} : Rgb{1, 1, 1}; }

double glassOpacity(const ThemeColors& c) {
    const Rgb worst = worstBehind(c);
    double need = -1;
    for (const Rgb* text : {&c.fg, &c.muted, &c.accent}) {
        for (int k = 0; k <= 100; k++) {
            if (contrastRatio(*text, quantise(over(c.card, static_cast<float>(k / 100.0), worst))) >= 4.5) { need = std::max(need, k / 100.0); break; }
        }                                                                    // a colour that never passes is left out, as in glass_alpha.py
    }
    if (need < 0) return 0.97;
    return std::min(0.97, std::round((need + 0.05) * 100) / 100);
}

Rgb glassOver(const ThemeColors& c, const Rgb& behind) { return quantise(over(c.card, static_cast<float>(glassOpacity(c)), behind)); }

Rgb onAccent(const ThemeColors& c) {
    if (contrastRatio(c.onacc, c.accentFill) >= 4.5) return c.onacc;
    return contrastRatio(c.bg, c.accentFill) >= contrastRatio(c.fg, c.accentFill) ? c.bg : c.fg;
}

// ---- keys ----
KeyOutcome applyKey(UiState& s, Key k, bool textInput, bool labHasPlay) {
    KeyOutcome o;
    switch (k) {
    case Key::Escape:
        if (s.search) s.search = false;
        else if (s.open != PanelId::None) s.open = PanelId::None;
        else if (s.reading) s.reading = false;
        else if (s.oldPanel) s.oldPanel = false;
        else return o;
        o.consumed = true;
        break;
    case Key::CtrlK:
        s.search = true; s.open = PanelId::None;
        o.consumed = true;
        break;
    case Key::Space:
        if (textInput || s.search || !labHasPlay) return o;
        o.consumed = o.togglePlay = true;
        break;
    case Key::Other: break;
    }
    return o;
}

// ---- the reading view ----
ReadingGeom readingGeometry(double winW, double winH, const Metrics& m) {
    ReadingGeom g;
    const double colW = std::min<double>(m.em(34), winW - 2.0 * m.s16);
    const double y = m.topBar + m.s12;
    g.column = {(winW - colW) / 2, y, colW, std::max(1.0, winH - y - m.s12)};
    g.rail = winW > m.em(60) + colW;
    if (g.rail) { const double tw = m.em(14); g.toc = {g.column.x - m.s24 - tw, y, tw, g.column.h}; }
    return g;
}

// ---- search ----
namespace {
std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }
std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') a++;
    while (b > a && s[b - 1] == ' ') b--;
    return s.substr(a, b - a);
}
}  // namespace

std::vector<int> searchResults(const std::string& query, const std::vector<SearchItem>& items, int maxResults, const std::vector<std::string>& recent) {
    const std::string q = lower(trim(query));
    const auto rank = [&](int i) { for (std::size_t r = 0; r < recent.size(); r++) if (recent[r] == items[i].id) return static_cast<int>(r); return 1 << 20; };
    std::vector<std::array<int, 3>> hit;                                     // (score, recent rank, index)
    if (q.empty()) {
        std::vector<int> out;
        for (const std::string& id : recent) {
            if (static_cast<int>(out.size()) >= maxResults) break;
            for (std::size_t i = 0; i < items.size(); i++) if (items[i].id == id) { out.push_back(static_cast<int>(i)); break; }
        }
        return out;
    }
    for (std::size_t i = 0; i < items.size(); i++) {
        const std::string l = lower(items[i].label);
        int score = -1;
        if (l.compare(0, q.size(), q) == 0) score = 0;
        else {
            for (std::size_t p = l.find(q); p != std::string::npos; p = l.find(q, p + 1))
                if (p > 0 && (l[p - 1] == ' ' || l[p - 1] == '/' || l[p - 1] == '(' || l[p - 1] == '-')) { score = 1; break; }
            if (score < 0 && l.find(q) != std::string::npos) score = 2;
        }
        if (score >= 0) hit.push_back({score, rank(static_cast<int>(i)), static_cast<int>(i)});
    }
    std::sort(hit.begin(), hit.end());                                       // score, then recent rank, then the list's order
    std::vector<int> out;
    for (const auto& h : hit) { if (static_cast<int>(out.size()) >= maxResults) break; out.push_back(h[2]); }
    return out;
}

std::vector<std::string> pushRecent(const std::vector<std::string>& recent, const std::string& id, std::size_t keep) {
    std::vector<std::string> out{id};
    for (const std::string& r : recent) if (r != id && out.size() < keep) out.push_back(r);
    return out;
}
std::string packRecent(const std::vector<std::string>& recent) {
    std::string s;
    for (const std::string& r : recent) s += (s.empty() ? "" : ";") + r;
    return s;
}
std::vector<std::string> unpackRecent(const std::string& packed) {
    std::vector<std::string> out;
    std::size_t a = 0;
    while (a <= packed.size()) {
        std::size_t b = packed.find(';', a);
        if (b == std::string::npos) b = packed.size();
        if (b > a) out.push_back(packed.substr(a, b - a));
        a = b + 1;
    }
    return out;
}

// ---- the fact line ----
double factAlpha(double t, double interval, double fade) {
    if (t <= 0) return 0;
    if (t < fade) return t / fade;
    if (t <= interval) return 1;
    if (t < interval + fade) return 1 - (t - interval) / fade;
    return 0;
}

bool factTick(FactClock& c, double dt, bool hovered, bool reduced, double interval, double fade) {
    if (reduced) return false;
    if (hovered) { if (c.t > interval) c.t = interval; else if (c.t < fade) c.t = fade; return false; }          // under the pointer the text is fully there, fading in or out or not
    c.t += dt;
    if (c.t >= interval + fade) { c.t = 0; return true; }
    return false;
}

Rect factLineRect(const Frame& f, const Metrics& m, double textW, double textH) {
    const double h = std::max<double>(m.control, textH > 0 ? textH + m.s4 : 0), arrows = 2.0 * m.row + m.s4, maxW = m.em(44), x0 = f.window.x + m.s12, wanted = std::min(textW + arrows, maxW);
    Rect r{x0, f.toolbar.y + (f.toolbar.h - h) / 2, 0, h};
    const double beside = f.toolbar.x - m.s16 - x0;
    if (beside >= m.em(14)) r.w = std::min(wanted, beside);
    else {                                                                   // no room beside the toolbar: one row above it, left of the readout
        r.y = f.toolbar.y - m.s8 - h;
        const bool nearReadout = f.readout.y < r.y + h && f.readout.bottom() > r.y;
        const double limit = nearReadout ? f.readout.x - m.s16 : (f.freeW > 0 ? f.freeW : f.window.right()) - m.s12;
        r.w = std::max(1.0, std::min(wanted, limit - x0));
    }
    return clampInto(r, Rect{f.window.x, f.window.y, f.freeW > 0 ? f.freeW : f.window.w, f.window.h});
}

double readoutContrast(const Rgb& t) {
    const Rgb black{0, 0, 0}, white{1, 1, 1};
    return std::min(contrastRatio(t, over(black, kReadoutAlpha, black)), contrastRatio(t, over(black, kReadoutAlpha, white)));
}
Rgb readoutText(const ThemeColors& c) {
    if (readoutContrast(c.muted) >= 4.5) return c.muted;
    if (readoutContrast(c.fg) >= 4.5) return c.fg;
    return Rgb{1, 1, 1};
}

Rgb textShadow(const ThemeColors& c) {
    const Rgb black{0, 0, 0}, white{1, 1, 1};
    return contrastRatio(c.fg, black) >= contrastRatio(c.fg, white) ? black : white;
}
double factContrast(const ThemeColors& c) {
    const Rgb sh = textShadow(c), tx = factTextColour(c);
    return std::min(contrastRatio(tx, over(sh, kShadowAlpha, Rgb{0, 0, 0})), contrastRatio(tx, over(sh, kShadowAlpha, Rgb{1, 1, 1})));
}
Rgb factTextColour(const ThemeColors& c) {
    const Rgb sh = textShadow(c);
    const double m = std::min(contrastRatio(c.muted, over(sh, kShadowAlpha, Rgb{0, 0, 0})), contrastRatio(c.muted, over(sh, kShadowAlpha, Rgb{1, 1, 1})));
    return m >= 4.5 ? c.muted : c.fg;
}

// ---- remembered panel states ----
namespace {
struct Token { std::string name; char state; };
std::vector<Token> parseStates(const std::string& s) {
    std::vector<Token> out;
    std::size_t a = 0;
    while (a <= s.size()) {
        std::size_t b = s.find(',', a);
        if (b == std::string::npos) b = s.size();
        const std::string t = s.substr(a, b - a);
        a = b + 1;
        const std::size_t eq = t.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 2 != t.size() || (t[eq + 1] != 'c' && t[eq + 1] != 'o')) continue;
        out.push_back({t.substr(0, eq), t[eq + 1]});
    }
    return out;
}
}  // namespace

bool panelCollapsed(const std::string& saved, const std::string& name, bool dflt) {
    for (const Token& t : parseStates(saved)) if (t.name == name) return t.state == 'c';
    return dflt;
}

std::string withPanelState(const std::string& saved, const std::string& name, bool collapsed) {
    std::vector<Token> toks = parseStates(saved);
    bool found = false;
    for (Token& t : toks) if (t.name == name) { t.state = collapsed ? 'c' : 'o'; found = true; }
    if (!found) toks.push_back({name, collapsed ? 'c' : 'o'});
    std::string out;
    for (const Token& t : toks) out += (out.empty() ? "" : ",") + t.name + "=" + t.state;
    return out;
}

}  // namespace app::shell
