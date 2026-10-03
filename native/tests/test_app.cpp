// App-wide tests: the settings file (save, load, bad lines), the themes (every colour of every theme,
// for the panels and the picture), and the Settings → Progress texts against the web's own run
// (tests/reference/make_app_reference.js runs index.html's Settings code in Node).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "app/prefs.h"
#include "app/shot.h"
#include "app/fps.h"
#include "app/history.h"
#include "app/bench.h"
#include "app/frame_time.h"
#include "app/gpu_timer.h"
#include "orbit/belts.h"
#include "orbit/far_masses.h"
#include "atom/atom_bench.h"
#include "atom/cloud_light.h"
#include "atom/quark_light.h"
#include "orbit/solar_system.h"

#include "reference/app_reference.inc"

// The native app's own data files, which name nothing of the web's other parts: Orbit Lab texts (the guide, the timeline, the far-away things)
#include "orbit/sky.h"
namespace nativeorbit {
using orbit::FarData;
using orbit::ProbeData;
#include "orbit/sky_data.inc"
}
namespace nativeui {
#include "ui/learn_data.inc"
}

using namespace app;

static int g_checks = 0, g_failed = 0;

static void checkStr(const std::string& what, const std::string& got, const std::string& want) {
    g_checks++;
    if (got == want) return;
    if (++g_failed <= 40) std::printf("FAIL %s\n   got:  %s\n   want: %s\n", what.c_str(), got.c_str(), want.c_str());
}
static void checkTrue(const std::string& what, bool ok) { checkStr(what, ok ? "true" : "false", "true"); }

