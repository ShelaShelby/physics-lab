#include "rich_text.h"

#include <cfloat>
#include <vector>

#include "app/html_entities.h"
#include "fonts.h"
#include "imgui.h"

namespace ui {

namespace {

enum class Style { Para, Muted, CardBig, CardSmall };
struct Run { std::string text; bool bold, italic, small; };
struct Block { Style style; std::vector<Run> runs; bool inCard; };

// HTML -> blocks of styled runs. Spaces and line breaks in the source fold into single spaces, as in a browser.
std::vector<Block> parse(const std::string& h) {
    std::vector<Block> blocks;
    Block cur{Style::Para, {}, false};
    bool bold = false, italic = false, small = false, inCard = false;
    std::string text;
    const auto flushText = [&]() {
        if (text.empty()) return;
        cur.runs.push_back({app::decodeHtmlEntities(text), bold, italic, small});   // 0.15 stage 2: "&rsquo;" is a character, not typed text
        text.clear();
    };
    const auto endBlock = [&]() {
        flushText();
        bool any = false;
        for (auto& r : cur.runs) for (char c : r.text) if (c != ' ') any = true;
        if (any) blocks.push_back(cur);
        cur = Block{Style::Para, {}, inCard};
    };
    for (std::size_t i = 0; i < h.size();) {
        if (h[i] == '<') {
            const std::size_t e = h.find('>', i);
            if (e == std::string::npos) break;
            const std::string tag = h.substr(i + 1, e - i - 1);
            i = e + 1;
            const bool close = !tag.empty() && tag[0] == '/';
            const std::string name = tag.substr(close ? 1 : 0, tag.find_first_of(" /", close ? 1 : 0) - (close ? 1 : 0));
            if (name == "b") { flushText(); bold = !close; }
            else if (name == "i") { flushText(); italic = !close; }
            else if (name == "small") { flushText(); small = !close; }
            else if (name == "br") { text += '\n'; }
            else if (name == "p" || name == "div") {
                const bool endsCard = close && cur.style == Style::CardSmall;   // a card ends with its small text
                endBlock();
                if (endsCard) { inCard = false; cur.inCard = false; }
                if (!close) {
                    if (tag.find("lrnCard") != std::string::npos) { inCard = true; cur.inCard = true; }
                    if (tag.find("lrnMuted") != std::string::npos) cur.style = Style::Muted;
                    if (tag.find("lrnBig") != std::string::npos) cur.style = Style::CardBig;
                    if (tag.find("lrnSm") != std::string::npos) cur.style = Style::CardSmall;
                }
            }
            continue;
        }
        const char c = h[i++];
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            const bool afterSpace = !text.empty() ? text.back() == ' ' : (cur.runs.empty() || cur.runs.back().text.back() == ' ');
            if (!afterSpace) text += ' ';                  // runs of spaces and line breaks become one space
        } else text += c;
    }
    endBlock();
    return blocks;
}

}  // namespace

void richHeading(const std::string& t) {
    const Fonts& F = fonts();
    ImGui::PushFont(F.bold, ImGui::GetFontSize() * 1.15f);
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(displayable(t, F.bold).c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}

void richText(const std::string& html, bool mutedBase, float sizeScale, float gap) {
    const Fonts& F = fonts();
    const std::vector<Block> blocks = parse(html);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float base = ImGui::GetFontSize() * sizeScale, pad = base * 0.6f;
    const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text), muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    std::size_t i = 0;
    while (i < blocks.size()) {
        const bool card = blocks[i].inCard;
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        if (card) dl->ChannelsSplit(2), dl->ChannelsSetCurrent(1);
        float y = start.y + (card ? pad : 0);
        const float left = start.x + (card ? pad : 0), right = start.x + width - (card ? pad : 0);
        do {   // one block (or all the blocks of one card)
            const Block& b = blocks[i];
            const float size = b.style == Style::CardBig ? base * 1.08f : b.style == Style::CardSmall ? base * 0.92f : base;
            const ImU32 blockCol = b.style == Style::Muted || b.style == Style::CardSmall || mutedBase ? muted : fg;
            float x = left, lineH = size * 1.35f;
            bool lineEmpty = true;
            for (const Run& r : b.runs) {
                ImFont* font = r.bold || b.style == Style::CardBig ? F.bold : r.italic ? F.italic : F.regular;
                const ImU32 col = mutedBase && r.bold ? fg : blockCol;
                const float sz = r.small ? size * 0.85f : size;
                // words with their following space; '\n' (from <br>) starts a new line
                std::size_t p = 0;
                while (p < r.text.size()) {
                    if (r.text[p] == '\n') { x = left; y += lineH; lineEmpty = true; p++; continue; }
                    std::size_t q = r.text.find_first_of(" \n", p);
                    if (q == std::string::npos) q = r.text.size();
                    else if (r.text[q] == ' ') q++;
                    std::string word = displayable(r.text.substr(p, q - p), font);
                    p = q;
                    if (word.empty()) continue;
                    if (lineEmpty && word == " ") continue;
                    const float w = font->CalcTextSizeA(sz, FLT_MAX, 0, word.c_str()).x;
                    std::string trimmed = word;
                    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                    const float wt = font->CalcTextSizeA(sz, FLT_MAX, 0, trimmed.c_str()).x;
                    if (!lineEmpty && x + wt > right) { x = left; y += lineH; lineEmpty = true; if (word == " ") continue; }
                    dl->AddText(font, sz, ImVec2(x, y + (size - sz) * 0.8f), col, word.c_str());
                    x += w;
                    lineEmpty = false;
                }
            }
            y += lineH + (card ? size * 0.25f : size * gap);
            i++;
        } while (card && i < blocks.size() && blocks[i].inCard && blocks[i - 1].style != Style::CardSmall);
        if (card) {
            y += pad * 0.5f;
            dl->ChannelsSetCurrent(0);
            dl->AddRectFilled(start, ImVec2(start.x + width, y), ImGui::GetColorU32(ImGuiCol_FrameBg), base * 0.5f);
            dl->ChannelsMerge();
            y += base * 0.5f;
        }
        ImGui::Dummy(ImVec2(width, y - start.y));
    }
}

}  // namespace ui
