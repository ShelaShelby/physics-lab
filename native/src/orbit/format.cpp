#include "format.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace fmt {

std::string fixed(double x, int d) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", d, x);
    std::string s = buf;
    if (s == "-0" || (s.size() > 1 && s[0] == '-' && s.find_first_not_of("-0.") == std::string::npos)) s.erase(0, 1);   // no "-0.00"
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

std::string km(double v) { return commas(v) + " km"; }
std::string kmPlain(double v) { return plainNumber(v) + " km"; }
std::string millionKm(double mkm) { return commas(mkm) + " million km"; }
std::string kms(double v, int d) { return fixed(v, d) + " km/s"; }
std::string celsius(double c) { return plainNumber(c) + " °C"; }
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
