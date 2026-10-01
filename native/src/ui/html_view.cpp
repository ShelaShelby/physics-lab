#include "html_view.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>

#include "fonts.h"
#include "imgui.h"

namespace ui {

namespace {

// ---- a tiny HTML tree ----
struct Node {
    std::string tag, cls, text;           // text: for text nodes (tag empty)
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
        t->text = text;
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
    return n.tag.empty() || n.tag == "b" || n.tag == "i" || n.tag == "small" || n.tag == "sub" || n.tag == "span" || n.tag == "br";
}

ImU32 cssColor(const std::string& c, float alpha = 1) {
    if (c.size() == 7 && c[0] == '#') {
        const unsigned long v = std::strtoul(c.c_str() + 1, nullptr, 16);
        return IM_COL32((v >> 16) & 255, (v >> 8) & 255, v & 255, int(alpha * 255));
    }
    return IM_COL32(128, 128, 128, int(alpha * 255));
}

float S() { return ImGui::GetFontSize() / 16.0f; }

// ---- inline text: runs laid out word by word ----
struct Run { std::string text; bool bold, italic, small, sub, br; ImU32 dot; };
void collect(const Node& n, bool bold, bool italic, bool small, bool sub, std::vector<Run>& out) {
    if (n.tag.empty()) { out.push_back({n.text, bold, italic, small, sub, false, 0}); return; }
    if (n.tag == "br") { out.push_back({"", bold, italic, small, sub, true, 0}); return; }
    if (n.tag == "span" && n.has("lrnDot")) { out.push_back({"", bold, italic, small, sub, false, cssColor(n.style.count("background") ? n.style.at("background") : "")}); return; }
    const bool b = bold || n.tag == "b" || (n.tag == "span" && n.has("monoKey")), it = italic || n.tag == "i", sm = small || n.tag == "small", su = sub || n.tag == "sub";
    for (const auto& k : n.kids) collect(*k, b, it, sm, su, out);
}

// Lays out runs from (x, y) to the right edge `right`; size in points; returns the y below the text.
float flow(const std::vector<Run>& runs, float x0, float y, float right, float sizePt, ImU32 col, ImU32 boldCol, bool forceBold = false, bool upper = false) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const Fonts& F = fonts();
    const float size = sizePt * S(), lineH = size * 1.45f;
    float x = x0;
    bool lineEmpty = true, any = false;
    for (const Run& r : runs) {
        if (r.br) { x = x0; y += lineH; lineEmpty = true; continue; }
        if (r.dot) {
            const float d = 9 * S();
            dl->AddCircleFilled(ImVec2(x + d / 2, y + size * 0.55f), d / 2, r.dot);
            x += d + 5 * S();
            lineEmpty = false; any = true;
            continue;
        }
        ImFont* font = r.bold || forceBold ? F.bold : r.italic ? F.italic : F.regular;
        const float sz = r.small || r.sub ? size * 0.82f : size;
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
            if (!lineEmpty && x + wt > right) { x = x0; y += lineH; lineEmpty = true; if (word == " ") continue; }
            dl->AddText(font, sz, ImVec2(x, y + (r.sub ? size * 0.35f : (size - sz) * 0.8f)), r.bold && !forceBold ? boldCol : col, word.c_str());
            x += w;
            lineEmpty = false; any = true;
        }
    }
    return any || !lineEmpty ? y + lineH : y;
}

struct Ctx { std::function<void()> chips; };

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
        blocks(n, cx, left, right, m ? 13 : sizePt, m || muted);
        ImGui::Dummy(ImVec2(0, 6 * s));
        return;
    }
    if (n.tag == "ul") {
        for (const auto& li : n.kids) {
            if (li->tag != "li") continue;
            const ImVec2 c = ImGui::GetCursorScreenPos();
            dl->AddText(ImGui::GetFont(), 14 * s, ImVec2(left + 6 * s, c.y), muted ? mu : fg, "•");
            blocks(*li, cx, left + 20 * s, right, 14, muted);
            ImGui::Dummy(ImVec2(0, 4 * s));
        }
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
        dl->AddRectFilled(o, ImVec2(right, bottom), IM_COL32(120, 140, 200, 31), 12 * s);
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
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + h), IM_COL32(120, 140, 200, 38), h / 2);
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
        dl->AddRectFilled(ImVec2(left, y), ImVec2(right, y + h), IM_COL32(128, 128, 128, 51), h / 2);
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

void htmlView(const std::string& html, const std::function<void()>& chips) {
    const auto root = parse(html);
    const float left = ImGui::GetCursorScreenPos().x, right = left + ImGui::GetContentRegionAvail().x;
    Ctx cx{chips};
    blocks(*root, cx, left, right, 14, false);
}

}  // namespace ui