static void testRoundTrip() {
    Prefs p;
    p.theme = "monochrome"; p.textSize = "xl"; p.font = "round"; p.motion = "on"; p.units = "imperial";
    p.unlocked = {"synthwave", "monochrome"};
    p.achievements = {{"decay", 1790000000000LL}, {"monochrome", 0}, {"synthwave", 1789000000000LL}};
    p.atomMore = false;
    p.lab = "Atom Lab";
    p.orbit = {{"zoom", "-2.5"}, {"view3D", "1"}, {"sky", "off"}};
    p.atom = {{"Z", "92"}, {"N", "143"}, {"view", "Cloud 3D"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkStr("theme", q.theme, p.theme);
    checkStr("textSize", q.textSize, p.textSize);
    checkStr("font", q.font, p.font);
    checkStr("motion", q.motion, p.motion);
    checkStr("units", q.units, p.units);
    checkTrue("unlocked", q.unlocked == p.unlocked);
    checkTrue("achievements", q.achievements == p.achievements);
    checkTrue("atomMore", q.atomMore.has_value() && !*q.atomMore);
    checkStr("lab", q.lab, p.lab);
    checkTrue("orbit settings", q.orbit == p.orbit);
    checkTrue("atom settings", q.atom == p.atom);
    // the defaults go through unchanged too
    const Prefs d = loadPrefs(savePrefs(Prefs{}));
    checkStr("default theme", d.theme, "auto");
    checkStr("default size", d.textSize, "normal");
    checkStr("default font", d.font, "auto");
    checkStr("default motion", d.motion, "auto");
    checkTrue("default atomMore unset", !d.atomMore.has_value());
    checkTrue("default no unlocks", d.unlocked.empty() && d.achievements.empty());
}

static void testBadLines() {
    const Prefs p = loadPrefs("theme=nope\r\ntextSize=huge\nfont=comic\nmotion=fast\nunits=furlongs\nlang=et\n"
                              "unlock=synthwave,rainbow\nach.blackhole=12\nach.unknown=5\nach.decay=soon\n"
                              "atomMore=maybe\nlab=Black Hole Lab\nno equals sign\n# a comment\n");
    checkStr("bad theme", p.theme, "auto");
    checkStr("bad size", p.textSize, "normal");
    checkStr("bad font", p.font, "auto");
    checkStr("bad motion", p.motion, "auto");
    checkStr("bad units", p.units, "metric");
    checkStr("language not ready", p.lang, "en");
    checkTrue("only real secret themes", p.unlocked.size() == 1 && p.unlocked.count("synthwave"));
    checkTrue("only known achievements with a time", p.achievements.size() == 2 && p.achievements.count("blackhole"));
    checkTrue("an unlocked theme counts as its achievement", p.achievements.count("synthwave") && p.achievements.at("synthwave") == 0);
    checkTrue("bad atomMore", !p.atomMore.has_value());
    checkStr("bad lab", p.lab, "Orbit Lab");
    // a secret theme that is not unlocked is not used
    checkStr("locked secret theme", loadPrefs("theme=synthwave\n").theme, "auto");
    checkStr("unlocked secret theme", loadPrefs("unlock=synthwave\ntheme=synthwave\n").theme, "synthwave");
    // a value with a line break is written on one line
    Prefs n; n.orbit["x"] = "a\nb";
    checkStr("no line break inside a value", loadPrefs(savePrefs(n)).orbit["x"], "ab");
}

static bool isHex(const std::string& s) {
    if (s.size() != 4 && s.size() != 7) return false;
    if (s[0] != '#') return false;
    for (std::size_t i = 1; i < s.size(); i++) if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

static void testThemes() {
    // the web's THEMES list, in its order (index.html line 5225)
    const char* ids[] = {"auto", "light", "dark", "black", "midnight", "solar", "paper", "sage", "contrast", "synthwave", "monochrome"};
    checkTrue("11 themes", count(kThemes) == 11);
    for (std::size_t i = 0; i < count(kThemes) && i < 11; i++) {
        const ThemeDef& t = kThemes[i];
        checkStr("theme order", t.id, ids[i]);
        checkTrue(std::string(t.id) + " has a name", t.name[0] && t.sub[0]);
        checkTrue(std::string(t.id) + " secret", t.secret == (i >= 9));
        if (!std::string(t.id).compare("auto")) continue;            // Auto takes Light's or Dark's colours
        for (const char* c : {t.bg, t.fg, t.muted, t.card, t.line, t.accent, t.onacc, t.swatchBg, t.swatchAccent})
            checkTrue(std::string(t.id) + " colour " + c, isHex(c));
    }
    // the colours the app uses for both the panels and the picture
    checkStr("auto in a light Windows", themeColors("auto", true).id, "light");
    checkStr("auto in a dark Windows", themeColors("auto", false).id, "dark");
    checkStr("unknown theme", themeColors("rainbow", false).id, "dark");
    checkTrue("light is light", themeColors("light", false).light && themeColors("paper", false).light);
    checkTrue("dark is dark", !themeColors("dark", true).light && !themeColors("black", true).light);
    const ThemeColors pa = themeColors("paper", false);
    const Rgb want = hexRgb(kThemes[6].bg);
    checkTrue("paper background", pa.bg.r == want.r && pa.bg.g == want.g && pa.bg.b == want.b);
    checkTrue("#fff", hexRgb("#fff").r == 1 && hexRgb("#fff").b == 1);
    checkTrue("#000000", hexRgb("#000000").g == 0);
    // every theme gives the panels and both pictures a full set of colours, and "light" matches its background
    for (const ThemeDef& t : kThemes) {
        const ThemeColors c = themeColors(t.id, false);
        bool ok = true;
        for (const Rgb& v : {c.bg, c.fg, c.muted, c.card, c.line, c.accent, c.onacc})
            for (float x : {v.r, v.g, v.b}) ok = ok && x >= 0 && x <= 1;
        checkTrue(std::string(t.id) + " colours in range", ok);
        const double lum = 0.2126 * c.bg.r + 0.7152 * c.bg.g + 0.0722 * c.bg.b;
        checkTrue(std::string(t.id) + " light flag", c.light == (lum > 0.5));
        checkTrue(std::string(t.id) + " text stands out", std::fabs((0.2126 * c.fg.r + 0.7152 * c.fg.g + 0.0722 * c.fg.b) - lum) > 0.4);
    }
    // secret themes need unlocking
    Prefs p;
    checkTrue("synthwave locked", !themeUsable(p, "synthwave") && themeUsable(p, "paper"));
    p.unlocked.insert("synthwave");
    checkTrue("synthwave unlocked", themeUsable(p, "synthwave") && !themeUsable(p, "monochrome"));
}

static void testProgress() {
    // the pop-up texts against the web's run, for every count (the web counts 5 achievements, the native app 4)
    // (the reference starts with the first `themes` secret themes and the first `achievements` achievements;
    // an unlocked secret theme also counts as its achievement, and those are the first two achievements)
    for (const ResetRef& r : kResetRef) {
        const std::string w = std::to_string(r.themes) + " themes, " + std::to_string(r.achievements) + " achievements: ";
        const int a = std::max(r.themes, r.achievements);
        checkStr(w + "reset question", r.themes + a ? kResetAsk[r.themes][a] : kResetNothing, r.ask);
        if (r.themes + a) checkStr(w + "reset done", kResetDone[r.themes][a], r.done);
        checkStr(w + "unlock line", kUnlockMsg[2 - r.themes][5 - a], r.unlock);
        // the same start in the native settings file: the same counts, less Point of No Return (Black Hole Lab)
        std::string file = "unlock=";
        for (int i = 0; i < r.themes; i++) file += std::string(i ? "," : "") + kAchievements[i].id;
        file += "\n";
        for (int i = 0; i < r.achievements; i++) file += std::string("ach.") + kAchievements[i].id + "=0\n";
        const Prefs p = loadPrefs(file);
        checkTrue(w + "native theme count", unlockedThemeCount(p) == r.themes);
        checkTrue(w + "native achievement count", earnedAchievementCount(p) == a - (r.achievements >= 3 ? 1 : 0));
    }
    // the counts the native pop-ups use, and what Reset / Unlock everything do
    Prefs p;
    p.theme = "monochrome";
    p.unlocked = {"monochrome"};
    p.achievements = {{"monochrome", 0}, {"blackhole", 0}, {"decay", 5}};
    checkTrue("themes counted", unlockedThemeCount(p) == 1);
    checkTrue("Point of No Return not counted", earnedAchievementCount(p) == 2);
    checkTrue("4 achievements in the native app", achievementCount() == 4);
    resetProgress(p);
    checkTrue("reset locks all", unlockedThemeCount(p) == 0 && p.achievements.empty());
    checkStr("reset leaves the secret theme", p.theme, "auto");
    const auto [t, a] = unlockEverything(p);
    checkTrue("unlock everything: 2 themes, 4 achievements", t == 2 && a == 4);
    const auto [t2, a2] = unlockEverything(p);
    checkTrue("unlock again: nothing new", t2 == 0 && a2 == 0);
    checkTrue("text sizes", textSizeFactor("small") == 0.9 && textSizeFactor("xl") == 1.3 && textSizeFactor("bad") == 1);
}

static void testPages() {
    // the header buttons (index.html line 219)
    const HeaderBtn* mine[] = {&kHdrTheme, &kHdrLang, &kHdrAbout, &kHdrSettings, &kHdrHistory, &kHdrVer};
    checkTrue("6 header buttons", count(kHeaderRef) == 6);
    for (std::size_t i = 0; i < 6 && i < count(kHeaderRef); i++) {
        checkStr(std::string("header ") + kHeaderRef[i].id + " label", mine[i]->label, kHeaderRef[i].label);
        checkStr(std::string("header ") + kHeaderRef[i].id + " tip", mine[i]->tip, kHeaderRef[i].tip);
    }
    // 👤 About, word for word (the web run lists the native app's two labs)
    checkStr("About title", kAboutTitle, kAboutRefTitle);
    // the page itself: see testNativeEdits (the web's page with the few native edits)
    checkTrue("About names only the native labs", std::string(kAboutHtml).find("Labs: Orbit Lab 7.7.2 · Atom Lab 7.7.3.") != std::string::npos);
    // 🌐 the language menu
    checkStr("language label", kLangLabel, kLangLabelRef);
    checkTrue("2 languages", count(kLangs) == count(kLangRef));
    for (std::size_t i = 0; i < count(kLangs) && i < count(kLangRef); i++) {
        const LangOpt& L = kLangs[i];
        const std::string html = std::string("<span style=\"font-size:18px\">") + L.flag + "</span><span class=\"nm\">" + L.name + "</span>" +
                                 (*L.soon ? std::string("<span class=\"soon\">") + L.soon + "</span>" : std::string("<span class=\"ck\">✓</span>"));
        checkStr("language " + std::to_string(i), html, kLangRef[i]);
    }
    // 🕰 history: every APP_LOG entry, as the web's timeline shows it
    checkStr("history title", kHistoryTitle, kHistoryRefTitle);
    checkStr("history intro", kHistoryIntro, kHistoryRefFirst);
    checkStr("history note", kHistoryNote, kHistoryRefLast);
    checkTrue("history length", count(kAppLog) == count(kHistoryRef));
    for (std::size_t i = 0; i < count(kAppLog) && i < count(kHistoryRef); i++) {
        const LogEntry& e = kAppLog[i];
        std::string html = std::string("<div class=\"lrnY\">") + kHistoryYear + e.v + (i == 0 ? kHistoryHere : "") + "</div><div class=\"lrnTt\">" + e.t +
                           "</div><div class=\"lrnD\">" + e.d + "</div>";
        if (*e.labButton) html += std::string("|") + e.labButton;
        checkStr(std::string("history ") + e.v, html, kHistoryRef[i]);
    }
}

static void testFactsAndTabs() {
    // 💡 every fact, word for word and in the web's order
    checkTrue("27 facts", count(kDyk) == count(kDykRef) && count(kDyk) == 27);
    for (std::size_t i = 0; i < count(kDyk) && i < count(kDykRef); i++) checkStr("fact " + std::to_string(i + 1), kDyk[i], kDykRef[i]);
    // the 🎁 and Monochrome pop-ups, first find and again
    for (int k = 0; k < 2; k++) {
        checkStr("Synthwave pop-up " + std::to_string(k), kGiftSynth[k], kGiftSynthRef[k]);
        checkStr("Monochrome pop-up " + std::to_string(k), kGiftMono[k], kGiftMonoRef[k]);
    }
    // 🏆 the Achievements tab, built the native way from the same saved state as the web run (decay on 5 Sept 2026, galactic "earlier")
    char count5[64];
    std::snprintf(count5, sizeof count5, kAchCount, 2, 5);
    checkStr("achievements head", std::string("<h2>") + kAchTitle + "</h2><div class=\"achCount\">" + count5 + "</div><div class=\"achBar\"><i style=\"width:40%\"></i></div>", kAchHeadRef);
    Prefs p = loadPrefs("ach.decay=1788609600000\nach.galactic=0\n");
    checkTrue("5 achievement cards", count(kAchievements) == count(kAchCardsRef));
    for (std::size_t i = 0; i < count(kAchievements) && i < count(kAchCardsRef); i++) {
        const AchDef& a = kAchievements[i];
        const auto it = p.achievements.find(a.id);
        const bool on = it != p.achievements.end();
        std::string html = std::string(on ? "achCard" : "achCard locked") + "|<div class=\"achIc\">" + (on ? a.ic : kAchLockedIcon) + "</div><div class=\"achTx\"><b>" + a.name +
                           "</b><span class=\"d\">" + (on ? a.done : a.how) + "</span>";
        if (on) html += std::string("<small>") + (it->second ? std::string(kAchUnlockedOn) + achievementDate(it->second) : std::string(kAchUnlockedEarlier)) + "</small>";
        html += "</div>";
        checkStr(std::string("achievement card ") + a.id, html, kAchCardsRef[i]);
    }
    for (const DateRef& d : kDateRef) checkStr("date " + std::to_string(d.ms), achievementDate(d.ms), d.text);
    // 📚 the Learn tab: the Atom Lab and Orbit Lab sections (the web's third one is the Spacetime Lab, not in the native app)
    checkStr("learn head", std::string("<h2>") + kLearnTitle + "</h2><div class=\"achCount\">" + kLearnIntro + "</div>", kLearnHeadRef);
    std::vector<std::string> mine;
    for (const LearnSection& sec : kLearnSections) {
        mine.push_back(std::string("h3|") + sec.heading);
        for (const LearnCard& c : kLearnCards)
            if (std::string(c.lab) == sec.lab)
                mine.push_back(std::string("<div class=\"achIc\">") + c.icon + "</div><div class=\"achTx\"><b>" + c.title + "</b><span class=\"d\">" + c.d + "</span></div>");
    }
    checkTrue("learn: fewer entries than the web", mine.size() < count(kLearnRef));
    for (std::size_t i = 0; i < mine.size() && i < count(kLearnRef); i++) checkStr("learn " + std::to_string(i), mine[i], kLearnRef[i]);
    checkStr("learn: then the Spacetime Lab", std::string(kLearnRef[mine.size()]).substr(0, 3), "h3|");
    // achievements and secret themes in the settings
    Prefs q;
    checkTrue("grant once", grantAchievement(q, "decay", 5) && !grantAchievement(q, "decay", 6) && q.achievements["decay"] == 5);
    checkTrue("unknown achievement", !grantAchievement(q, "nope", 1));
    checkTrue("unlock a theme earns its achievement", unlockTheme(q, "monochrome", 7) && q.achievements["monochrome"] == 7 && !unlockTheme(q, "monochrome", 8));
}


// ---- 📷 Save image: the file name and the PNG file ----

// a slow, plain CRC-32 (bit by bit), to check the encoder's table version
static uint32_t slowCrc(const uint8_t* p, std::size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return ~c;
}

// A small inflate for the blocks the encoder writes (stored, or the fixed Huffman code), written from RFC 1951 on its own.
static bool inflateFixed(const std::vector<uint8_t>& z, std::size_t pos, std::vector<uint8_t>& out) {
    std::size_t bit = pos * 8;
    auto get = [&](int n) { uint32_t v = 0; for (int i = 0; i < n; i++, bit++) v |= uint32_t((z[bit >> 3] >> (bit & 7)) & 1) << i; return v; };
    static const int lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        const uint32_t last = get(1), type = get(2);
        if (type == 0) {
            bit = (bit + 7) & ~std::size_t(7);
            const std::size_t at = bit >> 3, len = z[at] | (z[at + 1] << 8);
            out.insert(out.end(), z.begin() + at + 4, z.begin() + at + 4 + len);
            bit += (4 + len) * 8;
        } else if (type == 1) {
            for (;;) {                                           // read a code bit by bit: 7, 8 or 9 bits, most significant first
                uint32_t code = 0; int len = 0, sym = -1;
                while (sym < 0) {
                    code = (code << 1) | get(1); len++;
                    if (len == 7 && code <= 0x17) sym = 256 + int(code);
                    else if (len == 8 && code >= 0x30 && code <= 0xBF) sym = int(code) - 0x30;
                    else if (len == 8 && code >= 0xC0 && code <= 0xC7) sym = 280 + int(code) - 0xC0;
                    else if (len == 9 && code >= 0x190 && code <= 0x1FF) sym = 144 + int(code) - 0x190;
                    else if (len > 9) return false;
                }
                if (sym < 256) out.push_back(uint8_t(sym));
                else if (sym == 256) break;
                else {
                    const int lc = sym - 257;
                    if (lc > 28) return false;
                    const int length = lbase[lc] + int(get(lext[lc]));
                    uint32_t d = 0; for (int i = 0; i < 5; i++) d = (d << 1) | get(1);      // distance codes: 5 bits, most significant first
                    if (d > 29) return false;
                    const std::size_t dist = dbase[d] + get(dext[d]);
                    if (dist > out.size()) return false;
                    for (int i = 0; i < length; i++) out.push_back(out[out.size() - dist]);
                }
            }
        } else return false;
        if (last) return true;
    }
}

static uint32_t be32(const std::vector<uint8_t>& v, std::size_t i) { return (uint32_t(v[i]) << 24) | (v[i + 1] << 16) | (v[i + 2] << 8) | v[i + 3]; }

// Decodes a PNG made by encodePng into RGB pixels. Checks the signature, every chunk's CRC, the header, the zlib check sum and the filters.
static bool decodePng(const std::vector<uint8_t>& png, int& w, int& h, std::vector<uint8_t>& rgb, std::string& why) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (png.size() < 8 || !std::equal(sig, sig + 8, png.begin())) { why = "signature"; return false; }
    std::size_t i = 8;
    std::vector<uint8_t> idat;
    bool gotHeader = false, gotEnd = false;
    while (i + 12 <= png.size()) {
        const uint32_t len = be32(png, i);
        if (i + 12 + len > png.size()) { why = "chunk past the end"; return false; }
        if (be32(png, i + 8 + len) != slowCrc(&png[i + 4], 4 + len)) { why = "chunk CRC of " + std::string(png.begin() + i + 4, png.begin() + i + 8); return false; }
        const std::string type(png.begin() + i + 4, png.begin() + i + 8);
        if (type == "IHDR") {
            w = int(be32(png, i + 8)); h = int(be32(png, i + 12));
            if (len != 13 || png[i + 16] != 8 || png[i + 17] != 2 || png[i + 18] != 0 || png[i + 19] != 0 || png[i + 20] != 0) { why = "header fields"; return false; }
            gotHeader = true;
        } else if (type == "IDAT") idat.insert(idat.end(), png.begin() + i + 8, png.begin() + i + 8 + len);
        else if (type == "IEND") { gotEnd = len == 0 && i + 12 == png.size(); break; }
        i += 12 + len;
    }
    if (!gotHeader || !gotEnd) { why = "header or end missing"; return false; }
    if (idat.size() < 6 || (idat[0] & 15) != 8 || ((idat[0] << 8) | idat[1]) % 31 != 0) { why = "zlib header"; return false; }
    std::vector<uint8_t> raw;
    if (!inflateFixed(idat, 2, raw)) { why = "deflate data"; return false; }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    if (be32(idat, idat.size() - 4) != ((b << 16) | a)) { why = "zlib check sum"; return false; }
    const std::size_t stride = std::size_t(w) * 3;
    if (raw.size() != (stride + 1) * h) { why = "size of the data"; return false; }
    rgb.assign(stride * h, 0);
    for (int y = 0; y < h; y++) {
        const uint8_t f = raw[(stride + 1) * y];
        for (std::size_t x = 0; x < stride; x++) {
            const uint8_t v = raw[(stride + 1) * y + 1 + x];
            const uint8_t left = x >= 3 ? rgb[stride * y + x - 3] : 0, up = y ? rgb[stride * (y - 1) + x] : 0;
            if (f == 0) rgb[stride * y + x] = v;
            else if (f == 1) rgb[stride * y + x] = uint8_t(v + left);
            else if (f == 2) rgb[stride * y + x] = uint8_t(v + up);
            else { why = "filter type"; return false; }
        }
    }
    return true;
}

static void testShot() {
    // the button's texts are the web's
    checkStr("shot title", kShotTitle, kShotTitleRef);
    checkStr("shot label", kShotLabel, kShotLabelRef);
    checkStr("shot saved", kShotSaved, kShotSavedRef);
    checkStr("shot failed", kShotFailed, kShotFailedRef);
    // the file name: the web's picture name, then the local date and time
    const ShotTime t{2026, 10, 1, 9, 5, 7};
    const std::string stamp = "-2026-10-01-090507.png";
    checkStr("orbit file name", shotFileName("Orbit Lab", 1, 0, t), std::string(kShotNameRef[0]).substr(0, std::strlen(kShotNameRef[0]) - 4) + stamp);
    checkStr("orbit file name text", shotFileName("Orbit Lab", 1, 0, t), "orbit-lab-2026-10-01-090507.png");
    checkStr("atom file name", shotFileName("Atom Lab", 6, 6, t), std::string(kShotNameRef[1]).substr(0, std::strlen(kShotNameRef[1]) - 4) + stamp);
    checkStr("other lab name", shotFileName("Some other-lab 2", 0, 0, t), std::string(kShotNameRef[2]).substr(0, std::strlen(kShotNameRef[2]) - 4) + stamp);
    checkStr("atom file name with Z and N", shotFileName("Atom Lab", 92, 146, {2027, 12, 31, 23, 59, 59}), "atom-lab-Z92-N146-2027-12-31-235959.png");
    checkTrue("two saves a second apart differ", shotFileName("Orbit Lab", 1, 0, t) != shotFileName("Orbit Lab", 1, 0, {2026, 10, 1, 9, 5, 8}));
    checkTrue("no character Windows refuses", shotFileName("Atom Lab", 6, 6, t).find_first_of("<>:\"/\\|?*") == std::string::npos);
    checkStr("path on Windows", joinPath("C:\\Users\\Alex\\Pictures", "a.png"), "C:\\Users\\Alex\\Pictures\\a.png");
    checkStr("path with a trailing slash", joinPath("C:\\Users\\Alex\\Pictures\\", "a.png"), "C:\\Users\\Alex\\Pictures\\a.png");
    checkStr("path on Linux", joinPath("/home/alex/Pictures", "a.png"), "/home/alex/Pictures/a.png");
    checkStr("path with trailing /", joinPath("/home/alex/Pictures/", "a.png"), "/home/alex/Pictures/a.png");

    // the PNG: pictures of several kinds go through the encoder and come back exactly the same
    struct Case { const char* name; int w, h; int kind; };
    const Case cases[] = {{"1x1", 1, 1, 0}, {"flat", 64, 48, 1}, {"gradient", 200, 100, 2}, {"noise", 97, 61, 3}, {"stripes", 300, 40, 4}, {"wide and short", 1000, 3, 5},
                          {"picture-sized", 1040, 713, 6}};
    unsigned seed = 12345;
    for (const Case& c : cases) {
        std::vector<uint8_t> px(std::size_t(c.w) * c.h * 3);
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++)
                for (int k = 0; k < 3; k++) {
                    uint8_t v = 0;
                    seed = seed * 1664525u + 1013904223u;
                    switch (c.kind) {
                    case 0: v = uint8_t(200 + k); break;
                    case 1: v = uint8_t(10 + 5 * k); break;                                  // one flat colour: must be small
                    case 2: v = uint8_t(x + y + 40 * k); break;
                    case 3: v = uint8_t(seed >> 24); break;                                  // noise: cannot be squeezed
                    case 4: v = uint8_t(((x / 7) % 2) ? 255 : 3 * k); break;
                    case 5: v = uint8_t((x * 3 + k) ^ y); break;
                    default: v = uint8_t((x - 520) * (x - 520) + (y - 356) * (y - 356) < 90000 ? 40 + 60 * k : (x % 50 == 0 || y % 50 == 0 ? 90 : 5)); break;   // a dark picture with a disc and a grid
                    }
                    px[(std::size_t(y) * c.w + x) * 3 + k] = v;
                }
        const std::vector<uint8_t> png = encodePng(c.w, c.h, px.data());
        int w = 0, h = 0;
        std::vector<uint8_t> back;
        std::string why;
        const bool ok = decodePng(png, w, h, back, why);
        checkTrue(std::string("png ") + c.name + " is valid (" + why + ")", ok);
        checkTrue(std::string("png ") + c.name + " size", ok && w == c.w && h == c.h);
        checkTrue(std::string("png ") + c.name + " pixels come back exactly", ok && back == px);
        if (c.kind == 1) checkTrue("a flat picture is tiny", png.size() < 400);
        if (c.kind == 6) checkTrue("a picture-sized image is much smaller than raw", png.size() < px.size() / 10);
    }
}

