#include "html_view.h"
#include "style_draw.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <set>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <SDL3/SDL_misc.h>

#include "app/html_entities.h"
#include "fonts.h"
#include "look.h"
#include "imgui.h"
#include "ui/reading.h"
#include "ui/shell_test.h"
#include "ui/shell_widgets.h"

namespace ui {

namespace {

std::string g_focus;                                  // 0.40: the About source entry asked for by a fact's "Show in About" (words in its text); "" = none
bool g_focusScroll = false;                           // scroll to it once, the first frame it is drawn

// ---- a tiny HTML tree ----
struct Node {
    std::string tag, cls, text, href;     // text: for text nodes (tag empty); href: for links
    std::map<std::string, std::string> style;
    std::vector<std::unique_ptr<Node>> kids;
    bool has(const char* c) const { return (" " + cls + " ").find(std::string(" ") + c + " ") != std::string::npos; }
};

std::string attr(const std::string& tag, const char* name) {
    const std::string key = std::string(name) + "=\"";
    const std::size_t a = tag.find(key);
    if (a == std::string::npos) return "";
    const std::size_t b = tag.find('"', a + key.size());
    return tag.substr(a + key.size(), b - a - key.size());
}

std::unique_ptr<Node> parse(const std::string& h) {
    auto root = std::make_unique<Node>();
    root->tag = "root";
    std::vector<Node*> stack = {root.get()};
    std::string text;
    auto flush = [&] {
        if (text.empty()) return;
        auto t = std::make_unique<Node>();
        t->text = app::decodeHtmlEntities(text);   // 0.15 stage 2: "&rsquo;" is a character, not typed text
        stack.back()->kids.push_back(std::move(t));
        text.clear();
    };
    for (std::size_t i = 0; i < h.size();) {
        if (h.compare(i, 9, "[[chips]]") == 0) {
            flush();
            auto c = std::make_unique<Node>();
            c->tag = "chips";
            stack.back()->kids.push_back(std::move(c));
            i += 9;
            continue;
        }
        if (h[i] != '<') {
            const char c = h[i++];
            text += (c == '\n' || c == '\t' || c == '\r') ? ' ' : c;
            continue;
        }
        const std::size_t e = h.find('>', i);
        if (e == std::string::npos) break;
        const std::string tag = h.substr(i + 1, e - i - 1);
        i = e + 1;
        flush();
        if (tag[0] == '/') { if (stack.size() > 1) stack.pop_back(); continue; }
        auto n = std::make_unique<Node>();
        n->tag = tag.substr(0, tag.find_first_of(" /"));
        n->cls = attr(tag, "class");
        n->href = attr(tag, "href");
        const std::string st = attr(tag, "style");
        for (std::size_t a = 0; a < st.size();) {
            std::size_t b = st.find(';', a);
            if (b == std::string::npos) b = st.size();
            const std::string kv = st.substr(a, b - a);
            const std::size_t c = kv.find(':');
            if (c != std::string::npos) n->style[kv.substr(0, c)] = kv.substr(c + 1);
            a = b + 1;
        }
        Node* raw = n.get();
        stack.back()->kids.push_back(std::move(n));
        if (raw->tag != "br" && tag.back() != '/') stack.push_back(raw);
    }
    flush();
    return root;
}

bool isInline(const Node& n) {
    return n.tag.empty() || n.tag == "a" || n.tag == "b" || n.tag == "i" || n.tag == "small" || n.tag == "sub" || n.tag == "span" || n.tag == "br";
}

ImU32 cssColor(const std::string& c, float alpha = 1) {
    if (c.size() == 7 && c[0] == '#') {
        const unsigned long v = std::strtoul(c.c_str() + 1, nullptr, 16);
        return IM_COL32((v >> 16) & 255, (v >> 8) & 255, v & 255, int(alpha * 255));
    }
    return IM_COL32(128, 128, 128, int(alpha * 255));
}

float S() { return ImGui::GetFontSize() / 16.0f; }
const char* g_flowKind = nullptr;      // the reading view: the kind a heading reports to PL_LOG_SHELL ("h3"); null: body or caption by size

// ---- inline text: runs laid out word by word ----
struct Run { std::string text; bool bold, italic, small, sub, br; ImU32 dot; std::string href; };
void collect(const Node& n, bool bold, bool italic, bool small, bool sub, std::vector<Run>& out, const std::string& href = "") {
    if (n.tag.empty()) { out.push_back({n.text, bold, italic, small, sub, false, 0, href}); return; }
    if (n.tag == "br") { out.push_back({"", bold, italic, small, sub, true, 0, ""}); return; }
    if (n.tag == "span" && n.has("lrnDot")) { out.push_back({"", bold, italic, small, sub, false, cssColor(n.style.count("background") ? n.style.at("background") : ""), ""}); return; }
    const bool b = bold || n.tag == "b", it = italic || n.tag == "i", sm = small || n.tag == "small", su = sub || n.tag == "sub";
    // a link
    const std::string h = n.tag == "a" ? n.href : href;
    for (const auto& k : n.kids) collect(*k, b, it, sm, su, out, h);
}

// Lays out runs from (x, y) to the right edge `right`; size in points; returns the y below the text.
float flow(const std::vector<Run>& runs, float x0, float y, float right, float sizePt, ImU32 col, ImU32 boldCol, bool forceBold = false, bool upper = false) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const Fonts& F = fonts();
    const float rb = readingFloorBody(), rc = readingFloorCaption();   // the reading view of the new UI: no page text under the Body (14 pt and up) or the Caption size
    const float size = rb > 0 ? std::max(sizePt * S(), sizePt >= 14 ? rb : rc) : sizePt * S(), lineH = size * 1.45f;
    float x = x0;
    int chars = 0;                                       // characters on the line being laid out (the reading view reports the longest line)
    bool lineEmpty = true, any = false;
    std::string prevHref;                                // 0.15 stage 4: a link's underline is one line, not one piece per word: the gap between two words of the same link on one line is filled
    float prevLinkEnd = 0, prevLinkY = -1;
    for (const Run& r : runs) {
        if (r.br) { x = x0; y += lineH; lineEmpty = true; chars = 0; continue; }
        if (r.dot) {
            const float d = 9 * S();
            dl->AddCircleFilled(ImVec2(x + d / 2, y + size * 0.55f), d / 2, r.dot);
            x += d + 5 * S();
            lineEmpty = false; any = true;
            continue;
        }
        ImFont* font = r.bold || forceBold ? F.bold : r.italic ? F.italic : F.regular;
        const float sz = rb > 0 ? std::max(r.small || r.sub ? size * 0.82f : size, r.small || r.sub ? rc : size) : r.small || r.sub ? size * 0.82f : size;
        if (rb > 0 && !r.text.empty()) { std::size_t n = std::min<std::size_t>(24, r.text.size()); while (n > 0 && n < r.text.size() && (static_cast<unsigned char>(r.text[n]) & 0xC0) == 0x80) n--; auditText(g_flowKind ? g_flowKind : sz >= rb ? "pbody" : "pcaption", sz, r.text.substr(0, n)); }   // (cut at a whole character)
        std::string t = r.text;
        if (upper) for (char& c : t) c = char(std::toupper(static_cast<unsigned char>(c)));
        std::size_t p = 0;
        while (p < t.size()) {
            std::size_t q = t.find(' ', p);
            q = q == std::string::npos ? t.size() : q + 1;
            const std::string word = displayable(t.substr(p, q - p), font);
            p = q;
            if (word.empty() || (lineEmpty && word == " ")) continue;
            std::string trimmed = word;
            while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
            const float wt = font->CalcTextSizeA(sz, FLT_MAX, 0, trimmed.c_str()).x, w = font->CalcTextSizeA(sz, FLT_MAX, 0, word.c_str()).x;
            int wc = 0;                                  // the word's characters (not bytes)
            for (char ch : trimmed) if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) wc++;
            const bool tooMany = rb > 0 && sizePt >= 14 && !r.small && chars + wc > 75;   // 0.22: the reading view's Body lines also end at 75 characters (a 34 em column held 81 narrow ones in About)
            if (!lineEmpty && (x + wt > right || tooMany)) { x = x0; y += lineH; lineEmpty = true; chars = 0; if (word == " ") continue; }
            if (rb > 0) { for (char ch : trimmed) if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) chars++; chars += word.size() > trimmed.size() ? 1 : 0; readingLongestLine(chars - (word.size() > trimmed.size() ? 1 : 0), sizePt >= 14 && !r.small); }
            const ImVec2 at(x, y + (r.sub ? size * 0.35f : (size - sz) * 0.8f));
            if (!r.href.empty()) {                    // a link (.srcBox a): accent colour, underlined; a click opens it in the browser
                const ImU32 lc = ui::col(look().w.accent);
                dl->AddText(font, sz, at, lc, word.c_str());
                const float ly = at.y + sz + 1;
                const float prevX0 = prevLinkEnd;
                const bool sameLink = rb > 0 && r.href == prevHref && prevLinkY == ly && at.x > prevLinkEnd;   // (the new UI's reading view only: the old layout draws exactly what it did)
                dl->AddLine(ImVec2(sameLink ? prevLinkEnd : at.x, ly), ImVec2(at.x + wt, ly), lc);
                prevHref = r.href; prevLinkEnd = at.x + wt; prevLinkY = ly;
                if (rb > 0 && shellLogOn()) {                 // for the automatic checks: each piece of underline once (the test joins them and looks for gaps inside one link)
                    static std::set<std::string> seen;
                    char buf[512];
                    std::snprintf(buf, sizeof buf, "PLLINK id=%s y=%.1f x0=%.1f x1=%.1f", r.href.c_str(), ly, sameLink ? prevX0 : at.x, at.x + wt);
                    if (seen.insert(buf).second) { std::printf("%s\n", buf); std::fflush(stdout); }
                }
                if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(at, ImVec2(at.x + w, at.y + sz + 2))) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ui::SetTooltip("%s", r.href.c_str());
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) SDL_OpenURL(r.href.c_str());
                }
            } else dl->AddText(font, sz, at, r.bold && !forceBold ? boldCol : col, word.c_str());
            x += w;
            lineEmpty = false; any = true;
        }
    }
    return any || !lineEmpty ? y + lineH : y;
}

