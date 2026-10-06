#include "subject_card.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace orbit {

namespace {

constexpr const char* kDot = " · ";

// The label words a "Label: value" line may start with (the words card_text.cpp uses). Only these are split: a long note that happens to contain ": " stays a plain line.
const char* const kLabels[] = {"Size", "Mass", "Distance", "Speed", "Orbit", "One lap", "Temperature", "Radius", "Nucleus", "Speed now", "Closest to the Sun", "One trip round the Sun",
                               "Last closest approach", "Next", "Closest approach", "Surface (rough)", "Habitable zone (rough)", "Frost line (rough)", "Lifetime (rough)", "Phase now", "Total mass"};

bool split(const std::string& line, std::string& label, std::string& value) {
    const std::size_t c = line.find(": ");
    if (c == std::string::npos) return false;
    const std::string l = line.substr(0, c);
    for (const char* k : kLabels) if (l == k) { label = l; value = line.substr(c + 2); return true; }
    return false;
}

bool startsWith(const std::string& s, const char* p) { return s.compare(0, std::strlen(p), p) == 0; }

std::string beforeParen(const std::string& v) { const std::size_t p = v.find(" ("); return p == std::string::npos ? v : v.substr(0, p); }
std::string insideParen(const std::string& v) { const std::size_t a = v.find('('), b = v.find(')', a == std::string::npos ? 0 : a); return a == std::string::npos || b == std::string::npos ? v : v.substr(a + 1, b - a - 1); }

// "2.1 – 3.3 AU" / "50 AU": the first figure with its unit after "about " in a line (the belts' places are written "about 2.1 – 3.3 AU", "out to about 50 AU", "to about 200,000 AU")
std::string aboutAU(const std::string& line) {
    const std::size_t a = line.rfind("about ");
    if (a == std::string::npos) return "";
    const std::string rest = line.substr(a + 6);
    std::size_t i = 0;
    while (i < rest.size() && (std::isdigit(static_cast<unsigned char>(rest[i])) || rest[i] == ',' || rest[i] == '.')) i++;
    if (i == 0) return "";
    std::size_t j = i;
    static const std::string dash = " – ";
    if (rest.compare(j, dash.size(), dash) == 0) {
        std::size_t k = j + dash.size(), e = k;
        while (e < rest.size() && (std::isdigit(static_cast<unsigned char>(rest[e])) || rest[e] == ',' || rest[e] == '.')) e++;
        if (e > k) j = e;
    }
    if (rest.compare(j, 3, " AU") == 0) return rest.substr(0, j + 3);
    return "";
}

std::string degrees(const std::string& line) {          // "60°"
    const std::size_t d = line.find("°");
    if (d == std::string::npos) return "";
    std::size_t a = d;
    while (a > 0 && std::isdigit(static_cast<unsigned char>(line[a - 1]))) a--;
    return a == d ? "" : line.substr(a, d + std::strlen("°") - a);
}

}  // namespace

