#include "sunspots.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>


namespace orbit {

const unsigned char* embeddedSunspots(std::size_t& size);   // cmake/embed_spots.cmake

namespace {
struct Row { std::uint16_t day; std::int16_t lat10; std::uint16_t lon10, area; };
struct Table { SpotTableInfo info; std::vector<Row> rows; std::vector<std::uint16_t> missing; };

std::uint32_t u32(const unsigned char* p) { return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24; }
std::uint16_t u16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | p[1] << 8); }

Table load();
std::unique_ptr<Table> g_table;   // read on first use; sunspotTableReset drops it (the benchmark's response line times the first use again)
const Table& table() {
    if (!g_table) g_table = std::make_unique<Table>(load());
    return *g_table;
}
Table load() {
    {
        const auto t0 = std::chrono::steady_clock::now();
        Table t;
        std::size_t n = 0;
        const unsigned char* b = embeddedSunspots(n);
        constexpr std::size_t head = 8 + 6 * 4;
        if (!b || n < head || std::memcmp(b, "PLSPOTS1", 8) != 0) { t.info.why = "the built-in sunspot table is not readable"; return t; }
        SpotTableInfo& I = t.info;
        I.rows = static_cast<int>(u32(b + 8)); I.missing = static_cast<int>(u32(b + 12)); I.first = static_cast<int>(u32(b + 16)); I.last = static_cast<int>(u32(b + 20));
        I.srsFirst = static_cast<int>(u32(b + 24)); I.midnightFrom = static_cast<int>(u32(b + 28));
        if (n < head + 8 * std::size_t(I.rows) + 2 * std::size_t(I.missing)) { I.why = "the built-in sunspot table is cut short"; return t; }
        t.rows.resize(std::size_t(I.rows));
        const unsigned char* p = b + head;
        for (auto& r : t.rows) { r.day = u16(p); r.lat10 = static_cast<std::int16_t>(u16(p + 2)); r.lon10 = u16(p + 4); r.area = u16(p + 6); p += 8; }
        t.missing.resize(std::size_t(I.missing));
        for (auto& m : t.missing) { m = u16(p); p += 2; }
        I.ok = true;
        I.loadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return t;
    }
}
}  // namespace

const SpotTableInfo& sunspotTable() { return table().info; }
void sunspotTableReset() { g_table.reset(); }

int spotDayOf(int y, int m, int d) {   // days from 1874-01-01 (proleptic Gregorian; Fliegel and Van Flandern's day number)
    const auto jdn = [](int Y, int M, int D) { const int a = (14 - M) / 12, yy = Y + 4800 - a, mm = M + 12 * a - 3; return D + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045; };
    return jdn(y, m, d) - jdn(1874, 1, 1);
}

std::vector<std::array<int, 3>> spotRowsOfDay(int day) {
    std::vector<std::array<int, 3>> out;
    const Table& t = table();
    auto it = std::lower_bound(t.rows.begin(), t.rows.end(), day, [](const Row& r, int d) { return r.day < d; });
    for (; it != t.rows.end() && it->day == day; ++it) out.push_back({it->lat10, it->lon10, it->area});
    return out;
}

SpotDay sunspotsAt(double jdUT) {
    SpotDay s;
    const Table& t = table();
    if (!t.info.ok) return s;
    const double x = jdUT - kSpotsDay0JD;
    // before 1992-01-02 a row is at 12:00 UT of its day, from then at 00:00 UT (tools/make_sunspot_table.py): the record nearest the time
    const int day = x < t.info.midnightFrom ? static_cast<int>(std::floor(x)) : static_cast<int>(std::floor(x + 0.5));
    s.day = day;
    s.recordJD = kSpotsDay0JD + day + (day < t.info.midnightFrom ? 0.5 : 0.0);
    if (day < t.info.first || day > t.info.last || std::binary_search(t.missing.begin(), t.missing.end(), static_cast<std::uint16_t>(day))) { s.status = SpotDay::NoRecord; return s; }
    for (const auto& r : spotRowsOfDay(day)) s.groups.push_back({r[0] / 10.0, r[1] / 10.0, static_cast<double>(r[2])});
    if (!s.groups.empty()) s.status = SpotDay::Groups;
    else if (day < spotDayOf(1977, 1, 1) || day >= t.info.srsFirst) s.status = SpotDay::Spotless;   // Greenwich lists its missing days; an SRS file says "None"
    else s.status = SpotDay::NotSaid;                                                             // USAF/NOAA 1977-2016: a day with no row is not marked
    return s;
}

void spotGeometry(const SpotGroup& g, double v[3], double& rho) {
    const double d2r = 3.14159265358979323846 / 180, b = g.latDeg * d2r, l = g.lonDeg * d2r;
    v[0] = std::cos(b) * std::cos(l); v[1] = std::cos(b) * std::sin(l); v[2] = std::sin(b);
    rho = std::acos(std::clamp(1 - g.area * 1e-6, -1.0, 1.0));   // a cap of the group's area: 2 pi R^2 (1 - cos rho) = area x 1e-6 x 2 pi R^2
}

std::string spotSentence(const SpotDay& d) {
    switch (d.status) {
    case SpotDay::NoRecord: return "No sunspot record for this date";
    case SpotDay::Spotless: return "No sunspots in this date's record";
    case SpotDay::NotSaid: return "No sunspot group in this date's record (a spotless day or a day not observed: the record does not say)";
    case SpotDay::Groups: break;
    }
    return std::to_string(d.groups.size()) + (d.groups.size() == 1 ? " sunspot group" : " sunspot groups") + " in this date's record, drawn as outlines";
}

std::vector<std::string> spotCardLines(const SpotDay& d) {
    std::vector<std::string> l = {"Sunspots: " + spotSentence(d)};
    if (d.status == SpotDay::Groups || d.status == SpotDay::Spotless || d.status == SpotDay::NotSaid) {
        const bool srs = d.day >= sunspotTable().srsFirst;
        l.push_back(srs ? "From NOAA's daily Solar Region Summary (preliminary data, NOAA SWPC; archived by NOAA NCEI)"
                        : "From the Greenwich and USAF/NOAA sunspot group records (NASA Marshall, D. Hathaway)");
    }
    if (d.status == SpotDay::Groups) {
        l.push_back("Each outline: one group at its measured place, a round patch of its measured area (a group holds several spots; its shape is not drawn)");
        l.push_back("Outline only: how dark the spots are is not drawn (no measured contrast fits these records yet)");
        l.push_back("Turned with the Sun as one body from the record's time (at most half a day away)");
        if (d.day >= spotDayOf(1977, 1, 1)) l.push_back("Areas from 1977 on as recorded: NASA raises them by 1.4 to match Greenwich's; the app does not");
    }
    return l;
}

}  // namespace orbit