// ---- native edits ----
// The native app has the Orbit Lab and the Atom Lab only. The words that name the Black Hole Lab, the Spacetime Lab and
// the Slingshot game are taken out of the web's texts, and NOTHING else is changed. This is the whole list. Each text is
// compared with the web's own text (from the web run in tests/reference) after exactly these edits, so any other
// difference fails. An edit must match exactly once in its text. (tools/native_edits.js makes the native texts from the
// same list, written separately.)
struct NativeEdit {
    const char* what;
    const char* from;           // this text...
    const char* to;             // ...becomes this
    const char* until = nullptr;   // if set: remove from `from` up to (not including) `until`
};
static const NativeEdit kEditUnits = {"Settings → Units note: no Black Hole readout", ", the Black Hole readout", ""};
static const NativeEdit kEditsAbout[] = {
    {"About card: no black holes or space game", "real orbits, atoms, black holes and a little space game.", "real orbits and atoms."},
    {"About sources: no Black holes, no Spacetime and real objects", "<div class=\"srcH\">🕳️ Black holes</div>", "", "<div class=\"srcH\">🌌 Stars and far-away things</div>"},
    // the Orbit Lab still says Sagittarius A* weighs 4.3 million Suns: that source stays, in the far-away things
    {"About sources: keep the GRAVITY Collaboration source (Sagittarius A*)", "(27,000 light years)</li></ul>",
     "(27,000 light years)</li><li><a href=\"https://arxiv.org/abs/2112.07478\" target=\"_blank\" rel=\"noopener noreferrer\">GRAVITY Collaboration, 2022: mass distribution in the Galactic Centre</a> (Sagittarius A*, 4.3 million Suns)</li></ul>"},
};
static const NativeEdit kEditSgrA = {"Sagittarius A* card: no Spacetime Lab", " (see the Spacetime Lab)", ""};
static const NativeEdit kEditGuideSky = {"Orbit guide, stars and far-away things: no Spacetime Lab", " (the Spacetime Lab shows what a black hole like it does)", ""};
static const NativeEdit kEditSlingshot = {"Timeline 1977–1989: no slingshot game button", "Try the slingshot game", ""};

static int g_editUses = 0;
// the web's text with one edit; a failed match is a failed check (the edit would do nothing, or too much)
static std::string withEdit(const std::string& text, const NativeEdit& e) {
    std::size_t at = text.find(e.from), n = 0;
    for (std::size_t p = text.find(e.from); p != std::string::npos; p = text.find(e.from, p + 1)) n++;
    if (n != 1) { checkTrue(std::string(e.what) + ": the web text has the words exactly once", false); return text; }
    g_editUses++;
    if (!e.until) return text.substr(0, at) + e.to + text.substr(at + std::strlen(e.from));
    const std::size_t end = text.find(e.until, at);
    if (end == std::string::npos) { checkTrue(std::string(e.what) + ": the end of the cut is there", false); return text; }
    return text.substr(0, at) + e.to + text.substr(end);
}
static bool mentionsMissingParts(const std::string& t) {
    for (const char* w : {"Black Hole Lab", "Spacetime Lab", "Slingshot", "slingshot", "space game", "Black Hole readout", "Games tab"})
        if (t.find(w) != std::string::npos) return true;
    return false;
}

