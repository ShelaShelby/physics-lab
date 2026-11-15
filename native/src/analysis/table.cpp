// The Data panel's tables (0.51): see table.h.
#include "analysis/table.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>

#include "app/calc.h"

namespace analysis {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

long long gcdLL(long long a, long long b) { a = a < 0 ? -a : a; b = b < 0 ? -b : b; while (b) { const long long t = a % b; a = b; b = t; } return a ? a : 1; }

// "cm^-1" -> base "cm", power -1/1; "au^(3/2)" -> "au", 3/2; "km/s" -> false (not a single unit)
bool simpleUnit(const std::string& u, std::string& base, long long& num, long long& den) {
    num = 1; den = 1;
    const std::size_t c = u.find('^');
    base = u.substr(0, c);
    if (base.empty()) return false;
    for (unsigned char ch : base) if (!(std::isalnum(ch) || ch >= 0x80 || ch == '_')) return false;
    if (c == std::string::npos) return true;
    std::string p = u.substr(c + 1);
    if (p.size() > 2 && p.front() == '(' && p.back() == ')') p = p.substr(1, p.size() - 2);
    const std::size_t sl = p.find('/');
    char* end = nullptr;
    num = std::strtoll(p.c_str(), &end, 10);
    if (end == p.c_str()) return false;
    if (sl != std::string::npos) { if (end != p.c_str() + sl) return false; const char* d = p.c_str() + sl + 1; den = std::strtoll(d, &end, 10); if (end == d || *end || den == 0) return false; }
    else if (*end) return false;
    return true;
}
bool compound(const std::string& u) { return u.find_first_of("/ *·()") != std::string::npos; }

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    return s.substr(a, b - a);
}
bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
}  // namespace

bool Column::hasSigma() const {
    for (double s : sigma) if (std::isfinite(s)) return true;
    return false;
}