SubjectCard subjectCard(const Selection& sel, const std::string& beltKey, const Card& card) {
    SubjectCard s;
    s.all = card.lines;
    const std::vector<std::string>& L = card.lines;
    std::vector<bool> used(L.size(), false);

    // ---- the name and the kind ----
    const std::size_t dot = card.title.find(kDot);
    s.name = dot == std::string::npos ? card.title : card.title.substr(0, dot);
    if (dot != std::string::npos) s.kind = card.title.substr(dot + std::strlen(kDot));
    else if (sel.kind == Selection::Bary) s.kind = "balance point";
    else if (sel.kind == Selection::Belt) s.kind = beltKey == "oort" ? "cloud of icy bodies" : "belt of small bodies";
    else if (sel.kind == Selection::Body) s.kind = "comet";
    else if (sel.kind == Selection::Far && !L.empty()) { s.kind = L[0]; used[0] = true; }
    if (sel.kind == Selection::Star) { /* "Sun · G-type star": the kind is after the dot */ }

    // ---- the one big number ----
    const auto keyFrom = [&](const char* prefix, bool paren, const std::string& caption) {
        for (std::size_t i = 0; i < L.size(); i++) if (!used[i] && startsWith(L[i], prefix)) {
            const std::string v = L[i].substr(std::strlen(prefix));
            s.keyValue = paren ? insideParen(v) : beforeParen(v);
            s.keyCaption = caption;
            used[i] = true;
            return true;
        }
        return false;
    };
    bool keyed = false;
    if (sel.kind == Selection::Star) keyed = keyFrom("Radius: ", true, "radius");
    else if (sel.kind == Selection::Bary) {
        for (std::size_t i = 0; i < L.size() && !keyed; i++) if (startsWith(L[i], "Now ")) {
            const std::size_t e = L[i].find(" from the Sun");
            if (e != std::string::npos) { s.keyValue = L[i].substr(4, e - 4); s.keyCaption = "from the Sun’s centre now"; used[i] = true; keyed = true; }
        }
    } else if (sel.kind == Selection::Belt) {
        if (beltKey == "trojan") {
            for (std::size_t i = 0; i < L.size() && !keyed; i++) { const std::string d = degrees(L[i]); if (!d.empty()) { s.keyValue = d; s.keyCaption = "ahead and behind Jupiter"; used[i] = true; keyed = true; } }
        } else {
            for (std::size_t i = 0; i < L.size() && !keyed; i++) {
                const std::string a = aboutAU(L[i]);
                if (!a.empty()) { s.keyValue = a; s.keyCaption = beltKey == "main" ? "from the Sun" : "outer edge, from the Sun"; used[i] = true; keyed = true; }
            }
        }
    } else {
        keyed = keyFrom("Distance from the Sun now: ", false, "from the Sun now") || keyFrom("Distance from the Sun: ", false, "from the Sun") || keyFrom("Distance: ", false, "from the Sun");
        if (!keyed)
            for (std::size_t i = 0; i < L.size() && !keyed; i++) if (startsWith(L[i], "Distance from ")) {     // a moon: "Distance from Earth: 384,400 km"
                const std::size_t c = L[i].find(": ");
                if (c == std::string::npos) continue;
                s.keyValue = beforeParen(L[i].substr(c + 2));
                s.keyCaption = L[i].substr(std::strlen("Distance "), c - std::strlen("Distance "));
                used[i] = true; keyed = true;
            }
    }
    if (!keyed && !L.empty()) { s.keyValue = L[0]; s.keyCaption = "first fact"; used[0] = true; }    // never reached for the real cards (tested); a safe fallback

    // ---- the key values ----
    std::vector<const char*> want;
    if (sel.kind == Selection::Star) want = {"Mass", "Surface (rough)", "Habitable zone (rough)", "Frost line (rough)", "Lifetime (rough)"};
    else if (sel.kind == Selection::Moon) want = {"Radius", "Orbit", "Speed", "Phase now"};
    else if (sel.kind == Selection::Body && dot == std::string::npos) want = {"Nucleus", "Speed now", "Closest to the Sun", "One trip round the Sun", "Last closest approach", "Next", "Closest approach"};
    else if (sel.kind == Selection::Body) want = {"Size", "Mass", "Speed", "Orbit", "One lap", "Temperature"};
    else if (sel.kind == Selection::Far) want = {"Speed"};
    for (const char* w : want) {
        if (s.values.size() >= 6) break;
        for (std::size_t i = 0; i < L.size(); i++) {
            std::string lab, val;
            if (!used[i] && split(L[i], lab, val) && lab == w) { s.values.push_back({lab, val}); used[i] = true; break; }
        }
    }
    // fewer than 4: the other labelled lines, then the plain lines, in the old card's order
    for (int pass = 0; pass < 2 && s.values.size() < 4; pass++)
        for (std::size_t i = 0; i < L.size() && s.values.size() < 4; i++) {
            if (used[i]) continue;
            std::string lab, val;
            const bool labelled = split(L[i], lab, val);
            if (pass == 0 && labelled) s.values.push_back({lab, val});
            else if (pass == 1 && !labelled) s.values.push_back({"", L[i]});
            else continue;
            used[i] = true;
        }
    return s;
}

}  // namespace orbit