static void testNativeEdits() {
    // Settings → Units note
    checkStr("Units note", kUnitsNote, withEdit(kUnitsNoteRef, kEditUnits));
    checkTrue("Units note names no missing part", !mentionsMissingParts(kUnitsNote));
    // About
    std::string about = kAboutRef;
    for (const NativeEdit& e : kEditsAbout) about = withEdit(about, e);
    checkStr("About page (the web's, with the native edits)", kAboutHtml, about);
    checkTrue("About names no missing part", !mentionsMissingParts(kAboutHtml) && std::string(kAboutHtml).find("🕳️") == std::string::npos);
    checkTrue("About still lists the sources of both labs", std::string(kAboutHtml).find("NASA/JPL: planet positions") != std::string::npos && std::string(kAboutHtml).find("NUBASE") != std::string::npos);
    // the far-away things: every note is the web's, the Sagittarius A* one with its edit
    using nativeorbit::kFar;
    checkTrue("19 far-away things", count(kFar) == count(kFarNoteRef) && count(kFar) == 19);
    int sgra = 0;
    for (std::size_t i = 0; i < count(kFar) && i < count(kFarNoteRef); i++) {
        checkStr(std::string("far key ") + std::to_string(i), kFar[i].key, kFarNoteRef[i].key);
        const bool isSgra = std::string(kFarNoteRef[i].key) == "sgra";
        sgra += isSgra;
        checkStr(std::string("far note ") + kFar[i].key, kFar[i].note, isSgra ? withEdit(kFarNoteRef[i].note, kEditSgrA) : std::string(kFarNoteRef[i].note));
        checkTrue(std::string("far note names no missing part ") + kFar[i].key, !mentionsMissingParts(kFar[i].note));
    }
    checkTrue("one Sagittarius A*", sgra == 1);
    // the Orbit guide "The stars and the far-away things", in all 8 cases
    for (int v = 0; v < 8; v++) {
        int found = 0;
        for (int k = 0; k < nativeui::kLookSectionCount; k++) {
            const nativeui::LearnSection& sec = nativeui::kLook[v][k];
            if (std::string(sec.key) != "sky") { checkTrue(std::string("guide section ") + sec.key + " names no missing part", !mentionsMissingParts(sec.html)); continue; }
            found++;
            checkStr("guide sky section, case " + std::to_string(v), sec.html, withEdit(kLookSkyRef[v], kEditGuideSky));
        }
        checkTrue("guide has one sky section, case " + std::to_string(v), found == 1);
    }
    // the Solar System timeline: every step is the web's (text, who, title, year and picture), only the slingshot button is gone
    using nativeui::kTimeline;
    checkTrue("timeline length", count(kTimeline) == count(kTimelineRef) && count(kTimeline) == 29);
    int slingshot = 0;
    for (std::size_t i = 0; i < count(kTimeline) && i < count(kTimelineRef); i++) {
        const std::string n = "timeline " + std::to_string(i) + " ";
        checkStr(n + "year", kTimeline[i].when, kTimelineRef[i].y);
        checkStr(n + "who", kTimeline[i].who, kTimelineRef[i].who);
        checkStr(n + "title", kTimeline[i].title, kTimelineRef[i].t);
        checkStr(n + "text", kTimeline[i].text, kTimelineRef[i].d);
        checkStr(n + "picture", kTimeline[i].svg, kTimelineRef[i].svg);
        const bool game = std::string(kTimelineRef[i].button) == kEditSlingshot.from;
        slingshot += game;
        checkStr(n + "button", kTimeline[i].button, game ? withEdit(kTimelineRef[i].button, kEditSlingshot) : std::string(kTimelineRef[i].button));
        checkTrue(n + "no game button left", std::string(kTimeline[i].lab) != "game" && !mentionsMissingParts(std::string(kTimeline[i].button) + kTimeline[i].text + kTimeline[i].title));
        checkTrue(n + "has a picture", kTimeline[i].svg[0] == '<');
        checkTrue(n + "a button has an action and the other way round", (*kTimeline[i].button != 0) == (*kTimeline[i].lab != 0));
    }
    checkTrue("exactly one slingshot button in the web", slingshot == 1);
    // every edit was used (nothing in the list is left over): units 1 + about 3 + sgra 1 + guide 8 + slingshot 1
    checkTrue("every native edit was used", g_editUses == 1 + 3 + 1 + 8 + 1);
}

// ---- the frames-per-second counter text (the web: Math.round(frames * 1000 / ms) + ' fps') ----
static void testFps() {
    checkStr("fps before the first measurement", fpsText(-1), "");
    checkStr("fps zero", fpsText(0), "0 fps");
    checkStr("fps 59.49 rounds down", fpsText(59.49), "59 fps");
    checkStr("fps 59.5 rounds up like Math.round", fpsText(59.5), "60 fps");
    checkStr("fps 144.2", fpsText(144.2), "144 fps");
    checkStr("fps 1000", fpsText(1000), "1000 fps");
}

// ---- the window's size, place and maximised state in the settings file ----
static void testWindow() {
    Prefs p;
    p.window = WindowState{1234, 777, -50, 80, true, true};
    Prefs q = loadPrefs(savePrefs(p));
    checkTrue("window round trip", q.window.has_value() && *q.window == *p.window);
    p.window = WindowState{900, 600, 0, 0, false, false};          // no known place
    q = loadPrefs(savePrefs(p));
    checkTrue("window without a place", q.window && q.window->w == 900 && q.window->h == 600 && !q.window->hasPos && !q.window->maximized);
    checkTrue("no window saved: none loaded", !loadPrefs(savePrefs(Prefs{})).window.has_value());
    checkTrue("a window line in the file", savePrefs(Prefs{[] { Prefs x; x.window = WindowState{1000, 700, 10, 20, true, false}; return x; }()}).find("\nwindow=1000,700,10,20,0\n") != std::string::npos);
    // bad or odd lines are ignored, like the other settings
    for (const char* bad : {"window=\n", "window=abc\n", "window=1000,700\n", "window=1000,700,1,2\n", "window=1000,700,1,2,3\n", "window=0,700,1,2,0\n", "window=-5,700,1,2,0\n",
                            "window=1000,700,x,2,0\n", "window=1000,700,1,2,0,9\n", "window=99999999,700,1,2,0\n", "window=1000,700,999999999,2,0\n", "window=1000;700;1;2;0\n", "window=1000,700,-,2,0\n"})
        checkTrue(std::string("bad window line ignored: ") + bad, !loadPrefs(bad).window.has_value());
    checkTrue("a bad line does not touch the other settings", loadPrefs("theme=paper\nwindow=oops\nfont=serif\n").theme == "paper" && loadPrefs("theme=paper\nwindow=oops\nfont=serif\n").font == "serif");
    checkTrue("the last good line wins, a later bad one does not erase it", loadPrefs("window=1000,700,1,2,0\nwindow=bad\n").window && loadPrefs("window=1000,700,1,2,0\nwindow=bad\n").window->w == 1000);
    // never smaller than the safe minimum
    const Prefs tiny = loadPrefs("window=100,50,0,0,0\n");
    checkTrue("a tiny window is raised to the minimum", tiny.window && tiny.window->w == kMinWindowW && tiny.window->h == kMinWindowH);
    checkTrue("fitWindow raises a tiny size too", fitWindow(WindowState{10, 10, 0, 0, true, false}, {{0, 0, 1920, 1080}}).w == kMinWindowW);
    // the place: on a screen, partly on a screen, off every screen
    const std::vector<ScreenRect> one = {{0, 0, 1920, 1080}};
    const std::vector<ScreenRect> two = {{0, 0, 1920, 1080}, {1920, 0, 1920, 1080}};
    checkTrue("on the screen: kept", fitWindow(WindowState{1400, 860, 100, 50, true, false}, one).hasPos);
    checkTrue("on the second screen: kept", fitWindow(WindowState{1400, 860, 2000, 50, true, false}, two).hasPos);
    checkTrue("second monitor unplugged: centred", !fitWindow(WindowState{1400, 860, 2000, 50, true, false}, one).hasPos);
    checkTrue("left of every screen: centred", !fitWindow(WindowState{1000, 700, -3000, 50, true, false}, one).hasPos);
    checkTrue("below every screen: centred", !fitWindow(WindowState{1000, 700, 100, 2000, true, false}, one).hasPos);
    checkTrue("only a sliver on screen: centred", !fitWindow(WindowState{1000, 700, 1850, 50, true, false}, one).hasPos);
    checkTrue("title bar on screen, body hanging off the bottom: kept", fitWindow(WindowState{1000, 700, 100, 1000, true, false}, one).hasPos);
    checkTrue("title bar above the screen: centred", !fitWindow(WindowState{1000, 700, 100, -300, true, false}, one).hasPos);
    checkTrue("no screens known: the place is kept out", !fitWindow(WindowState{1000, 700, 100, 100, true, false}, {}).hasPos);
    const WindowState big = fitWindow(WindowState{5000, 3000, 0, 0, true, false}, one);
    checkTrue("bigger than the biggest screen: shrunk to it", big.w == 1920 && big.h == 1080);
    checkTrue("maximised is kept", fitWindow(WindowState{1000, 700, 100, 100, true, true}, one).maximized);
}

// ---- the native additions to the 🕰 history page ----
static void testHistoryNotes() {
    const std::string note = kHistoryWebNote;
    checkTrue("web note says the entries are the web's history", note.find("the entries below are the web app's history, word for word") != std::string::npos);
    checkTrue("web note names what native lacks", note.find("Black Hole Lab") != std::string::npos && note.find("Games tab") != std::string::npos && note.find("it has the Orbit Lab and the Atom Lab") != std::string::npos);
    checkTrue("web note no longer points to a list after it", note.find("listed after") == std::string::npos && note.find("after them") == std::string::npos);
    checkTrue("two different headings", std::string(kHistoryNativeHeading) != kHistoryWebHeading && std::string(kHistoryWebHeading).find("web") != std::string::npos);
    // the web's own history text is still compared word for word in testPages (kAppLog against the web run, intro, closing note)
}

// ---- the benchmark: the numbers, the run, the text, and giving everything back ----
static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps * std::max(1.0, std::fabs(b)); }