// ---- transforms ----
const char* transformName(Transform t) {
    switch (t) {
    case Transform::None: return "x"; case Transform::Inverse: return "1/x"; case Transform::InverseSquare: return "1/x²"; case Transform::Square: return "x²"; case Transform::Cube: return "x³";
    case Transform::Sqrt: return "√x"; case Transform::Ln: return "ln x"; case Transform::Log10: return "log₁₀ x"; case Transform::Count: break;
    }
    return "x";
}
const char* transformId(Transform t) {
    switch (t) {
    case Transform::None: return "none"; case Transform::Inverse: return "inv"; case Transform::InverseSquare: return "inv2"; case Transform::Square: return "sq"; case Transform::Cube: return "cube";
    case Transform::Sqrt: return "sqrt"; case Transform::Ln: return "ln"; case Transform::Log10: return "log10"; case Transform::Count: break;
    }
    return "none";
}
Transform transformById(const std::string& id) {
    for (int i = 0; i < static_cast<int>(Transform::Count); i++) if (id == transformId(static_cast<Transform>(i))) return static_cast<Transform>(i);
    return Transform::None;
}
bool applyTransform(Transform t, double v, double s, double& out, double& os) {
    if (!std::isfinite(v)) return false;
    switch (t) {
    case Transform::None: out = v; os = s; return true;
    case Transform::Inverse: if (v == 0) return false; out = 1 / v; os = s / (v * v); return true;
    case Transform::InverseSquare: if (v == 0) return false; out = 1 / (v * v); os = 2 * s / std::fabs(v * v * v); return true;
    case Transform::Square: out = v * v; os = 2 * std::fabs(v) * s; return true;
    case Transform::Cube: out = v * v * v; os = 3 * v * v * s; return true;
    case Transform::Sqrt: if (v < 0) return false; out = std::sqrt(v); os = v > 0 ? s / (2 * out) : kNaN; return true;
    case Transform::Ln: if (!(v > 0)) return false; out = std::log(v); os = s / v; return true;
    case Transform::Log10: if (!(v > 0)) return false; out = std::log10(v); os = s / (v * std::log(10.0)); return true;
    case Transform::Count: break;
    }
    return false;
}
std::string unitPower(const std::string& unit, int num, int den) {
    if (unit.empty() || num == 0) return "";
    std::string base;
    long long n = 1, d = 1;
    if (simpleUnit(unit, base, n, d)) {
        n *= num; d *= den;
        if (d < 0) { n = -n; d = -d; }
        const long long g = gcdLL(n, d);
        n /= g; d /= g;
        if (n == 1 && d == 1) return base;
        if (d == 1) return base + "^" + std::to_string(n);
        return base + "^(" + std::to_string(n) + "/" + std::to_string(d) + ")";
    }
    if (num == 1 && den == 1) return unit;
    const long long g = gcdLL(num, den);
    long long a = num / g, b = den / g;
    if (b < 0) { a = -a; b = -b; }
    return "(" + unit + ")^" + (b == 1 ? std::to_string(a) : "(" + std::to_string(a) + "/" + std::to_string(b) + ")");
}
std::string transformedUnit(const std::string& unit, Transform t) {
    switch (t) {
    case Transform::None: return unit;
    case Transform::Inverse: return unitPower(unit, -1);
    case Transform::InverseSquare: return unitPower(unit, -2);
    case Transform::Square: return unitPower(unit, 2);
    case Transform::Cube: return unitPower(unit, 3);
    case Transform::Sqrt: return unitPower(unit, 1, 2);
    default: return "";
    }
}
std::string axisLabel(const std::string& name, const std::string& unit, Transform t) {
    const bool wrap = name.find_first_of(" /+−-") != std::string::npos;
    const std::string n = wrap ? "(" + name + ")" : name;
    std::string label;
    switch (t) {
    case Transform::None: label = name; break;
    case Transform::Inverse: label = "1/" + n; break;
    case Transform::InverseSquare: label = "1/" + n + "²"; break;
    case Transform::Square: label = n + "²"; break;
    case Transform::Cube: label = n + "³"; break;
    case Transform::Sqrt: label = "√" + n; break;
    case Transform::Ln: return unit.empty() ? "ln " + n : "ln(" + name + " / " + unit + ")";
    case Transform::Log10: return unit.empty() ? "log₁₀ " + n : "log₁₀(" + name + " / " + unit + ")";
    case Transform::Count: break;
    }
    const std::string u = transformedUnit(unit, t);
    return u.empty() ? label : label + " (" + u + ")";
}
std::string paramUnit(const std::string& yUnit, const std::string& xUnit, int yPow, int xPow) {
    std::string num, den;
    if (yPow > 0) num = unitPower(yUnit, yPow);
    if (xPow > 0 && !xUnit.empty()) num += (num.empty() ? "" : " ") + unitPower(xUnit, xPow);
    if (xPow < 0 && !xUnit.empty()) den = unitPower(xUnit, -xPow);
    if (yPow < 0 && !yUnit.empty()) den += (den.empty() ? "" : " ") + unitPower(yUnit, -yPow);
    if (den.empty()) return num;
    if (num.empty()) {
        if (xPow < 0 && yPow >= 0) return unitPower(xUnit, xPow);
        return "1/(" + den + ")";
    }
    return num + "/" + (compound(den) ? "(" + den + ")" : den);
}
bool unitKnown(const std::string& unit) {
    if (unit.empty()) return true;
    const calc::Result r = calc::evaluate("1 (" + unit + ")");
    return r.ok && r.q.im == 0;
}

