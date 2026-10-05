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
#include "app/grid.h"
#include "app/frame_time.h"
#include "app/gpu_timer.h"
#include "orbit/belts.h"
#include "orbit/far_masses.h"
#include "orbit/today.h"
#include "orbit/nbody.h"
#include "atom/atom_bench.h"
#include "atom/cloud_light.h"
#include "atom/outline.h"
#include <chrono>
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

// ---- 0.10.1: the name a theme card shows ("???" only while a secret theme is still locked) ----
static void testThemeShownName() {
    const std::set<std::string> none, one = {"synthwave"}, both = {"synthwave", "monochrome"};
    const ThemeDef &dark = kThemes[2], &synth = kThemes[9], &mono = kThemes[10];
    checkStr("shown name: a normal theme shows its name (nothing unlocked)", themeShownName(dark, none), "Dark");
    checkStr("shown name: a locked secret theme is ???", themeShownName(synth, none), "???");
    checkStr("shown name: the other locked secret theme is ???", themeShownName(mono, none), "???");
    checkStr("shown name: an unlocked secret theme shows its real name", themeShownName(synth, one), "Synthwave");
    checkStr("shown name: the one still locked stays ??? when only the other is unlocked", themeShownName(mono, one), "???");
    checkStr("shown name: both unlocked, Monochrome shows its name", themeShownName(mono, both), "Monochrome");
    bool ok = true;
    for (const ThemeDef& t : kThemes) {
        ok = ok && (t.secret || themeShownName(t, none) == t.name);                 // every normal theme always shows its name
        ok = ok && themeShownName(t, both) == t.name;                                // with both unlocked every theme shows its name
        ok = ok && (!t.secret || themeShownName(t, none) == "???");                 // every secret theme is ??? while locked
    }
    checkTrue("shown name: the rule holds for every theme in the list", ok);
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
     "(27,000 light years)</li><li><a href=\"https://arxiv.org/abs/2112.07478\" target=\"_blank\" rel=\"noopener noreferrer\">GRAVITY Collaboration, 2022: mass distribution in the Galactic Centre</a> (the source of the Sagittarius A* mass shown in the Orbit Lab: 4.3 million Suns)</li><li><a href=\"https://arxiv.org/abs/1601.01636\" target=\"_blank\" rel=\"noopener noreferrer\">Pourbaix &amp; Boffin, 2016: Parallax and masses of alpha Centauri revisited</a> (the masses of Alpha Centauri A and B)</li><li><a href=\"https://arxiv.org/abs/1807.01318\" target=\"_blank\" rel=\"noopener noreferrer\">Zurlo et al., 2018: The gravitational mass of Proxima Centauri measured with SPHERE from a microlensing event</a> (the mass of Proxima Centauri)</li><li><a href=\"https://arxiv.org/abs/1703.10625\" target=\"_blank\" rel=\"noopener noreferrer\">Bond et al., 2017: The Sirius System and its Astrophysical Puzzles: Hubble Space Telescope and Ground-Based Astrometry</a> (the masses of Sirius A and B)</li><li><a href=\"https://arxiv.org/abs/1510.00485\" target=\"_blank\" rel=\"noopener noreferrer\">Bond et al., 2015: Hubble Space Telescope Astrometry of the Procyon System</a> (the masses of Procyon A and B)</li><li><a href=\"https://arxiv.org/abs/1206.6353\" target=\"_blank\" rel=\"noopener noreferrer\">Mamajek, 2012: On the Age and Binarity of Fomalhaut and its Giant Planet (ApJ 754, L20)</a> (the mass of Fomalhaut, worked out with stellar models)</li><li><a href=\"https://arxiv.org/abs/1109.4425\" target=\"_blank\" rel=\"noopener noreferrer\">Ram&iacute;rez &amp; Allende Prieto, 2011: Fundamental Parameters and Chemical Composition of Arcturus (ApJ 743, 135)</a> (the mass of Arcturus, worked out with stellar models)</li></ul>"},
    // 0.9.9 stage 2: the other sources named in the code (the star masses in far_masses.h; the textbooks named in the cloud and outline tests)
    {"About sources: a note on the star masses", "<div class=\"lrnP lrnMuted\">Distances of Polaris, Betelgeuse and Rigel are uncertain, so they are rounded.</div>",
     "<div class=\"lrnP lrnMuted\">Distances of Polaris, Betelgeuse and Rigel are uncertain, so they are rounded.</div><div class=\"lrnP lrnMuted\">The star masses are published estimates, in solar masses, and some are uncertain; the Fomalhaut and Arcturus masses come from stellar models, not from measuring an orbit. The numbers were taken from the papers&rsquo; summaries and have not been checked against their tables. Stars whose mass could not be sourced show none.</div>"},
    // 0.10: the sources of "Planet physics: Real"
    {"About sources: the Real planet physics (GM values, the integrator, the Mercury check)", "(the moons: sizes and orbits)</li>", "(the moons: sizes and orbits)</li><li><a href=\"https://ssd.jpl.nasa.gov/astro_par.html\" target=\"_blank\" rel=\"noopener noreferrer\">NASA/JPL: astrodynamic parameters</a> (GM of the Sun and of each planet with its moons, DE440 values, for &ldquo;Planet physics: Real&rdquo;; first read from an excerpt of the page, then read from the page itself and matched the code, 2026-10-03)</li><li>Park et al., 2021, The Astronomical Journal: the DE440 and DE441 planetary and lunar ephemerides (the paper behind those DE440 values)</li><li><a href=\"https://doi.org/10.1086/115978\" target=\"_blank\" rel=\"noopener noreferrer\">Wisdom &amp; Holman, 1991: Symplectic maps for the N-body problem</a> (the method that moves the planets in &ldquo;Planet physics: Real&rdquo;)</li><li>Duncan, Levison &amp; Lee, 1998: A multiple time step symplectic algorithm for integrating close encounters (The Astronomical Journal; the variables that method is written in)</li><li>Bate, Mueller &amp; White, 1971: <i>Fundamentals of Astrodynamics</i> (moving a planet along its Kepler orbit with universal variables)</li><li>Clemence, 1947: The relativity effect in planetary motions (Reviews of Modern Physics; the 532 arcseconds per century of Mercury&rsquo;s perihelion advance from the other planets, used to check &ldquo;Planet physics: Real&rdquo;)</li>"},
        // 0.10.1: the sources of "General relativity (1PN)"
    {"About sources: general relativity for the planets (REBOUNDx potential, Nobili & Roxburgh, the published Mercury value)", "used to check &ldquo;Planet physics: Real&rdquo;)</li>", "used to check &ldquo;Planet physics: Real&rdquo;)</li><li><a href=\"https://github.com/dtamayo/reboundx/blob/main/src/gr_potential.c\" target=\"_blank\" rel=\"noopener noreferrer\">REBOUNDx, the file gr_potential.c</a> (Tamayo, Rein, Shi &amp; Hernandez, 2020, Monthly Notices of the Royal Astronomical Society 491, 2885: the simple potential of the Sun behind &ldquo;General relativity (1PN)&rdquo; for the planets; the formula was read in that source file)</li><li>Nobili &amp; Roxburgh, 1986: Simulation of general relativistic corrections in long term numerical integrations of planetary orbits (IAU Symposium 114; the idea of that potential; the paper itself was not read)</li><li><a href=\"https://arxiv.org/abs/1403.7377\" target=\"_blank\" rel=\"noopener noreferrer\">Will, 2014: The confrontation between general relativity and experiment</a> (Living Reviews in Relativity 17; Mercury&rsquo;s relativistic perihelion advance of 42.98 arcseconds per century, used to check &ldquo;General relativity (1PN)&rdquo;; read from a search excerpt, not from the paper)</li>"},
    // 0.10.3: the Horizons sources (the start of "Planet physics: Real"; the astrodynamic-parameters entry above now says its values were read from the page itself)
    {"About sources: JPL Horizons API, DE440 and DE441, Horizons data retrieved 2026-10-03", "Park et al., 2021, The Astronomical Journal: the DE440 and DE441 planetary and lunar ephemerides (the paper behind those DE440 values)</li>", "Park et al., 2021, The Astronomical Journal: the DE440 and DE441 planetary and lunar ephemerides (the paper behind those DE440 values)</li><li><a href=\"https://ssd-api.jpl.nasa.gov/doc/horizons.html\" target=\"_blank\" rel=\"noopener noreferrer\">NASA/JPL: Horizons API documentation</a> (how the Horizons vector tables used for the start of &ldquo;Planet physics: Real&rdquo; are asked for)</li><li><a href=\"https://ssd.jpl.nasa.gov/doc/de440_de441.html\" target=\"_blank\" rel=\"noopener noreferrer\">NASA/JPL: DE440 and DE441</a> (the note on the two ephemerides: Horizons&rsquo; default is DE441, which spans &minus;13200 to +17191 and is a little less accurate than DE440 for the present century, mainly for the Moon; the page was not opened in the session that wrote this, the figures are as the owner read them; the Real note&rsquo;s errors are against DE441, not DE440)</li><li>NASA/JPL Horizons data retrieved 2026-10-03 (Horizons API, ephemeris DE441: position and velocity of the Sun and of each planet system, 1 January 12:00 TDB of every year from 1800 to 2100; the start of &ldquo;Planet physics: Real&rdquo; is its 1 January 2026 row, the other rows check the motion)</li>"},
    // 0.10.4 stage 1: the sources of the Moon (ELP2000-82B) and the Horizons Moon table
    {"About sources: ELP2000-82B and the Horizons Moon table (retrieved 2026-10-03)", "the other rows check the motion)</li>", "the other rows check the motion)</li><li><a href=\"https://cdsarc.cds.unistra.fr/ftp/VI/79/\" target=\"_blank\" rel=\"noopener noreferrer\">Chapront-Touz&eacute; and Chapront, 1988, Astronomy &amp; Astrophysics 190, 342: the lunar theory ELP2000-82B</a> (CDS catalogue VI/79: the series and the Fortran routine it is written from; the Moon&rsquo;s position, from the 487 largest of its 37,872 terms. It is a 1988 theory fitted to JPL&rsquo;s older DE200, so it is less accurate than JPL&rsquo;s own ephemerides: against Horizons (DE441) at the dates checked between 1800 and 2100 it is within 5.2 km (2.9 arcseconds) at worst. Real planet physics in the Orbit Lab draws Earth&rsquo;s Moon from it: its direction and phase are real, its distance is drawn exaggerated like every moon&rsquo;s; positions are geometric (no light time), dates are used as TDB, and outside 1800 to 1 Jan 2100 it is not used. The phase names: the four main phases are exact, the names in between (crescent, gibbous) use this app&rsquo;s own cut-offs, at 2, 40, 60 and 98 % lit)</li><li>NASA/JPL Horizons data retrieved 2026-10-03, the Moon table (Horizons API, ephemeris DE441: the position of the Moon and the Sun seen from the Earth&rsquo;s centre, every 5 years from 1800 to 2100, daily for 60 days from 2025-12-02 and hourly on 2026-01-02 to 04; used only to check the ELP2000-82B Moon and its phases)</li><li>NASA/JPL Horizons data retrieved 2026-10-03, the Earth table (Horizons API, ephemeris DE441: the position of the Earth&rsquo;s centre seen from the Sun&rsquo;s centre, 1 January 12:00 TDB of every year from 1800 to 2100; used only to check where Real planet physics puts Earth)</li>"},
    // 0.10.2: the sources of the complex orbitals and the phase colours
    {"About sources: complex orbitals and the phase wheel (DLMF 14.30, SciPy, scikit-image, CIE L*a*b*)", "read from a search excerpt, not from the paper)</li>", "read from a search excerpt, not from the paper)</li><li><a href=\"https://dlmf.nist.gov/14.30\" target=\"_blank\" rel=\"noopener noreferrer\">NIST Digital Library of Mathematical Functions, section 14.30: spherical harmonics</a> (the definition and the Condon&ndash;Shortley phase of Y<sub>lm</sub> behind the complex orbitals m = +1, &minus;1, &hellip; in the Atom Lab; the page itself was not opened from the session that wrote this)</li><li>SciPy 1.17.1 (<code>scipy.special.sph_harm_y</code>) and scikit-image 0.26.0 (<code>skimage.color.lab2rgb</code>): used only in the tests, as independent checks of those spherical harmonics and of the phase colour wheel</li><li>CIE 1976 L*a*b* colour space and the sRGB standard (IEC 61966-2-1): the phase colour wheel is a circle of constant lightness in L*a*b*, so the colour shows the phase and not a brightness pattern (the formulas are the standard ones, written from knowledge, not looked up here)</li>"},
    // 0.10.2 stage 2: the sources of the quantum beats
    {"About sources: quantum beats (Fitzpatrick, CODATA 2018)", "the formulas are the standard ones, written from knowledge, not looked up here)</li>", "the formulas are the standard ones, written from knowledge, not looked up here)</li><li>Richard Fitzpatrick, <i>Introductory Quantum Mechanics</i> (lecture notes, University of Texas at Austin), the section on the 2P&ndash;1S transitions in hydrogen: the matrix element &lang;1s|z|2p&rang; = 128&radic;2/243 a<sub>0</sub> that sets the size of the oscillating dipole in the Atom Lab&rsquo;s quantum beats (the page was not opened from the session that wrote this; the number was checked by numerical integration in the tests)</li><li>CODATA 2022 recommended values (NIST): the Planck constant, the Rydberg energy and the proton&ndash;electron mass ratio, used in the tests to check the beat period h/(E<sub>2</sub>&minus;E<sub>1</sub>) (the values were read from the NIST pages on 2026-10-04, see the next entry)</li>"},
    // 0.10.2 stage 3: the sources of the smooth photon jumps
    {"About sources: smooth photon jumps (Griffiths two-level picture, CODATA Bohr radius)", "(the values were read from the NIST pages on 2026-10-04, see the next entry)</li>", "(the values were read from the NIST pages on 2026-10-04, see the next entry)</li><li>David J. Griffiths, <i>Introduction to Quantum Mechanics</i>, the chapter on time-dependent perturbation theory (the two-level system): the textbook picture of a transition as a mixing of two states, which the Atom Lab&rsquo;s smooth photon jump follows. It is a picture, not a calculation of how a photon is absorbed (the book was not opened from the session that wrote this)</li><li>CODATA 2022 recommended values (NIST, the pages <code>physics.nist.gov/cgi-bin/cuu/Value?</code> followed by bohrrada0, hrev, aut, alph, rydhcev, mec2mev, meu, mpsme, ukg, muc2mev, me, mp, mn, mpu, mnu, k, h, c and e, read on 2026-10-04; each page says &ldquo;2022 CODATA recommended values&rdquo;): the Atom Lab&rsquo;s constants since 0.10.6 stage 2: the Bohr radius 5.29177210544&times;10<sup>&minus;11</sup> m, the Hartree energy 27.211386245981 eV, the atomic unit of time 2.4188843265864&times;10<sup>&minus;17</sup> s (so the atomic unit of speed), the fine-structure constant, the Rydberg energy 13.605693122990 eV, the electron mass energy, the electron, proton and neutron masses (in kg and in u), the atomic mass constant (in kg and in MeV), the proton&ndash;electron mass ratio, the Boltzmann constant, and the exact h, c and e (so h&middot;c = 1239.841984 eV&middot;nm). Before that the lab used the web version&rsquo;s rounded 0.529 &Aring;, 27.211 eV and 24.19 as (the Bohr radius was 0.03 % too small). Not from NIST and not re-read: the mass of the hydrogen-1 atom, 1.00782503223 u (the atomic mass evaluation behind the isotope data), and the proton radius, kept at the web&rsquo;s rounded 0.84 fm (CODATA 2022 gives 0.84075 fm). The tests compare the constants with these values typed in again from the pages, and check that the atomic unit of speed equals the fine-structure constant times c</li>"},
    {"About sources: the visible range and the Dirac 1s energy (0.10.6 stage 5)", "check that the atomic unit of speed equals the fine-structure constant times c</li>", "check that the atomic unit of speed equals the fine-structure constant times c</li><li>Visible light, 380&ndash;780 nm: a convention this app uses (it is the range of the web version&rsquo;s colour code); no source for the range was read. The Atom Lab uses this one range for the spectrum card, the colour of every line and the (UV) and (IR) tags (the spectrum card used 380&ndash;750 nm before 0.10.6 stage 5).</li><li>The Dirac 1s energy of a one-electron atom with a point nucleus, mc&sup2;(1 &minus; &radic;(1 &minus; (Z&alpha;)&sup2;)): used in the heavy-atom readout, and to choose the line (Z = 27, where it is about 1 % above the non-relativistic energy) from which the Atom Lab warns that its non-relativistic model is off. The formula is the standard one, written from knowledge, not looked up here; the percentages are worked out from it by the lab and the tests.</li>"},
    {"About sources: AME2020 masses and NIST ASD lines (0.10.6 stage 6)", "the percentages are worked out from it by the lab and the tests.</li>", "the percentages are worked out from it by the lab and the tests.</li><li>AME2020, the atomic mass evaluation (Huang, Wang, Kondev, Audi &amp; Naimi, 2021, Chinese Physics C 45, 030002; the table <code>www-nds.iaea.org/amdc/ame2020/mass.mas20.txt</code>, read on 2026-10-04): the binding energies the Atom Lab already used (3,557 nuclides) agree with its atomic masses, and since 0.10.6 stage 6 they give the nucleus&rsquo;s mass for the spectrum lines (its protons and neutrons, from CODATA 2022, minus its binding energy; before that A times the atomic mass unit, which was 0.70 % too light for deuterium and put its D-alpha line 1.3 pm too long). 695 of the nuclides on the neutron slider lie beyond the table and keep the old rough mass. The tests compare the masses with AME2020&rsquo;s atomic masses for H-1, H-2, H-3, He-4, C-12, Fe-56 and U-238.</li><li>NIST Atomic Spectra Database, lines form (<code>physics.nist.gov/cgi-bin/ASD/lines1.pl</code>, read on 2026-10-04: the vacuum Ritz wavelengths of the fine-structure components of H I and He II): used only in the tests, to check that the lab&rsquo;s level-averaged, non-relativistic lines (Lyman-alpha, H-alpha, H-beta and two He II lines) lie within those components widened by (Z&alpha;)&sup2;, the size of what the lab leaves out (fine structure, the Lamb shift, spin).</li>"},
    // 0.10.7 stage 2: the real spectrum from NIST ASD
    {"About sources: NIST ASD lines, the real spectrum beside the lab's (0.10.7 stage 2)", "the size of what the lab leaves out (fine structure, the Lamb shift, spin).</li>", "the size of what the lab leaves out (fine structure, the Lamb shift, spin).</li><li>NIST Atomic Spectra Database (ASD), lines form (<code>physics.nist.gov/cgi-bin/ASD/lines1.pl</code>, retrieved on 2026-10-04, vacuum wavelengths, version 5.12 as listed on ASD&rsquo;s version history page): the real measured spectrum beside the lab&rsquo;s, in the Atom Lab&rsquo;s &ldquo;Real spectrum (NIST)&rdquo; card, as two separate comparisons. (a) The hydrogen-like ion of the element (H I, He II, &hellip; Fe XXVI: one electron round a nucleus of charge Z, the ion the lab models): the lines between levels n &le; 6, with the fine-structure components, to check the lab; they are observed wavelengths where ASD has them and otherwise Ritz wavelengths worked out from measured energy levels, each with the uncertainty ASD gives. (b) The neutral atom of the element (H I, He I, &hellip; Fe I): the 30 strongest observed lines in 100&ndash;2500 nm that ASD gives an intensity for (ASD says its relative intensities are qualitative, have no common scale and mean something only within one spectrum, so the lab ranks and draws them within one spectrum only), to show the limit of the lab, which has one electron where a real atom has Z. Spectra ASD does not have (the ions of Z = 111&ndash;118 and the neutral atoms of Fm and heavier) are said to be missing on screen. Since 0.10.8 stage 3 the same ASD lines also give the lab&rsquo;s lifetimes and jump rates: each line&rsquo;s transition probability A (the Einstein A coefficient, the spontaneous-emission rate, in s<sup>&minus;1</sup>), the accuracy code ASD gives it (AAA &le; 0.3 %, AA &le; 1 %, A+ &le; 2 %, A &le; 3 %, B+ &le; 7 %, B &le; 10 %, C+ &le; 18 %, C &le; 25 %, D+ &le; 40 %, D &le; 50 %, E &gt; 50 %; a prime means a multiplet was split assuming pure LS coupling) and its transition type (blank = allowed electric dipole E1; M1, E2, M1+E2, UT = forbidden), all kept from the same retrieval (no new request) and explained by ASD&rsquo;s lines help page, read on 2026-10-04, which also states ASD&rsquo;s own rule for an allowed line (levels of opposite parity, J changing by at most 1). The lab adds only the lifetime 1/&Sigma;A and the branching shares A/&Sigma;A over the allowed jumps to a lower n, and gives no number where ASD has none (no rate is calculated). In that data ASD lists the 1s&ndash;2s line of Cs LV (Z = 55) as allowed although l does not change; the lab follows the selection rule and does not use that value. Cited as NIST asks: Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024). NIST Atomic Spectra Database (version 5.12), [Online]. Available: https://physics.nist.gov/asd. National Institute of Standards and Technology, Gaithersburg, MD. DOI: https://doi.org/10.18434/T4W30F. The Dirac energy formula the tests use to explain the heavy ions&rsquo; difference is the standard one, written from knowledge, not looked up here.</li>"},
    // 0.10.8 stage 1: the Dirac levels
    {"About sources: the Dirac levels, the Relativity setting (0.10.8 stage 1)", "The Dirac energy formula the tests use to explain the heavy ions&rsquo; difference is the standard one, written from knowledge, not looked up here.</li>", "The Dirac energy formula the tests use to explain the heavy ions&rsquo; difference is the standard one, written from knowledge, not looked up here.</li><li>The Dirac energy levels of a one-electron atom with a point nucleus (the Sommerfeld fine-structure formula, E = mc&sup2;[(1 + (Z&alpha;)&sup2;/(n &minus; &delta;)&sup2;)<sup>&minus;1/2</sup> &minus; 1], &delta; = (j + &frac12;) &minus; &radic;((j + &frac12;)&sup2; &minus; (Z&alpha;)&sup2;)): the Atom Lab&rsquo;s &ldquo;Relativity (Dirac levels)&rdquo; setting (0.10.8, off by default) uses it for the energy levels and lines of the two spectrum cards. The formula is standard textbook material (Hans A. Bethe and Edwin E. Salpeter, <i>Quantum Mechanics of One- and Two-Electron Atoms</i>, listed in this section; Griffiths gives its expansion); it was written from knowledge and not looked up in the session that wrote it, and the tests check it against hand arithmetic, its expansion and the measured NIST lines (the ASD source above: the lab&rsquo;s Dirac lines are matched to ASD&rsquo;s components by n and j, and they land closer than the non-relativistic lines for H I, He II and Fe XXVI). The constants are the CODATA 2022 ones above. The nucleus&rsquo;s motion enters only through the reduced mass in the main energy; the relativistic correction is that of an infinitely heavy nucleus, and no source for the further recoil terms was read. Not in it: the Lamb shift and other QED effects, and the size of the nucleus.</li>"},
    // 0.10.9 stage 1: the measured nuclear charge radii
    {"About sources: nuclear charge radii from the IAEA table (0.10.9 stage 1)", "Not in it: the Lamb shift and other QED effects, and the size of the nucleus.</li>", "Not in it: the Lamb shift and other QED effects, and the size of the nucleus.</li><li>Nuclear charge radii, the size every nucleus with a measurement is drawn at (0.10.9): the IAEA Nuclear Data Section&rsquo;s table of nuclear charge radii, the file <code>www-nds.iaea.org/radii/charge_radii.csv</code> (retrieved on 2026-10-04, 28,869 bytes). It has 957 rows: 908 isotopes of 90 elements, from hydrogen to curium (Z = 96), with a measured rms charge radius and its uncertainty; 48 more with only a preliminary value, which the lab does not use; and the neutron&rsquo;s row, a negative number (so a mean-square radius in fm&sup2;, not a radius), which the lab leaves out. The file carries no citation text and does not say which edition it is. The compilation behind a table like it is I. Angeli and K. P. Marinova, <i>Atomic Data and Nuclear Data Tables</i> 99 (2013) 69, &ldquo;Table of experimental nuclear ground state charge radii: An update&rdquo; (the title and its 909 isotopes of 92 elements were read from a search listing; the volume and page were not checked on the journal&rsquo;s page, and neither was which edition the IAEA file is). A charge radius is the rms radius of the protons&rsquo; electric charge. Neutrons carry no charge, so it is not the matter radius, and that is why halo nuclei are not drawn from it. The lab draws the nucleus as a uniform ball of radius R = &radic;(5/3) &times; the rms radius (a uniform ball has rms radius &radic;(3/5) R, a calculus result; that the nucleus is a uniform ball is an assumption, real nuclei have soft edges), and the nucleon positions are scaled so that the protons&rsquo; rms radius is the measured one. Where the table has no value the usual rule r = 1.2 A<sup>1/3</sup> fm is kept and the card says so. Hydrogen-1 keeps the CODATA 2022 proton radius above. That helium-4 is smaller than helium-3 and that lithium&rsquo;s radius falls to lithium-9 and rises at lithium-11 are the table&rsquo;s own numbers; the binding energies beside them are the AME2020 ones above. The relation R = &radic;(5/3) &times; rms was written from knowledge and not looked up.</li>"},
    // 0.10.9 stage 2: the measured quadrupole moments
    {"About sources: nuclear quadrupole moments from the IAEA table (0.10.9 stage 2)", "The relation R = &radic;(5/3) &times; rms was written from knowledge and not looked up.</li>", "The relation R = &radic;(5/3) &times; rms was written from knowledge and not looked up.</li><li>Nuclear electric quadrupole moments, where the nucleus&rsquo;s shape comes from (0.10.9 stage 2): the IAEA Nuclear Data Section&rsquo;s recommended values, the file <code>www-nds.iaea.org/nuclearmoments/elec_mom_recomm.csv</code> (retrieved on 2026-10-04, 135,013 bytes, 1,155 rows: 599 ground states and 556 isomers; the lab uses the ground states only). Each row names the INDC report indc-nds-0833 and an NSR reference key; the page that states how to cite the table could not be read (a bot challenge), so no further citation is given. The file&rsquo;s second column, headed n.n+n.z, is the mass number (read that way every ground-state row is a known nuclide; read as a neutron count only 53 of 599 are). Values are in barns. Seven rows that are not one clean number (five of them typing errors), 41 values with no sign, four with a tentative sign and 34 with a tentative spin are not used for the shape; a duplicate K-41 row is left out. The table gives the spectroscopic moment, which is zero for spin 0 and 1/2 whatever the shape. The lab turns it into the intrinsic moment with the strong-coupling relation (K = I), Q = Q<sub>0</sub> I(2I &minus; 1) / ((I + 1)(2I + 3)): a model assumption, not a measurement, and a poor one for light nuclei and near closed shells; it is not used where it would need |&beta;<sub>2</sub>| above 0.6 (the lab&rsquo;s own limit, no source). The shape is a uniformly charged body r = R(1 + &beta;<sub>2</sub>Y<sub>20</sub>) whose volume is kept (an assumption), and &beta;<sub>2</sub> is solved from Q<sub>0</sub> and the measured charge radius above. Where there is no usable moment the lab keeps its B(E2) values (its own table of 11 isotopes, whose source the code does not name) or its estimate (Casten&rsquo;s P-factor), and the card says which. The relation and the conversion were written from knowledge, not looked up.</li>"},
    // 0.11 stage 2: NUBASE2020 for the chart of nuclides
    {"About sources: NUBASE2020 spins and the chart of nuclides (0.11 stage 2)", "The relation and the conversion were written from knowledge, not looked up.</li>", "The relation and the conversion were written from knowledge, not looked up.</li><li>Nuclear spins, the kind of each half-life and the chart of nuclides (0.11 stage 2): NUBASE2020, F.G. Kondev, M. Wang, W.J. Huang, S. Naimi and G. Audi, Chin. Phys. C45, 030001 (2021), the file <code>www-nds.iaea.org/amdc/ame2020/nubase_4.mas20.txt</code> (retrieved on 2026-10-04, 761,906 bytes; 3,558 ground-state lines: the free neutron and the 3,557 nuclides the chart draws, the same ones the lab already had). The half-lives, decay modes and odds on the chart come from the lab&rsquo;s own table (also NUBASE2020, through the mendeleev package); the file adds the spin and parity of each nuclide, with NUBASE&rsquo;s own marks (directly measured, estimated from systematics, tentative), and tells apart a half-life that is only an extrapolation from systematics (369 nuclides, never shown as a number), a limit, an approximate value and none. The page that states how the table may be cited could not be read (a bot challenge), so the credit is only the citation the file gives. The file&rsquo;s decay column marks some entries with a question mark and does not explain the mark in its header; the app shows such entries as written. The colour of a square is a convention of this lab, not a source: stable where the lab&rsquo;s table says stable (251; NUBASE2020 says stable for 253, and the two extra, krypton-78 and xenon-124, have measured half-lives in the lab&rsquo;s table), otherwise the kind of decay with the largest total odds (a beta-delayed branch counts as its beta); where the odds are not measured, the kind if all listed modes agree, else grey. The cross-check against the file&rsquo;s own decay column agrees for 3,054 of the 3,083 nuclides where it gives usable numbers; in most of the rest the lab&rsquo;s table fills an unquantified remainder that NUBASE marks with a question mark. The magic numbers 2, 8, 20, 28, 50, 82 and 126 (filled shells in the nuclear shell model) were written from knowledge and not looked up; 126 is a neutron number only, since the chart ends at 118 protons.</li>"},
    // 0.11 stage 3: the landscape heights are a chosen scale
    {"About sources: the landscape heights are a chosen scale (0.11 stage 3)", "126 is a neutron number only, since the chart ends at 118 protons.</li>", "126 is a neutron number only, since the chart ends at 118 protons.</li><li>The heights of the chart&rsquo;s landscape (0.11 stage 3) are a chosen scale, not a measurement and not from a source: the logarithm (base 10) of the half-life in seconds, from 10<sup>&minus;23</sup> s (flat) to 10<sup>32</sup> s (the top), which are the shortest and the longest half-lives in the lab&rsquo;s table rounded outwards to whole powers of ten; one scale height is drawn 12 tiles tall, a number chosen by looking so that the proton-rich side is not hidden behind the stable ridge. Stable nuclides have no half-life and all stand at one fixed height, 1.1 scale heights, above the top; a nuclide with no measured half-life, or only NUBASE&rsquo;s extrapolation from systematics, has no height at all (a flat hollow or pale square); a limit or an approximate value is drawn as a pale block at the limit&rsquo;s height. The projection is isometric: the chart is turned 45 degrees and tipped 54.74 degrees (arccos of 1/&radic;3) away from straight down, the standard angles that make the three axes look equally long; they were written from knowledge and not looked up.</li>"},
    // 0.11 stage 4: AME2020 and the binding height; the valley and the island
    {"About sources: AME2020, the binding height, the valley and the island (0.11 stage 4)", "they were written from knowledge and not looked up.</li>", "they were written from knowledge and not looked up.</li><li>Binding energy per nucleon, the chart&rsquo;s second height (0.11 stage 4): AME2020, the file <code>www-nds.iaea.org/amdc/ame2020/mass.mas20.txt</code> (retrieved on 2026-10-04, 476,239 bytes, last modified 7 March 2021), cited as the file itself gives it: W.J. Huang, M. Wang, F.G. Kondev, G. Audi and S. Naimi, Chin. Phys. C45, 030002 (2021), and M. Wang, W.J. Huang, F.G. Kondev, G. Audi and S. Naimi, Chin. Phys. C45, 030003 (2021). The file marks a value that is only an estimate (non-experimental) with a #: 1,008 of the 3,557 nuclides, among them every nuclide above Z = 110, whose masses have not been measured. Those are drawn flat and pale, never as a height. The lab&rsquo;s own binding table agrees with the file to 1 keV for all 3,557 nuclides. The height is a chosen linear scale, 0 to 9,000 keV per nucleon (not the total, which would only grow with the mass number); the highest measured value is read from the table, not typed: nickel-62, 8,794.56 keV, then iron-58 and iron-56. The sentence that fusing nuclei below the peak releases energy and splitting nuclei above it does is standard nuclear physics, written from knowledge and not looked up.</li><li>The valley and the island of stability (0.11 stage 4): the valley of stability is the band of stable nuclides that the chart already draws, so it is measured (the lab&rsquo;s table, NUBASE2020). The island of stability is a theoretical prediction, and no source for it could be read from these sessions: every host tried (Wikipedia, arXiv, INSPIRE, Crossref, journal sites and laboratory sites) answered with a connection refusal from the session&rsquo;s proxy (HTTP 403). So the app draws no predicted region and quotes no predicted number. It says only that theory expects such a region beyond the heaviest known nuclides (Z = 118 in NUBASE2020), that none of it has been observed, that this lab has no sourced prediction to draw, and that where it lies is disputed; that wording follows the project owner&rsquo;s brief and could not be checked against a source.</li>"},
    // 0.12 stage 1: decay chains
    {"About sources: decay chains on the chart (0.12 stage 1)", "that wording follows the project owner&rsquo;s brief and could not be checked against a source.</li>", "that wording follows the project owner&rsquo;s brief and could not be checked against a source.</li><li>Decay chains on the chart (0.12 stage 1): every half-life, decay mode and branching odds are the lab&rsquo;s table (NUBASE2020 through the mendeleev package, above). The move of each mode on the chart is nuclear bookkeeping (an alpha takes away 2 protons and 2 neutrons, a beta minus turns a neutron into a proton, a beta plus or electron capture does the opposite), written from knowledge. The four classic series (uranium-238 to lead-206, uranium-235 to lead-207, thorium-232 to lead-208, neptunium-237 to thallium-205) were typed into the tests from standard textbook tables, <b>not</b> from a source read in the session that wrote them: no online source could be reached (every host answered with the session&rsquo;s proxy denial). What the tests check without any table is the mass-number rule (an alpha takes 4 off A and a beta changes only Z, so every member of a series has A = A<sub>0</sub> mod 4), the charge and mass count at the end of each series, the exponential law (the median lifetime is the half-life, the mean is the half-life divided by ln 2) and binomial statistics for the random walk. The walk uses the listed odds added up to 1. Where NUBASE2020 marks the odds as not measured the lab&rsquo;s table lists equal shares, and the chain uses those only as a marked stand-in, never as data. A nuclide whose half-life is only a limit, only an extrapolation from systematics, or not given is never given a time: the chain stops there and says so. The time on the screen is not real time: each step&rsquo;s lifetime is a random draw from the exponential law with the real half-life, and the steps are shown about 1.6 seconds apart whatever the drawn lifetimes were.</li>"},
    // 0.12 stage 2: the accuracy clean-up
    {"About sources: the accuracy clean-up of half-lives, modes and magic numbers (0.12 stage 2)", "and the steps are shown about 1.6 seconds apart whatever the drawn lifetimes were.</li>", "and the steps are shown about 1.6 seconds apart whatever the drawn lifetimes were.</li><li>Accuracy clean-up of the half-lives and decay modes (0.12 stage 2). Every half-life in the Atom Lab is now checked against NUBASE2020, the file named above: all 2,799 plain values agree with it to 0.5 % (the lab writes three digits). To find where the lab&rsquo;s older table came from, the <code>mendeleev</code> package, version 1.3.0 from PyPI (read on 2026-10-04), was opened: it agrees with the older table for 2,774 of 2,799 half-lives, the others being superheavy nuclides. What changed: (1) 113 superheavy half-lives that the older table had rounded and marked approximate are now NUBASE2020&rsquo;s own measured values (the lab can no longer say &ldquo;approximate&rdquo; about them); (2) 16 more superheavy values were an older evaluation (up to 2.8 times different, for example seaborgium-269: 14 minutes before, 5 minutes now) and are now NUBASE2020&rsquo;s; (3) krypton-78 and xenon-124 had half-lives (9.2&times;10<sup>21</sup> and 1.8&times;10<sup>22</sup> years) that are in neither NUBASE2020 nor <code>mendeleev</code>, which both call them stable (NUBASE2020 with lower limits &gt;110 Ey and &gt;200 Ty); no source for those numbers is in this repository and none could be read, so they were removed and the two are shown as NUBASE2020 shows them, as observationally stable. Nuclides with no half-life number are described by what NUBASE2020 says (an extrapolation from systematics, only a limit, only an approximate value, &ldquo;p-unst&rdquo;, nothing): the text &ldquo;seen only fleetingly&rdquo; was not in the data. The decay clock times only nuclides with a measured or approximate half-life; the half-lives the older table held for 369 extrapolated nuclides and 11 limits are not used. The decay modes that NUBASE2020 lists with a question mark and no intensity (the file&rsquo;s header does not define the question mark) are said to be left out of the odds, for the 644 nuclides where that is so. The magic numbers Z = 114 and N = 184 in the nucleus&rsquo;s shape estimate come from the web version and have no source (they are predictions for superheavy nuclei, never observed); they are kept and labelled, and 286 nuclides&rsquo; estimated shapes depend on them. The textbook magic numbers 2, 8, 20, 28, 50, 82 and 126 were not looked up.</li>"},
    {"About sources: the textbooks for the hydrogen-like orbitals", "(the 1913 paper behind the Bohr 1913 style)</li>", "(the 1913 paper behind the Bohr 1913 style)</li><li>Textbooks for the hydrogen-like orbitals (the average distance of the electron, the radial probability, the cloud tests): David J. Griffiths, <i>Introduction to Quantum Mechanics</i>; Hans A. Bethe and Edwin E. Salpeter, <i>Quantum Mechanics of One- and Two-Electron Atoms</i></li>"},
    // 0.10.5 stage 3: the sources of the planets' gravity (JPL physical parameters), the Sun's radius (Horizons header) and the comet records
    {"About sources: planet gravity from JPL (the fact-sheet entry no longer claims gravity)", "(sizes, masses, gravity, temperatures)</li>", "(sizes, masses and temperatures; the planets&rsquo; gravity is from JPL, below)</li>"},
    {"About sources: JPL physical parameters, the Sun's radius, the comet records", "(dwarf planets, asteroids, comets)</li>", "(dwarf planets, asteroids, comets)</li><li><a href=\"https://ssd.jpl.nasa.gov/planets/phys_par.html\" target=\"_blank\" rel=\"noopener noreferrer\">NASA/JPL: planetary physical parameters</a> (each planet&rsquo;s surface gravity at the equator, read from the page on 2026-10-03; the cards give it in g, with Earth&rsquo;s 9.80 m/s&sup2; on the same page as 1 g)</li><li>NASA/JPL Horizons, the Sun&rsquo;s header (Horizons API, COMMAND 10, read 2026-10-03): &ldquo;Solar radius (IAU2015) = 695700 km&rdquo;, the nominal solar radius the lab uses; and &ldquo;Adopted sid. rot. per. = 25.38 d&rdquo;, the Sun&rsquo;s standard turning period on its card</li><li>NASA/JPL Small-Body Database records of the six famous comets (SBDB API, read 2026-10-03: orbit 75 Halley, 226 Hale&ndash;Bopp, 31 NEOWISE, K273/20 Encke, 32 Swift&ndash;Tuttle; 67P: orbit K284/1 as Horizons gives it at 2021-11-02), and Horizons position vectors of the comets at two dates each, used to check them; the comets&rsquo; nucleus sizes are the Small-Body Database&rsquo;s diameters (it has none for NEOWISE). Each comet is one fixed two-body orbit, so it drifts from the real one away from the record&rsquo;s date</li><li>NASA/JPL astrodynamic parameters page, the astronomical unit: 149,597,870,700 m exactly, which the lab uses for every AU, and the speed of light, 299,792,458 m/s (quoted there as a 2018 CODATA value). The page names the IAU 2012 resolution as B1, the owner's decision calls it B2; the IAU text itself (and the SYRTE page about it) could not be opened, so it was not read. The lab used the web version's 149.6e9 m before 0.10.5 stage 4. A light year is worked out as that speed times a 365.25-day year, in AU (the IAU value was not looked up)</li><li>Figures with no named source, marked rough in the app or stated as the app's own rule: the Sun's surface temperature, habitable zone, frost line and lifetime; the Sun's 25-day equator and 34-day pole turning; the counts and masses on the belt cards (asteroid, Trojan, Kuiper, Oort); NEOWISE's 5 km nucleus; the 30 % reflected-light assumption for temperatures without an atmosphere; the Moon's phase names between the four main phases (this app's own cut-offs)</li>"},
    // 0.13 stage 1: the phase colours on the graphics card
    {"About sources: the phase colours on the graphics card (0.13 stage 1)", "(the formulas are the standard ones, written from knowledge, not looked up here)</li>", "(the formulas are the standard ones, written from knowledge, not looked up here)</li><li>Phase colours on the graphics card (0.13 stage 1): the same colour map written a second time in the shader language (GLSL 330), as a direct port of the C++ function and in 32-bit instead of 64-bit numbers; no new data or source, only who works out the colour changed. The tests check that its constants are the C++ ones, that glslang (the Khronos reference compiler) accepts every shader, and that pictures drawn by the old way (the processor colours each dot) and the new way (the graphics card picks the colour) with Mesa&rsquo;s software OpenGL (llvmpipe) are the same pixel for pixel (largest difference in the pictures tested: 0 of 255). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>"},
    // 0.13 stage 2: superposition and jumps on the graphics card
    {"About sources: superposition and photon jumps on the graphics card (0.13 stage 2)", "The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>", "The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li><li>Superposition and photon jumps on the graphics card (0.13 stage 2): which dots of a superposition or a cloud jump are shown, and their colours, are now worked out by a compute shader (OpenGL 4.3) from the amplitudes stored with each dot; where the PC has only OpenGL 3.3 the processor does it as before. No new physics and no new data: the formulas are the lab&rsquo;s own (the hydrogen-like wave functions and the quantum beats, from the textbooks listed above: Griffiths, <i>Introduction to Quantum Mechanics</i>, and Bethe and Salpeter), with the CODATA 2022 constants already used (Bohr radius 5.29177210544e-11 m, Hartree energy 27.211386245981 eV, proton-electron mass ratio 1836.152673426, h = 6.62607015e-34 J s exact). The tests check the superposed density, phase and beat period at fixed points against those textbook formulas written out separately, and the graphics-card result against the processor&rsquo;s for every dot, in 32-bit against 64-bit numbers: a dot within rounding of a threshold may come out differently (a handful in a million), and each such dot is checked to be that close. Found while checking: the lab normalised each orbital by a numerical sum that left its amplitude too big by 3e-6 (2p) to 1.6e-5 (3d z&sup2;) against the exact textbook value 1; made exact in 0.13 stage 3 (see below). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>"},
    // 0.13 stage 3: the 90 % outline on the graphics card, the exact normalisation
    {"About sources: the 90 % outline on the graphics card and the exact normalisation (0.13 stage 3)", "3d z&sup2;) against the exact textbook value 1; made exact in 0.13 stage 3 (see below). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>", "3d z&sup2;) against the exact textbook value 1; made exact in 0.13 stage 3 (see below). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li><li>The 90 % outline on the graphics card, and the exact normalisation of the orbitals (0.13 stage 3). Normalisation: the integral of the squared wave function is now worked out in closed form. The radial part is (n/2)<sup>2l+3</sup> &middot; 2n &middot; (n+l)!/(n&minus;l&minus;1)!, from the orthogonality and the three-term recurrence of the generalised Laguerre polynomials (NIST Digital Library of Mathematical Functions, chapter 18; Abramowitz and Stegun, chapter 22), and it is multiplied by the sphere integral of the square of each angular polynomial (the real cubic harmonics are combinations of the spherical harmonics, normalised as in Griffiths, <i>Introduction to Quantum Mechanics</i>, chapter 4). The formulas are the standard ones, written from knowledge; the chapter references were not opened in this session. They are checked by hand for 1s, 2p z, 3d z&sup2; and 4f z&sup3;, and by an independent quadrature for every orbital the app offers (real and complex, Z = 1, 2 and 5): each integrates to 1 within 1e-10 (before: off by up to 8.8e-5 in the density). Outline: the density on a 64&times;64&times;64 grid and its 90 % level (found by a search over the cell values instead of sorting them) are now worked out by compute shaders (OpenGL 4.3) without the picture waiting, so the outline appears a frame or two after it is switched on; the processor does it where there is no OpenGL 4.3. The region is defined as before (the densest cells holding 90 % of the probability, the standard textbook definition); the graphics-card level agrees with the processor&rsquo;s to 1e-4 and its cells to float rounding. The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>"},
    // 0.13 stage 4: the Orbit Lab rocks and the chart on the graphics card
    {"About sources: the Orbit Lab rocks and the chart on the graphics card (0.13 stage 4)", "the graphics-card level agrees with the processor&rsquo;s to 1e-4 and its cells to float rounding. The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>", "the graphics-card level agrees with the processor&rsquo;s to 1e-4 and its cells to float rounding. The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li><li>The Orbit Lab&rsquo;s rocks and the chart of nuclides on the graphics card (0.13 stage 4, the last of the four &ldquo;GPU update&rdquo; stages). No new data and no new physics. Rocks: where each belt rock is now worked out by a compute shader (OpenGL 4.3) from its Kepler ellipse, with the same formulas as before: Kepler&rsquo;s equation M = E &minus; e sin E, then x&prime; = a (cos E &minus; e) and y&prime; = a &radic;(1 &minus; e<sup>2</sup>) sin E, and the mean motion n = &radic;(GM/a<sup>3</sup>) from Kepler&rsquo;s third law; they are the formulas of the NASA/JPL page &ldquo;Keplerian Elements for Approximate Positions of the Major Planets&rdquo; (listed above, re-read on 5 October 2026 for the first two). The time and the mean anomaly are kept in double precision on the graphics card and brought into &minus;&pi;..&pi; there; only that small angle goes into 32-bit numbers, so a rock is where it was after 10,000 years at the fastest time speed too: the tests compare every rock of the benchmark&rsquo;s scenes 1 and 6 with the processor&rsquo;s and with Kepler&rsquo;s equation solved separately by bisection, at times up to ten million years; the places agree to 0.003 of a point on the picture. Found while checking: the belt code solved Kepler&rsquo;s equation with exactly three Newton steps, which is exact for every rock the app makes (the largest eccentricity is 0.498) but not for e of 0.8 and more (0.45 radian off at e = 0.99); it is now iterated until converged, so the app&rsquo;s rocks do not move. Chart of nuclides: the 3,557 squares or blocks and the ground are made once and kept in a graphics-card buffer, and the tilt, zoom and pan are applied there; they are rebuilt only when the picture size, the theme colours or the height meaning changes. The pictures and the hit test agree with the old way (tested triangle by triangle, and by pictures). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>"},
    // 0.14 stage 1: the contrast rule and Frutiger Aero
    {"About sources: the contrast rule and Frutiger Aero (0.14 stage 1)", "the hit test agree with the old way (tested triangle by triangle, and by pictures). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li>", "the hit test agree with the old way (tested triangle by triangle, and by pictures). The speed-up is expected, not measured: it has not been run on a real graphics card or on Windows</li><li>The colour themes&rsquo; contrast rule and the Frutiger Aero theme (0.14 stage 1). No new data and no physics. Contrast follows the W3C&rsquo;s Web Content Accessibility Guidelines 2.x (www.w3.org/TR/WCAG21, the definitions of &ldquo;relative luminance&rdquo; and &ldquo;contrast ratio&rdquo;, and success criteria 1.4.3 and 1.4.11): L = 0.2126 R + 0.7152 G + 0.0722 B, where each sRGB channel c (0 to 1) becomes c/12.92 if c &le; 0.04045, else ((c + 0.055)/1.055)<sup>2.4</sup>; the ratio is (L<sub>1</sub> + 0.05)/(L<sub>2</sub> + 0.05) with L<sub>1</sub> the lighter colour; it must be at least 4.5 for body text and 3 for large text and parts of the interface. Every text and background pair of Frutiger Aero was computed with it, including its glass over the lightest and the darkest point of the sky-blue page, and the tests check the formula against values worked by hand (black on white 21, #777777 on white 4.48). The W3C page could not be opened from the session that wrote this, so the formula is written from knowledge and checked only by that hand arithmetic; older WCAG text says 0.03928 instead of 0.04045, which changes no 8-bit colour. Only the panels, buttons and text were checked: the pictures of the labs keep their own colours. Frutiger Aero uses Segoe UI when Windows has it (a Microsoft font, used from the PC and never shipped with the app); without it the default font is used. The look has not been seen on a real PC.</li>"},
    {"About sources: Frutiger Aero's real look, the Light accent fix, Y2K Ice and Night, and the Michroma font (0.14 stage 2)", "The look has not been seen on a real PC.</li>", "The look has not been seen on a real PC.</li><li>Frutiger Aero&rsquo;s real look, the Light accent fix and the Y2K Ice and Y2K Night themes (0.14 stage 2). No new data and no physics. The colours, textures and shapes are original style choices made for this app: no logos, icons, characters, photos or artwork of any company were used. The contrast rule is the same W3C WCAG 2.x formula as in the item above, now applied to every text and background pair over the page&rsquo;s clouds, bubbles, film grain, halftone dots, grid lines and light streak at their strongest, over every state of the jelly and chrome buttons (the bright top and the deep lower half) and over every title bar. Light&rsquo;s accent text (#2f6fed on its page #f7f7f5) was 4.24:1, under the 4.5 that body-size text needs; it is now #2d6ae3 (4.57:1), the same hue. The window titles and the app name in the two Y2K themes are set in Michroma (&copy; 2011 The Michroma Project Authors, <a href=\"https://github.com/googlefonts/Michroma-font\" target=\"_blank\" rel=\"noopener noreferrer\">github.com/googlefonts/Michroma-font</a>), a free wide font under the SIL Open Font License 1.1: the font file and its licence text are built into the app, unchanged. The look has not been seen on a real PC or a real graphics card.</li>"},
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
    checkTrue("About lists the IAEA table of nuclear charge radii, its credit and the limits (0.10.9 stage 1)", std::string(kAboutHtml).find("www-nds.iaea.org/radii/charge_radii.csv") != std::string::npos && std::string(kAboutHtml).find("Angeli and K. P. Marinova") != std::string::npos &&
              std::string(kAboutHtml).find("not the matter radius") != std::string::npos && std::string(kAboutHtml).find("is an assumption") != std::string::npos);
    checkTrue("About lists the IAEA quadrupole table, what its second column is, the model assumption and the limits (0.10.9 stage 2)", std::string(kAboutHtml).find("elec_mom_recomm.csv") != std::string::npos && std::string(kAboutHtml).find("is the mass number") != std::string::npos &&
              std::string(kAboutHtml).find("a model assumption, not a measurement") != std::string::npos && std::string(kAboutHtml).find("zero for spin 0 and 1/2 whatever the shape") != std::string::npos);
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
    // every edit was used (nothing in the list is left over): units 1 + about 14 + sgra 1 + guide 8 + slingshot 1
    checkTrue("About lists NUBASE2020 (file, size, citation as the file gives it), that the citation page could not be read, the colour rule as a convention, and that the magic numbers were not looked up (0.11 stage 2)", [&] { const std::string a = kAboutHtml; for (const char* w : {"nubase_4.mas20.txt", "761,906 bytes", "Chin. Phys. C45, 030001 (2021)", "could not be read", "a convention of this lab", "3,054 of the 3,083", "not looked up; 126 is a neutron number only"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About says the landscape heights are a chosen scale, not a measurement: the log of the half-life, the two ends, the plateau for stable nuclides, no height for unknown ones, the isometric angles written from knowledge (0.11 stage 3)", [&] { const std::string a = kAboutHtml; for (const char* w : {"a chosen scale, not a measurement", "10<sup>&minus;23</sup> s", "10<sup>32</sup> s", "12 tiles tall", "1.1 scale heights", "has no height at all", "54.74 degrees"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the 0.12 stage 2 clean-up: the mendeleev package read, what changed (113 rounded, 16 stale, Kr-78 and Xe-124 removed because no source), no time for extrapolated half-lives, the odds left out, and the unsourced magic numbers 114 and 184 kept and labelled", [&] { const std::string a = kAboutHtml; for (const char* w : {"Accuracy clean-up of the half-lives", "version 1.3.0 from PyPI", "2,774 of 2,799", "seaborgium-269: 14 minutes before, 5 minutes now", "no source for those numbers is in this repository", "seen only fleetingly", "369 extrapolated nuclides and 11 limits", "the 644 nuclides", "Z = 114 and N = 184", "286 nuclides"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the phase colours on the graphics card (0.13 stage 1): a port of the C++ function, glslang, llvmpipe, expected and not measured", [&] { const std::string a = kAboutHtml; for (const char* w : {"Phase colours on the graphics card (0.13 stage 1)", "direct port of the C++ function", "glslang (the Khronos reference compiler)", "llvmpipe", "expected, not measured"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists superposition and jumps on the graphics card (0.13 stage 2): the textbooks and CODATA 2022 as already used, the textbook check, the 32-bit edge dots, the normalisation found, expected and not measured", [&] { const std::string a = kAboutHtml; for (const char* w : {"Superposition and photon jumps on the graphics card (0.13 stage 2)", "OpenGL 4.3", "1836.152673426", "a dot within rounding of a threshold", "too big by 3e-6", "expected, not measured"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the Orbit Lab rocks and the chart on the graphics card (0.13 stage 4): the JPL page formulas, the double precision rule, the 3-step solver finding, the chart rebuild rule, expected and not measured", [&] { const std::string a = kAboutHtml; for (const char* w : {"The Orbit Lab&rsquo;s rocks and the chart of nuclides on the graphics card (0.13 stage 4", "Keplerian Elements for Approximate Positions of the Major Planets", "double precision on the graphics card", "exactly three Newton steps", "0.45 radian off at e = 0.99", "only when the picture size, the theme colours or the height meaning changes", "expected, not measured"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the 90 % outline on the graphics card and the exact normalisation (0.13 stage 3): the closed form and its sources, the checks, the 1e-10, expected and not measured", [&] { const std::string a = kAboutHtml; for (const char* w : {"The 90 % outline on the graphics card, and the exact normalisation of the orbitals (0.13 stage 3)", "(n+l)!/(n&minus;l&minus;1)!", "generalised Laguerre polynomials", "within 1e-10", "chapter references were not opened", "expected, not measured"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the decay chains: the four series typed from textbooks and NOT from a source that was read, the checks that need no table (the mass-number rule, the exponential law, binomial statistics), the normalised odds, the stand-in for odds not measured, no time for a limit or an extrapolation, and that the time shown is not real time (0.12 stage 1)", [&] { const std::string a = kAboutHtml; for (const char* w : {"Decay chains on the chart (0.12 stage 1)", "typed into the tests from standard textbook tables, <b>not</b> from a source read in the session", "A = A<sub>0</sub> mod 4", "the median lifetime is the half-life", "only as a marked stand-in, never as data", "is never given a time", "is not real time", "about 1.6 seconds apart"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists AME2020 (file, size, date, citation as the file gives it), the # estimates drawn flat, the chosen linear scale, the peak read from the table, that no source for the island could be read, that nothing predicted is drawn or quoted, and that the wording follows the brief (0.11 stage 4)", [&] { const std::string a = kAboutHtml; for (const char* w : {"mass.mas20.txt", "476,239 bytes", "Chin. Phys. C45, 030002 (2021)", "1,008 of the 3,557 nuclides", "drawn flat and pale", "a chosen linear scale, 0 to 9,000 keV per nucleon", "nickel-62, 8,794.56 keV", "no source for it could be read", "draws no predicted region and quotes no predicted number", "that wording follows the project owner&rsquo;s brief"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the WCAG 2.x contrast formula and thresholds, that the W3C page was not reachable, and Segoe UI as the PC's font that is never shipped (0.14 stage 1)", [&] { const std::string a = kAboutHtml; for (const char* w : {"www.w3.org/TR/WCAG21", "0.2126 R + 0.7152 G + 0.0722 B", "0.04045", "at least 4.5", "could not be opened", "Segoe UI", "never shipped"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists Michroma with its licence (SIL Open Font License), the Light accent fix and that no brand artwork was used (0.14 stage 2)", [&] { const std::string a = kAboutHtml; for (const char* w : {"Michroma", "SIL Open Font License 1.1", "github.com/googlefonts/Michroma-font", "#2d6ae3", "4.24:1", "no logos, icons, characters, photos or artwork"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("every native edit was used", g_editUses == 1 + 31 + 1 + 8 + 1);   // the About list has 31 edits since 0.14 stage 2 (Aero's real look, Light's accent, Y2K and the Michroma font), 30 since 0.14 stage 1 (the contrast rule and Frutiger Aero), 29 since 0.13 stage 4 (the Orbit Lab rocks and the chart on the graphics card), 28 since 0.13 stage 3 (the outline on the graphics card, the exact normalisation), 27 since 0.13 stage 2 (superposition and jumps on the graphics card), 26 since 0.13 stage 1 (the phase colours on the graphics card), 25 since 0.12 stage 2 (the accuracy clean-up), 24 since 0.12 stage 1 (decay chains), 23 since 0.11 stage 4 (AME2020, the valley and the island), 22 since 0.11 stage 3 (the landscape heights are a chosen scale), 21 since 0.11 stage 2 (NUBASE2020 and the chart of nuclides), 20 since 0.10.9 stage 2 (the IAEA quadrupole moments), 19 since 0.10.9 stage 1 (the IAEA charge radii), 18 since 0.10.8 stage 1 (the Dirac levels), 17 since 0.10.7 stage 2 (the NIST ASD lines), 16 since 0.10.6 stage 6 (15 since stage 5: 14 since 0.10.5 stage 3: two added for the gravity, Sun radius and comet sources; one more for the visible range and the Dirac energy)
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
// ---- 0.10.6 stage 7: the Atom Lab speed-risk scenes (audit findings 10, 11, 12): each sets up, steps headlessly, and ends in the state it declared ----
static void testBenchRiskScenes() {
    using atom::kBenchFirstRisk; using atom::kBenchSceneCount;
    checkTrue("risk scenes: they come after the 5 old Atom Lab scenes, 11 of them, then the flat chart scene (0.11 stage 2: 16 to 17) the landscape scene (stage 3: 17 to 18) the binding landscape scene (stage 4: 18 to 19) and the decay-chain scene (0.12 stage 1: 19 to 20, on purpose), then the two CPU-colour scenes (0.13 stage 1: 20 to 22, on purpose: scenes 23 and 24) and the four CPU-superposition scenes (0.13 stage 2: 22 to 26, on purpose: scenes 25 to 28) and the two CPU-outline scenes (0.13 stage 3: 26 to 28, on purpose: scenes 29 and 30)", kBenchFirstRisk == 5 && atom::kBenchChartScene == 16 && atom::kBenchLandscapeScene == 17 && atom::kBenchBindingScene == 18 && atom::kBenchChainScene == 19 && atom::kBenchWheelCpuScene == 20 && atom::kBenchWheel2DCpuScene == 21 && atom::kBenchJumpCpuScene == 22 && atom::kBenchSuperCpuFirst == 23 && atom::kBenchOutlineCpuFirst == 26 && kBenchSceneCount == 28);
    struct Want { const char* view; const char* orb; const char* cstyle; const char* cdens; const char* phase; bool super; const char* outline; };
    const Want want[] = {
        {"Cloud 3D", "4f", "Classic", "Ultra", "Wheel", false, "Off"},   // 5  finding 10, 3D, phase colours
        {"Cloud 3D", "4f", "Classic", "Ultra", "Sign", false, "Off"},    // 6  the same with two colours (what the old look costs)
        {"Cloud", "4f", "Classic", "Ultra", "Wheel", false, "Off"},      // 7  finding 10, 2D
        {"Cloud 3D", "4f", "Classic", "Ultra", "Sign", false, "Off"},    // 8  finding 11, a jump held half way
        {"Cloud 3D", "4f", "Classic", "Ultra", "Sign", false, "Off"},    // 9  finding 11, jumps one after another
        {"Cloud 3D", "2p", "Classic", "Ultra", "Sign", true, "Off"},     // 10 finding 11, orthogonal pair, one energy
        {"Cloud 3D", "2p", "Classic", "Ultra", "Sign", true, "Off"},     // 11 finding 11, overlapping pair
        {"Cloud 3D", "2p", "Classic", "Ultra", "Sign", true, "Off"},     // 12 finding 11, orthogonal pair, two energies (it beats)
        {"Cloud 3D", "4f", "Classic", "Normal", "Sign", false, "On"},    // 13 finding 12, first use of an outline
        {"Slice", "4f", "Classic", "Normal", "Sign", false, "On"},       // 14 finding 12, the cut dragged
        {"Orbit", "1s", "Glow", "High", "Sign", false, "Off"},           // 15 (the lab's own defaults for the cloud: it is not drawn) finding 12, the nucleus view
    };
    std::vector<std::string> names;
    for (int which = kBenchFirstRisk; which < atom::kBenchChartScene; which++) {   // (the chart scenes have their own tests below)
        const std::string w = "risk scene " + std::to_string(which - kBenchFirstRisk + 8) + ": ";
        const Want& e = want[which - kBenchFirstRisk];
        atom::AtomLab L, again;
        oddAtomLab(L);                                          // a lab the user has changed everywhere: the scene must not care
        atom::benchSetup(L, which, 800, 600);
        atom::benchSetup(again, which, 800, 600);
        checkTrue(w + "same settings from an odd lab and from a default one", sameParams(L.p, again.p));
        const bool nucleus = which == 15;
        checkTrue(w + "the nucleus asked for", L.p.Z == (nucleus ? 118 : 92) && L.p.N == (nucleus ? 176 : 146));
        checkTrue(w + "state the drawing reads is fresh", !L.moreStored && !L.paused && !L.hold && L.chain.empty() && L.s.layer == "Atom" && !L.s.spin);
        checkTrue(w + "speed is the default 5", L.p.speed == 5);
        // step it the way the benchmark does: 200 warm-up frames, then 400 counted ones, one frame of the lab (1/240 s) each
        int jumpStarts = 0; bool wasJumping = false; double lastT = 0;         // a start: a jump appears, or its clock goes back to the start (one began the frame the last one ended)
        const int frames = which == 9 ? 1500 : 600;       // the jumps one after another: 1500 frames = 6.25 s of the lab's time
        for (int f = 0; f < frames; f++) {
            atom::benchFrame(L, which, f, f < 200 ? -1 : f - 200);
            if (L.s.trans && (!wasJumping || L.s.trans->t < lastT)) jumpStarts++;
            wasJumping = bool(L.s.trans); lastT = L.s.trans ? L.s.trans->t : 0;
            L.step(1.0 / 240);
        }
        checkTrue(w + "view, orbital, cloud style, phase, outline as declared", L.p.view == e.view && (L.p.orb == e.orb || (which == 9 && L.p.orb == "3d")) && L.p.cstyle == e.cstyle && L.p.phase == e.phase && L.p.outline == e.outline);
        checkTrue(w + "dot count as declared", L.p.cdens == e.cdens && L.cloudCap() == (std::string(e.cdens) == "Ultra" ? 48000 : std::string(e.cdens) == "High" ? 24000 : 12000) && (nucleus || L.s.k >= L.cloudCap()));
        checkTrue(w + "superposition as declared", L.superOn() == e.super && L.p.super == (e.super ? "On" : "Off"));
        names.push_back(atom::benchSceneInfo(which).name);
        const std::string& nm = names.back();
        checkTrue(w + "the name says the setting the cost depends on",
                  (nm.find("Wheel") != std::string::npos) == (L.p.phase == "Wheel") && (which == 13 || which == 14 || nucleus || nm.find("48,000") != std::string::npos)
                  && (L.p.view != "Cloud" || nm.find("2D") != std::string::npos) && (L.p.view != "Cloud 3D" || nm.find("3D") != std::string::npos));
        const std::string txt = atom::benchSettingsText(L);
        checkTrue(w + "the settings text names orbital, dots, cloud style and phase", nucleus || (txt.find("orbital " + L.p.orb) != std::string::npos && txt.find(std::to_string(L.cloudCap()) ) != std::string::npos && txt.find("cloud style Classic") != std::string::npos && txt.find("phase " + L.p.phase) != std::string::npos));
        if (which == 8) {
            checkTrue(w + "a jump is in flight, held half way (not over, not at the start)", L.s.trans && L.s.trans->t > 0.3 && L.s.trans->t < 0.7 && jumpStarts == 1 && L.s.jn == L.cloudCap());
        }
        if (which == 9) checkTrue(w + "jumps start again and again (at least 3 in 1500 frames: emit, absorb, emit)", jumpStarts >= 3);
        if (which >= 10 && which <= 12) {
            const atom::Superposition sp = L.superState();
            const double ov = std::abs(atom::overlapAB(sp.idA, sp.idB));
            checkTrue(w + (which == 11 ? "the pair overlaps (the stage 4 sampling path)" : "the pair is orthogonal (the old thinning path)"), which == 11 ? ov != 0.0 : ov == 0.0);
            const double eA = atom::levelEV(L.p.Z, L.p.N, atom::orbitals()[L.fullOrb(L.p.orb)].n), eB = atom::levelEV(L.p.Z, L.p.N, atom::orbitals()[L.fullOrb(L.p.orb2)].n);
            checkTrue(w + (which == 12 ? "the two energies differ (a beat)" : "one energy (no beat)"), which == 12 ? eA != eB : eA == eB);
            checkTrue(w + "mix 50 %", L.p.mix == 50);
        }
        if (which == 13) checkTrue(w + "the outline is on, and its cache was emptied at counted frame 360 (nothing here draws, so it is still empty)", L.p.outline == "On" && atom::outlineCacheSize() == 0);
        if (which == 14) checkTrue(w + "the cut has moved from the middle, inside -1 .. 1", L.p.cut != 0 && std::fabs(L.p.cut) <= 1);
        if (nucleus) checkTrue(w + "every nucleon is there to draw, the whole nucleus in view", int(L.s.nuc.size()) == 294 && L.s.focus == -1 && L.p.qstyle == "Cinematic" && L.p.detail == "+ Sea");
        // repeatable: the same scene stepped the same way ends in the same state
        atom::AtomLab B1, B2;
        for (atom::AtomLab* Q : {&B1, &B2}) {
            atom::benchSetup(*Q, which, 800, 600);
            for (int f = 0; f < 120; f++) { atom::benchFrame(*Q, which, f, f < 40 ? -1 : f - 40); Q->step(1.0 / 240); }
        }
        checkTrue(w + "repeatable: the same dots and the same cut", B1.s.pts.size() == B2.s.pts.size() && std::equal(B1.s.pts.begin(), B1.s.pts.begin() + std::min<std::size_t>(3000, B1.s.pts.size()), B2.s.pts.begin()) && B1.p.cut == B2.p.cut && B1.s.k == B2.s.k);
    }
    checkTrue("risk scenes: every name is different and not empty", [&] { for (std::size_t i = 0; i < names.size(); i++) { if (names[i].empty()) return false; for (std::size_t j = i + 1; j < names.size(); j++) if (names[i] == names[j]) return false; } return true; }());
    checkTrue("risk scenes: the old scene numbers 0 to 4 still ask for what they did (the benchmark stays comparable)", [] {
        atom::AtomLab L; atom::benchSetup(L, 1, 800, 600);
        return L.p.cstyle == "Classic" && L.p.cdens == "Normal" && L.p.view == "Cloud 3D" && L.p.orb == "4f" && L.p.phase == "Sign" && L.p.super == "Off" && L.p.outline == "Off";
    }());
    // the outline cache can be emptied (the first-use scene needs a cold start) and fills again
    atom::clearOutlineCache();
    checkTrue("the outline cache is empty after clearOutlineCache", atom::outlineCacheSize() == 0);
    const int orbA = atom::orbIndex("2p|z");
    (void)atom::outlineOf(orbA);
    checkTrue("one outline cached after one use", atom::outlineCacheSize() == 1);
    atom::clearOutlineCache();
    checkTrue("and empty again", atom::outlineCacheSize() == 0 && atom::outlineOf(orbA).mass > 0.89);
}
// ---- 0.11 stage 2: the chart of nuclides scene: sets up, steps headlessly, ends in the state it declared ----
static void testBenchChartScene() {
    atom::AtomLab L, again;
    oddAtomLab(L);
    atom::benchSetup(L, atom::kBenchChartScene, 800, 600);
    atom::benchSetup(again, atom::kBenchChartScene, 800, 600);
    checkTrue("chart scene: same settings from an odd lab and from a default one", sameParams(L.p, again.p));
    checkTrue("chart scene: the chart view, uranium-238 chosen (the marker is drawn), nothing paused or held", L.chartOn() && L.p.Z == 92 && L.p.N == 146 && !L.paused && !L.hold && L.s.layer == "Atom");
    for (int f = 0; f < 600; f++) { atom::benchFrame(L, atom::kBenchChartScene, f, f < 200 ? -1 : f - 200); L.step(1.0 / 240); }
    checkTrue("chart scene: after 600 frames it is still the same chart with the same nuclide (nothing moves by itself)", L.chartOn() && L.p.Z == 92 && L.p.N == 146 && !L.s.trans && !L.s.photon);
    const std::string nm = atom::benchSceneInfo(atom::kBenchChartScene).name, what = atom::benchSceneInfo(atom::kBenchChartScene).what, txt = atom::benchSettingsText(L);
    checkTrue("chart scene: the name says it is the whole chart and how many squares; the text names the view and the nuclide", nm.find("Chart of nuclides") != std::string::npos && nm.find("3,557") != std::string::npos &&
              txt.find("Chart of nuclides") != std::string::npos && txt.find("3557") != std::string::npos && txt.find("Z 92 N 146") != std::string::npos);
    checkTrue("chart scene: the description tells the owner to keep the pointer off the picture and what to look at", what.find("pointer") != std::string::npos && what.find("GPU time") != std::string::npos);
}
// ---- 0.11 stage 3: the landscape scene (full tilt), beside the flat chart scene ----
static void testBenchLandscapeScene() {
    atom::AtomLab L, again, flat;
    oddAtomLab(L);
    L.setChartTilt(0.4); L.chartZoomAt(3, 100, 100);                       // an odd lab: tilt and zoom left over from the user
    atom::benchSetup(L, atom::kBenchLandscapeScene, 800, 600);
    atom::benchSetup(again, atom::kBenchLandscapeScene, 800, 600);
    atom::benchSetup(flat, atom::kBenchChartScene, 800, 600);
    checkTrue("landscape scene: same settings from an odd lab (tilt and zoom left over) and from a default one", sameParams(L.p, again.p) && L.s.chartView == again.s.chartView);
    checkTrue("landscape scene: the chart view at full tilt, zoom 1, no pan, uranium-238 chosen; the flat chart scene is still flat (tilt 0, zoom 1)", L.chartOn() && L.s.chartView.tilt == 1 && L.s.chartView.zoom == 1 && L.s.chartView.panX == 0 &&
              L.p.Z == 92 && L.p.N == 146 && flat.s.chartView.flat());
    for (int f = 0; f < 600; f++) { atom::benchFrame(L, atom::kBenchLandscapeScene, f, f < 200 ? -1 : f - 200); L.step(1.0 / 240); }
    checkTrue("landscape scene: after 600 frames nothing has moved (same nuclide, tilt 1, zoom 1)", L.chartOn() && L.p.Z == 92 && L.p.N == 146 && L.s.chartView.tilt == 1 && L.s.chartView.zoom == 1);
    const std::string nm = atom::benchSceneInfo(atom::kBenchLandscapeScene).name, nf = atom::benchSceneInfo(atom::kBenchChartScene).name, what = atom::benchSceneInfo(atom::kBenchLandscapeScene).what, txt = atom::benchSettingsText(L);
    checkTrue("landscape scene: its name says landscape and tilted, differs from the flat scene's, and says 3,557 blocks; the settings text says tilt 100 %", nm.find("landscape") != std::string::npos && nm.find("tilted") != std::string::npos && nm != nf &&
              nm.find("3,557") != std::string::npos && txt.find("tilt 100 %") != std::string::npos && atom::benchSettingsText(flat).find("tilt 0 %") != std::string::npos);
    checkTrue("landscape scene: the description says it is the same data as the flat scene, to compare the two, what to look at first and to keep the pointer off", what.find("flat chart scene") != std::string::npos && what.find("DIFFERENCE") != std::string::npos &&
              what.find("CPU frame time") != std::string::npos && what.find("pointer") != std::string::npos);
}
// ---- 0.11 stage 4: the binding-height landscape scene ----
static void testBenchBindingScene() {
    atom::AtomLab L, again, half;
    oddAtomLab(L);
    L.setParam("cheight", std::string("Binding energy per nucleon")); L.setChartTilt(0.3); L.chartZoomAt(2, 100, 100);   // an odd lab: the other height, a tilt and a zoom left over
    atom::benchSetup(L, atom::kBenchBindingScene, 800, 600);
    atom::benchSetup(again, atom::kBenchBindingScene, 800, 600);
    atom::benchSetup(half, atom::kBenchLandscapeScene, 800, 600);
    checkTrue("binding scene: same settings from an odd lab and from a default one", sameParams(L.p, again.p) && L.s.chartView == again.s.chartView);
    checkTrue("binding scene: full tilt, zoom 1, no pan, the BINDING height, uranium-238 chosen; the half-life landscape scene (before it) still has the half-life height", L.chartOn() && L.s.chartView.tilt == 1 && L.s.chartView.zoom == 1 && L.s.chartView.panX == 0 &&
              L.s.chartView.height == atom::ChartHeightMode::Binding && half.s.chartView.height == atom::ChartHeightMode::HalfLife && L.p.Z == 92 && L.p.N == 146);
    for (int f = 0; f < 600; f++) { atom::benchFrame(L, atom::kBenchBindingScene, f, f < 200 ? -1 : f - 200); L.step(1.0 / 240); }
    checkTrue("binding scene: after 600 frames nothing has moved", L.chartOn() && L.p.Z == 92 && L.p.N == 146 && L.s.chartView.tilt == 1 && L.s.chartView.zoom == 1 && L.s.chartView.height == atom::ChartHeightMode::Binding);
    const std::string nm = atom::benchSceneInfo(atom::kBenchBindingScene).name, nl = atom::benchSceneInfo(atom::kBenchLandscapeScene).name, what = atom::benchSceneInfo(atom::kBenchBindingScene).what, txt = atom::benchSettingsText(L);
    checkTrue("binding scene: the name says BINDING-ENERGY height and tilted and differs from the half-life landscape scene's; the settings text says binding energy per nucleon and tilt 100 %; the half-life scene's text says half-life", nm.find("BINDING-ENERGY") != std::string::npos &&
              nm.find("tilted") != std::string::npos && nm != nl && txt.find("height binding energy per nucleon") != std::string::npos && txt.find("tilt 100 %") != std::string::npos && atom::benchSettingsText(half).find("height half-life") != std::string::npos);
    checkTrue("binding scene: the description names the three chart scenes, what the difference between the last two is, the CPU frame time and the pointer", what.find("flat, tilted with half-life height and tilted with binding height") != std::string::npos && what.find("DIFFERENCE") != std::string::npos &&
              what.find("CPU frame time") != std::string::npos && what.find("pointer") != std::string::npos);
}
static void testBenchChainScene() {                       // 0.12 stage 1: the landscape with the decay chain of U-238 drawn and walked five steps
    atom::AtomLab L, again, plain;
    oddAtomLab(L);
    L.setParam("cheight", std::string("Binding energy per nucleon")); L.setChartTilt(0.3); L.chartZoomAt(2, 100, 100); L.s.showChain = false; L.s.walkAuto = true;   // an odd lab: another height, a tilt, a zoom, the chain off and auto-walk on
    atom::benchSetup(L, atom::kBenchChainScene, 800, 600);
    atom::benchSetup(again, atom::kBenchChainScene, 800, 600);
    atom::benchSetup(plain, atom::kBenchLandscapeScene, 800, 600);
    checkTrue("chain scene: same settings from an odd lab and from a default one, the same walk (a fixed seed: the same five steps)", sameParams(L.p, again.p) && L.s.chartView == again.s.chartView && L.s.walk.steps.size() == 5 && again.s.walk.steps.size() == 5 &&
              L.s.walk.Z == again.s.walk.Z && L.s.walk.N == again.s.walk.N && L.s.walk.drawnS == again.s.walk.drawnS);
    checkTrue("chain scene: full tilt, the half-life height, uranium-238 chosen, the chain shown, auto-walk off; the three older chart scenes draw no chain (they stay comparable with 0.11)", L.chartOn() && L.s.chartView.tilt == 1 && L.s.chartView.height == atom::ChartHeightMode::HalfLife && L.p.Z == 92 && L.p.N == 146 &&
              L.s.showChain && !L.s.walkAuto && !plain.s.showChain && L.s.walk.startZ == 92);
    atom::AtomLab flat, bind; atom::benchSetup(flat, atom::kBenchChartScene, 800, 600); atom::benchSetup(bind, atom::kBenchBindingScene, 800, 600);
    checkTrue("chain scene: the flat chart scene and the binding scene draw no chain either", !flat.s.showChain && !bind.s.showChain);
    for (int f = 0; f < 600; f++) { atom::benchFrame(L, atom::kBenchChainScene, f, f < 200 ? -1 : f - 200); L.tick(1.0 / 240); L.step(1.0 / 240); }
    checkTrue("chain scene: after 600 frames the walk has not advanced and the chosen nuclide has not moved", L.s.walk.steps.size() == 5 && L.p.Z == 92 && L.p.N == 146 && L.s.chartView.tilt == 1);
    const std::string nm = atom::benchSceneInfo(atom::kBenchChainScene).name, nl = atom::benchSceneInfo(atom::kBenchLandscapeScene).name, what = atom::benchSceneInfo(atom::kBenchChainScene).what, txt = atom::benchSettingsText(L);
    checkTrue("chain scene: the name says DECAY CHAIN and tilted and differs from the landscape scene's; the settings text says the chain is drawn and walked 5 steps; the description says the DIFFERENCE with the scene before and that scenes 19 to 21 (the benchmark's numbers of the three chart scenes) draw no chain",
              nm.find("DECAY CHAIN") != std::string::npos && nm.find("tilted") != std::string::npos && nm != nl && txt.find("decay chain drawn, walked 5 steps") != std::string::npos && what.find("DIFFERENCE") != std::string::npos &&
              what.find("Scenes 19 to 21 draw no chain") != std::string::npos && what.find("Scenes 16 to 18") == std::string::npos && atom::benchSettingsText(plain).find("decay chain not drawn") != std::string::npos);
}
// ---- 0.13 stage 1: the GPU-colours switch and its two comparison scenes; the benchmark texts' scene numbers ----
static void testBenchGpuColours() {
    using atom::kBenchFirstRisk;
    const auto shown = [](int which) { return which - kBenchFirstRisk + 8; };         // the number the benchmark shows for a risk scene (ui/benchmark.cpp: the first risk scene is 8)
    checkTrue("GPU colours: a new lab has them On (the default)", atom::AtomLab().gpuColours);
    atom::AtomLab odd; odd.gpuColours = false;
    atom::AtomLab s8, s9, s10, s23, s24;
    atom::benchSetup(odd, 5, 800, 600);
    checkTrue("GPU colours: scene 8 switches them On even if the lab had them Off (no setting of the user's leaks into a scene)", odd.gpuColours);
    atom::benchSetup(s8, 5, 800, 600); atom::benchSetup(s9, 6, 800, 600); atom::benchSetup(s10, 7, 800, 600);
    atom::benchSetup(s23, atom::kBenchWheelCpuScene, 800, 600); atom::benchSetup(s24, atom::kBenchWheel2DCpuScene, 800, 600);
    checkTrue("GPU colours: scenes 8, 9 and 10 On, scenes 23 and 24 Off", s8.gpuColours && s9.gpuColours && s10.gpuColours && !s23.gpuColours && !s24.gpuColours);
    checkTrue("scene numbers: the stage 1 scenes are 23 and 24, the stage 2 scenes follow as 25 to 28 and the stage 3 scenes as 29 and 30 (always at the end, so the old numbers did not move; 0.13 stage 3 moved the last one from 28 to 30 on purpose)", shown(atom::kBenchWheelCpuScene) == 23 && shown(atom::kBenchWheel2DCpuScene) == 24 && shown(atom::kBenchJumpCpuScene) == 25 && shown(atom::kBenchOutlineCpuFirst) == 29 && shown(atom::kBenchSceneCount - 1) == 30);
    checkTrue("GPU colours: scene 23 is scene 8 in everything but the switch (same settings, same 48,000 dots), scene 24 is scene 10",
              sameParams(s8.p, s23.p) && sameParams(s10.p, s24.p) && s23.s.k == s8.s.k && s24.s.k == s10.s.k && s8.s.k == 48000 &&
              std::equal(s8.s.pts.begin(), s8.s.pts.begin() + 3 * 48000, s23.s.pts.begin()) && std::equal(s10.s.pts.begin(), s10.s.pts.begin() + 3 * 48000, s24.s.pts.begin()) &&
              std::equal(s8.s.ph.begin(), s8.s.ph.begin() + 48000, s23.s.ph.begin()));
    const std::string t8 = atom::benchSettingsText(s8), t23 = atom::benchSettingsText(s23), t9 = atom::benchSettingsText(s9);
    checkTrue("GPU colours: the settings text of a run says which way it ran, so both are labelled", t8.find("phase Wheel, GPU colours On") != std::string::npos && t23.find("phase Wheel, GPU colours Off") != std::string::npos && t9.find("GPU colours On") != std::string::npos &&
              atom::benchSettingsText(s24).find("GPU colours Off") != std::string::npos && atom::benchSettingsText(s10).find("GPU colours On") != std::string::npos);
    const auto info = [](int which) { return atom::benchSceneInfo(which); };
    const std::string n23 = info(atom::kBenchWheelCpuScene).name, w23 = info(atom::kBenchWheelCpuScene).what, n24 = info(atom::kBenchWheel2DCpuScene).name, w24 = info(atom::kBenchWheel2DCpuScene).what;
    checkTrue("GPU colours: the names of scenes 23 and 24 say OFF, Wheel, 3D / 2D and the scene they compare with; the descriptions say the DIFFERENCE and that it was not measured in the cloud session",
              n23.find("GPU colours OFF") != std::string::npos && n23.find("Wheel") != std::string::npos && n23.find("3D") != std::string::npos && n23.find("scene 8") != std::string::npos && w23.find("DIFFERENCE") != std::string::npos && w23.find("scene 8") != std::string::npos &&
              n24.find("GPU colours OFF") != std::string::npos && n24.find("Wheel") != std::string::npos && n24.find("2D") != std::string::npos && n24.find("scene 10") != std::string::npos && w24.find("DIFFERENCE") != std::string::npos && w24.find("scene 10") != std::string::npos &&
              w23.find("not measured") != std::string::npos && w24.find("not measured") != std::string::npos);
    const std::string w8 = info(5).what, w10 = info(7).what, n8 = info(5).name, n10 = info(7).name;
    checkTrue("GPU colours: scenes 8 and 10 keep their names (so old reports stay comparable), and their descriptions point at scenes 23 and 24 and say the speed-up is expected, not measured",
              n8 == "Risk 10: phase colours (Wheel), Classic dots, Cloud 3D, 48,000 dots" && n10 == "Risk 10: phase colours (Wheel), Classic dots, Cloud 2D (side view), 48,000 dots" && w8.find("Scene 23") != std::string::npos && w10.find("Scene 24") != std::string::npos &&
              w8.find("EXPECTED, not measured") != std::string::npos && w10.find("not measured") != std::string::npos);
    // the scene numbers the texts mention: each one is the scene it says
    const std::string w19 = info(atom::kBenchChartScene).what, w22 = info(atom::kBenchChainScene).what;
    checkTrue("scene 19 (the flat chart): it says to compare with scene 2 (the uranium-238 nucleus, the Orbit view), not with scene 8 (the Wheel cloud)", shown(atom::kBenchChartScene) == 19 && w19.find("Compare it with scene 2 (the Atom Lab's own Orbit view of uranium-238") != std::string::npos && w19.find("scene 8") == std::string::npos);
    atom::AtomLab s2; atom::benchSetup(s2, 0, 800, 600);
    checkTrue("scene 2 is what scene 19's text says: the Orbit view of uranium-238 (92 protons, 146 neutrons)", s2.p.view == "Orbit" && s2.p.Z == 92 && s2.p.N == 146);
    checkTrue("scene 22 (the chain): it says scenes 19 to 21 draw no chain, and those are the three other chart scenes", shown(atom::kBenchChainScene) == 22 && shown(atom::kBenchChartScene) == 19 && shown(atom::kBenchLandscapeScene) == 20 && shown(atom::kBenchBindingScene) == 21 && w22.find("Scenes 19 to 21 draw no chain") != std::string::npos);
    // every "scene N" in the texts of the risk scenes is a scene that exists (1 to 24)
    bool inRange = true; int mentions = 0;
    for (int which = kBenchFirstRisk; which < atom::kBenchSceneCount; which++) {
        const std::string t = info(which).name + " " + info(which).what;
        for (std::size_t i = 0; (i = t.find("cene", i)) != std::string::npos; i += 4) {
            std::size_t j = i + 4; while (j < t.size() && (t[j] == 's' || t[j] == ' ')) j++;
            if (j < t.size() && std::isdigit((unsigned char)t[j])) { const int n = std::atoi(t.c_str() + j); mentions++; inRange = inRange && n >= 1 && n <= shown(atom::kBenchSceneCount - 1); }
        }
    }
    checkTrue("scene numbers: every scene number in the risk scenes' texts (" + std::to_string(mentions) + " of them) is between 1 and 30", inRange && mentions >= 8);
}
// ---- 0.13 stage 2: the CPU-comparison scenes for the superposition and the jump, and the sentence about which way it runs ----
static void testBenchGpuCompute() {
    using atom::kBenchFirstRisk;
    const auto info = [](int which) { return atom::benchSceneInfo(which); };
    atom::AtomLab fresh; checkTrue("GPU superposition: a new lab has the switch On (the default)", fresh.gpuCompute);
    atom::AtomLab odd; odd.gpuCompute = false; atom::benchSetup(odd, 10, 800, 600);
    checkTrue("GPU superposition: scene 13 switches it On even if the lab had it Off (no setting of the user's leaks into a scene)", odd.gpuCompute);
    // scenes 25 to 28 (which 22 to 25) are scenes 11, 13, 14, 15 (which 8, 10, 11, 12) in everything but the switch: the same settings and the same dots
    const int pairs[4][2] = {{22, 8}, {23, 10}, {24, 11}, {25, 12}};
    for (const auto& pr : pairs) {
        atom::AtomLab cpu, gpu;
        atom::benchSetup(cpu, pr[0], 800, 600); atom::benchSetup(gpu, pr[1], 800, 600);
        const std::string w = "scene " + std::to_string(pr[0] + 3) + " against scene " + std::to_string(pr[1] + 3) + ": ";
        bool same = sameParams(cpu.p, gpu.p) && cpu.s.k == gpu.s.k && std::equal(cpu.s.pts.begin(), cpu.s.pts.begin() + 3 * 48000, gpu.s.pts.begin()) && cpu.s.sa == gpu.s.sa && cpu.gpuColours && gpu.gpuColours;
        checkTrue(w + "the same settings and the same 48,000 dots; the CPU scene has GPU superposition Off, the other On, and both have GPU colours On (stage 2 is compared apart from stage 1)", same && !cpu.gpuCompute && gpu.gpuCompute);
        const std::string tc = atom::benchSettingsText(cpu), tg = atom::benchSettingsText(gpu);
        checkTrue(w + "the settings text says which way each ran", tc.find("GPU superposition Off") != std::string::npos && tg.find("GPU superposition On") != std::string::npos);
        const std::string n = info(pr[0]).name, d = info(pr[0]).what;
        checkTrue(w + "the name says OFF and the scene it compares with, the description says DIFFERENCE and that it was not measured in the cloud session", n.find("GPU superposition OFF") != std::string::npos && n.find("scene " + std::to_string(pr[1] + 3)) != std::string::npos &&
                  d.find("DIFFERENCE") != std::string::npos && d.find("ot measured") != std::string::npos);
        const std::string dg = info(pr[1]).what;
        checkTrue(w + "the original scene keeps its name and now says the speed-up is expected, not measured, and points at its comparison scene", info(pr[1]).name.find("Risk 11") == 0 && dg.find("EXPECTED, not measured") != std::string::npos && dg.find("OpenGL 4.3") != std::string::npos);
    }
    // the jump scene driven by its CPU twin: benchFrame does what the original does (a jump is held half way)
    atom::AtomLab J; atom::benchSetup(J, 22, 800, 600);
    for (int f = 0; f < 600; f++) { atom::benchFrame(J, 22, f, f < 200 ? -1 : f - 200); J.tick(1.0 / 240); J.step(1.0 / 240); }
    checkTrue("scene 25 (the CPU twin of scene 11): a jump is held half way, as in scene 11", J.s.trans && J.s.trans->t > 0.3 && J.s.trans->t < 0.7 && !J.gpuCompute);
    // the sentence for the Performance card and the benchmark report
    checkStr("compute path text, 4.3 with the switch on", app::computePathText(4, 3, true, true), "OpenGL 4.3: GPU superposition on");
    checkStr("compute path text, 3.3 (no compute)", app::computePathText(3, 3, false, true), "OpenGL 3.3: CPU path");
    checkStr("compute path text, 4.6 with the switch off", app::computePathText(4, 6, true, false), "OpenGL 4.6: GPU superposition off (switched off, CPU path)");
    checkStr("compute path text, no compute even with the switch on says CPU path", app::computePathText(4, 1, false, true), "OpenGL 4.1: CPU path");
    BenchEnv env; env.version = "0.13"; env.computePath = "OpenGL 4.3: GPU superposition on";
    checkTrue("the benchmark report prints the compute path (and says unknown when it was not given)", benchResultText(env, {}).find("Atom Lab superposition and jumps: OpenGL 4.3: GPU superposition on\n") != std::string::npos && benchResultText(BenchEnv(), {}).find("Atom Lab superposition and jumps: unknown\n") != std::string::npos);
}
// ---- 0.13 stage 3: the GPU outline switch and its two comparison scenes ----
static void testBenchGpuChart() {
    // 0.13 stage 4 part B: the GPU chart cache switch and its two twin scenes (which 28, 29 = benchmark scenes 33, 34, after the Orbit Lab twins 31, 32)
    using atom::kBenchChartCpuFirst; using atom::kBenchAtomScenes;
    atom::AtomLab fresh; checkTrue("GPU chart cache: a new lab has the switch On (the default)", fresh.gpuChart);
    checkTrue("chart twins: which 28 and 29 are the last two Atom Lab scenes, after the run of scenes numbered which + 3", kBenchChartCpuFirst == 28 && kBenchAtomScenes == 30 && atom::kBenchSceneCount == 28);
    atom::AtomLab odd; odd.gpuChart = false; atom::benchSetup(odd, atom::kBenchChartScene, 800, 600);
    checkTrue("GPU chart cache: scene 19 switches it On even if the lab had it Off (no setting of the user's leaks into a scene)", odd.gpuChart);
    const int pairs[2][2] = {{28, 16}, {29, 17}};                    // scenes 33 and 34 are scenes 19 and 20 (which 16, 17)
    for (const auto& pr : pairs) {
        atom::AtomLab off, on;
        atom::benchSetup(off, pr[0], 800, 600); atom::benchSetup(on, pr[1], 800, 600);
        const std::string w = "scene " + std::to_string(pr[0] + 5) + " against scene " + std::to_string(pr[1] + 3) + ": ";
        checkTrue(w + "the same view, tilt, zoom, chosen nuclide and height; only the chart cache differs (Off in the twin), stage 1 to 3 switches On in both",
                  off.p.view == on.p.view && off.s.chartView == on.s.chartView && off.p.Z == on.p.Z && off.p.N == on.p.N && !off.gpuChart && on.gpuChart && off.gpuColours && off.gpuCompute && off.gpuOutline);
        const std::string t0 = atom::benchSettingsText(off), t1 = atom::benchSettingsText(on);
        checkTrue(w + "the settings text says which way each ran", t0.find("GPU chart cache Off") != std::string::npos && t1.find("GPU chart cache On") != std::string::npos);
        const std::string n = atom::benchSceneInfo(pr[0]).name, d = atom::benchSceneInfo(pr[0]).what;
        checkTrue(w + "the name says OFF and the scene it compares with, the description says DIFFERENCE and that it was not measured in the cloud session", n.find("GPU chart cache OFF") != std::string::npos &&
                  n.find(std::string("scene ") + std::to_string(pr[1] + 3)) != std::string::npos && d.find("DIFFERENCE") != std::string::npos && d.find("ot measured") != std::string::npos);
    }
}

static void testBenchGpuRocks() {
    // 0.13 stage 4 part A: the Orbit Lab's GPU rocks switch, its sentence and the report line
    orbit::ViewSettings fresh;
    checkTrue("GPU rocks: a new view has the switch On (the default); it is not in the saved settings (settings.cpp writes and reads named keys only, none for it)", fresh.gpuRocks);
    checkStr("rocks path text, 4.3 with the switch on", app::rocksPathText(4, 3, true, true), "OpenGL 4.3: GPU rocks on");
    checkStr("rocks path text, 3.3 (no compute)", app::rocksPathText(3, 3, false, true), "OpenGL 3.3: CPU path");
    checkStr("rocks path text, 4.3 but the shader did not compile (no double precision): the CPU path", app::rocksPathText(4, 3, false, true), "OpenGL 4.3: CPU path");
    checkStr("rocks path text, 4.6 with the switch off", app::rocksPathText(4, 6, true, false), "OpenGL 4.6: GPU rocks off (switched off, CPU path)");
    BenchEnv env; env.version = "0.13"; env.rocksPath = "OpenGL 4.3: GPU rocks on";
    checkTrue("the benchmark report prints the rocks path (and says unknown when it was not given)", benchResultText(env, {}).find("Orbit Lab rocks: OpenGL 4.3: GPU rocks on\n") != std::string::npos && benchResultText(BenchEnv(), {}).find("Orbit Lab rocks: unknown\n") != std::string::npos);
}

static void testBenchGpuOutline() {
    using atom::kBenchFirstRisk;
    const auto info = [](int which) { return atom::benchSceneInfo(which); };
    atom::AtomLab fresh; checkTrue("GPU outline: a new lab has the switch On (the default)", fresh.gpuOutline);
    atom::AtomLab odd; odd.gpuOutline = false; atom::benchSetup(odd, 13, 800, 600);
    checkTrue("GPU outline: scene 16 switches it On even if the lab had it Off (no setting of the user's leaks into a scene)", odd.gpuOutline);
    const int pairs[2][2] = {{26, 13}, {27, 14}};                  // scenes 29 and 30 (which 26, 27) are scenes 16 and 17 (which 13, 14)
    for (const auto& pr : pairs) {
        atom::AtomLab cpu, gpu;
        atom::benchSetup(cpu, pr[0], 800, 600); atom::benchSetup(gpu, pr[1], 800, 600);
        const std::string w = "scene " + std::to_string(pr[0] + 3) + " against scene " + std::to_string(pr[1] + 3) + ": ";
        const bool same = sameParams(cpu.p, gpu.p) && cpu.s.k == gpu.s.k && std::equal(cpu.s.pts.begin(), cpu.s.pts.begin() + 3 * 12000, gpu.s.pts.begin()) && cpu.gpuColours && gpu.gpuColours && cpu.gpuCompute && gpu.gpuCompute;
        checkTrue(w + "the same settings and the same dots; the CPU scene has GPU outline Off, the other On, and stages 1 and 2 are On in both (stage 3 is compared apart from the others)", same && !cpu.gpuOutline && gpu.gpuOutline);
        const std::string tc = atom::benchSettingsText(cpu), tg = atom::benchSettingsText(gpu);
        checkTrue(w + "the settings text says which way each ran", tc.find("GPU outline Off") != std::string::npos && tg.find("GPU outline On") != std::string::npos);
        const std::string n = info(pr[0]).name, d = info(pr[0]).what, dg = info(pr[1]).what;
        checkTrue(w + "the name says OFF and the scene it compares with, the description says DIFFERENCE and that it was not measured in the cloud session", n.find("GPU outline OFF") != std::string::npos && n.find("scene " + std::to_string(pr[1] + 3)) != std::string::npos &&
                  d.find("DIFFERENCE") != std::string::npos && d.find("ot measured") != std::string::npos);
        checkTrue(w + "the original scene keeps its name and now says the improvement is expected, not measured, and points at its comparison scene", info(pr[1]).name.find("Risk 12") == 0 && dg.find("EXPECTED") != std::string::npos && dg.find("not measured") != std::string::npos && dg.find("cene " + std::to_string(pr[0] + 3)) != std::string::npos);
    }
    // the CPU twin of scene 16 is driven like scene 16: the outline is switched on at the first counted frame and the cache is emptied every 120 frames
    atom::AtomLab J; atom::benchSetup(J, 26, 800, 600);
    checkTrue("scene 29: the outline is Off at the start, as in scene 16", J.p.outline == "Off" && !J.gpuOutline);
    atom::benchFrame(J, 26, 200, 0);
    checkTrue("scene 29: the first counted frame switches the outline On, as scene 16 does", J.p.outline == "On");
    checkStr("outline path text, 4.3 with the switch on", app::outlinePathText(4, 3, true, true), "OpenGL 4.3: GPU outline on");
    checkStr("outline path text, 3.3 (no compute)", app::outlinePathText(3, 3, false, true), "OpenGL 3.3: CPU path");
    checkStr("outline path text, 4.6 with the switch off", app::outlinePathText(4, 6, true, false), "OpenGL 4.6: GPU outline off (switched off, CPU path)");
    BenchEnv env; env.version = "0.13"; env.outlinePath = "OpenGL 4.3: GPU outline on";
    checkTrue("the benchmark report prints the outline path (and says unknown when it was not given)", benchResultText(env, {}).find("Atom Lab 90 % outline: OpenGL 4.3: GPU outline on\n") != std::string::npos && benchResultText(BenchEnv(), {}).find("Atom Lab 90 % outline: unknown\n") != std::string::npos);
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
    {   // 0.10.8 stage 1: the Relativity (Dirac levels) setting travels through the settings file; it is off by default and a lab that never turned it on leaves no trace in the file
        Prefs d; d.atom = {{"dirac", "On"}, {"view", "Slice"}};
        const Prefs e = loadPrefs(savePrefs(d));
        checkTrue("Relativity setting: On goes through the settings file unchanged, and only Off and On are accepted", e.atom == d.atom && atom::AtomLab::validChoice("dirac", "On") && atom::AtomLab::validChoice("dirac", "Off") && !atom::AtomLab::validChoice("dirac", "on") && !atom::AtomLab::validChoice("dirac", "1"));
        checkTrue("Check-a-jump settings (0.10.8 stage 3): the shells 1s .. 4f are accepted for the upper and the lower one, nothing else; the defaults are 2p and 1s; neither is saved to the settings file",
                  atom::AtomLab::validChoice("jfrom", "4f") && atom::AtomLab::validChoice("jto", "1s") && !atom::AtomLab::validChoice("jfrom", "5s") && !atom::AtomLab::validChoice("jto", "") && atom::AtomParams().jfrom == "2p" && atom::AtomParams().jto == "1s");
        checkTrue("Relativity setting: off in the web-like struct and in the app's defaults, and a default settings file has no 'dirac' line", atom::AtomParams().dirac == "Off" && atom::AtomParams::appDefaults().dirac == "Off" && savePrefs(Prefs()).find("dirac") == std::string::npos);
    }
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

// ---- 0.9.7: "Reset to today" keeps the zoom and the four switches (and the Time speed), and resets the date, the selection and Follow ----
static void testResetToToday() {
    orbit::SolarSystem sys;
    orbit::ViewSettings set;
    orbit::OrbitView view;
    const int64_t picked = 1060000000000LL, today = 1790000000000LL;      // a jumped-to date, and "now" (fixed numbers for the test)
    sys.loadAt(picked); sys.jumped = true;
    set.zoom = 1.7; set.moons = false; set.dwarfs = false; set.comets = true; set.belts = false; set.tspeed = 1.25; set.view3D = true; set.far = false;
    view.sel = orbit::Selection{orbit::Selection::Body, 3, 0}; view.follow = true; view.ox = 40; view.oy = -12; view.cdep = 5e9;
    const orbit::ViewSettings before = set;
    orbit::backToToday(sys, set, view, today);
    checkTrue("date: the Solar System is loaded at today's date and no longer 'jumped'", sys.epochMs == today && !sys.jumped && sys.t == 0);
    checkTrue("zoom is unchanged", set.zoom == before.zoom && set.zoom == 1.7);
    checkTrue("Moons, Dwarfs, Comets, Belts are unchanged", set.moons == false && set.dwarfs == false && set.comets == true && set.belts == false);
    checkTrue("Time speed, 2D/3D and Far-away things are unchanged", set.tspeed == before.tspeed && set.view3D == before.view3D && set.far == before.far);
    checkTrue("selection and Follow are reset", view.sel.kind == orbit::Selection::None && !view.follow);
    checkTrue("the moved view is back in the middle", view.ox == 0 && view.oy == 0 && view.cdep == 0);
    // the bodies really are at today's positions: the same as a fresh load at that date
    orbit::SolarSystem fresh; fresh.loadAt(today);
    bool same = fresh.bodies.size() == sys.bodies.size();
    for (std::size_t i = 0; same && i < fresh.bodies.size(); i++) same = fresh.bodies[i].pos.x == sys.bodies[i].pos.x && fresh.bodies[i].pos.y == sys.bodies[i].pos.y;
    checkTrue("the planets are where a fresh load at today's date puts them", same);
}

// ---- 0.9.8: the 90 % outline. Independent references: the closed-form 1s distribution, and a separate fine integration of the exact |psi|^2. ----
struct FineRegion { double frac, rmax, total; };
// The probability inside {|psi|^2 >= c} divided by the probability in all space, by midpoint integration in spherical coordinates (r, cos(theta), phi) for the
// hydrogen-like orbital with nuclear charge Z. A different integration from the one the app uses (a Cartesian grid), on a fine grid.
static void gaussLegendre(int n, std::vector<double>& x, std::vector<double>& w) {          // nodes and weights on (-1, 1), by Newton iteration on P_n
    x.assign(n, 0); w.assign(n, 0);
    for (int i = 0; i < n; i++) {
        double z = std::cos(atom::kPi * (i + 0.75) / (n + 0.5)), pp = 1;
        for (int it = 0; it < 100; it++) {
            double p1 = 1, p2 = 0;
            for (int j = 1; j <= n; j++) { const double p3 = p2; p2 = p1; p1 = ((2.0 * j - 1) * z * p2 - (j - 1.0) * p3) / j; }
            pp = n * (z * p1 - p2) / (z * z - 1);
            const double dz = p1 / pp; z -= dz;
            if (std::fabs(dz) < 1e-15) break;
        }
        x[i] = z; w[i] = 2 / ((1 - z * z) * pp * pp);
    }
}
static FineRegion fineRegion(int orb, int Z, double c, double dr = 0.1, int nu = 40, int nphi = 36) {
    const atom::Orbital& o = atom::orbitals()[orb];
    const double top = 2.0 * atom::kExt[o.n] / Z;
    const int nr = int(std::ceil(top / dr));
    const double h = top / nr;
    std::vector<double> gx, gw;
    gaussLegendre(nu, gx, gw);                                         // Gauss-Legendre in cos(theta): exact for the polynomial angular parts
    double all = 0, in = 0, rmax = 0;
    for (int ir = 0; ir < nr; ir++) {
        const double r = (ir + 0.5) * h;
        for (int iu = 0; iu < nu; iu++) {
            const double u = gx[iu], s = std::sqrt(1 - u * u);
            for (int ip = 0; ip < nphi; ip++) {
                const double ph = 2 * atom::kPi * (ip + 0.5) / nphi;
                const double p = atom::psi(orb, Z, r * s * std::cos(ph), r * s * std::sin(ph), r * u), w = p * p * r * r * gw[iu];
                all += w;
                if (p * p >= c) { in += w; rmax = std::max(rmax, r); }
            }
        }
    }
    return {in / all, rmax, all * h * (2 * atom::kPi / nphi)};     // total = the integral of |psi|^2 over all space
}
static void testOutline() {
    using atom::kA0;
    // --- 1s against the closed form: the enclosed probability is 1 - e^(-2r) (1 + 2r + 2r^2) (r in a0/Z); solve for 0.9 ---
    {
        double lo = 0, hi = 20;
        for (int i = 0; i < 200; i++) { const double m = 0.5 * (lo + hi); (1 - std::exp(-2 * m) * (1 + 2 * m + 2 * m * m) < 0.9 ? lo : hi) = m; }
        const double r90 = 0.5 * (lo + hi);
        const int orb = atom::orbIndex("1s");
        const atom::Outline& O = atom::outlineOf(orb);
        const double p0 = atom::psi(orb, 1, 0, 0, 0);
        const double rc = -0.5 * std::log(O.c / (p0 * p0));                      // |psi|^2 = |psi(0)|^2 e^(-2r): the radius where it falls to the level c
        double rs = 0; int ns = 0;
        for (const atom::OutSeg2& g : atom::outlineSlice(orb, false, 0.0)) { rs += std::hypot(g.x0, g.y0); ns++; }
        std::printf("  1s: exact 90%% radius %.4f a0 (closed form, root of 1-e^-2r(1+2r+2r^2)=0.9); outline level gives %.4f a0; the slice contour averages %.4f a0 over %d segments; grid says it holds %.4f\n", r90, rc, rs / ns, ns, O.mass);
        checkTrue("1s: the outline radius is the exact 90 % radius (within 0.03 a0, the grid's tolerance)", std::fabs(rc - r90) < 0.03 && ns > 20 && std::fabs(rs / ns - r90) < 0.03);
        checkTrue("1s: the grid's own mass inside is 0.90 +- 0.01", std::fabs(O.mass - 0.9) < 0.01);
    }
    // --- every orbital and orientation: the region holds 0.90 of the probability, by a separate fine integration ---
    double worst = 0; int count = 0;
    double slowest = 0;
    for (int orb = 0; orb < int(atom::orbitals().size()); orb++) {
        const auto t0 = std::chrono::steady_clock::now();
        const atom::Outline& O = atom::outlineOf(orb);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        slowest = std::max(slowest, ms);
        const FineRegion F = fineRegion(orb, 1, O.c);
        const double nrm = atom::orbitalNorm(orb);
        worst = std::max(worst, std::fabs(F.frac - 0.9)); count++;
        if (std::fabs(nrm / F.total - 1) > 0.002) std::printf("  note: %s norm ratio app/fine = %.5f, region fraction %.4f\n", atom::orbitals()[orb].key.c_str(), nrm / F.total, F.frac);
        checkTrue("outline " + atom::orbitals()[orb].key + ": probability inside |psi|^2 >= c is 0.90 +- 0.01 (fine integration)", std::fabs(F.frac - 0.9) < 0.01);
        checkTrue("outline " + atom::orbitals()[orb].key + ": the region stays inside the grid and has lines", O.rmax < O.half - 1 && !O.wire.empty() && !O.side.empty());
        checkTrue("outline " + atom::orbitals()[orb].key + ": the app's normalising integral of |psi|^2 agrees with the fine one within 0.5 %", std::fabs(nrm / F.total - 1) < 0.005);
    }
    {   // the cut-plane contour of the biggest orbital (what a Slice view recomputes when the cut slider moves)
        const auto t0 = std::chrono::steady_clock::now();
        const auto segs = atom::outlineSlice(atom::orbIndex("4f|xyz"), false, 3.0);
        std::printf("  4f|xyz cut-plane contour (200 x 200 points): %zu segments in %.1f ms\n", segs.size(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    std::printf("  %d orbitals: the probability inside the outline is 0.90 within %.4f at worst; the slowest outline took %.0f ms to compute (software, this machine)\n", count, worst, slowest);
    // --- the sampled dots (Ultra, fixed seeds): the fraction inside the outline is 0.90 within 4 standard errors (batch means) ---
    for (const char* shell : {"1s", "2s", "2p", "3d", "4f"}) {
        atom::AtomLab L;
        fillCloud(L, shell, "Ultra", 9001);
        const int orb = L.fullOrb();
        const atom::Outline& O = atom::outlineOf(orb);
        double mean, se;
        batchMean(L, [&](long i) { const double p = atom::psi(orb, 1, L.s.pts[3 * i] / kA0, L.s.pts[3 * i + 1] / kA0, L.s.pts[3 * i + 2] / kA0); return p * p >= O.c ? 1.0 : 0.0; }, mean, se);
        std::printf("  %s: %.4f of the 48,000 dots lie inside the outline (standard error %.4f; off by %.2f standard errors)\n", atom::orbitals()[orb].key.c_str(), mean, se, std::fabs(mean - 0.9) / se);
        checkTrue(std::string(shell) + ": the fraction of dots inside the outline is 0.90 within 4 standard errors", std::fabs(mean - 0.9) < 4 * se);
    }
    // --- the setting: default, good and bad values, the settings file, Defaults, the benchmark ---
    {
        atom::AtomParams web, app = atom::AtomParams::appDefaults();
        checkTrue("90% outline: Off by default (the struct, and what the app starts with and Defaults give)", web.outline == "Off" && app.outline == "Off");
        checkTrue("90% outline: Off and On are accepted", atom::AtomLab::validChoice("outline", "Off") && atom::AtomLab::validChoice("outline", "On"));
        checkTrue("90% outline: bad values are refused (the loader keeps the default)", !atom::AtomLab::validChoice("outline", "on") && !atom::AtomLab::validChoice("outline", "90") && !atom::AtomLab::validChoice("outline", ""));
        Prefs p; p.atom = {{"outline", "On"}, {"view", "Slice"}};
        const Prefs q = loadPrefs(savePrefs(p));
        checkTrue("90% outline: goes through the settings file unchanged", q.atom == p.atom && q.atom.at("outline") == "On");
        atom::AtomLab L; L.setParam("outline", std::string("On"));
        checkTrue("90% outline: setParam turns it on", L.p.outline == "On");
        for (int which = 0; which < 5; which++) {
            atom::AtomLab B; B.setParam("outline", std::string("On"));
            atom::benchSetup(B, which, 800, 600);
            checkTrue("benchmark scene " + std::to_string(which + 1) + ": the outline is Off", B.p.outline == "Off");
        }
    }
    // --- Z scaling: psi at Z = 2 is psi at Z = 1 with all lengths halved, so the outline is exactly half the size ---
    for (const char* key : {"1s", "2s", "2p|z", "3d|z2", "4f|xyz"}) {
        const int orb = atom::orbIndex(key);
        const atom::Outline& O = atom::outlineOf(orb);
        bool same = true;
        for (const atom::OutSeg2& g : atom::outlineSlice(orb, false, 0.0, 60)) {          // points of the Z = 1 contour, and the same points halved for Z = 2
            const double a = atom::psi(orb, 1, g.x0, 0, g.y0), b = atom::psi(orb, 2, g.x0 / 2, 0, g.y0 / 2);
            if (std::fabs(a * a - b * b) > 1e-9 * std::max(1e-30, a * a)) same = false;
        }
        const FineRegion F2 = fineRegion(orb, 2, O.c);
        const double tol = 2 * O.half / atom::kOutlineGrid + 0.2;
        std::printf("  %s: Z=1 reaches %.2f a0, Z=2 reaches %.2f a0 (half is %.2f); probability inside at Z=2: %.4f\n", key, O.rmax, F2.rmax, O.rmax / 2, F2.frac);
        checkTrue(std::string(key) + ": psi(Z=2) at the halved points equals psi(Z=1) (the contour scales exactly)", same);
        checkTrue(std::string(key) + ": at Z = 2 the same level holds 0.90 and the reach is half (within the grid tolerance)", std::fabs(F2.frac - 0.9) < 0.01 && std::fabs(2 * F2.rmax - O.rmax) < tol);
    }
}

// ---- 0.9.9: the Theme picker's grid arithmetic ----
static void testThemeGrid() {
    const struct { int count, rows, lastRow, lastCol; } cases[] = {{1, 1, 0, 0}, {3, 1, 0, 2}, {4, 2, 1, 0}, {11, 4, 3, 1}, {13, 5, 4, 0}};
    for (const auto& c : cases) {
        const std::string w = std::to_string(c.count) + " themes in 3 columns: ";
        checkTrue(w + "rows = ceil(count / 3)", app::gridRows(c.count, 3) == c.rows);
        const app::GridCell last = app::gridCell(c.count - 1, 3);
        checkTrue(w + "the last card is at row " + std::to_string(c.lastRow) + ", column " + std::to_string(c.lastCol), last.row == c.lastRow && last.col == c.lastCol);
        bool ok = true;                                                 // every card in its own place, left to right then down, inside the grid
        for (int i = 0; i < c.count; i++) {
            const app::GridCell g = app::gridCell(i, 3);
            ok = ok && g.row == i / 3 && g.col == i % 3 && g.row < app::gridRows(c.count, 3) && g.col < 3 && (i == 0 || g.row * 3 + g.col == i);
        }
        checkTrue(w + "all cards fill the grid in order", ok);
    }
    checkTrue("no cards, no rows", app::gridRows(0, 3) == 0);
    checkTrue("the app has 11 themes now (4 rows)", int(sizeof(kThemes) / sizeof(kThemes[0])) == 11 && app::gridRows(int(sizeof(kThemes) / sizeof(kThemes[0])), 3) == 4);
}

// ---- 0.10: "Planet physics: Simple / Real" ----
static void testPlanetPhysics() {
    orbit::ViewSettings vs;
    checkTrue("Planet physics: Simple by default", !vs.realPlanets && std::string(orbit::planetPhysicsText(vs.realPlanets)) == "Simple");
    bool real = false;
    checkTrue("Planet physics: 'Real' and 'Simple' are accepted", orbit::planetPhysicsFromText("Real", real) && real && orbit::planetPhysicsFromText("Simple", real) && !real);
    real = true;
    checkTrue("Planet physics: bad values are refused and leave the value alone", !orbit::planetPhysicsFromText("real", real) && !orbit::planetPhysicsFromText("", real) && !orbit::planetPhysicsFromText("Fancy", real) && real);
    Prefs p; p.orbit = {{"physics", "Real"}, {"zoom", "-2"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("Planet physics: goes through the settings file unchanged", q.orbit == p.orbit && q.orbit.at("physics") == "Real");
    // Simple gives exactly the planet positions it gave before: the elements' positions, and switching Real on and off changes nothing
    const int64_t ms = 1790000000000LL;
    orbit::SolarSystem a, b;
    a.loadAt(ms); b.loadAt(ms);
    const double jd = ms / 86400000.0 + 2440587.5;
    bool same = true;
    for (std::size_t i = 0; i < 8; i++) { const orbit::State st = orbit::planetStateAt(i, jd); same = same && a.bodies[i].pos.x == st.pos.x && a.bodies[i].pos.y == st.pos.y && a.bodies[i].pos.z == st.pos.z && a.bodies[i].vel.x == st.vel.x; }
    checkTrue("Simple: the planets start exactly at the orbital elements' state", same);
    b.setRealPlanets(true);
    bool differs = false;
    for (std::size_t i = 0; i < 8; i++) differs = differs || b.bodies[i].pos.x != a.bodies[i].pos.x;
    checkTrue("Real: the planets are somewhere else than Simple (the other planets' pulls and the start)", differs && b.realPlanets());
    b.setRealPlanets(false);
    same = true;
    for (std::size_t i = 0; i < a.bodies.size(); i++) same = same && a.bodies[i].pos.x == b.bodies[i].pos.x && a.bodies[i].vel.y == b.bodies[i].vel.y;
    checkTrue("switching Real on and back to Simple restores exactly the Simple positions", same && !b.realPlanets());
    for (int k = 0; k < 50; k++) { a.step(0.25, -0.3); b.step(0.25, -0.3); }
    same = true;
    for (std::size_t i = 0; i < a.bodies.size(); i++) same = same && a.bodies[i].pos.x == b.bodies[i].pos.x && a.bodies[i].pos.z == b.bodies[i].pos.z;
    checkTrue("Simple stepping after the switch is identical to Simple that never left", same);
    // Real at the reference epoch is the start state; stepping and jumping follow the N-body clock
    const double refMs = (orbit::kNBodyRefJD - 2440587.5) * 86400000.0;
    orbit::SolarSystem c;
    c.setRealPlanets(true);
    c.loadAt(int64_t(std::llround(refMs)));
    const orbit::NBodyState st0 = orbit::nbodyStartState();
    same = true;
    // 0.10.4 stage 2: in Real, Earth is drawn off the Earth-Moon barycentre (the N-body body) by the Moon's share, so for Earth (i == 2) the barycentre is compared (still exactly); its offset is tested in moon_real_tests
    for (std::size_t i = 0; i < 8; i++) { const orbit::Vec3 p = i == 2 ? c.embPosition() : c.bodies[i].pos; same = same && p.x == st0.x[i + 1].x - st0.x[0].x && p.y == st0.x[i + 1].y - st0.x[0].y && c.bodies[i].vel.z == st0.v[i + 1].z - st0.v[0].z; }
    checkTrue("Real at the reference epoch (2026-01-01 12:00 TDB) equals the start state, the JPL state (Sun-centred)", same);
    for (int k = 0; k < 30; k++) c.step(1.0 / 60, 0.5);                       // about a year of simulated time
    orbit::NBodyClock clock;
    orbit::Vec3 x[orbit::kNBodies], v[orbit::kNBodies];
    clock.stateAt(c.t, x, v);
    double worst = 0;
    for (std::size_t i = 0; i < 8; i++) { const orbit::Vec3 p = i == 2 ? c.embPosition() : c.bodies[i].pos; worst = std::max(worst, orbit::length(p - (x[i + 1] - x[0])) / orbit::length(p)); }   // Earth: the barycentre (0.10.4 stage 2)
    checkTrue("Real: after stepping, the planets are where the N-body clock has them", worst < 1e-12 && c.t > 1e6);
    const int64_t jump = 1000000000000LL;                                     // September 2001
    c.loadAt(jump);
    clock.stateAt((double(jump) - refMs) / 1000.0, x, v);
    worst = 0;
    for (std::size_t i = 0; i < 8; i++) { const orbit::Vec3 p = i == 2 ? c.embPosition() : c.bodies[i].pos; worst = std::max(worst, orbit::length(p - (x[i + 1] - x[0])) / orbit::length(p)); }   // Earth: the barycentre (0.10.4 stage 2)
    checkTrue("Real: a jump to another date puts the planets where the N-body clock has them for that date", worst < 1e-12 && c.realPlanets());
    for (int64_t when : {-5364662400000LL, 946728000000LL, 1790000000000LL, 4133894400000LL}) {      // 1800-01-01, 2000-01-01, Oct 2026, 2100-12-31
        orbit::SolarSystem sa, sb;
        sa.loadAt(when); sb.setRealPlanets(true); sb.loadAt(when);
        double w = 0; std::size_t which = 0;
        for (std::size_t i = 0; i < 8; i++) { const double d = orbit::length(sa.bodies[i].pos - sb.bodies[i].pos) / orbit::length(sa.bodies[i].pos); if (d > w) { w = d; which = i; } }
        std::printf("  Real vs Simple (elements) at %s: biggest planet difference %.2e of its distance (%s)\n", fmt::dateUTC(when).c_str(), w, sa.bodies[which].name.c_str());
    }
}

// ---- 0.10.1: "General relativity (1PN)" for the Real planets ----
// ---- 0.10.2 stage 1: the Atom Lab's "Phase colours" setting and the complex orientations ----
static void testPhaseSetting() {
    const atom::AtomParams web, app = atom::AtomParams::appDefaults();
    checkStr("Phase colours: Sign by default (the web's struct and the app's defaults)", web.phase + "/" + app.phase, "Sign/Sign");
    checkTrue("Phase colours: 'Sign' and 'Wheel' are accepted", atom::AtomLab::validChoice("phase", "Sign") && atom::AtomLab::validChoice("phase", "Wheel"));
    checkTrue("Phase colours: a bad value is refused (the settings loader then keeps the default)", !atom::AtomLab::validChoice("phase", "Rainbow") && !atom::AtomLab::validChoice("phase", "wheel") && !atom::AtomLab::validChoice("phase", ""));
    checkTrue("Orientation: m = +-1 is accepted for p, d and f; m = +-4 and m = 0 are not; m = +-3 not for d",
              atom::AtomLab::validChoice("pAx", "m+1") && atom::AtomLab::validChoice("dAx", "m-2") && atom::AtomLab::validChoice("fAx", "m+3") &&
              !atom::AtomLab::validChoice("fAx", "m+4") && !atom::AtomLab::validChoice("fAx", "m0") && !atom::AtomLab::validChoice("dAx", "m+3"));
    Prefs p; p.atom = {{"phase", "Wheel"}, {"pAx", "m-1"}, {"cstyle", "Glow"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("Phase colours and a complex orientation go through the settings file unchanged", q.atom == p.atom && q.atom.at("phase") == "Wheel" && q.atom.at("pAx") == "m-1");
    // Defaults puts it back: the app's Defaults assigns appDefaults() to the lab's settings
    atom::AtomLab lab;
    lab.p.phase = "Wheel"; lab.p.pAx = "m+1";
    lab.p = atom::AtomParams::appDefaults(); lab.restart();
    checkTrue("Defaults: Phase colours is Sign again and the orientation is back to the default", lab.p.phase == "Sign" && lab.p.pAx == "z" && lab.complexM() == 0 && !lab.usesWheel());
    // the lab: a complex orientation turns the wheel on by itself, the real ones do not, and Sign stays Sign
    lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string("3d"));
    checkTrue("a real orientation with Sign does not use the wheel", !lab.usesWheel() && lab.complexM() == 0);
    lab.setParam("dAx", std::string("m+2"));
    checkTrue("a complex orientation (3d, m = +2) always uses the wheel, and says so", lab.complexM() == 2 && lab.usesWheel() && lab.p.phase == "Sign" && lab.stateText() == "3d (m = +2)");
    lab.setParam("pAx", std::string("m-1"));                                   // the p setting does not matter while a d shell is shown
    checkTrue("only the shown shell's orientation counts", lab.complexM() == 2);
    lab.setParam("phase", std::string("Wheel")); lab.setParam("dAx", std::string("xy"));
    checkTrue("Wheel with a real orientation uses the wheel", lab.usesWheel() && lab.complexM() == 0 && lab.stateText() == "3dxy");
}

// ---- 0.10.2 stage 2: the Atom Lab's Superposition setting (quantum beats) ----
static void testSuperSetting() {
    const atom::AtomParams web, app = atom::AtomParams::appDefaults();
    checkStr("Superposition: Off by default, second orbital 2p, mix 50 %", web.super + "/" + app.super + "/" + web.orb2 + "/" + std::to_string(int(web.mix)), "Off/Off/2p/50");
    checkTrue("Superposition: 'On' and 'Off' accepted, anything else refused", atom::AtomLab::validChoice("super", "On") && atom::AtomLab::validChoice("super", "Off") && !atom::AtomLab::validChoice("super", "on") && !atom::AtomLab::validChoice("super", ""));
    checkTrue("Second orbital and its orientation: the same choices as the first, bad values refused",
              atom::AtomLab::validChoice("orb2", "4f") && !atom::AtomLab::validChoice("orb2", "5s") && atom::AtomLab::validChoice("fAx2", "m-3") && !atom::AtomLab::validChoice("dAx2", "m+3") && atom::AtomLab::validChoice("pAx2", "y"));
    Prefs p; p.atom = {{"super", "On"}, {"orb2", "3d"}, {"dAx2", "m+2"}, {"mix", "30"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("Superposition, second orbital, orientation and mix go through the settings file unchanged", q.atom == p.atom);
    atom::AtomLab lab; lab.nowMs = [] { return 0.0; };
    lab.setParam("mix", 150.0);
    checkTrue("Mix is held to 0 to 100 %", lab.p.mix == 100);
    lab.setParam("mix", -5.0);
    checkTrue("Mix is held to 0 to 100 % (low end)", lab.p.mix == 0);
    lab.p.super = "On"; lab.p.mix = 30; lab.p.orb2 = "3d";
    lab.p = atom::AtomParams::appDefaults(); lab.restart();
    checkTrue("Defaults: Superposition Off, 2p, 50 % again", lab.p.super == "Off" && lab.p.orb2 == "2p" && lab.p.mix == 50 && !lab.superOn());
    bool allOff = true;
    for (int which = 0; which < 5; which++) { atom::AtomLab b; b.nowMs = [] { return 0.0; }; b.p.super = "On"; atom::benchSetup(b, which, 800, 600); allOff = allOff && b.p.super == "Off" && !b.superOn(); }
    checkTrue("the benchmark scenes (all five) set Superposition Off, even if it was on", allOff);
    // the lab: the control only works in the cloud views; the beat clock runs 4 s per beat at Playback speed 5, faster with the speed; equal energies: no beats; no photon jumps
    lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string("1s")); lab.setParam("orb2", std::string("2p")); lab.setParam("super", std::string("On"));
    checkTrue("Superposition is on in a cloud view", lab.superOn() && lab.usesWheel());
    for (int i = 0; i < 960; i++) lab.step(1.0 / 240);                         // 4 s at speed 5
    const double th4 = lab.beatTheta();
    lab.setParam("speed", 10.0);
    for (int i = 0; i < 480; i++) lab.step(1.0 / 240);                         // 2 s at speed 10 = 4 more s of beat time
    const double th8 = lab.beatTheta();
    std::printf("beat clock: after 4 s at speed 5 theta = %.9f (2 pi = %.9f); after 2 more s at speed 10 theta = %.9f (4 pi = %.9f)\n", th4, 2 * 3.14159265358979, th8, 4 * 3.14159265358979);
    checkTrue("one beat takes 4 s on screen at Playback speed 5 (theta = 2 pi), and the speed scales it", std::fabs(th4 - 2 * 3.14159265358979) < 1e-9 && std::fabs(th8 - 4 * 3.14159265358979) < 1e-9);
    const std::string txt = lab.beatSlowText();
    checkTrue("the readout gives the real beat period and the slow-down (hydrogen 1s + 2p: about 406 as, slowed by 10^16)", txt.find("real beat period") != std::string::npos && txt.find("slowed by") != std::string::npos && lab.beatRealPeriodS() > 4.0e-16 && lab.beatRealPeriodS() < 4.1e-16);
    checkTrue("no photon jumps from a superposition", lab.targets(-1).empty() && lab.targets(1).empty());
    lab.setParam("orb", std::string("2s")); lab.setParam("orb2", std::string("2p"));
    for (int i = 0; i < 480; i++) lab.step(1.0 / 240);
    checkTrue("2s + 2p: same energy, no beats: theta stays 0, and the readout says so", lab.beatTheta() == 0 && lab.beatSlowText().find("no beats") != std::string::npos && lab.beatKindText().find("NO beats") != std::string::npos && lab.beatKindText().find("Lamb") != std::string::npos);
    lab.setParam("orb", std::string("1s")); lab.setParam("orb2", std::string("2s"));
    checkTrue("1s + 2s: same parity: the cloud breathes", lab.beatKindText().find("breathes") != std::string::npos);
    lab.setParam("orb2", std::string("2p")); lab.setParam("pAx2", std::string("z"));
    checkTrue("1s + 2p: opposite parity: the centre sloshes along z", lab.beatKindText().find("sloshes") != std::string::npos);
    lab.setParam("view", std::string("Orbit"));
    checkTrue("a Bohr orbit has no superposition: the setting does nothing in the Orbit view", !lab.superOn());
}

static void testRelativity() {
    orbit::ViewSettings vs;
    // 0.10.3: off by default (measured against JPL it moves every planet away from JPL; REPORT.md). Before 0.10.3 it was on, so this check changed on purpose.
    checkTrue("General relativity (1PN): off by default (it only acts in Real)", !vs.relativity);
    vs.relativity = true; vs = orbit::ViewSettings{};                         // what the Defaults button does (putBackDefaults: app.set = ViewSettings{})
    checkTrue("General relativity (1PN): the Defaults button puts it back to off", !vs.relativity);
    Prefs p; p.orbit = {{"physics", "Real"}, {"gr", "0"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("General relativity (1PN): goes through the settings file unchanged", q.orbit == p.orbit && q.orbit.at("gr") == "0");
    // a gr value already saved in an old settings file is kept (readBool in ui/settings.cpp overwrites the default only when the file has "0" or "1")
    Prefs p1; p1.orbit = {{"physics", "Real"}, {"gr", "1"}};
    const Prefs q1 = loadPrefs(savePrefs(p1));
    checkTrue("General relativity (1PN): a saved gr=1 (old settings file) comes back as 1", q1.orbit == p1.orbit && q1.orbit.at("gr") == "1");
    // the system: a new SolarSystem has it off; the app passes the setting (also off by default since 0.10.3)
    const double refMs = (orbit::kNBodyRefJD - 2440587.5) * 86400000.0;
    orbit::SolarSystem off, on;
    checkTrue("a new solar system starts with relativity off", !off.relativity());
    off.setRealPlanets(true); on.setRealPlanets(true); on.setRelativity(true);
    off.loadAt(int64_t(std::llround(refMs))); on.loadAt(int64_t(std::llround(refMs)));
    bool same = true;
    for (std::size_t i = 0; i < 8; i++) same = same && off.bodies[i].pos.x == on.bodies[i].pos.x && off.bodies[i].vel.y == on.bodies[i].vel.y;
    checkTrue("at the reference epoch GR on and off start from the same state (the clocks begin at the same start state)", same && on.relativity());
    // stepping about 100 years: the planets follow the clock with the same GR setting; GR on puts Mercury somewhere else than GR off
    for (int k = 0; k < 100; k++) { off.step(1.0 / 60, 1.3); on.step(1.0 / 60, 1.3); }
    orbit::NBodyClock cOn(orbit::kNBodyStep, true), cOff(orbit::kNBodyStep, false);
    orbit::Vec3 xa[orbit::kNBodies], va[orbit::kNBodies], xb[orbit::kNBodies], vb[orbit::kNBodies];
    cOn.stateAt(on.t, xa, va); cOff.stateAt(off.t, xb, vb);
    double wOn = 0, wOff = 0;
    for (std::size_t i = 0; i < 8; i++) {
        wOn = std::max(wOn, orbit::length(on.bodies[i].pos - (xa[i + 1] - xa[0])) / orbit::length(on.bodies[i].pos));
        wOff = std::max(wOff, orbit::length(off.bodies[i].pos - (xb[i + 1] - xb[0])) / orbit::length(off.bodies[i].pos));
    }
    checkTrue("Real with GR on follows the GR clock, and with GR off the plain clock (both to 1e-12)", wOn < 1e-12 && wOff < 1e-12 && on.t > 1e8);
    checkTrue("GR on moves Mercury away from the GR-off place", orbit::length(on.bodies[0].pos - off.bodies[0].pos) > 1e3 && off.t == on.t);
    // switching GR while Real is on: the planets go to the other clock's place for the date shown; switching off again gives the old place exactly
    const orbit::Vec3 mercuryOn = on.bodies[0].pos;
    on.setRelativity(false);
    same = true;
    for (std::size_t i = 0; i < 8; i++) same = same && on.bodies[i].pos.x == xb[i + 1].x - xb[0].x;
    checkTrue("switching GR off while Real is on moves the planets to the plain clock's state for the same date", same && !on.relativity() && on.bodies[0].pos.x != mercuryOn.x);
    on.setRelativity(true);
    checkTrue("switching GR back on gives exactly the GR state again", on.bodies[0].pos.x == mercuryOn.x && on.bodies[0].pos.y == mercuryOn.y);
    // in Simple, GR does nothing
    orbit::SolarSystem sa, sb;
    sa.loadAt(1790000000000LL); sb.loadAt(1790000000000LL); sb.setRelativity(true);
    for (int k = 0; k < 20; k++) { sa.step(0.25, -0.3); sb.step(0.25, -0.3); }
    same = true;
    for (std::size_t i = 0; i < sa.bodies.size(); i++) same = same && sa.bodies[i].pos.x == sb.bodies[i].pos.x && sa.bodies[i].vel.z == sb.bodies[i].vel.z;
    checkTrue("in Simple the GR setting changes nothing", same && !sb.realPlanets());
}

// ---- 0.10.1: the Sun's wobble round the barycentre (Real planets) ----
static void testSunWobble() {
    orbit::ViewSettings vs;
    checkTrue("Sun's wobble: 'Exaggerate' is on by default", vs.exaggerateWobble && orbit::kSunWobbleScale == 50);
    Prefs p; p.orbit = {{"physics", "Real"}, {"wobble", "0"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("Sun's wobble: the 'Exaggerate' setting goes through the settings file unchanged", q.orbit == p.orbit && q.orbit.at("wobble") == "0");
    // Simple: the barycentre is the web's (every body's mass in Earth masses, the Sun = 333,000 Earths) and there is no Sun path
    orbit::SolarSystem sm;
    sm.loadAt(1790000000000LL);
    for (int k = 0; k < 40; k++) sm.step(0.25, -0.3);
    double mt = units::kSunInEarthMasses; orbit::Vec3 web;
    for (const orbit::Body& b : sm.bodies) { web = web + b.pos * b.massEarths; mt += b.massEarths; }
    web = web * (1.0 / mt);
    const orbit::Vec3 bs = sm.barycentre();
    checkTrue("Simple: the barycentre is the web's formula, unchanged", bs.x == web.x && bs.y == web.y && bs.z == web.z && sm.sunTrail.empty());
    // Real: the barycentre is where the integrator has it: minus the Sun's barycentric position (the Sun moves against the planets)
    const double refMs = (orbit::kNBodyRefJD - 2440587.5) * 86400000.0;
    orbit::SolarSystem r;
    r.setRealPlanets(true);
    r.loadAt(int64_t(std::llround(refMs)));
    for (int k = 0; k < 90; k++) r.step(1.0 / 60, 1.3);                          // about 130 years
    orbit::NBodyClock clock;
    orbit::Vec3 x[orbit::kNBodies], v[orbit::kNBodies];
    clock.stateAt(r.t, x, v);
    const orbit::Vec3 b = r.barycentre();
    const double dev = orbit::length(b + x[0]);
    checkTrue("Real: the barycentre is minus the Sun's barycentric position from the N-body clock (to 1 mm)", dev < 1e-3 && orbit::length(b) > 1e8 && orbit::length(b) < 2.3 * units::kSunRadius);
    // independent of the clock's bookkeeping: with the centre of mass at rest at the origin, the Sun sits at -sum(GM_i Q_i) / sum(GM) from it
    double total = 0; orbit::Vec3 sq;
    for (int i = 0; i < orbit::kNBodies; i++) total += orbit::kRealGM[i];
    for (int i = 1; i < orbit::kNBodies; i++) sq = sq + r.bodies[std::size_t(i - 1)].pos * orbit::kRealGM[i];
    const double dev2 = orbit::length(b - sq * (1.0 / total));
    checkTrue("Real: the barycentre equals sum(GM_i x_i) / sum(GM) of the planets' Sun-centred positions (to 1 mm)", dev2 < 1e-3);
    // the Sun's path: a trail of its barycentric positions, spaced by kSunTrailStep, at most 1,000 points, ending near the Sun's present place
    const std::size_t n = r.sunTrail.size();
    bool spaced = n > 10, inside = true;
    for (std::size_t i = 1; i < n; i++) spaced = spaced && orbit::length(r.sunTrail[i] - r.sunTrail[i - 1]) >= orbit::kSunTrailStep;
    for (const orbit::Vec3& pt : r.sunTrail) inside = inside && orbit::length(pt) < 2.3 * units::kSunRadius;
    checkTrue("Real: the Sun's path has many points, each at least kSunTrailStep from the last, all within 2.3 Sun radii of the barycentre", spaced && inside && n <= 1000);
    checkTrue("Real: the end of the Sun's path is within one step of the Sun's present place", n > 0 && orbit::length(r.sunTrail.back() + b) < 2 * orbit::kSunTrailStep + 1e-3 * units::kSunRadius);
    r.loadAt(1790000000000LL);
    checkTrue("a jump to another date starts a new Sun's path", r.sunTrail.size() <= 1);
    for (int k = 0; k < 30; k++) r.step(1.0 / 60, 1.3);
    r.setRealPlanets(false);
    checkTrue("back to Simple: no Sun's path, and the web's barycentre again", r.sunTrail.empty() && !r.realPlanets());
}

#include "test_theme_style.inc"   // 0.14 stage 1: richer theme styles and Frutiger Aero

int main() {
    testRoundTrip();
    testBadLines();
    testThemes();
    testThemeShownName();
    testProgress();
    testPages();
    testFactsAndTabs();
    testShot();
    testFps();
    testHistoryNotes();
    testWindow();
    testBench();
    testBenchAtomScenes();
    testBenchRiskScenes();
    testBenchGpuColours();
    testBenchGpuCompute();
    testBenchGpuOutline();
    testBenchGpuRocks();
    testBenchGpuChart();
    testBenchChartScene();
    testBenchLandscapeScene();
    testBenchBindingScene();
    testBenchChainScene();
    testBenchInputLock();
    testCloud();
    testFarMasses();
    testQuarkLight();
    testResetToToday();
    testOutline();
    testThemeGrid();
    testPlanetPhysics();
    testRelativity();
    testPhaseSetting();
    testSuperSetting();
    testSunWobble();
    testLiveAndGpu();
    testNativeEdits();
    testThemeStyle();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