static void testBench() {
    // the statistics, with known inputs
    BenchStats e = benchStats({});
    checkTrue("no frames: zeros", e.frames == 0 && e.avgFps == 0 && e.worstMs == 0 && e.low1Fps == 0);
    BenchStats a = benchStats({10, 10, 10, 10});
    checkTrue("four frames of 10 ms: 100 fps", a.frames == 4 && near(a.avgMs, 10) && near(a.avgFps, 100) && near(a.worstMs, 10) && near(a.low1Fps, 100) && near(a.totalS, 0.04));
    std::vector<double> v(200, 10.0);
    v.push_back(100); v.insert(v.begin() + 7, 100);                         // 202 frames, two slow ones (not at the end)
    BenchStats b = benchStats(v);
    checkTrue("average of 202 frames", near(b.avgMs, (200 * 10.0 + 200) / 202) && near(b.avgFps, 1000 / ((2000.0 + 200) / 202)));
    checkTrue("worst frame", near(b.worstMs, 100));
    checkTrue("1 % low: the slowest ceil(1 %) = 3 frames (100, 100, 10)", near(b.low1Fps, 1000 / 70.0));
    BenchStats one = benchStats({25});
    checkTrue("one frame: it is its own 1 % low", near(one.low1Fps, 40) && near(one.avgFps, 40) && near(one.worstMs, 25));
    std::vector<double> hundred(100, 5.0); hundred[42] = 50;
    checkTrue("100 frames: the 1 % low is the single slowest frame", near(benchStats(hundred).low1Fps, 20));
    std::vector<double> sorted = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, rev(sorted.rbegin(), sorted.rend());
    checkTrue("order does not matter", near(benchStats(sorted).low1Fps, benchStats(rev).low1Fps) && near(benchStats(rev).low1Fps, 100));

    // a run: 3 scenes, 2 s warm-up and 8 s counted, frames of 100 ms
    BenchRun run(3);
    checkTrue("the first scene is set up first", run.sceneToSetUp() == 0 && !run.finished());
    run.gpuSample(1, 1); run.frame(100, 5);                                  // before the set-up is done: ignored
    checkTrue("a frame before the set-up does not count", run.sceneElapsedS() == 0);
    run.setUpDone();
    int setups = 1, frames = 0;
    std::vector<int> order = {0};
    while (!run.finished() && frames < 100000) {
        if (run.sceneToSetUp() >= 0) { order.push_back(run.sceneToSetUp()); run.setUpDone(); setups++; }
        if (run.scene() != 1) run.gpuSample(2, 3);                              // scene 1 has no GPU numbers
        run.frame(100, 4 + run.scene());
        frames++;
    }
    checkTrue("3 scenes set up in order", setups == 3 && order == std::vector<int>({0, 1, 2}));
    checkTrue("every scene takes 10 s: 100 frames of 100 ms", frames >= 300 && frames <= 303);
    checkTrue("finished, total 30 s", run.finished() && near(run.totalS(), 30) && near(run.doneS(), 30));
    for (int i = 0; i < 3; i++) {
        const BenchScene r = run.result(i);
        checkTrue("scene " + std::to_string(i) + ": about 80 counted frames, the warm-up not counted", r.frames.frames >= 79 && r.frames.frames <= 81);
        checkTrue("scene " + std::to_string(i) + ": CPU average", near(r.cpuAvgMs, 4 + i));
        checkTrue("scene " + std::to_string(i) + ": GPU average or not available", i == 1 ? (r.gpuAvgMs < 0 && r.gpuSamples == 0) : (near(r.gpuAvgMs, 2) && near(r.gpuSpanAvgMs, 3) && r.gpuSamples > 0 && r.gpuProblem.empty()));
        checkTrue("scene " + std::to_string(i) + ": 10 fps", near(r.frames.avgFps, 10));
    }
    BenchRun early(2);
    early.setUpDone();
    for (int i = 0; i < 150; i++) early.frame(100, 1);                  // 15 s: scene 0 done, scene 1 waiting for its set-up
    checkTrue("a cancelled run knows how many scenes are done", !early.finished() && early.doneScenes() == 1 && early.sceneToSetUp() == 1);

    // the text
    BenchEnv env;
    env.version = "0.9"; env.when = "2026-10-01 15:30:45"; env.gpu = "Test GPU 9000"; env.glVersion = "3.3.0 Test"; env.vendor = "Test Vendor";
    env.windowW = 1400; env.windowH = 860; env.pictureW = 1040; env.pictureH = 713; env.refreshHz = 144; env.textSize = "xl"; env.theme = "paper"; env.fullscreen = true; env.gpuTimer = true; env.gpuReliable = true; env.gpuCheck = "self-check with a known load: timer 3.10 ms, clock 3.20 ms";
    BenchScene s1; s1.name = "1. First"; s1.what = "some settings"; s1.frames = benchStats({10, 10, 10, 10}); s1.cpuAvgMs = 3.5; s1.gpuAvgMs = 6.25; s1.gpuSpanAvgMs = 7.5; s1.gpuSamples = 4;
    BenchScene s2; s2.name = "2. Second"; s2.what = "other settings"; s2.frames = benchStats({20, 20}); s2.cpuAvgMs = 1; s2.gpuAvgMs = -1;
    const std::string t = benchResultText(env, {s1, s2});
    const char* want[] = {"Physics Lab (native) benchmark", "Version: 0.9", "Date: 2026-10-01 15:30:45", "GPU: Test GPU 9000", "OpenGL: 3.3.0 Test", "Driver / vendor: Test Vendor",
                          "Window: 1400 x 860 pixels, full screen", "Picture area: 1040 x 713 pixels", "Display refresh rate: 144 Hz", "Text size: xl, theme: paper",
                          "VSync: off during the test. Each scene: 2 s warm-up (not counted), then 8 s counted.", "GPU timer (GL_TIME_ELAPSED): available", "GPU timer check: reliable, self-check with a known load: timer 3.10 ms, clock 3.20 ms"};
    std::size_t at = 0;
    bool inOrder = true;
    for (const char* w : want) { const std::size_t p = t.find(w, at); if (p == std::string::npos) { inOrder = false; checkTrue(std::string("result text has: ") + w, false); break; } at = p; }
    checkTrue("the result text starts with the version, date and graphics, in order", inOrder && t.rfind("Physics Lab (native) benchmark\n", 0) == 0);
    checkTrue("result text: scene 1 numbers", t.find("  Average: 10.00 ms per frame = 100.0 fps\n  1 % low: 100.0 fps") != std::string::npos && t.find("  Worst frame: 10.00 ms\n  CPU work per frame: 3.50 ms\n  GPU time per frame: 6.25 ms") != std::string::npos);
    checkTrue("result text: the GPU span line", t.find("  GPU span per frame: 7.50 ms") != std::string::npos);
    checkTrue("result text: no GPU time says not available, it does not guess", t.find("2. Second\n") != std::string::npos && t.find("GPU time per frame: not available") != std::string::npos);
    checkTrue("result text: the average over the scenes", t.find("Average fps over the scenes: 75.0") != std::string::npos);
    BenchEnv none = env; none.gpuTimer = false; none.refreshHz = 0; none.vendor = ""; none.fullscreen = false; none.cancelled = true;
    const std::string t2 = benchResultText(none, {s1});
    checkTrue("unknown things are said to be unknown", t2.find("Display refresh rate: not available") != std::string::npos && t2.find("Driver / vendor: not available") != std::string::npos &&
                                                      t2.find("GPU timer (GL_TIME_ELAPSED): not available") != std::string::npos && t2.find("not full screen") != std::string::npos && t2.find("THE TEST WAS CANCELLED") != std::string::npos);
    checkStr("benchmark file name", benchFileName({2026, 10, 1, 9, 5, 7}), "physics-lab-benchmark-2026-10-01-090507.txt");

    // giving everything back: the Atom Lab and the Solar System are copied before, put back after, and come back exactly
    atom::AtomLab L;
    L.setParam("Z", 6.0); L.setParam("N", 6.0); L.setParam("view", std::string("Slice")); L.setParam("zoom", 1.5); L.setParam("n", 3.0);
    for (int i = 0; i < 500; i++) L.step(1.0 / 240);
    const auto describe = [](atom::AtomLab x) {
        std::string d;
        for (const std::string& l : x.info()) d += l + "|";
        return d + x.p.view + x.p.qstyle + x.p.detail + x.p.style + x.p.orb + std::to_string(x.p.Z) + "," + std::to_string(x.p.N) + "," + std::to_string(x.p.zoom) + "," + std::to_string(x.p.n) + "," +
               std::to_string(x.p.speed) + "," + std::to_string(x.s.t) + "," + std::to_string(x.s.k) + "," + std::to_string(x.s.focus) + "," + std::to_string(x.s.nucSeed) + "," + std::to_string(x.s.spin);
    };
    const std::string before = describe(L);
    const atom::AtomLab saved = L;
    std::vector<std::string> sceneText;
    for (int sc = 0; sc < 3; sc++) {
        atom::benchSetup(L, sc, 1040, 713);
        checkTrue("scene " + std::to_string(sc + 1) + " changes the lab", describe(L) != before && L.p.Z == 92 && L.p.N == 146);
        sceneText.push_back(describe(L));
    }
    checkTrue("scenes 1 to 3 differ from each other", sceneText[0] != sceneText[1] && sceneText[1] != sceneText[2] && sceneText[0] != sceneText[2]);
    checkTrue("scene 2 is Cloud 3D with all the dots", [&] { atom::benchSetup(L, 1, 1040, 713); return L.p.view == "Cloud 3D" && L.s.k >= atom::kCap && L.p.orb == "4f"; }());
    checkTrue("scene 3 is zoomed into the first nucleon", [&] { atom::benchSetup(L, 2, 1040, 713); return L.s.focus == 0 && L.p.qstyle == "Accurate" && L.p.detail == "+ Sea"; }());
    L = saved;
    checkStr("the Atom Lab is exactly as it was after the benchmark", describe(L), before);
    // repeatable: the same scene twice gives the same cloud and the same readout
    atom::benchSetup(L, 1, 1040, 713);
    const std::vector<float> cloud1(L.s.pts.begin(), L.s.pts.begin() + 3000);
    const std::string d1 = describe(L);
    atom::benchSetup(L, 1, 1040, 713);
    checkTrue("the cloud scene is repeatable (same dots)", std::equal(cloud1.begin(), cloud1.end(), L.s.pts.begin()) && describe(L) == d1);
    atom::benchSetup(L, 0, 1040, 713); const std::string n1 = describe(L);
    atom::benchSetup(L, 2, 1040, 713); atom::benchSetup(L, 0, 1040, 713);
    checkStr("the nucleus scene is repeatable", describe(L), n1);

    orbit::SolarSystem sys;
    sys.loadAt(1790769600000LL);
    std::vector<double> x0;
    for (const auto& body : sys.bodies) x0.push_back(body.pos.x + body.pos.y + body.pos.z);
    const orbit::SolarSystem keep = sys;
    for (int i = 0; i < 2000; i++) sys.step(1.0 / 240, 2.5);                 // the benchmark's fastest time speed
    checkTrue("the Solar System moved during the benchmark", sys.t != keep.t);
    sys = keep;
    bool same = sys.bodies.size() == x0.size() && sys.t == keep.t;
    for (std::size_t i = 0; same && i < sys.bodies.size(); i++) same = sys.bodies[i].pos.x + sys.bodies[i].pos.y + sys.bodies[i].pos.z == x0[i];
    checkTrue("the Solar System is back exactly as it was", same);
}

