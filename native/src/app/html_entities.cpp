#include "app/html_entities.h"

#include <algorithm>
#include <cstring>

namespace app {

namespace {

struct Entity { const char* name; unsigned a, b; };
const Entity kEntities[] = {
#include "app/html_entities_data.inc"
};
constexpr std::size_t kCount = sizeof(kEntities) / sizeof(kEntities[0]);

const Entity* find(const std::string& name) {
    const Entity* end = kEntities + kCount;
    const Entity* it = std::lower_bound(kEntities, end, name, [](const Entity& e, const std::string& n) { return std::strcmp(e.name, n.c_str()) < 0; });
    return it != end && name == it->name ? it : nullptr;
}

bool isNameChar(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

}  // namespace

std::string utf8FromCodepoint(unsigned cp) {
    std::string o;
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return o;
    if (cp < 0x80) o += static_cast<char>(cp);
    else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { o += static_cast<char>(0xF0 | (cp >> 18)); o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    return o;
}

std::string decodeHtmlEntities(const std::string& s) {
    if (s.find('&') == std::string::npos) return s;
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] != '&') { out += s[i++]; continue; }
        const std::size_t semi = s.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 40) { out += s[i++]; continue; }          // the longest name in the list has 32 characters
        const std::string body = s.substr(i + 1, semi - i - 1);
        std::string rep;
        if (!body.empty() && body[0] == '#') {
            const bool hex = body.size() > 1 && (body[1] == 'x' || body[1] == 'X');
            const std::size_t from = hex ? 2 : 1;
            bool ok = body.size() > from;
            unsigned long v = 0;
            for (std::size_t k = from; ok && k < body.size(); k++) {
                const char c = body[k];
                const int d = c >= '0' && c <= '9' ? c - '0' : hex && c >= 'a' && c <= 'f' ? c - 'a' + 10 : hex && c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (d < 0) ok = false;
                else { v = v * (hex ? 16 : 10) + static_cast<unsigned long>(d); if (v > 0x10FFFF) ok = false; }
            }
            if (ok && v >= 1) rep = utf8FromCodepoint(static_cast<unsigned>(v));
        } else if (!body.empty() && std::all_of(body.begin(), body.end(), isNameChar)) {
            if (const Entity* e = find(body)) { rep = utf8FromCodepoint(e->a); if (e->b) rep += utf8FromCodepoint(e->b); }
        }
        if (rep.empty()) { out += s[i++]; continue; }                                       // not a reference: keep the "&" and go on after it
        out += rep;
        i = semi + 1;
    }
    return out;
}

}  // namespace app
