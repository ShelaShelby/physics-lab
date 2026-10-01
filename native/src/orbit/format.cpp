#include "format.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <regex>

namespace fmt {

static void shortestDigits(double x, std::string& digits, int& exp10);

// Round the exact binary value of x to d decimals, ties away from zero (as JavaScript's toFixed does;
// printf would round ties to even: 2.5 -> "2", where JavaScript writes "3").
static std::string roundFixed(double x, int d) {
    const bool neg = std::signbit(x) && x != 0;
    char buf[400];
    std::snprintf(buf, sizeof buf, "%.*f", d + 40, std::fabs(x));   // enough exact digits to see the remainder
    std::string s = buf;
    const std::size_t dot = s.find('.');
    std::string ip = s.substr(0, dot), fp = s.substr(dot + 1);
    const bool up = fp[d] >= '5';
    fp = fp.substr(0, d);
    std::string digits = ip + fp;
    if (up) {
        int i = static_cast<int>(digits.size()) - 1;
        while (i >= 0 && digits[i] == '9') { digits[i] = '0'; i--; }
        if (i < 0) digits.insert(digits.begin(), '1'); else digits[i]++;
    }
    const std::size_t il = digits.size() - d;
    std::string out = digits.substr(0, il) + (d > 0 ? "." + digits.substr(il) : "");
    return (neg ? "-" : "") + out;
}

std::string fixed(double x, int d) {
    if (std::fabs(x) >= 1e21) return jsString(x);            // as toFixed
    std::string s = roundFixed(x, d);
    if (s.size() > 1 && s[0] == '-' && s.find_first_not_of("-0.") == std::string::npos) s.erase(0, 1);   // no "-0.00"
    return s;
}

std::string commas(double x) {
    long long v = std::llround(x);
    const bool neg = v < 0;
    std::string digits = std::to_string(neg ? -v : v), out;
    for (std::size_t i = 0; i < digits.size(); i++) {
        if (i && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return neg ? "-" + out : out;
}

std::string plainNumber(double x) {
    if (x == 0 || !std::isfinite(x)) return x == 0 ? "0" : fixed(x, 0);
    char buf[64];
    int p = 1;
    for (; p < 17; p++) {                             // the fewest significant digits that read back as the same number
        std::snprintf(buf, sizeof buf, "%.*g", p, x);
        if (std::strtod(buf, nullptr) == x) break;
    }
    // written out without an exponent: -110, 2439.7, 0.0553
    const int exponent = static_cast<int>(std::floor(std::log10(std::fabs(x))));
    std::snprintf(buf, sizeof buf, "%.*f", std::max(0, p - 1 - exponent), x);
    return buf;
}

std::string precision(double x, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*e", digits - 1, x);
    return plainNumber(std::strtod(buf, nullptr));
}

std::string superscript(int n) {
    static const char* sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string s = n < 0 ? "⁻" : "";
    for (char c : std::to_string(std::abs(n))) s += sup[c - '0'];
    return s;
}

static bool g_imperial = false;
void setImperial(bool on) { g_imperial = on; }
bool imperial() { return g_imperial; }

// Shortest digits that read back as x, and the decimal exponent: x = 0.d1d2d3... × 10^(exp)  (d1 ≠ 0)
static void shortestDigits(double x, std::string& digits, int& exp10) {
    char buf[64];
    for (int p = 1; p <= 17; p++) {
        std::snprintf(buf, sizeof buf, "%.*e", p - 1, x);
        if (std::strtod(buf, nullptr) == x) break;
    }
    // buf: "d.ddde±XX"
    std::string m = buf;
    const std::size_t e = m.find('e');
    exp10 = std::atoi(m.c_str() + e + 1) + 1;
    digits.clear();
    for (std::size_t i = 0; i < e; i++) if (m[i] >= '0' && m[i] <= '9') digits += m[i];
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
}

std::string jsString(double x) {           // ECMAScript Number::toString
    if (std::isnan(x)) return "NaN";
    if (x == 0) return "0";
    if (std::isinf(x)) return x > 0 ? "Infinity" : "-Infinity";
    if (x < 0) return "-" + jsString(-x);
    std::string d; int n;
    shortestDigits(x, d, n);
    const int k = static_cast<int>(d.size());
    if (k <= n && n <= 21) return d + std::string(n - k, '0');
    if (0 < n && n <= 21) return d.substr(0, n) + "." + d.substr(n);
    if (-6 < n && n <= 0) return "0." + std::string(-n, '0') + d;
    const int e = n - 1;
    const std::string es = (e >= 0 ? "+" : "-") + std::to_string(std::abs(e));
    return k == 1 ? d + "e" + es : d.substr(0, 1) + "." + d.substr(1) + "e" + es;
}

// x > 0 rounded to p significant digits from its exact binary value, ties away from zero (as JavaScript
// does): x ≈ d.ddd × 10^e, with the p digits in `digits`
static void roundSig(double x, int p, std::string& digits, int& e) {
    char buf[128];
    std::snprintf(buf, sizeof buf, "%.*e", p + 40, x);
    std::string m = buf;
    const std::size_t ep = m.find('e');
    e = std::atoi(m.c_str() + ep + 1);
    std::string all;
    for (std::size_t i = 0; i < ep; i++) if (m[i] >= '0' && m[i] <= '9') all += m[i];
    digits = all.substr(0, p);
    if (all[p] >= '5') {
        int i = p - 1;
        while (i >= 0 && digits[i] == '9') { digits[i] = '0'; i--; }
        if (i < 0) { digits = "1" + std::string(p - 1, '0'); e++; } else digits[i]++;
    }
}

static std::string expTail(int e) { return std::string("e") + (e >= 0 ? "+" : "-") + std::to_string(std::abs(e)); }

std::string jsToExponential(double x, int f) {
    if (x == 0) return (f > 0 ? "0." + std::string(f, '0') : "0") + "e+0";
    if (x < 0) return "-" + jsToExponential(-x, f);
    std::string d; int e;
    roundSig(x, f + 1, d, e);
    return d.substr(0, 1) + (f > 0 ? "." + d.substr(1) : "") + expTail(e);
}

std::string jsToPrecision(double x, int p) {
    if (x == 0) return p > 1 ? "0." + std::string(p - 1, '0') : "0";
    if (x < 0) return "-" + jsToPrecision(-x, p);
    std::string d; int e;
    roundSig(x, p, d, e);
    if (e < -6 || e >= p) return d.substr(0, 1) + (p > 1 ? "." + d.substr(1) : "") + expTail(e);
    if (e >= 0) return d.substr(0, e + 1) + (p > e + 1 ? "." + d.substr(e + 1) : "");
    return "0." + std::string(-e - 1, '0') + d;
}

std::string jsLocale(double x, int maxFrac) {   // toLocaleString('en'): grouping, at most maxFrac fraction digits
    if (std::isnan(x)) return "NaN";
    if (x == 0) return std::signbit(x) ? "-0" : "0";
    const bool neg = x < 0;
    // Intl rounds the shortest decimal form (1.0005 -> 1.001), ties away from zero
    std::string d; int n;
    shortestDigits(std::fabs(x), d, n);       // |x| = 0.d × 10^n
    std::string all = (n > 0 ? "" : std::string(-n + 1, '0')) + d;   // digits with the point after `ip` of them
    const int ipLen = n > 0 ? n : 1;
    if (static_cast<int>(all.size()) < ipLen + maxFrac + 1) all += std::string(ipLen + maxFrac + 1 - all.size(), '0');
    std::string keep = all.substr(0, ipLen + maxFrac);
    if (all[ipLen + maxFrac] >= '5') {
        int i = static_cast<int>(keep.size()) - 1;
        while (i >= 0 && keep[i] == '9') { keep[i] = '0'; i--; }
        if (i < 0) keep.insert(keep.begin(), '1'); else keep[i]++;
    }
    std::string ip = keep.substr(0, keep.size() - maxFrac), fp = keep.substr(keep.size() - maxFrac);
    while (ip.size() > 1 && ip[0] == '0') ip.erase(0, 1);
    while (!fp.empty() && fp.back() == '0') fp.pop_back();
    std::string g;
    for (std::size_t i = 0; i < ip.size(); i++) {
        if (i && (ip.size() - i) % 3 == 0) g += ',';
        g += ip[i];
    }
    return (neg ? "-" : "") + g + (fp.empty() ? "" : "." + fp);   // Intl keeps the sign of a tiny negative: "-0"
}

// as the web version's uLen, uLenD, uMil, uSpd, uSpdN and uTemp
std::string km(double v) { return g_imperial ? commas(v * kMilesPerKm) + " mi" : commas(v) + " km"; }
std::string kmPlain(double v) {
    if (!g_imperial) return plainNumber(v) + " km";
    const double mi = v * kMilesPerKm;
    return (mi >= 100 ? commas(mi) : precision(mi, 3)) + " mi";
}
std::string millionKm(double mkm) { return g_imperial ? commas(mkm * kMilesPerKm) + " million mi" : commas(mkm) + " million km"; }
std::string kmsNumber(double v, int d) { return fixed(g_imperial ? v * kMilesPerKm : v, d); }
std::string kms(double v, int d) { return kmsNumber(v, d) + (g_imperial ? " mi/s" : " km/s"); }
std::string celsius(double c) { return g_imperial ? commas(c * 9 / 5 + 32) + " °F" : plainNumber(c) + " °C"; }

std::string unitText(const std::string& t) {
    if (!g_imperial) return t;
    // a number (with commas and decimals), maybe " million", then " km", " km/h" or " km/s" (not followed by a letter);
    // and a temperature "−65 °C". The same rules as the web version's uText.
    static const std::regex len(R"((\d[\d,]*(?:\.\d+)?)( million)? km(/h|/s)?(?![A-Za-z]))");
    static const std::regex temp(R"((−|-)?(\d[\d,]*(?:\.\d+)?) ?°C)");
    const auto num = [](const std::string& s) { std::string d; for (char c : s) if (c != ',') d += c; return std::stod(d); };
    const auto uNum = [](double x) { return x >= 100 ? commas(std::stod(precision(x, 3))) : precision(x, 2); };   // 2–3 significant figures
    std::string out, rest = t;
    std::smatch m;
    while (std::regex_search(rest, m, len)) {
        const std::string per = m[3].str();
        out += m.prefix().str() + uNum(num(m[1].str()) * kMilesPerKm) + m[2].str() + (per == "/h" ? " mph" : per == "/s" ? " mi/s" : " mi");
        rest = m.suffix().str();
    }
    out += rest;
    rest = out; out.clear();
    while (std::regex_search(rest, m, temp)) {
        const double c = (m[1].matched ? -1 : 1) * num(m[2].str());
        std::string f = commas(c * 9 / 5 + 32);
        if (!f.empty() && f[0] == '-') f = "−" + f.substr(1);
        out += m.prefix().str() + f + " °F";
        rest = m.suffix().str();
    }
    return out + rest;
}
std::string au(double x) { return (x < 10 ? fixed(x, 2) : fixed(x, 1)) + " AU"; }

std::string years(double y) {
    if (y < 0.1) return fixed(y * 365.25, 0) + " days";
    if (y < 10) return fixed(y, 2) + " years";
    return commas(y) + " years";
}

std::string kg(double v) {
    const int e = static_cast<int>(std::floor(std::log10(v)));
    return fixed(v / std::pow(10.0, e), 1) + "×10" + superscript(e) + " kg";
}

// Days since 1 Jan 1970 -> year, month, day (proleptic Gregorian calendar; H. Hinnant's algorithm).
static void civilFromDays(long long z, int& y, int& m, int& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long yy = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yy + (m <= 2));
}

static long long floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

std::string dateUTC(int64_t ms) {
    static const char* mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int y, m, d;
    civilFromDays(floorDiv(ms, 86400000), y, m, d);
    return std::to_string(d) + " " + mon[m - 1] + " " + std::to_string(y);
}

int yearUTC(int64_t ms) {
    int y, m, d;
    civilFromDays(floorDiv(ms, 86400000), y, m, d);
    return y;
}

std::string simDate(int64_t epochMs, double yrs) {
    if (yrs < 200000) return dateUTC(epochMs + static_cast<int64_t>(std::llround(yrs * 365.25 * 86400000.0)));
    return "the year " + commas(yearUTC(epochMs) + yrs);
}

}  // namespace fmt