// ---- the time step, the live-frame counters, the GPU timer ring and its verdict, the stress belts (version 0.9) ----
static void testLiveAndGpu() {
    // the time step of a frame
    checkTrue("step: a normal frame counts as it is", near(stepSeconds(0.016, 0.05), 0.016));
    checkTrue("step: a slow frame counts as at most 0.05 s", near(stepSeconds(0.2, 0.05), 0.05) && near(stepSeconds(0.9, 0.05), 0.05));
    checkTrue("step: a stall (the window held by the system, 3 s) counts as nothing, no giant step", stepSeconds(3.0, 0.05) == 0 && stepSeconds(kStallS + 0.001, 0.05) == 0);
    checkTrue("step: zero, negative and not-a-number gaps count as nothing", stepSeconds(0, 0.05) == 0 && stepSeconds(-1, 0.05) == 0 && stepSeconds(std::nan(""), 0.05) == 0);
    const auto physicsSteps = [](const std::vector<double>& raws) {     // fixed 1/240 s steps, as the main loop does
        double acc = 0; int steps = 0;
        for (double raw : raws) { acc += stepSeconds(raw, 0.05); while (acc >= 1.0 / 240) { acc -= 1.0 / 240; steps++; } }
        return steps;
    };
    checkTrue("a 5 s freeze in the middle adds no physics steps: no jump, no catching up", physicsSteps({0.016, 0.016, 5.0, 0.016, 0.016}) == physicsSteps({0.016, 0.016, 0.016, 0.016}) && physicsSteps({0.016, 0.016, 0.016, 0.016}) == 15);

    // frames drawn while the window system holds the loop
    LiveStats ls;
    ls.frame(false, 16); ls.frame(true, 700); ls.frame(true, 10); ls.frame(true, 14); ls.frame(false, 16); ls.frame(false, 16); ls.frame(true, 300); ls.frame(true, 12);
    checkTrue("live counters: 5 live frames in 2 drags", ls.frames == 5 && ls.drags == 2);
    checkTrue("live counters: the worst gap inside a drag is 14 ms, the longest lead-in 700 ms", near(ls.worstGapMs, 14) && near(ls.leadInMs, 700));
    LiveStats none; none.frame(false, 16); none.frame(false, 16);
    checkTrue("live counters: no drag, no live frames", none.frames == 0 && none.drags == 0);

    // the ring of GPU queries
    QueryRing ring(3);
    const int a = ring.acquire(); ring.submitted(a);
    const int b = ring.acquire(); ring.submitted(b);
    const int c = ring.acquire(); ring.submitted(c);
    checkTrue("ring: three slots in turn", a == 0 && b == 1 && c == 2 && ring.slots() == 3);
    checkTrue("ring: all busy, nothing free", ring.acquire() == -1 && ring.waiting() == 3);
    std::vector<int> got;
    std::vector<bool> ready(3, false);
    ring.collect([&](int s) { return bool(ready[s]); }, [](int s) { return s; }, got);
    checkTrue("ring: nothing is read before it is ready", got.empty() && ring.waiting() == 3);
    ready[1] = true;                                       // a later slot ready while the oldest is not: wait for the oldest (the GPU finishes in order)
    ring.collect([&](int s) { return bool(ready[s]); }, [](int s) { return s; }, got);
    checkTrue("ring: results come oldest first, a younger one does not jump the queue", got.empty());
    ready[0] = true;
    ring.collect([&](int s) { return bool(ready[s]); }, [](int s) { return s; }, got);
    checkTrue("ring: two results, oldest first, and their slots are free again", got == std::vector<int>({0, 1}) && ring.waiting() == 1 && ring.acquire() == 0);
    ring.reset();
    checkTrue("ring: reset frees everything", ring.waiting() == 0 && ring.acquire() == 0);

    // is the timer to be trusted?
    checkTrue("verdict: timer and clock agree: reliable", judgeGpuTimer(3.0, 3.2).reliable && judgeGpuTimer(3.0, 3.2).why.find("timer 3.00 ms, clock 3.20 ms") != std::string::npos);
    const GpuVerdict low = judgeGpuTimer(0.09, 3.0);
    checkTrue("verdict: the timer says 0.09 ms for a load the clock needs 3 ms for: not reliable, and the text says both numbers", !low.reliable && low.why.find("0.09") != std::string::npos && low.why.find("3.00") != std::string::npos);
    checkTrue("verdict: a timer that says far too much is not reliable either", !judgeGpuTimer(9, 3).reliable);
    checkTrue("verdict: 0 is not reliable", !judgeGpuTimer(0, 3).reliable);
    checkTrue("verdict: a load that is too small tells nothing", !judgeGpuTimer(0.1, 0.1).reliable && judgeGpuTimer(0.1, 0.1).why.find("too small") != std::string::npos);
    checkTrue("verdict: the edges 60 % and 130 % are accepted", judgeGpuTimer(1.8, 3.0).reliable && judgeGpuTimer(3.9, 3.0).reliable && !judgeGpuTimer(1.7, 3.0).reliable);

    // one scene's numbers
    checkTrue("scene check: fine", gpuSceneProblem(1000, 900, 1.0, 1.5, 2.0).empty());
    checkTrue("scene check: no results", !gpuSceneProblem(1000, 0, -1, -1, 2.0).empty());
    checkTrue("scene check: fewer than half of the frames gave a result", gpuSceneProblem(1000, 400, 1.0, 1.5, 2.0).find("400 of 1000") != std::string::npos);
    checkTrue("scene check: a span shorter than the busy time inside it is a contradiction", gpuSceneProblem(1000, 900, 1.4, 0.88, 2.0).find("contradict") != std::string::npos && gpuSceneProblem(1000, 900, 1.0, 0.8, 2.0).empty());
    checkTrue("scene check: longer than the frame itself", !gpuSceneProblem(1000, 900, 5.0, 6.0, 2.0).empty());

    // the text when the timer is not reliable: no number that looks real
    BenchEnv env;
    env.version = "0.9"; env.gpuTimer = true; env.gpuReliable = false; env.gpuCheck = low.why;
    BenchScene s; s.name = "1. First"; s.frames = benchStats({2, 2, 2, 2}); s.gpuAvgMs = 0.09; s.gpuSpanAvgMs = 0.5; s.gpuSamples = 4;
    const std::string t = benchResultText(env, {s});
    checkTrue("text: says not reliable instead of a number", t.find("GPU time per frame: not reliable (") != std::string::npos && t.find("GPU time per frame: 0.09") == std::string::npos &&
                                                             t.find("GPU timer check: NOT RELIABLE") != std::string::npos);
    checkTrue("text: the raw values are kept, marked as not to be trusted", t.find("raw values, do not trust them: busy 0.09 ms, span 0.50 ms, 4 of 4 frames") != std::string::npos);
    BenchEnv okEnv = env; okEnv.gpuReliable = true;
    BenchScene bad = s; bad.gpuProblem = "only 1 of 4 frames gave a result";
    checkTrue("text: a reliable timer but a bad scene still says not reliable", benchResultText(okEnv, {bad}).find("not reliable (only 1 of 4 frames gave a result)") != std::string::npos);
    checkTrue("text: it says what the GPU time covers", t.find("up to just before the buffer swap") != std::string::npos);

    // the stress belts: scale 1 is the app's normal belts; scale 8 has 8 times the rocks of the same kinds, always the same
    const int64_t day = 1790000000000LL;
    const auto base = orbit::makeBelts(day), same = orbit::makeBelts(day, 1), big = orbit::makeBelts(day, 8), big2 = orbit::makeBelts(day, 8);
    bool equal = base.size() == same.size(), eight = big.size() == base.size(), repeat = true;
    for (std::size_t g = 0; g < base.size() && equal && eight; g++) {
        equal = equal && base[g].n == same[g].n && base[g].a == same[g].a && base[g].x == same[g].x;
        eight = eight && big[g].n == 8 * base[g].n && big[g].key == base[g].key;
        repeat = repeat && big[g].n == big2[g].n && big[g].a == big2[g].a && big[g].x == big2[g].x;
    }
    checkTrue("belts: scale 1 is the normal belts, scale 8 has 8 times the rocks, scale 0 or less counts as 1", equal && eight && repeat && orbit::makeBelts(day, 0)[0].n == base[0].n);
    checkTrue("belts: the first rocks of the big main belt are the normal ones (same kind of orbit, same seed)", std::equal(base[0].a.begin(), base[0].a.end(), big[0].a.begin()));
    // the stress Atom Lab scene is repeatable and is the biggest nucleus
    atom::AtomLab A1, A2;
    atom::benchSetup(A1, 3, 800, 600); atom::benchSetup(A2, 3, 800, 600);
    checkTrue("stress atom scene: oganesson-294, identical twice", A1.p.Z == 118 && A1.p.N == 176 && A1.p.zoom == A2.p.zoom && A1.p.qstyle == "Accurate");
}