// ---- the table ----
std::size_t Table::rows() const {
    std::size_t n = 0;
    for (const Column& c : cols) n = std::max(n, c.isText() ? c.text.size() : c.v.size());
    return n;
}
int Table::find(const std::string& name) const {
    for (std::size_t i = 0; i < cols.size(); i++) if (cols[i].name == name) return static_cast<int>(i);
    return -1;
}
std::string numberText(double v, int digits) {
    if (!std::isfinite(v)) return "";
    char b[48];
    std::snprintf(b, sizeof b, "%.*g", digits, v == 0 ? 0.0 : v);
    return b;
}
std::string cellText(const Table& t, int col, std::size_t row) {
    if (col < 0 || col >= static_cast<int>(t.cols.size())) return "";
    const Column& c = t.cols[std::size_t(col)];
    if (c.isText()) return row < c.text.size() ? c.text[row] : "";
    return row < c.v.size() ? numberText(c.v[row]) : "";
}
std::vector<std::string> groupValues(const Table& t) {
    std::vector<std::string> out;
    if (t.group < 0) return out;
    for (std::size_t r = 0; r < t.rows(); r++) {
        const std::string v = cellText(t, t.group, r);
        if (!v.empty() && std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
    }
    return out;
}

// ---- text out ----
namespace {
std::string csvField(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string o = "\"";
    for (char c : s) { if (c == '"') o += '"'; o += c; }
    return o + "\"";
}
std::string header(const Column& c, bool sigma) {
    const std::string n = (sigma ? "σ " : "") + c.name;
    return c.unit.empty() ? n : n + " [" + c.unit + "]";
}
}  // namespace

std::string toCsv(const Table& t, const std::string& appVersion, const std::vector<std::string>& extra) {
    std::ostringstream o;
    o << "# Physics Lab " << appVersion << ": " << t.title << "\n";
    if (!t.about.empty()) o << "# " << t.about << "\n";
    for (const std::string& s : t.sources) o << "# Source: " << s << "\n";
    for (const Column& c : t.cols)
        if (!c.isText()) o << "# Column " << c.name << ": " << (c.unit.empty() ? "a plain number" : "unit " + c.unit) << (c.source.empty() ? "" : "; " + c.source) << (c.hasSigma() ? "; with its 1-sigma uncertainty" : "") << "\n";
    for (const std::string& e : extra) o << "# " << e << "\n";
    bool first = true;
    for (const Column& c : t.cols) {
        o << (first ? "" : ",") << csvField(header(c, false));
        first = false;
        if (c.hasSigma()) o << "," << csvField(header(c, true));
    }
    o << "\n";
    for (std::size_t r = 0; r < t.rows(); r++) {
        first = true;
        for (const Column& c : t.cols) {
            o << (first ? "" : ",");
            first = false;
            if (c.isText()) o << csvField(r < c.text.size() ? c.text[r] : "");
            else o << (r < c.v.size() ? numberText(c.v[r], 15) : "");
            if (c.hasSigma()) o << "," << (r < c.sigma.size() ? numberText(c.sigma[r], 15) : "");
        }
        o << "\n";
    }
    return o.str();
}
std::string toTsv(const Table& t) {
    std::ostringstream o;
    bool first = true;
    for (const Column& c : t.cols) {
        o << (first ? "" : "\t") << header(c, false);
        first = false;
        if (c.hasSigma()) o << "\t" << header(c, true);
    }
    o << "\n";
    for (std::size_t r = 0; r < t.rows(); r++) {
        first = true;
        for (const Column& c : t.cols) {
            o << (first ? "" : "\t");
            first = false;
            if (c.isText()) { std::string s = r < c.text.size() ? c.text[r] : ""; std::replace(s.begin(), s.end(), '\t', ' '); o << s; }
            else o << (r < c.v.size() ? numberText(c.v[r], 15) : "");
            if (c.hasSigma()) o << "\t" << (r < c.sigma.size() ? numberText(c.sigma[r], 15) : "");
        }
        o << "\n";
    }
    return o.str();
}

// ---- text in ----
namespace {
std::vector<std::string> splitLine(const std::string& line, char sep) {
    std::vector<std::string> out;
    if (sep == ' ') {
        std::istringstream is(line);
        std::string w;
        while (is >> w) out.push_back(w);
        return out;
    }
    std::string cur;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); i++) {
        const char c = line[i];
        if (c == '"') { if (quoted && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; i++; } else quoted = !quoted; }
        else if (c == sep && !quoted) { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(trim(cur));
    return out;
}
// a number cell: true with v (NaN for an empty cell or "NaN"); false when it is text
bool numberCell(std::string s, bool commaDecimal, double& v) {
    s = trim(s);
    if (s.empty() || lower(s) == "nan" || s == "-" || s == "—") { v = kNaN; return true; }
    if (commaDecimal) std::replace(s.begin(), s.end(), ',', '.');
    // "1.5e3", "−2" (the typographic minus), "6.02×10^23" are numbers
    std::string t;
    for (std::size_t i = 0; i < s.size();) {
        if (s.compare(i, 3, "−") == 0) { t += '-'; i += 3; continue; }
        if (s.compare(i, 5, "×10^") == 0) { t += 'e'; i += 5; continue; }
        t += s[i++];
    }
    char* end = nullptr;
    v = std::strtod(t.c_str(), &end);
    return end && *end == 0 && end != t.c_str() && std::isfinite(v);
}
// 0.51.1: "dV", "ΔV", "δV" (or with a space) right after a column V is V's uncertainty: the physicist's own way of heading it (only for the column just before)
bool sigmaOf(const std::string& n, const std::string& prev) {
    if (prev.empty()) return false;
    for (const char* p : {"d", "Δ", "δ"})
        if (startsWith(n, p)) { std::string rest = n.substr(std::strlen(p)); if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1); if (rest == prev) return true; }
    return false;
}
bool sigmaName(const std::string& n) {
    const std::string l = lower(n);
    return startsWith(n, "σ") || startsWith(n, "±") || startsWith(l, "sigma") || startsWith(l, "err") || startsWith(l, "unc") || startsWith(l, "u(") || startsWith(l, "d_") || startsWith(l, "delta");
}
// "t (s)" -> t, s;  "t [s]" -> t, s;  "t" -> t, ""
void nameUnit(const std::string& h, std::string& name, std::string& unit) {
    std::string s = trim(h);
    unit.clear();
    for (const char* br : {"[]", "()"}) {
        if (!s.empty() && s.back() == br[1]) {
            const std::size_t o = s.rfind(br[0]);
            if (o != std::string::npos && o > 0) { unit = trim(s.substr(o + 1, s.size() - o - 2)); s = trim(s.substr(0, o)); break; }
        }
    }
    name = s;
}
}  // namespace

