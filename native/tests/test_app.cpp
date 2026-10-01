// App-wide tests: the settings file (save, load, bad lines), the themes (every colour of every theme,
// for the panels and the picture), and the Settings → Progress texts against the web's own run
// (tests/reference/make_app_reference.js runs index.html's Settings code in Node).
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "app/app_data.h"
#include "app/prefs.h"

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

int main() {
    testRoundTrip();
    testBadLines();
    testThemes();
    testProgress();
    testPages();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