// ---- 0.9.3: the Atom Lab benchmark scenes start from the lab's defaults, whatever the user had set; the input lock; the result lines ----
static bool sameParams(const atom::AtomParams& a, const atom::AtomParams& b) {
    return a.scale == b.scale && a.zoom == b.zoom && a.qstyle == b.qstyle && a.detail == b.detail && a.style == b.style && a.Z == b.Z && a.N == b.N
        && a.view == b.view && a.ostyle == b.ostyle && a.orb == b.orb && a.pAx == b.pAx && a.dAx == b.dAx && a.fAx == b.fAx && a.cutAxis == b.cutAxis
        && a.cut == b.cut && a.n == b.n && a.speed == b.speed;
}
static void oddAtomLab(atom::AtomLab& L) {                    // deliberately odd values in every setting and in the state the drawing depends on
    L.setParam("Z", 8.0); L.setParam("view", std::string("Slice")); L.setParam("ostyle", std::string("Bohr 1913")); L.setParam("scale", std::string("Fixed"));
    L.setParam("qstyle", std::string("Classic")); L.setParam("detail", std::string("Valence")); L.setParam("style", std::string("Hard balls"));
    L.setParam("orb", std::string("3d")); L.setParam("pAx", std::string("x")); L.setParam("dAx", std::string("xy")); L.setParam("fAx", std::string("xyz"));
    L.setParam("cutAxis", std::string("Horizontal")); L.setParam("cut", 0.6); L.setParam("n", 3.0); L.setParam("speed", 20.0); L.setParam("zoom", 5.0);
    L.moreStored = true; L.paused = true; L.hold = true; L.s.focus = 3; L.s.layer = "Quarks"; L.s.spin = true;
    L.chain.push_back(atom::ChainStep{"x", "", 0, std::nullopt});
}
static void testBenchAtomScenes() {
    for (int which = 0; which < 4; which++) {
        const std::string w = "atom scene " + std::to_string(which + 1) + ": ";
        atom::AtomLab odd, clean;
        oddAtomLab(odd);
        atom::benchSetup(odd, which, 800, 600);
        atom::benchSetup(clean, which, 800, 600);            // a lab with the defaults (the reference)
        checkTrue(w + "same settings as from a default lab", sameParams(odd.p, clean.p));
        // what the scene itself asks for, written out
        const bool cloud = which == 1, big = which == 3;
        checkTrue(w + "Orbit style is Modern (the Bohr picture draws one small ball, no nucleons)", odd.p.ostyle == "Modern");
        checkTrue(w + "default scale, orientations and cut", odd.p.scale == "Auto" && odd.p.pAx == "z" && odd.p.dAx == "z2" && odd.p.fAx == "z3" && odd.p.cutAxis == "Vertical" && odd.p.cut == 0 && odd.p.n == 1 && odd.p.speed == 5);
        checkTrue(w + "the nucleus asked for", odd.p.Z == (big ? 118 : 92) && odd.p.N == (big ? 176 : 146));
        checkTrue(w + "view", odd.p.view == (cloud ? "Cloud 3D" : "Orbit") && odd.p.orb == (cloud ? "4f" : "1s"));
        if (!cloud) {
            checkTrue(w + "quark style, detail and nucleons", odd.p.qstyle == (which == 0 ? "Cinematic" : "Accurate") && odd.p.detail == "+ Sea" && odd.p.style == "Fuzzy");
            checkTrue(w + "the nucleons are there to draw (the whole nucleus)", int(odd.s.nuc.size()) == odd.p.Z + odd.p.N);
        }
        checkTrue(w + "state the drawing reads is fresh", !odd.moreStored && !odd.paused && !odd.hold && odd.chain.empty() && odd.s.layer == "Atom" && !odd.s.spin);
        checkTrue(w + "focus: only the one-nucleon scene follows a nucleon", odd.s.focus == (which == 2 ? 0 : -1));
        const std::string txt = atom::benchSettingsText(odd);
        checkTrue(w + "settings text names view, Orbit style and quark style", txt.find("Orbit style Modern") != std::string::npos && txt.find(cloud ? "view Cloud 3D" : "view Orbit") != std::string::npos && txt.find("quark style " + odd.p.qstyle) != std::string::npos);
    }
}
static void testBenchInputLock() {
    checkTrue("input is allowed when no benchmark runs", labInputAllowed(false));
    checkTrue("input to the labs and app is blocked while the benchmark runs", !labInputAllowed(true));
    BenchEnv env; env.version = "0.9.3"; env.atomSettings = "scene 2: view Orbit, Orbit style Modern, quark style Cinematic";
    const std::string t = benchResultText(env, {});
    checkTrue("result says input was blocked", t.find("Input during the test: blocked\n") != std::string::npos);
    checkTrue("result lists the Atom Lab settings used", t.find("Atom Lab settings used: scene 2: view Orbit, Orbit style Modern, quark style Cinematic\n") != std::string::npos);
    checkTrue("no Atom Lab line when no Atom Lab scene ran", benchResultText(BenchEnv(), {}).find("Atom Lab settings used") == std::string::npos);
}