bool parseText(const std::string& text, Table& out, std::string& why, std::vector<std::string>& notes) {
    out = Table();
    out.id = "user";
    out.title = "Your data";
    out.about = "Typed or pasted into the Data panel.";
    std::vector<std::string> lines;
    {
        std::string cur;
        for (char c : text) { if (c == '\n') { lines.push_back(cur); cur.clear(); } else if (c != '\r') cur += c; }
        lines.push_back(cur);
    }
    std::vector<std::string> kept;
    for (const std::string& l : lines) { const std::string t = trim(l); if (!t.empty() && t[0] != '#') kept.push_back(l); }
    if (kept.empty()) { why = "Nothing to read: type or paste rows of numbers."; return false; }
    char sep = ' ';
    for (const std::string& l : kept) {
        if (l.find('\t') != std::string::npos) { sep = '\t'; break; }
        if (l.find(';') != std::string::npos) sep = ';';
        else if (sep == ' ' && l.find(',') != std::string::npos) sep = ',';
    }
    bool commaDecimal = false;
    if (sep == ';' || sep == '\t') {                               // "1,5;2,3" or "1,5<tab>2,3": a decimal comma (when no cell has a point)
        bool comma = false, point = false;
        for (const std::string& l : kept) { comma = comma || l.find(',') != std::string::npos; point = point || l.find('.') != std::string::npos; }
        commaDecimal = comma && !point;
        if (commaDecimal) notes.push_back("Commas read as decimal points (the columns are separated by " + std::string(sep == ';' ? "semicolons" : "tabs") + ").");
    }
    std::vector<std::vector<std::string>> cells;
    for (const std::string& l : kept) cells.push_back(splitLine(l, sep));
    // a header: the first line has a cell that is not a number
    bool hasHeader = false;
    for (const std::string& c : cells[0]) { double v; if (!numberCell(c, commaDecimal, v)) hasHeader = true; }
    std::size_t ncol = 0;
    for (const auto& r : cells) ncol = std::max(ncol, r.size());
    std::vector<std::string> names(ncol), units(ncol);
    for (std::size_t c = 0; c < ncol; c++) {
        if (hasHeader && c < cells[0].size()) nameUnit(cells[0][c], names[c], units[c]);
        if (names[c].empty()) names[c] = "column " + std::to_string(c + 1);
    }
    const std::size_t first = hasHeader ? 1 : 0;
    if (cells.size() <= first) { why = "Only a header line: add rows of numbers under it."; return false; }
    // each column: numbers, or text
    struct Raw { std::vector<double> v; std::vector<std::string> s; bool text = false; };
    std::vector<Raw> raw(ncol);
    for (std::size_t r = first; r < cells.size(); r++)
        for (std::size_t c = 0; c < ncol; c++) {
            const std::string cell = c < cells[r].size() ? cells[r][c] : "";
            double v;
            if (!numberCell(cell, commaDecimal, v)) raw[c].text = true;
            raw[c].v.push_back(numberCell(cell, commaDecimal, v) ? v : kNaN);
            raw[c].s.push_back(trim(cell));
        }
    for (std::size_t c = 0; c < ncol; c++) {
        const bool isSigma = c > 0 && !out.cols.empty() && (sigmaName(names[c]) || sigmaOf(names[c], out.cols.back().name)) && !raw[c].text && !out.cols.back().isText() && out.cols.back().sigma.empty();
        if (isSigma) {
            Column& prev = out.cols.back();
            double factor = 1;
            if (!units[c].empty() && units[c] != prev.unit) {      // an uncertainty in another unit of the same kind: converted
                const calc::Result a = calc::evaluate("1 (" + units[c] + ")"), b = calc::evaluate("1 (" + (prev.unit.empty() ? "1" : prev.unit) + ")");
                if (a.ok && b.ok && a.q.dim == b.q.dim && b.q.v != 0) { factor = a.q.v / b.q.v; notes.push_back(names[c] + " converted from " + units[c] + " to " + prev.unit + "."); }
                else notes.push_back(names[c] + ": its unit " + units[c] + " is not the same kind as " + prev.unit + "; taken as " + (prev.unit.empty() ? "a plain number" : prev.unit) + ".");
            }
            prev.sigma = raw[c].v;
            for (double& s : prev.sigma) { s = std::fabs(s * factor); if (s == 0) s = kNaN; }
            continue;
        }
        Column col;
        col.name = names[c];
        col.unit = units[c];
        col.source = "your data";
        if (raw[c].text) col.text = raw[c].s;
        else col.v = raw[c].v;
        if (!col.unit.empty() && !unitKnown(col.unit)) {
            notes.push_back("The calculator does not know the unit \"" + col.unit + "\" of " + col.name + ": it is kept as a label, and fitted parameters will not go to the calculator with a unit.");
            col.name += " (" + col.unit + ")";
            col.unit.clear();
        }
        out.cols.push_back(std::move(col));
    }
    int numeric = 0;
    for (const Column& c : out.cols) if (!c.isText()) { for (double v : c.v) if (std::isfinite(v)) { numeric++; break; } }
    if (numeric == 0) { why = "No column of numbers found."; return false; }
    out.sources.push_back("your own data");
    for (const Column& c : out.cols) if (c.isText() && out.group < 0) {    // a text column with a few repeated values splits the rows into groups
        std::vector<std::string> distinct;
        for (const std::string& s : c.text) if (std::find(distinct.begin(), distinct.end(), s) == distinct.end()) distinct.push_back(s);
        if (distinct.size() >= 2 && distinct.size() * 2 <= c.text.size()) out.group = static_cast<int>(&c - &out.cols[0]);
    }
    return true;
}

}  // namespace analysis
