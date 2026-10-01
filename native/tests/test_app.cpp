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

#include "reference/app_reference.inc"

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
    checkStr("About page", kAboutHtml, kAboutRef);
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

int main() {
    testRoundTrip();
    testBadLines();
    testThemes();
    testProgress();
    testPages();
    testFactsAndTabs();
    testShot();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