// ---- 0.9.4: the glowing, denser electron cloud. Only how the dots are drawn and how many are kept change; the sampling does not. ----
static void fillCloud(atom::AtomLab& L, const char* orb, const char* density, uint32_t seed, int Z = 1) {
    atom::seedRandom(seed);
    L.p = atom::AtomParams(); L.paused = false;
    L.p.Z = Z; L.p.N = atom::defaultN(Z); L.p.view = "Cloud 3D"; L.p.orb = orb; L.p.cdens = density;
    L.restart();
    L.s.spin = false;
    for (int i = 0; i < 100000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
}
// mean and standard error of the mean of f over the dots, from batch means (the dots follow each other along one random walk, so
// neighbours are not independent: the spread of the means of whole batches of 1,000 dots is the honest error)
template <class F> static void batchMean(const atom::AtomLab& L, F f, double& mean, double& se) {
    const long n = L.cloudCap(), B = 1000, nb = n / B;
    std::vector<double> m(nb, 0.0);
    for (long b = 0; b < nb; b++) { for (long i = b * B; i < (b + 1) * B; i++) m[b] += f(i); m[b] /= B; }
    mean = 0; for (double v : m) mean += v / nb;
    double var = 0; for (double v : m) var += (v - mean) * (v - mean) / (nb - 1);
    se = std::sqrt(var / nb);
}
static void testCloud() {
    using atom::kA0;
    // the settings: values, defaults, bad values
    atom::AtomParams web, app = atom::AtomParams::appDefaults();
    checkTrue("the struct keeps the web's cloud (Classic, Normal): the step-by-step tests use it", web.cstyle == "Classic" && web.cdens == "Normal");
    checkTrue("the app starts and Defaults give Glow and High", app.cstyle == "Glow" && app.cdens == "High");
    checkTrue("only the cloud settings differ between the two", [&] { atom::AtomParams a = app; a.cstyle = web.cstyle; a.cdens = web.cdens;
        return a.scale == web.scale && a.zoom == web.zoom && a.qstyle == web.qstyle && a.view == web.view && a.orb == web.orb && a.Z == web.Z && a.ostyle == web.ostyle; }());
    checkTrue("dots kept: 12,000 / 24,000 / 48,000", atom::AtomLab::capFor("Normal") == 12000 && atom::AtomLab::capFor("High") == 24000 && atom::AtomLab::capFor("Ultra") == 48000 && atom::kCap == 12000);
    checkTrue("good cloud settings are accepted", atom::AtomLab::validChoice("cstyle", "Classic") && atom::AtomLab::validChoice("cstyle", "Glow")
        && atom::AtomLab::validChoice("cdens", "Normal") && atom::AtomLab::validChoice("cdens", "High") && atom::AtomLab::validChoice("cdens", "Ultra"));
    checkTrue("bad cloud settings are refused (the loader keeps the default)", !atom::AtomLab::validChoice("cstyle", "Fancy") && !atom::AtomLab::validChoice("cstyle", "glow")
        && !atom::AtomLab::validChoice("cdens", "Huge") && !atom::AtomLab::validChoice("cdens", "") && !atom::AtomLab::validChoice("cdens", "24000"));
    Prefs p; p.atom = {{"cstyle", "Classic"}, {"cdens", "Ultra"}, {"view", "Slice"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("cloud settings go through the settings file unchanged", q.atom == p.atom && q.atom.at("cstyle") == "Classic" && q.atom.at("cdens") == "Ultra");
    // the sampling is unchanged: with the same random numbers, the first 12,000 dots of a High / Ultra cloud are exactly the Normal cloud's dots
    {
        atom::AtomLab N, H, U;
        fillCloud(N, "3d", "Normal", 777); fillCloud(H, "3d", "High", 777); fillCloud(U, "3d", "Ultra", 777);
        bool same = N.s.k == 12000 && H.s.k == 24000 && U.s.k == 48000;
        for (long i = 0; same && i < 12000 * 3; i++) same = N.s.pts[i] == H.s.pts[i] && N.s.pts[i] == U.s.pts[i];
        for (long i = 0; same && i < 12000; i++) same = N.s.sg[i] == H.s.sg[i] && N.s.sg[i] == U.s.sg[i];
        checkTrue("the first 12,000 dots (positions and signs) are the same at Normal, High and Ultra", same);
        checkTrue("the arrays are the size of the density", N.s.pts.size() == 36000 && H.s.pts.size() == 72000 && U.s.pts.size() == 144000 && U.s.sg.size() == 48000);
        // a density change refills the cloud (and never writes outside the arrays); more steps keep the ring, not grow it
        U.setParam("cdens", std::string("Normal"));
        checkTrue("changing the density starts a fresh cloud of the new size", U.s.k == 0 && U.s.pts.size() == 36000 && U.s.sg.size() == 12000);
        for (int i = 0; i < 2000; i++) U.step(1.0 / 240);
        checkTrue("the ring keeps 12,000 dots after many more steps", U.s.k > 12000 && U.s.pts.size() == 36000);
        U.setParam("cdens", std::string("Ultra"));
        for (int i = 0; i < 100000 && U.s.k < 48000; i++) U.step(1.0 / 240);
        checkTrue("and refills to 48,000 after going back up", U.s.k == 48000);
    }
    // Independent references (not our own earlier output): for a hydrogen-like atom the expectation value of the radius is
    // <r> = (a0 / 2Z) [3 n^2 - l (l+1)]  (e.g. D. J. Griffiths, Introduction to Quantum Mechanics, 3rd ed., problem 6.? / Bethe & Salpeter, Quantum
    // Mechanics of One- and Two-Electron Atoms, section 3), and the 2s radial function is R20 = (2 a0)^(-3/2) (2 - r/a0) exp(-r/2a0)
    // (Griffiths eq. 4.? ; the node is at r = 2 a0 / Z), so P(r) = r^2 R20^2 = r^2 (2 - r)^2 exp(-r) / 8 in units of a0 for Z = 1.
    // The chapter / equation numbers are NOT checked here (no access to the books in this session); the formulas themselves are standard.
    // the mean radius of the dots is the exact hydrogen-like value <r> = (3 n² − l (l+1)) / 2 · a0 / Z, to within 4 standard errors
    for (const char* orb : {"1s", "2s", "2p", "3d", "4f"}) {
        atom::AtomLab L;
        fillCloud(L, orb, "Ultra", 12345);
        const atom::Orbital& o = atom::orbitals()[atom::orbIndex(orb)];
        const double exact = (3.0 * o.n * o.n - o.l * (o.l + 1)) / 2;
        double mean, se;
        batchMean(L, [&](long i) { return std::hypot(double(L.s.pts[3 * i]), double(L.s.pts[3 * i + 1]), double(L.s.pts[3 * i + 2])) / kA0; }, mean, se);
        std::printf("  %s at Ultra (48,000 dots): mean radius %.4f a0, exact %.4f, standard error %.4f, off by %.2f standard errors\n", orb, mean, exact, se, std::fabs(mean - exact) / se);
        checkTrue(std::string(orb) + ": mean radius within 4 standard errors of the exact value", std::fabs(mean - exact) < 4 * se);
    }
    // the node of 2s (r = 2 a0): almost no dots in a thin shell around it, many in a shell of the same width at the density maximum
    {
        atom::AtomLab L;
        fillCloud(L, "2s", "Ultra", 4242);
        const auto P = [](double r) { return r * r * (2 - r) * (2 - r) * std::exp(-r) / 8; };        // exact radial probability density of 2s (Z = 1), integrates to 1
        double rmax = 0; for (double r = 0.01; r < 20; r += 0.001) if (P(r) > P(rmax > 0 ? rmax : 0.01)) rmax = r;
        const double half = 0.15;                                                                       // shells of width 0.3 a0
        const auto exactFrac = [&](double c) { double s = 0; for (double r = c - half; r < c + half; r += 1e-4) s += P(r + 5e-5) * 1e-4; return s; };
        const auto inShell = [&](double c) { return [&, c](long i) { const double r = std::hypot(double(L.s.pts[3 * i]), double(L.s.pts[3 * i + 1]), double(L.s.pts[3 * i + 2])) / kA0; return std::fabs(r - c) < half ? 1.0 : 0.0; }; };
        double fn, sn, fp, sp;
        batchMean(L, inShell(2.0), fn, sn); batchMean(L, inShell(rmax), fp, sp);
        std::printf("  2s: density maximum at r = %.3f a0. Dots in the shell round the node (r = 2): %.5f (exact %.5f); in the same-width shell at the maximum: %.4f (exact %.4f)\n", rmax, fn, exactFrac(2.0), fp, exactFrac(rmax));
        checkTrue("2s: node shell holds under a tenth of the fraction at the density maximum", fn < 0.1 * fp);
        checkTrue("2s: fraction at the maximum within 4 standard errors of the exact one", std::fabs(fp - exactFrac(rmax)) < 4 * sp);
        checkTrue("2s: fraction at the node within 4 standard errors (or 0.0005) of the exact one", std::fabs(fn - exactFrac(2.0)) < std::max(4 * sn, 0.0005));
    }
    // the benchmark: scene 3 is exactly the old cloud (Classic, Normal, 12,000 dots, the same dots), the new stress scene is Glow / Ultra
    {
        atom::AtomLab B3, B7, ref;
        atom::benchSetup(B3, 1, 800, 600); atom::benchSetup(B7, 4, 800, 600);
        checkTrue("benchmark scene 3: Classic, Normal, Cloud 3D 4f, cloud full", B3.p.cstyle == "Classic" && B3.p.cdens == "Normal" && B3.p.view == "Cloud 3D" && B3.p.orb == "4f" && B3.s.k >= 12000);
        checkTrue("benchmark scene 7: Glow, Ultra, Cloud 3D 4f, cloud full", B7.p.cstyle == "Glow" && B7.p.cdens == "Ultra" && B7.p.view == "Cloud 3D" && B7.p.orb == "4f" && B7.s.k >= 48000 && B7.s.sg.size() == 48000);
        atom::seedRandom(20260101);                                   // the older scene 3, written out: web settings, uranium-238, 4f, filled by stepping
        ref.p = atom::AtomParams(); ref.viewW = 800; ref.viewH = 600; ref.setParam("Z", 92.0); ref.setParam("N", 146.0); ref.restart(); ref.s.nucSeed = 1; ref.s.spin = false;
        ref.setParam("view", std::string("Cloud 3D")); ref.setParam("orb", std::string("4f")); ref.setParam("zoom", 0.0); ref.resetView();
        for (int i = 0; i < 40000 && ref.s.k < atom::kCap; i++) ref.step(1.0 / 240);
        bool same = B3.s.k == ref.s.k;
        for (long i = 0; same && i < 12000 * 3; i++) same = B3.s.pts[i] == ref.s.pts[i];
        checkTrue("benchmark scene 3 draws exactly the dots of the older versions", same);
    }
    // the total light of the cloud is the same at every density (a dot's light scales with 12,000 / dots kept)
    {
        const double base = 1.0, expo = 0.7;
        double total[3]; int i = 0;
        for (long cap : {12000L, 24000L, 48000L}) total[i++] = cap * atom::cloudDotLight(cap, expo, base);
        checkTrue("total light: Normal = High = Ultra", std::fabs(total[0] - total[1]) < 1e-9 * total[0] && std::fabs(total[0] - total[2]) < 1e-9 * total[0]);
        checkTrue("one dot at High / Ultra gives half / a quarter of a Normal dot", near(atom::cloudDotLight(24000, expo, base) * 2, atom::cloudDotLight(12000, expo, base)) && near(atom::cloudDotLight(48000, expo, base) * 4, atom::cloudDotLight(12000, expo, base)));
        checkTrue("exposure: never above 1, never brighter than needed, falls as the picture gets brighter",
                  atom::cloudExposure(0.1, 1.0, 1.0) == 1.0 && near(atom::cloudExposure(4.0, 1.0, 1.0), 0.25) && atom::cloudExposure(8.0, 1.0, 1.0) < atom::cloudExposure(4.0, 1.0, 1.0));
        // the same peak (as if 12,000 dots) gives the same peak light at every density: peak × exposure-limited light is below the target
        for (long cap : {12000L, 24000L, 48000L}) {
            const double peakAtCap = 6.0 * (double(cap) / 12000);                    // the summed footprint of a place grows with the number of dots
            const double normalised = peakAtCap * atom::cloudDotWeight(cap);
            checkTrue("peak light after exposure is at the target at " + std::to_string(cap) + " dots", near(normalised * 1.0 * atom::cloudExposure(normalised, 1.0, 1.0), 1.0));
        }
    }
}

// ---- 0.9.5: the published masses under the far-away stars' names ----
static void testFarMasses() {
    using nativeorbit::kFar;
    const int nFar = int(sizeof(kFar) / sizeof(kFar[0]));
    int withMass = 0;
    std::vector<std::string> keys;
    for (int i = 0; i < orbit::kFarMassCount; i++) {
        const orbit::MassData& m = orbit::kFarMass[i];
        const std::string w = std::string("mass of ") + m.key + (*m.part ? std::string(" ") + m.part : std::string()) + ": ";
        bool known = false;
        for (int k = 0; k < nFar; k++) if (std::string(kFar[k].key) == m.key) known = true;
        checkTrue(w + "belongs to a far-away thing in the table", known);
        checkTrue(w + "has a named source", m.source && std::strlen(m.source) > 12);                  // e.g. "Bond et al. 2017, arXiv:1703.10625"
        checkTrue(w + "says what kind of number it is", !std::strcmp(m.how, "orbit") || !std::strcmp(m.how, "lensing") || !std::strcmp(m.how, "models"));
        checkTrue(w + "value inside its stated range (low <= value <= high), range not empty", m.low <= m.value && m.value <= m.high && m.low < m.high && m.low > 0);
        if (std::find(keys.begin(), keys.end(), m.key) == keys.end()) keys.push_back(m.key);
    }
    withMass = int(keys.size());
    // a star shows a mass only if it has a sourced entry; the stars left out (no source reached in the session that wrote them) show none
    for (int k = 0; k < nFar; k++) {
        const bool listed = std::find(keys.begin(), keys.end(), std::string(kFar[k].key)) != keys.end();
        checkTrue(std::string("far ") + kFar[k].key + ": a mass text exactly when it has a sourced entry", listed == !orbit::farMassText(kFar[k].key).empty());
    }
    for (const char* left : {"barn", "lal", "eri", "tau", "altair", "vega", "cyg61", "pol", "bet", "rig", "pleia", "m42"})
        checkTrue(std::string("no mass shown for ") + left + " (not sourced)", orbit::farMassText(left).empty());
    checkTrue("7 far-away things show a mass", withMass == 7);
    // the text
    checkStr("Sirius", orbit::farMassText("sirius"), "Mass: A 2.06 · B 1.02 M☉");
    checkStr("Procyon", orbit::farMassText("procyon"), "Mass: A 1.48 · B 0.592 M☉");
    checkStr("Alpha Centauri", orbit::farMassText("acen"), "Mass: A 1.13 · B 0.97 M☉");
    checkStr("Proxima is shown with its wide range and as uncertain", orbit::farMassText("prox"), "Mass: 0.15 (0.099 to 0.21) M☉, uncertain");
    checkStr("model-based masses say so", orbit::farMassText("fom"), "Mass: about 1.92 M☉ (from models)");
    checkStr("Arcturus", orbit::farMassText("arct"), "Mass: about 1.08 M☉ (from models)");
    checkStr("Sagittarius A*", orbit::farMassText("sgra"), "Mass: 4.3 million M☉");
    // the existing note on Sagittarius A* ("about 4.3 million Suns") agrees with the sourced value
    for (int k = 0; k < nFar; k++) if (std::string(kFar[k].key) == "sgra")
        checkTrue("Sagittarius A* note and sourced mass agree (4.3 million)", std::string(kFar[k].note).find("about 4.3 million Suns") != std::string::npos && orbit::kFarMass[orbit::kFarMassCount - 1].value > 4.25e6 && orbit::kFarMass[orbit::kFarMassCount - 1].value < 4.35e6);
}

// ---- 0.9.6: the quarks on light themes: darker colours, ordinary blending, no dark backdrop. A contrast check that can be computed (it does not prove the look). ----
static double relLuminance(double r, double g, double b) {          // WCAG 2.x relative luminance of an sRGB colour (0-255)
    const auto lin = [](double c) { c /= 255; return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
}
static void testQuarkLight() {
    int lightThemes = 0;
    for (const char* id : {"light", "dark", "black", "midnight", "solar", "paper", "sage", "contrast", "synthwave", "monochrome"}) {
        const ThemeColors t = themeColors(id, false);
        if (!t.light) continue;
        lightThemes++;
        for (const Rgb& bg : {t.bg, t.card}) {
            const double lb = relLuminance(bg.r * 255, bg.g * 255, bg.b * 255);
            for (int c = 0; c < 3; c++) {
                double d[3];
                atom::lightQuarkColour(atom::kColourCharge[c], d);
                const double ratio = (lb + 0.05) / (relLuminance(d[0], d[1], d[2]) + 0.05);
                const double before = (lb + 0.05) / (relLuminance(atom::kColourCharge[c][0], atom::kColourCharge[c][1], atom::kColourCharge[c][2]) + 0.05);
                checkTrue(std::string(id) + ": quark colour " + std::to_string(c) + " is darker than the page and at least 3:1 (WCAG graphics) against it", relLuminance(d[0], d[1], d[2]) < lb && ratio >= 3.0 && ratio > before);
            }
        }
    }
    checkTrue("the themes with light pages were checked", lightThemes >= 2);
    // the three colour charges stay clearly different from each other after darkening (distance in RGB, 0-255 each)
    double d[3][3];
    for (int c = 0; c < 3; c++) atom::lightQuarkColour(atom::kColourCharge[c], d[c]);
    for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++)
        checkTrue("colour charges " + std::to_string(a) + " and " + std::to_string(b) + " stay apart (RGB distance over 80)", std::sqrt(std::pow(d[a][0] - d[b][0], 2) + std::pow(d[a][1] - d[b][1], 2) + std::pow(d[a][2] - d[b][2], 2)) > 80);
}

int main() {
    testRoundTrip();
    testBadLines();
    testThemes();
    testProgress();
    testPages();
    testFactsAndTabs();
    testShot();
    testFps();
    testHistoryNotes();
    testWindow();
    testBench();
    testBenchAtomScenes();
    testBenchInputLock();
    testCloud();
    testFarMasses();
    testQuarkLight();
    testLiveAndGpu();
    testNativeEdits();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