struct Ctx { std::function<void()> chips; };

std::string plainText(const Node& n) { std::string t = n.text; for (const auto& k : n.kids) t += plainText(*k); return t; }

// Draws block node n inside [left, right] starting at the cursor; advances the cursor.
void block(const Node& n, const Ctx& cx, float left, float right, float sizePt, bool muted);

void blocks(const Node& n, const Ctx& cx, float left, float right, float sizePt, bool muted) {
    std::vector<Run> runs;
    auto flushRuns = [&] {
        bool content = false;
        for (const Run& r : runs) if (r.dot || r.br || r.text.find_first_not_of(' ') != std::string::npos) content = true;
        if (content) {
            const ImVec2 c = ImGui::GetCursorScreenPos();
            const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text), mu = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            const float y = flow(runs, left, c.y, right, sizePt, muted ? mu : fg, fg);
            ImGui::SetCursorScreenPos(ImVec2(left, y));
            ImGui::Dummy(ImVec2(right - left, 0));
        }
        runs.clear();
    };
    for (const auto& k : n.kids) {
        if (isInline(*k)) { collect(*k, false, false, false, false, runs); continue; }
        flushRuns();
        block(*k, cx, left, right, sizePt, muted);
    }
    flushRuns();
}

void block(const Node& n, const Ctx& cx, float left, float right, float sizePt, bool muted) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = S();
    const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text), mu = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImGui::SetCursorScreenPos(ImVec2(left, ImGui::GetCursorScreenPos().y));
    if (n.tag == "chips") { if (cx.chips) cx.chips(); ImGui::Dummy(ImVec2(0, 4 * s)); return; }
    if (n.tag == "p") {
        const bool m = n.has("lrnMuted");
        if (!m && !n.kids.empty() && n.kids[0]->tag == "b" && !n.kids[0]->kids.empty() && !n.kids[0]->kids[0]->text.empty() && n.kids[0]->kids[0]->text.back() == ':')
            noteHeading(2, n.kids[0]->kids[0]->text.substr(0, n.kids[0]->kids[0]->text.size() - 1));      // "The developer:" and the like lead a section
        blocks(n, cx, left, right, m ? 13 : sizePt, m || muted);
        ImGui::Dummy(ImVec2(0, 6 * s));
        return;
    }
    if (n.tag == "ul") {
        const bool src = n.has("srcL");                          // the About page's source lists: smaller, muted, closer together
        const float sz = src ? 13 : 14;
        for (const auto& li : n.kids) {
            if (li->tag != "li") continue;
            const ImVec2 c = ImGui::GetCursorScreenPos();
            dl->AddText(ImGui::GetFont(), sz * s, ImVec2(left + 6 * s, c.y), muted || src ? mu : fg, "•");
            const bool focus = src && !g_focus.empty() && plainText(*li).find(g_focus) != std::string::npos;
            if (focus) dl->ChannelsSplit(2), dl->ChannelsSetCurrent(1);       // the text over the highlight drawn after it
            blocks(*li, cx, left + 20 * s, right, sz, muted || src);
            if (focus) {                                          // 0.40: the fact's source: an accent box behind it and a bar at its left
                const float y1 = ImGui::GetCursorScreenPos().y;
                dl->ChannelsSetCurrent(0);
                dl->AddRectFilled(ImVec2(left, c.y - 3 * s), ImVec2(right, y1 + 1 * s), col(look().w.accent, 0.16f), 4 * s);
                dl->AddRectFilled(ImVec2(left, c.y - 3 * s), ImVec2(left + 3 * s, y1 + 1 * s), col(look().w.accent));
                dl->ChannelsMerge();
                if (g_focusScroll) { ImGui::SetScrollFromPosY(c.y - ImGui::GetWindowPos().y, 0.25f); g_focusScroll = false; }
                if (shellLogOn()) { std::printf("PLFOCUS top=%.1f bottom=%.1f\n", c.y, y1); std::fflush(stdout); }
            }
            ImGui::Dummy(ImVec2(0, (src ? 0 : 4) * s));
        }
        ImGui::Dummy(ImVec2(0, 4 * s));
        return;
    }
    if (n.tag == "div" && n.has("srcH")) {                       // a heading in the source lists
        ImGui::Dummy(ImVec2(0, 6 * s));
        std::vector<Run> runs;
        collect(n, true, false, false, false, runs);
        const bool rv = readingFloorBody() > 0;
        if (rv) noteHeading(3, displayable(n.text.empty() && !n.kids.empty() ? n.kids[0]->text : n.text));
        g_flowKind = rv ? "h3" : nullptr;
        const float y = flow(runs, left, ImGui::GetCursorScreenPos().y, right, rv ? sh::headingPx("h3") / S() : 13, fg, fg, true);   // (a heading of the reading view is H3 sized: 17 u, floor 15 px)
        g_flowKind = nullptr;
        ImGui::SetCursorScreenPos(ImVec2(left, y));
        ImGui::Dummy(ImVec2(right - left, 0));
        return;
    }
    if (n.tag == "div" && n.has("lrnMuted")) {                   // a muted note inside a block
        blocks(n, cx, left, right, 13, true);
        ImGui::Dummy(ImVec2(0, 4 * s));
        return;
    }
    if (n.tag == "div" && n.has("lrnCard")) {                  // a card: a soft box behind its text
        const float pad = 12 * s;
        const ImVec2 o = ImGui::GetCursorScreenPos();
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        ImGui::SetCursorScreenPos(ImVec2(left + pad, o.y + pad));
        for (const auto& k : n.kids) {
            if (k->tag != "div") continue;
            const bool big = k->has("lrnBig");
            float sz = big ? 16.0f : 13.0f;
            if (k->style.count("font-size")) sz = float(std::atof(k->style.at("font-size").c_str()));
            if (k->style.count("margin-top") || !big) ImGui::Dummy(ImVec2(0, (k->style.count("margin-top") ? 8 : 4) * s));
            blocks(*k, cx, left + pad + 2 * s, right - pad, sz, !big);
        }
        const float bottom = ImGui::GetCursorScreenPos().y + pad * 0.6f;
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(o, ImVec2(right, bottom), col(look().w.soft, 31/255.f), 12 * s);
        dl->ChannelsMerge();
        ImGui::SetCursorScreenPos(ImVec2(left, bottom + 10 * s));
        ImGui::Dummy(ImVec2(right - left, 0));
        return;
    }
    if (n.tag == "div" && n.has("lrnFacts")) {                 // fact boxes in a grid (at least 170 points wide)
        const float gap = 8 * s, W = right - left;
        const int cols = std::max(1, int((W + gap) / (170 * s + gap)));
        const float cw = (W - gap * (cols - 1)) / cols;
        float rowTop = ImGui::GetCursorScreenPos().y, rowBottom = rowTop;
        int i = 0;
        for (const auto& f : n.kids) {
            if (!f->has("lrnF")) continue;
            const int c = i % cols;
            if (c == 0 && i > 0) { rowTop = rowBottom + gap; }
            const float x = left + c * (cw + gap), pad = 9 * s;
            ImGui::SetCursorScreenPos(ImVec2(x + pad, rowTop + pad));
            for (const auto& k : f->kids) {
                std::vector<Run> runs;
                collect(*k, false, false, false, false, runs);
                const ImVec2 cp = ImGui::GetCursorScreenPos();
                float y;
                if (k->has("lrnFa")) y = flow(runs, x + pad, cp.y, x + cw - pad, 12, mu, mu, false, true);
                else if (k->has("lrnFb")) y = flow(runs, x + pad, cp.y + 2 * s, x + cw - pad, 18, fg, fg, true);
                else y = flow(runs, x + pad, cp.y, x + cw - pad, 13, mu, fg);
                ImGui::SetCursorScreenPos(ImVec2(x + pad, y));
            }
            const float b = ImGui::GetCursorScreenPos().y + pad * 0.6f;
            rowBottom = std::max(rowBottom, b);
            i++;
            if (i % cols == 0 || i == int(n.kids.size())) {
                // draw the borders of this row's boxes at the row's full height
                for (int k = 0; k <= (i - 1) % cols; k++) {
                    const float bx = left + k * (cw + gap);
                    dl->AddRect(ImVec2(bx, rowTop), ImVec2(bx + cw, rowBottom), ImGui::GetColorU32(ImGuiCol_Border), 10 * s);
                }
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(left, rowBottom + 10 * s));
        ImGui::Dummy(ImVec2(W, 0));
        return;
    }
    auto bar = [&](const Node& track, float x0, float x1, float y, float h) {   // a rounded track with a coloured bar in it
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + h), col(look().w.soft, 38/255.f), h / 2);
        for (const auto& b : track.kids) {
            if (!b->has("lrnBar")) continue;
            const float w = float(std::atof(b->style.count("width") ? b->style.at("width").c_str() : "0")) / 100;
            dl->AddRectFilled(ImVec2(x0, y), ImVec2(x0 + (x1 - x0) * w, y + h), cssColor(b->style.count("background") ? b->style.at("background") : ""), h / 2);
        }
    };
    if (n.tag == "div" && n.has("lrnRow")) {                   // a name (150 points) and a log-scale bar
        const float col = 150 * s, y0 = ImGui::GetCursorScreenPos().y;
        float yText = y0;
        const Node* track = nullptr;
        for (const auto& k : n.kids) {
            if (k->has("lrnRn")) { std::vector<Run> runs; collect(*k, false, false, false, false, runs); yText = flow(runs, left, y0, left + col, 13, fg, fg); }
            if (k->has("lrnTr")) track = k.get();
        }
        if (track) bar(*track, left + col + 10 * s, right, (y0 + yText) / 2 - 7 * s, 14 * s);
        ImGui::SetCursorScreenPos(ImVec2(left, yText + 7 * s));
        ImGui::Dummy(ImVec2(right - left, 0));
        return;
    }
    if (n.tag == "div" && n.has("lrnTr")) {                    // a bar on its own
        const float h = float(n.style.count("height") ? std::atof(n.style.at("height").c_str()) : 14) * s, y = ImGui::GetCursorScreenPos().y;
        bar(n, left, right, y, h);
        ImGui::SetCursorScreenPos(ImVec2(left, y + h + 8 * s));
        ImGui::Dummy(ImVec2(right - left, 0));
        return;
    }
    if (n.tag == "div" && n.has("lrnStack")) {                 // the stacked bar of protons, neutrons and electrons
        const float h = 16 * s, y = ImGui::GetCursorScreenPos().y + 6 * s;
        dl->AddRectFilled(ImVec2(left, y), ImVec2(right, y + h), col(look().w.neutral, 51/255.f), h / 2);
        float x = left;
        for (const auto& k : n.kids) {
            const float w = float(std::atof(k->style.count("width") ? k->style.at("width").c_str() : "0")) / 100 * (right - left);
            const float x1 = std::min(right, x + w);
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x1, y + h), cssColor(k->style.count("background") ? k->style.at("background") : ""), x == left || x1 >= right ? h / 2 : 0,
                              x == left && x1 >= right ? 0 : x == left ? ImDrawFlags_RoundCornersLeft : x1 >= right ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone);
            x = x1;
        }
        ImGui::SetCursorScreenPos(ImVec2(left, y + h + 10 * s));
        ImGui::Dummy(ImVec2(right - left, 0));
        return;
    }
    blocks(n, cx, left, right, sizePt, muted);                // any other block: its children
}

}  // namespace

void setHtmlFocus(const std::string& words) { g_focus = words; g_focusScroll = !words.empty(); }

void htmlView(const std::string& html, const std::function<void()>& chips) {
    const auto root = parse(html);
    const float left = ImGui::GetCursorScreenPos().x, right = left + ImGui::GetContentRegionAvail().x;
    Ctx cx{chips};
    blocks(*root, cx, left, right, 14, false);
}

}  // namespace ui
