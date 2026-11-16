// App-wide tests: the settings file (save, load, bad lines), the themes, the texts, the benchmark and the window rules.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <csignal>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "app/app_data.h"
#include "app/prefs.h"
#include "app/shot.h"
#include "app/fps.h"
#include "app/bench.h"
#include "app/bench_scenes.h"
#include "app/grid.h"
#include "app/frame_time.h"
#include "app/gpu_timer.h"
#include "app/gpu_guard.h"
#include "orbit/time_scale.h"
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


// Orbit Lab texts (the guide, the timeline, the far-away things)
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

#include "blackhole/presets.h"
#include "test_entities.inc"   // 0.15 stage 2, C1: HTML entities
#include "test_stage2.inc"     // 0.15 stage 2, C3 and C4: power request, the benchmark's focus flag, display scale
#include "test_shell.inc"      // 0.15 stage 3: the new UI's shell (type scale, layout, centre shift, glass, keys, reading column, search)

static void testRoundTrip() {
    Prefs p;
    p.theme = "monochrome"; p.textSize = "xl"; p.font = "round"; p.motion = "on"; p.units = "imperial";
    p.atomMore = false;
    p.lab = "Atom Lab";
    p.orbit = {{"zoom", "-2.5"}, {"view3D", "1"}, {"sky", "off"}};
    p.atom = {{"Z", "92"}, {"N", "143"}, {"view", "Cloud 3D"}};
    p.blackhole = {{"preset", "m87"}, {"dist", "3.5"}, {"bend", "off"}, {"sky", "black"}};   // 0.55.0
    const Prefs q = loadPrefs(savePrefs(p));
    checkStr("theme", q.theme, p.theme);
    checkStr("textSize", q.textSize, p.textSize);
    checkStr("font", q.font, p.font);
    checkStr("motion", q.motion, p.motion);
    checkStr("units", q.units, p.units);
    checkTrue("atomMore", q.atomMore.has_value() && !*q.atomMore);
    checkStr("lab", q.lab, p.lab);
    checkTrue("orbit settings", q.orbit == p.orbit);
    checkTrue("atom settings", q.atom == p.atom);
    checkTrue("0.55.0: black hole settings", q.blackhole == p.blackhole);
    { Prefs b = p; b.lab = "Black Hole Lab"; checkStr("0.55.0: the Black Hole Lab is remembered", loadPrefs(savePrefs(b)).lab, "Black Hole Lab"); }
    // the defaults go through unchanged too
    const Prefs d = loadPrefs(savePrefs(Prefs{}));
    checkStr("default theme", d.theme, "auto");
    checkStr("default size", d.textSize, "normal");
    checkStr("default font", d.font, "auto");
    checkStr("default motion", d.motion, "auto");
    checkTrue("default atomMore unset", !d.atomMore.has_value());
}

static void testBadLines() {
    const Prefs p = loadPrefs("theme=nope\r\ntextSize=huge\nfont=comic\nmotion=fast\nunits=furlongs\nlang=et\n"
                              "atomMore=maybe\nlab=Some Lab\nno equals sign\n# a comment\n");
    checkStr("bad theme", p.theme, "auto");
    checkStr("bad size", p.textSize, "normal");
    checkStr("bad font", p.font, "auto");
    checkStr("bad motion", p.motion, "auto");
    checkStr("bad units", p.units, "metric");
    checkStr("language not ready", p.lang, "en");
    checkTrue("bad atomMore", !p.atomMore.has_value());
    checkStr("bad lab", p.lab, "Orbit Lab");
    // 0.52.0: Monochrome is an ordinary theme (it was secret until 0.51.3): a file that names it opens it, with or without an old unlock line
    checkStr("Monochrome is an ordinary theme", loadPrefs("theme=monochrome\n").theme, "monochrome");
    // a value with a line break is written on one line
    Prefs n; n.orbit["x"] = "a\nb";
    checkStr("no line break inside a value", loadPrefs(savePrefs(n)).orbit["x"], "ab");
}

// 0.52.0: the achievements, the secret themes and their progress were removed. A settings file written by 0.51 with those lines still loads: the old lines are
// skipped, every other line around them is read, and the file written back has none of them.
static void testOldProgressLines() {
    const std::string old = "# Physics Lab settings. Written by the app; safe to delete.\ntheme=monochrome\ntextSize=large\nunlock=monochrome\n"
                            "ach.monochrome=0\nach.decay=1790000000000\nach.galactic=1789000000000\nunits=imperial\nlab=Atom Lab\natom.Z=92\n";
    const Prefs p = loadPrefs(old);
    checkStr("0.51 file with achievements: theme kept", p.theme, "monochrome");
    checkStr("0.51 file with achievements: the line after the unlock line is read", p.textSize, "large");
    checkStr("0.51 file with achievements: the line after the ach. lines is read", p.units, "imperial");
    checkStr("0.51 file with achievements: lab", p.lab, "Atom Lab");
    checkTrue("0.51 file with achievements: lab settings", p.atom.count("Z") && p.atom.at("Z") == "92");
    const std::string back = savePrefs(p);
    checkTrue("0.51 file with achievements: written back without unlock or ach. lines", back.find("unlock=") == std::string::npos && back.find("ach.") == std::string::npos);
    checkTrue("0.51 file with achievements: the rest is written back the same", savePrefs(loadPrefs(back)) == back);
}

// 0.19: the Defense Lab was removed; a settings file written by 0.18 must still load (removed lab -> the default lab,
// unknown lines ignored, every other setting kept) and the lab indexes are Orbit 0, Atom 1, Molecule 2
static void testSettingsFrom018() {
    const Prefs p = loadPrefs("theme=paper\nfont=serif\nlab=Defense Lab\ndefense=v2 seed=123 zoom=2\n"
                              "molecule=v1 mol=H2O style=ball\nshellpanels=defense.card=c,orbit.legend=o\norbit.zoom=-2.5\natom.Z=92\n");
    checkStr("0.18 file: the removed Defense Lab falls back to the default lab", p.lab, "Orbit Lab");
    checkStr("0.18 file: theme kept", p.theme, "paper");
    checkStr("0.18 file: font kept", p.font, "serif");
    checkStr("0.18 file: molecule line kept", p.molecule, "v1 mol=H2O style=ball");
    checkStr("0.18 file: panels kept", p.shellPanels, "defense.card=c,orbit.legend=o");
    checkTrue("0.18 file: lab settings kept", p.orbit.at("zoom") == "-2.5" && p.atom.at("Z") == "92");
    checkStr("0.18 file: lab index 3 (old Molecule index) is not a lab name", loadPrefs("lab=3\n").lab, "Orbit Lab");
    checkStr("0.18 file: Molecule Lab kept", loadPrefs("lab=Molecule Lab\n").lab, "Molecule Lab");
    checkTrue("lab indexes: Orbit 0, Atom 1, Molecule 2, Black Hole 3 (0.55.0)", labIndex("Orbit Lab") == 0 && labIndex("Atom Lab") == 1 && labIndex("Molecule Lab") == 2 && labIndex("Black Hole Lab") == 3);
    checkTrue("lab index of a removed or unknown lab is the default lab", labIndex("Defense Lab") == 0 && labIndex("3") == 0 && labIndex("") == 0 && labIndex("Black Hole lab") == 0);
    checkTrue("lab names round trip", std::string(labName(0)) == "Orbit Lab" && std::string(labName(1)) == "Atom Lab" && std::string(labName(2)) == "Molecule Lab" && std::string(labName(3)) == "Black Hole Lab" && std::string(labName(4)) == "Orbit Lab" && std::string(labName(-1)) == "Orbit Lab");
    checkTrue("lab count", kLabCount == 4);
    checkTrue("0.55.0: lab keys (settings and panel prefixes)", std::string(labKey(0)) == "orbit" && std::string(labKey(1)) == "atom" && std::string(labKey(2)) == "molecule" && std::string(labKey(3)) == "blackhole" && std::string(labKey(9)) == "orbit");
    // 0.55.0: a 0.54 file (no Black Hole lines) loads as before and saves byte for byte; a newer file read by an older app: an unknown lab name opens the
    // Orbit Lab (as the Defense Lab above) and unknown lines are ignored, which this app does too for a lab it does not know
    { const std::string t = "# Physics Lab settings. Written by the app; safe to delete.\ntheme=dark\ntextSize=normal\nfont=auto\nmotion=auto\nunits=metric\nlang=en\nlab=Molecule Lab\nmolecule=v1 mol=H2O\norbit.zoom=-2.5\natom.Z=92\n";
      const Prefs o = loadPrefs(t); checkTrue("0.54 file: loads and saves the same, no black hole lines", o.blackhole.empty() && savePrefs(o) == t); }
    { const Prefs n = loadPrefs("lab=Wormhole Lab\nwormhole.size=3\nblackhole.dist=7\n"); checkTrue("a lab this app does not know opens the Orbit Lab; its lines are ignored", n.lab == "Orbit Lab" && n.blackhole.at("dist") == "7"); }
    // 0.20: the builder's line: kept as read; a file without it saves without it, byte for byte (older settings files unchanged)
    { const std::string t = savePrefs(loadPrefs("molbuild=v1 mode=build atoms=C:0:0 bonds=\nlab=Molecule Lab\n")); checkTrue("0.24: an old molbuild line (the builder was removed) is read without harm and dropped", t.find("molbuild") == std::string::npos && t.find("lab=Molecule Lab") != std::string::npos); }
    { const std::string t = savePrefs(loadPrefs("lab=Molecule Lab\nmolecule=v1 mol=H2O\n")); checkTrue("0.20: no molbuild line unless one was set", t.find("molbuild") == std::string::npos && savePrefs(loadPrefs(t)) == t); }
}


// 0.19 audit: the settings file fuzzed (random lines of known and unknown keys, random values and raw bytes): never a crash, and what is read is written back
// the same way (save(load(save(load(x)))) == save(load(x)))
static void testPrefsFuzz() {
    uint64_t st = 0x2545F4914F6CDD1Dull;
    const auto next = [&st] { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; };
    const char* keys[] = {"theme", "textSize", "font", "motion", "shellpanels", "molecule", "facts", "recent", "units", "lang", "unlock", "ach.decay", "ach.monochrome", "ach.nope",
                          "atomMore", "window", "lab", "orbit.zoom", "atom.Z", "defense", "", "#", "orbit.", "atom.", "x=y"};
    const char* vals[] = {"dark", "monochrome", "xl", "on", "1", "0", "", "=", "Atom Lab", "Molecule Lab", "Defense Lab", "1280,800,-,-,0", "1,1,0,0,1", "99999999999,1,1,1,1",
                          "-1e400", "nan", "monochrome,synthwave", "v1 mol=H2O", "é", "\r", "a,b,c,d,e", "1792320474943", "-9223372036854775809"};
    int bad = 0;
    for (int i = 0; i < 20000; i++) {
        std::string t;
        for (int k = static_cast<int>(next() % 14); k > 0; k--) {
            if (next() % 10 == 0) { for (int b = static_cast<int>(next() % 10); b > 0; b--) t += static_cast<char>(next() % 256); t += "\n"; continue; }   // raw bytes
            t += std::string(keys[next() % (sizeof keys / sizeof *keys)]) + "=" + vals[next() % (sizeof vals / sizeof *vals)] + (next() % 4 ? "\n" : "\r\n");
        }
        const std::string once = savePrefs(loadPrefs(t)), twice = savePrefs(loadPrefs(once));
        if (once != twice && bad++ < 3) {
            std::size_t at = 0;
            while (at < once.size() && at < twice.size() && once[at] == twice[at]) at++;
            const std::size_t ls = once.rfind('\n', at) == std::string::npos ? 0 : once.rfind('\n', at) + 1;
            std::printf("  prefs fuzz: a file does not read back the same, first at: [%s] / [%s]\n", once.substr(ls, 60).c_str(), twice.substr(ls, 60).c_str());
        }
        if (labIndex(loadPrefs(t).lab) < 0 || labIndex(loadPrefs(t).lab) >= kLabCount) bad++;
    }
    checkTrue("settings fuzz: 20,000 random settings files load and read back the same (" + std::to_string(bad) + " wrong)", bad == 0);
}


// 0.19 audit: the settings file is written to a side file first and then renamed over the old one, and a failed write says so (the app then keeps the old
// file and tries again later). Before, a full disk (here a file-size limit) or a crash in the middle left a cut file, and the app believed it was saved.
static void testSettingsWrite() {
    const std::string dir = "settings_write_test";
    std::remove((dir + "/s.txt").c_str()); std::remove((dir + "/s.txt.tmp").c_str());
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
    const std::string path = dir + "/s.txt";
    checkTrue("settings write: a new file", writeSettingsFile(path, "theme=paper\n"));
    checkTrue("settings write: an old file replaced", writeSettingsFile(path, "theme=dark\nfont=serif\n"));
    std::string got;
    if (FILE* f = std::fopen(path.c_str(), "rb")) { char b[256]; std::size_t n; while ((n = std::fread(b, 1, sizeof b, f)) > 0) got.append(b, n); std::fclose(f); }
    checkStr("settings write: the file holds exactly the new text", got, "theme=dark\nfont=serif\n");
    FILE* tmp = std::fopen((path + ".tmp").c_str(), "rb");
    checkTrue("settings write: no side file left behind", tmp == nullptr);
    if (tmp) std::fclose(tmp);
#ifndef _WIN32
    {   // a full disk, made with a file-size limit of 1,000 bytes (the write then fails with EFBIG; SIGXFSZ ignored): reported, and the old file is kept whole
        struct rlimit old {};
        getrlimit(RLIMIT_FSIZE, &old);
        void (*prev)(int) = std::signal(SIGXFSZ, SIG_IGN);
        struct rlimit small = old;
        small.rlim_cur = 1000;
        setrlimit(RLIMIT_FSIZE, &small);
        const bool ok = writeSettingsFile(path, std::string(100000, 'x'));
        setrlimit(RLIMIT_FSIZE, &old);
        std::signal(SIGXFSZ, prev);
        checkTrue("settings write: a full disk is reported as a failure", !ok);
        std::string kept;
        if (FILE* f = std::fopen(path.c_str(), "rb")) { char b[256]; std::size_t n; while ((n = std::fread(b, 1, sizeof b, f)) > 0) kept.append(b, n); std::fclose(f); }
        checkStr("settings write: after a failed write the old file is still whole", kept, "theme=dark\nfont=serif\n");
    }
#endif
    checkTrue("settings write: a folder that does not exist is a failure", !writeSettingsFile(dir + "/no/such/folder/s.txt", "x"));
}

static bool isHex(const std::string& s) {
    if (s.size() != 4 && s.size() != 7) return false;
    if (s[0] != '#') return false;
    for (std::size_t i = 1; i < s.size(); i++) if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

static void testThemes() {
    const char* ids[] = {"auto", "light", "dark", "black", "midnight", "solar", "paper", "sage", "contrast", "monochrome"};
    checkTrue("10 themes", count(kThemes) == 10);
    for (std::size_t i = 0; i < count(kThemes) && i < 10; i++) {
        const ThemeDef& t = kThemes[i];
        checkStr("theme order", t.id, ids[i]);
        checkTrue(std::string(t.id) + " has a name", t.name[0] && t.sub[0]);
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
    // 0.52.0: no theme is secret: every theme in the list can be used, an archived or unknown one cannot
    Prefs p;
    checkTrue("every theme in the list is usable", std::all_of(std::begin(kThemes), std::end(kThemes), [&p](const ThemeDef& t) { return themeUsable(p, t.id); }));
    checkTrue("an archived or unknown theme is not usable", !themeUsable(p, "synthwave") && !themeUsable(p, "rainbow"));
}

static void testTextSizes() {
    checkTrue("text sizes", textSizeFactor("small") == 0.9 && textSizeFactor("xl") == 1.3 && textSizeFactor("bad") == 1);
}

static void testPages() {
    checkTrue("About names the four labs", std::string(kAboutHtml).find("Labs: Orbit Lab · Atom Lab · Molecule Lab · Black Hole Lab.") != std::string::npos);
    {   // 0.55.0: About's Black Hole Lab sources: every paper with its public address, and the presets' numbers as presets.h has them
        const std::string a = kAboutHtml;
        bool links = true;
        for (const char* u : {"arxiv.org/abs/1502.03808", "1972ApJ...178..347B", "1979A%26A....75..228L", "arxiv.org/abs/1910.12881", "arxiv.org/abs/1910.12873", "arxiv.org/abs/2603.05810",
                              "arxiv.org/abs/2105.07101", "arxiv.org/abs/1511.06025", "arxiv.org/abs/2112.07478", "arxiv.org/abs/2311.08680", "arxiv.org/abs/1906.11238"}) links = links && a.find(u) != std::string::npos;
        checkTrue("0.55.0: About lists the Black Hole Lab's eleven papers with their arXiv or ADS address", links);
        const blackhole::Preset* sg = blackhole::preset("sgra");
        const blackhole::Preset* m8 = blackhole::preset("m87");
        checkTrue("0.55.0: About's preset numbers are presets.h's", sg && m8 && sg->massSun == 4.297e6 && sg->distPc == 8277 && sg->ringMuas == 51.8 && sg->ringErr == 2.3 && m8->massSun == 6.5e9 && m8->distPc == 16.8e6 && m8->ringMuas == 42 && m8->ringErr == 3 &&
                  a.find("4.297 million Suns, 8,277 parsecs") != std::string::npos && a.find("51.8 &plusmn; 2.3") != std::string::npos && a.find("6.5 billion Suns, 16.8 million parsecs") != std::string::npos && a.find("42 &plusmn; 3") != std::string::npos);
    }
    checkTrue("the header buttons have a label", *kHdrTheme.label && *kHdrAbout.label && *kHdrSettings.label && *kHdrHistory.label);
}

static void testFactsAndTabs() {
    // 💡 the facts
    checkTrue("25 facts (0.52.0: the 26th, a gift line with no source, was removed)", count(kDyk) == 25);
    bool sourced = true;                                             // every fact has its source (0.40); none is a gift without one any more
    for (std::size_t i = 0; i < count(kDyk); i++) sourced = sourced && *kDykSrc[i].label && *kDykSrc[i].about;
    checkTrue("every fact has a source", sourced);
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
    // the file name: the lab's name, then the local date and time
    const ShotTime t{2026, 10, 1, 9, 5, 7};
    const std::string stamp = "-2026-10-01-090507.png";
    checkStr("orbit file name text", shotFileName("Orbit Lab", 1, 0, t), "orbit-lab-2026-10-01-090507.png");
    checkStr("atom file name", shotFileName("Atom Lab", 6, 6, t), "atom-lab-Z6-N6" + stamp);
    checkStr("other lab name", shotFileName("Some other-lab 2", 0, 0, t), "some-other-lab-2" + stamp);
    checkStr("0.55.0: the Black Hole Lab's file name", shotFileName("Black Hole Lab", 0, 0, t), "black-hole-lab" + stamp);
    checkStr("atom file name with Z and N", shotFileName("Atom Lab", 92, 146, {2027, 12, 31, 23, 59, 59}), "atom-lab-Z92-N146-2027-12-31-235959.png");
    checkTrue("two saves a second apart differ", shotFileName("Orbit Lab", 1, 0, t) != shotFileName("Orbit Lab", 1, 0, {2026, 10, 1, 9, 5, 8}));
    checkTrue("no character Windows refuses", shotFileName("Atom Lab", 6, 6, t).find_first_of("<>:\"/\\|?*") == std::string::npos);
    checkStr("path on Windows", joinPath("C:\\Users\\User\\Pictures", "a.png"), "C:\\Users\\User\\Pictures\\a.png");
    checkStr("path with a trailing slash", joinPath("C:\\Users\\User\\Pictures\\", "a.png"), "C:\\Users\\User\\Pictures\\a.png");
    checkStr("path on Linux", joinPath("/home/user/Pictures", "a.png"), "/home/user/Pictures/a.png");
    checkStr("path with trailing /", joinPath("/home/user/Pictures/", "a.png"), "/home/user/Pictures/a.png");

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


static void testAppTexts() {
    // About: what it must contain
    checkTrue("About lists the calculator's CODATA 2022 paper, the Voyagers' Horizons table, and NASA's Voyager 1 page (0.49)",
              std::string(kAboutHtml).find("arXiv:2409.03787") != std::string::npos && std::string(kAboutHtml).find("COMMAND -31 and -32") != std::string::npos &&
              std::string(kAboutHtml).find("passed Pioneer 10") != std::string::npos &&
              std::string(kAboutHtml).find("8.6.2") == std::string::npos);
    checkTrue("About starts with its intro card", std::string(kAboutHtml).rfind("<div class=\"lrnCard\">", 0) == 0 && std::string(kAboutHtml).find("Where the facts come from") != std::string::npos);
    checkTrue("About lists the IAEA table of nuclear charge radii, its credit and the limits (0.10.9 stage 1)", std::string(kAboutHtml).find("www-nds.iaea.org/radii/charge_radii.csv") != std::string::npos && std::string(kAboutHtml).find("Angeli and K. P. Marinova") != std::string::npos &&
              std::string(kAboutHtml).find("not the matter radius") != std::string::npos && std::string(kAboutHtml).find("is an assumption") != std::string::npos);
    checkTrue("About lists the IAEA quadrupole table, what its second column is, the model assumption and the limits (0.10.9 stage 2)", std::string(kAboutHtml).find("elec_mom_recomm.csv") != std::string::npos && std::string(kAboutHtml).find("is the mass number") != std::string::npos &&
              std::string(kAboutHtml).find("a model assumption, not a measurement") != std::string::npos && std::string(kAboutHtml).find("zero for spin 0 and 1/2 whatever the shape") != std::string::npos);
    // 0.52.0: the libraries built into the .exe are credited beside stb_image: name, the version CMakeLists.txt pins, the licence and the link
    checkTrue("About credits SDL3 (version, zlib licence, link) and Dear ImGui (version, MIT licence, link) beside stb_image",
              std::string(kAboutHtml).find("stb_image v2.30") != std::string::npos &&
              std::string(kAboutHtml).find("SDL3 3.4.16") != std::string::npos && std::string(kAboutHtml).find("zlib licence") != std::string::npos &&
              std::string(kAboutHtml).find("https://github.com/libsdl-org/SDL") != std::string::npos &&
              std::string(kAboutHtml).find("Dear ImGui 1.92.9b") != std::string::npos && std::string(kAboutHtml).find("https://github.com/ocornut/imgui") != std::string::npos &&
              std::string(kAboutHtml).find("Dear ImGui 1.92.9b (Omar Cornut)</a> (the panels, buttons and text; MIT licence") != std::string::npos);
    checkTrue("About still lists the sources of both labs", std::string(kAboutHtml).find("NASA/JPL: planet positions") != std::string::npos && std::string(kAboutHtml).find("NUBASE") != std::string::npos);
    // the far-away things and the Solar System timeline
    using nativeorbit::kFar;
    checkTrue("19 far-away things", count(kFar) == 19);
    using nativeui::kTimeline;
    checkTrue("timeline length", count(kTimeline) == 29);
    for (std::size_t i = 0; i < count(kTimeline); i++) {
        const std::string n = "timeline " + std::to_string(i) + " ";
        checkTrue(n + "has a picture", kTimeline[i].svg[0] == '<');
        checkTrue(n + "a button has an action and the other way round", (*kTimeline[i].button != 0) == (*kTimeline[i].lab != 0));
    }
    checkTrue("About lists NUBASE2020 (file, size, citation as the file gives it), that the citation page could not be read, the colour rule as a convention, and that the magic numbers were not looked up (0.11 stage 2)", [&] { const std::string a = kAboutHtml; for (const char* w : {"nubase_4.mas20.txt", "761,906 bytes", "Chin. Phys. C45, 030001 (2021)", "could not be read", "a convention of this lab", "3,054 of the 3,083", "not looked up; 126 is a neutron number only"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About says the landscape heights are a chosen scale, not a measurement: the log of the half-life, the two ends, the plateau for stable nuclides, no height for unknown ones, the isometric angles written from knowledge (0.11 stage 3)", [&] { const std::string a = kAboutHtml; for (const char* w : {"a chosen scale, not a measurement", "10<sup>&minus;23</sup> s", "10<sup>32</sup> s", "12 tiles tall", "1.1 scale heights", "has no height at all", "54.74 degrees"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the 0.12 stage 2 clean-up: the mendeleev package read, what changed (113 rounded, 16 stale, Kr-78 and Xe-124 removed because no source), no time for extrapolated half-lives, the odds left out, and (0.46.3) the unsourced magic numbers 114 and 184 gone with the estimate, replaced by FRDM(2012)", [&] { const std::string a = kAboutHtml; for (const char* w : {"Accuracy clean-up of the half-lives", "version 1.3.0 from PyPI", "2,774 of 2,799", "seaborgium-269: 14 minutes before, 5 minutes now", "no source for those numbers is in this repository", "seen only fleetingly", "369 extrapolated nuclides and 11 limits", "the 644 nuclides", "unsourced magic numbers 114 and 184", "FRDM(2012)"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the phase colours on the graphics card (0.13 stage 1): a port of the C++ function, glslang, llvmpipe", [&] { const std::string a = kAboutHtml; for (const char* w : {"Phase colours on the graphics card (0.13 stage 1)", "direct port of the C++ function", "glslang (the Khronos reference compiler)", "llvmpipe"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists superposition and jumps on the graphics card (0.13 stage 2): the textbooks and CODATA 2022 as already used, the textbook check, the 32-bit edge dots, the normalisation found", [&] { const std::string a = kAboutHtml; for (const char* w : {"Superposition and photon jumps on the graphics card (0.13 stage 2)", "OpenGL 4.3", "1836.152673426", "a dot within rounding of a threshold", "too big by 3e-6"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the Orbit Lab rocks and the chart on the graphics card (0.13 stage 4): the JPL page formulas, the double precision rule, the 3-step solver finding, the chart rebuild rule", [&] { const std::string a = kAboutHtml; for (const char* w : {"The Orbit Lab&rsquo;s rocks and the chart of nuclides on the graphics card (0.13 stage 4", "Keplerian Elements for Approximate Positions of the Major Planets", "double precision on the graphics card", "exactly three Newton steps", "0.45 radian off at e = 0.99", "only when the picture size, the theme colours or the height meaning changes"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the 90 % outline on the graphics card and the exact normalisation (0.13 stage 3): the closed form and its sources, the checks, the 1e-10", [&] { const std::string a = kAboutHtml; for (const char* w : {"The 90 % outline on the graphics card, and the exact normalisation of the orbitals (0.13 stage 3)", "(n+l)!/(n&minus;l&minus;1)!", "generalised Laguerre polynomials", "within 1e-10", "chapter references were not opened"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the decay chains: the four series typed from textbooks and NOT from a source that was read, the checks that need no table (the mass-number rule, the exponential law, binomial statistics), the normalised odds, the stand-in for odds not measured, no time for a limit or an extrapolation, and that the time shown is not real time (0.12 stage 1)", [&] { const std::string a = kAboutHtml; for (const char* w : {"Decay chains on the chart (0.12 stage 1)", "typed into the tests from standard textbook tables, <b>not</b> from a source read in the session", "A = A<sub>0</sub> mod 4", "the median lifetime is the half-life", "only as a marked stand-in, never as data", "is never given a time", "is not real time", "about 1.6 seconds apart"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists AME2020 (file, size, date, citation as the file gives it), the # estimates drawn flat, the chosen linear scale, the peak read from the table, that no source for the island could be read, that nothing predicted is drawn or quoted, and that the wording follows the brief (0.11 stage 4)", [&] { const std::string a = kAboutHtml; for (const char* w : {"mass.mas20.txt", "476,239 bytes", "Chin. Phys. C45, 030002 (2021)", "1,008 of the 3,557 nuclides", "drawn flat and pale", "a chosen linear scale, 0 to 9,000 keV per nucleon", "nickel-62, 8,794.56 keV", "no source for it could be read", "draws no predicted region and quotes no predicted number", "that wording follows the project owner&rsquo;s brief"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the WCAG 2.x contrast formula and thresholds, that the W3C page was not reachable (0.14 stage 1; its Segoe UI sentence went with Frutiger Aero, 0.15 stage 5m, on purpose)", [&] { const std::string a = kAboutHtml; for (const char* w : {"www.w3.org/TR/WCAG21", "0.2126 R + 0.7152 G + 0.0722 B", "0.04045", "at least 4.5", "could not be opened"}) if (a.find(w) == std::string::npos) return false; return true; }());
    checkTrue("About lists the Light accent fix (0.14 stage 2; the Michroma font and the no-artwork sentence went with the Y2K and Aero themes, 0.15 stage 5m, on purpose)", [&] { const std::string a = kAboutHtml; for (const char* w : {"#2d6ae3", "4.24:1"}) if (a.find(w) == std::string::npos) return false; return true; }());
}

// ---- the frames-per-second counter text ----
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
    // 0.16 stage 3: frames slower than 16.7 ms (below 60 fps), so a one-time hitch (1) can be told from a repeating one (many)
    checkTrue("slow frames: only those over 16.7 ms count (16.7 itself does not)", benchStats({10, 16.7, 16.8, 55.3, 5}).slowFrames == 2 && benchStats({}).slowFrames == 0 && b.slowFrames == 2 && a.slowFrames == 0);

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
    checkTrue("result text: the frames slower than 16.7 ms, for every scene (0.16 stage 3)", t.find("  Worst frame: 20.00 ms\n  Frames slower than 16.7 ms: 2 of 2\n") != std::string::npos);
    const char* want[] = {"Physics Lab benchmark", "Version: 0.9", "Date: 2026-10-01 15:30:45", "GPU: Test GPU 9000", "OpenGL: 3.3.0 Test", "Driver / vendor: Test Vendor",
                          "Window: 1400 x 860 pixels, full screen", "Picture area: 1040 x 713 pixels", "Display refresh rate: 144 Hz", "Text size: xl, theme: paper",
                          "VSync: off during the test. Each scene: 2 s warm-up (not counted), then 8 s counted.", "GPU timer (GL_TIME_ELAPSED): available", "GPU timer check: reliable, self-check with a known load: timer 3.10 ms, clock 3.20 ms"};
    std::size_t at = 0;
    bool inOrder = true;
    for (const char* w : want) { const std::size_t p = t.find(w, at); if (p == std::string::npos) { inOrder = false; checkTrue(std::string("result text has: ") + w, false); break; } at = p; }
    checkTrue("result text: no Theme effects line any more (the setting was archived with the themes, 0.15 stage 5m; deleted on purpose)", t.find("Theme effects") == std::string::npos && t.find("Text size: xl, theme: paper\n") != std::string::npos && t.find("New UI") == std::string::npos);
    checkTrue("the result text starts with the version, date and graphics, in order", inOrder && t.rfind("Physics Lab benchmark\n", 0) == 0);
    checkTrue("result text: scene 1 numbers", t.find("  Average: 10.00 ms per frame = 100.0 fps\n  1 % low: 100.0 fps") != std::string::npos && t.find("  Worst frame: 10.00 ms\n  Frames slower than 16.7 ms: 0 of 4\n  CPU work per frame: 3.50 ms\n  GPU time per frame: 6.25 ms") != std::string::npos);
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
    L.setParam("qstyle", std::string("Simple")); L.setParam("detail", std::string("Valence")); L.setParam("style", std::string("Density"));   // 0.43: Classic -> Simple; Hard balls is gone, Density is the other non-default
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
            checkTrue(w + "quark style, detail and nucleons", odd.p.qstyle == "Accurate" && odd.p.detail == "+ Sea" && odd.p.style == "Nucleons");   // 0.43: scene 1 was Cinematic (gone), Fuzzy is Nucleons
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
        if (nucleus) checkTrue(w + "every nucleon is there to draw, the whole nucleus in view", int(L.s.nuc.size()) == 294 && L.s.focus == -1 && L.p.qstyle == "Accurate" && L.p.detail == "+ Sea");   // 0.43: was Cinematic (gone)
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
    checkTrue("GPU colours: the names of scenes 23 and 24 say OFF, Wheel, 3D / 2D and the scene they compare with; the descriptions say the DIFFERENCE",
              n23.find("GPU colours OFF") != std::string::npos && n23.find("Wheel") != std::string::npos && n23.find("3D") != std::string::npos && n23.find("scene 8") != std::string::npos && w23.find("DIFFERENCE") != std::string::npos && w23.find("scene 8") != std::string::npos &&
              n24.find("GPU colours OFF") != std::string::npos && n24.find("Wheel") != std::string::npos && n24.find("2D") != std::string::npos && n24.find("scene 10") != std::string::npos && w24.find("DIFFERENCE") != std::string::npos && w24.find("scene 10") != std::string::npos);
    const std::string w8 = info(5).what, w10 = info(7).what, n8 = info(5).name, n10 = info(7).name;
    checkTrue("GPU colours: scenes 8 and 10 keep their names (so old reports stay comparable), and their descriptions point at scenes 23 and 24",
              n8 == "Risk 10: phase colours (Wheel), Classic dots, Cloud 3D, 48,000 dots" && n10 == "Risk 10: phase colours (Wheel), Classic dots, Cloud 2D (side view), 48,000 dots" && w8.find("Scene 23") != std::string::npos && w10.find("Scene 24") != std::string::npos);
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
// ---- 0.16 stage 4: the trimmed benchmark list (11 scenes, 7 old-CPU twins behind a box): every "scene N" in the texts exists, each twin names its original ----
// ---- 0.55.5: the Black Hole Lab's own lines in the benchmark report (frame thread, worker stages, behind; glare and fine table at rest) ----
static void testBenchBlackHole() {
    BenchBlackHole h;
    h.move[0].frames = 4; h.move[0].frameSum = 4.0; h.move[0].frameWorst = 1.5; h.move[0].behindFrames = 3; h.move[0].behindWorst = 120;
    h.move[0].pictures = 30; h.move[0].seconds = 1.0;
    h.move[0].stars.add(20); h.move[0].stars.add(30); h.move[0].look.add(10);
    h.move[1].frames = 2; h.move[1].frameSum = 3.0; h.move[1].frameWorst = 2.0; h.move[1].coarse.add(100);
    h.move[3].frames = 1; h.move[3].frameSum = 0.5; h.move[3].frameWorst = 0.5; h.move[3].pictures = 5; h.move[3].seconds = 0.5;
    h.glare.add(150); h.glareWaitWorst = 400; h.fine.add(2500); h.fineStartMs = 700;
    h.move[1].sharpMs = 450; h.move[1].restS = 1.4; h.move[0].sharpMs = 0;
    h.card.add(40); h.card.add(60); h.path = "the graphics card (start-up check passed)";
    const std::vector<std::string> v = benchBlackHoleLines(h);
    const auto has = [&](const std::string& w) { for (const std::string& l : v) if (l.find(w) != std::string::npos) return true; return false; };
    checkTrue("black hole lines: turning's frame thread average and worst, behind 3 of 4 frames, at most 120 ms, 30 new pictures in 1 s",
              has("Black Hole Lab, spin 0.9, turning round the axis: frame thread 1.00 ms average, 1.50 ms worst; picture behind the settings in 3 of 4 frames, at most 120 ms; 30 new pictures in 1.0 s (30 a second)"));
    checkTrue("black hole lines: the stages' averages, worsts and counts; a stage not made says so",
              has("worker: coarse table not needed; star bins not needed; stars' images 25.0 ms average, 30.0 ms worst (2x); exposure 10.0 ms average, 10.0 ms worst (1x)"));
    checkTrue("black hole lines: tilting's coarse table; distance with no frames says so", has("coarse table 100.0 ms average, 100.0 ms worst (1x)") && has("Black Hole Lab, spin 0.9, changing distance: no frames counted"));
    checkTrue("black hole lines (0.55.6): the same movements at spin 0", has("Black Hole Lab, spin 0, turning round the axis: frame thread 0.50 ms average") && has("5 new pictures in 0.5 s (10 a second)") && has("Black Hole Lab, spin 0, tilting: no frames counted"));
    checkTrue("black hole lines (0.55.6): which path filled the spin table, why, and the card's fill time",
              has("Black Hole Lab, the spinning hole's light paths: the graphics card (start-up check passed); the card's fills 50.0 ms average, 60.0 ms worst (2x)"));
    checkTrue("black hole lines: at rest the glare (and how long after its picture) and the fine table",
              has("Black Hole Lab, at rest: glare 150.0 ms average, 150.0 ms worst (1x), shown 400 ms after its picture at most; fine spin table 2500.0 ms average, 2500.0 ms worst (1x)"));
    checkTrue("black hole lines (0.55.8): how long after the stop the sharp picture came, or that it did not within the rest; the fine table made at the start",
              has("  at rest: the sharp picture (the fine table) 450 ms after the stop") && has("  at rest: the sharp picture (the fine table) 0 ms after the stop") &&
              !has("spin 0.9, changing distance: no frames counted\n  at rest") && has("the fine spin table made at the scene's start (all cores) 700 ms"));
    h.move[2].frames = 1; h.move[2].frameSum = 1; h.move[2].restS = 1.4;
    checkTrue("black hole lines (0.55.8): no sharp picture within the rest says so", [&] { for (const std::string& l : benchBlackHoleLines(h)) if (l == "  at rest: no sharp picture within the 1.4 s at rest") return true; return false; }());
    BenchScene s; s.name = "13. Black hole"; s.frames = benchStats({5, 5}); s.notes = v;
    const std::string t = benchResultText(BenchEnv(), {s});
    checkTrue("black hole lines: printed in the report under their scene", t.find("  Black Hole Lab, spin 0.9, tilting: frame thread 1.50 ms average") != std::string::npos);
}
static void testBenchSceneNumbers() {
    const std::vector<app::BenchSceneText>& v = app::benchSceneTexts();
    checkTrue("benchmark list: 13 default scenes and 21 in all (0.46, on purpose: scene 12 and its twin, the whole atom's Glow cloud; 0.55.0: scene 13, the Black Hole Lab)", app::kBenchDefaultScenes == 13 && app::kBenchAllScenes == 21 && int(v.size()) == 21 && v[12].kind == app::BenchKind::BlackHole);
    bool numbered = true, plain = true, inRange = true; int mentions = 0;
    for (std::size_t i = 0; i < v.size(); i++) {
        numbered = numbered && v[i].name.rfind(std::to_string(i + 1) + ". ", 0) == 0;
        plain = plain && v[i].name.find("Stress") == std::string::npos && v[i].name.find("Risk ") == std::string::npos;
        const std::string t = v[i].name.substr(v[i].name.find(". ") + 2) + " " + v[i].what;       // (the scene's own number in front of the name is not a mention)
        for (std::size_t p = 0; (p = t.find("cene", p)) != std::string::npos; p += 4) {
            std::size_t j = p + 4; while (j < t.size() && (t[j] == 's' || t[j] == ' ')) j++;
            while (j < t.size() && std::isdigit((unsigned char)t[j])) {
                const int n = std::atoi(t.c_str() + j); mentions++; inRange = inRange && n >= 1 && n <= 21 && n != int(i + 1);
                while (j < t.size() && std::isdigit((unsigned char)t[j])) j++;
                if (t.compare(j, 5, " and ") == 0) j += 5; else if (t.compare(j, 4, " to ") == 0) j += 4; else break;
            }
        }
    }
    checkTrue("benchmark list: the names are numbered 1 to 21 in order, without 'Stress:' or 'Risk NN:'", numbered && plain);
    checkTrue("benchmark list: every scene number in a name or text (" + std::to_string(mentions) + " of them) is a scene of the list, and none is the scene itself", inRange && mentions >= 20);
    bool twins = true;
    for (std::size_t i = 0; i < v.size(); i++) {
        const int o = v[i].twinOf;
        if ((i >= 13) != (o != 0)) { twins = false; continue; }
        if (!o) continue;
        const std::string want = "scene " + std::to_string(o);
        twins = twins && o <= 12 && v[i].name.find("compare with " + want) != std::string::npos && v[i].what.find(want) != std::string::npos
                && v[o - 1].what.find("cene " + std::to_string(i + 1)) != std::string::npos        // and the original points back at its twin
                && (v[i].kind == app::BenchKind::Atom) == (v[o - 1].kind == app::BenchKind::Atom);
    }
    checkTrue("benchmark list: scenes 14 to 21 are the twins of 6, 7, 8, 9, 10, 1, 2 and 12; each names its original, and the original names it", twins && v[13].twinOf == 6 && v[14].twinOf == 7 && v[15].twinOf == 8 && v[16].twinOf == 9 && v[17].twinOf == 10 && v[18].twinOf == 1 && v[19].twinOf == 2 && v[20].twinOf == 12);
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
        checkTrue(w + "the name says OFF and the scene it compares with, the description says DIFFERENCE", n.find("GPU superposition OFF") != std::string::npos && n.find("scene " + std::to_string(pr[1] + 3)) != std::string::npos &&
                  d.find("DIFFERENCE") != std::string::npos);
        const std::string dg = info(pr[1]).what;
        checkTrue(w + "the original scene keeps its name and points at its comparison scene", info(pr[1]).name.find("Risk 11") == 0 && dg.find("OpenGL 4.3") != std::string::npos);
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
    checkTrue("chart twins: which 28 and 29 come after the run of scenes numbered which + 3; 0.16 stage 3 adds which 30 after them (changed on purpose: kBenchAtomScenes 30 -> 31, 0.46: -> 33 with the two whole-atom Glow scenes)", kBenchChartCpuFirst == 28 && atom::kBenchFullZoomScene == 30 && kBenchAtomScenes == 33 && atom::kBenchSceneCount == 28);
    {   // 0.16 stage 3: the nucleus at full zoom with the camera turning (the owner's slow case), the last scene of the list
        atom::AtomLab L; L.p.qstyle = "Simple"; L.gpuChart = false;
        atom::benchSetup(L, atom::kBenchFullZoomScene, 1040, 713);
        checkTrue("full-zoom scene: lawrencium-266, Orbit view, Accurate, + Sea, Nucleons (0.43: were Cinematic and Fuzzy), the largest zoom (7 = x10,000,000), no glide, the whole nucleus there",
                  L.p.Z == 103 && L.p.N == 163 && L.p.view == "Orbit" && L.p.ostyle == "Modern" && L.p.qstyle == "Accurate" && L.p.detail == "+ Sea" && L.p.style == "Nucleons" && L.p.zoom == 7 && !L.s.zoomTo && int(L.s.nuc.size()) == 266 && L.gpuChart);
        const double y0 = L.s.oyaw, p0 = L.s.opitch;
        atom::benchFrame(L, atom::kBenchFullZoomScene, 0, -1); const double y1 = L.s.oyaw;
        atom::benchFrame(L, atom::kBenchFullZoomScene, 1, 0);
        checkTrue("full-zoom scene: the camera turns every frame, warm-up included, as a fast drag does (0.15 rad a frame = 15 points at 0.01 rad per point)", near(y1 - y0, 0.15) && near(L.s.oyaw - y1, 0.15) && L.s.opitch > 0 && L.s.opitch < 3.15 && near(L.s.opitch, p0, 0.2));
        const atom::BenchInfo& in = atom::benchSceneInfo(atom::kBenchFullZoomScene);
        checkTrue("full-zoom scene: the name and the settings in words", in.name.find("full zoom") != std::string::npos && in.what.find("lawrencium-266") != std::string::npos && in.what.find("x10,000,000") != std::string::npos && in.what.find("+ Sea") != std::string::npos);
    }
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
        checkTrue(w + "the name says OFF and the scene it compares with, the description says DIFFERENCE", n.find("GPU chart cache OFF") != std::string::npos &&
                  n.find(std::string("scene ") + std::to_string(pr[1] + 3)) != std::string::npos && d.find("DIFFERENCE") != std::string::npos);
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
        checkTrue(w + "the name says OFF and the scene it compares with, the description says DIFFERENCE", n.find("GPU outline OFF") != std::string::npos && n.find("scene " + std::to_string(pr[1] + 3)) != std::string::npos &&
                  d.find("DIFFERENCE") != std::string::npos);
        checkTrue(w + "the original scene keeps its name and points at its comparison scene", info(pr[1]).name.find("Risk 12") == 0 && dg.find("cene " + std::to_string(pr[0] + 3)) != std::string::npos);
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
    atom::AtomParams base, app = atom::AtomParams::appDefaults();
    checkTrue("the struct keeps the plain cloud (Classic, Normal)", base.cstyle == "Classic" && base.cdens == "Normal");
    checkTrue("the app starts and Defaults give Glow and High", app.cstyle == "Glow" && app.cdens == "High");
    checkTrue("only the cloud settings differ between the two", [&] { atom::AtomParams a = app; a.cstyle = base.cstyle; a.cdens = base.cdens; a.style = base.style;
        return a.scale == base.scale && a.zoom == base.zoom && a.qstyle == base.qstyle && a.view == base.view && a.orb == base.orb && a.Z == base.Z && a.ostyle == base.ostyle; }());
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
        checkTrue("Check-a-jump settings (0.10.8 stage 3): the shells 1s .. 6f are accepted for the upper and the lower one, nothing else (0.46, on purpose: was 1s .. 4f); the defaults are 2p and 1s; neither is saved to the settings file",
                  atom::AtomLab::validChoice("jfrom", "4f") && atom::AtomLab::validChoice("jfrom", "5s") && atom::AtomLab::validChoice("jfrom", "6f") && atom::AtomLab::validChoice("jto", "1s") && !atom::AtomLab::validChoice("jfrom", "7s") && !atom::AtomLab::validChoice("jfrom", "5g") && !atom::AtomLab::validChoice("jto", "") && atom::AtomParams().jfrom == "2p" && atom::AtomParams().jto == "1s");
        checkTrue("Relativity setting: ON in the plain struct and in the app's defaults (0.49.3, outside review; it was off), and a default settings file has no 'dirac' line", atom::AtomParams().dirac == "On" && atom::AtomParams::appDefaults().dirac == "On" && savePrefs(Prefs()).find("dirac") == std::string::npos);
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
        atom::seedRandom(20260101);
        ref.p = atom::AtomParams(); ref.p.elec = "Hydrogen-like"; ref.p.hrel = "Non-relativistic"; ref.viewW = 800;   // (0.49.3: the old versions' cloud is the non-relativistic one)
        ref.viewH = 600; ref.setParam("Z", 92.0); ref.setParam("N", 146.0); ref.restart(); ref.s.nucSeed = 1; ref.s.spin = false;
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
    for (const char* id : {"light", "dark", "black", "midnight", "solar", "paper", "sage", "contrast", "monochrome"}) {
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
        atom::AtomParams base, app = atom::AtomParams::appDefaults();
        checkTrue("90% outline: Off by default (the struct, and what the app starts with and Defaults give)", base.outline == "Off" && app.outline == "Off");
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
    checkTrue("the app shows 10 themes now (4 rows; 11 before Synthwave was archived in 0.15 stage 5m, changed on purpose)", allThemes().size() == 10 && app::gridRows(int(allThemes().size()), 3) == 4);
    checkTrue("the theme list has 10 (4 rows)", int(sizeof(kThemes) / sizeof(kThemes[0])) == 10 && app::gridRows(int(sizeof(kThemes) / sizeof(kThemes[0])), 3) == 4);
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
    const double jd = timescale::tdbFromUT(ms / 86400000.0 + 2440587.5);   // 0.46.3: the elements are evaluated at the TDB moment of the UT date
    bool same = true;
    // 0.52.1: Earth's orbit point is pos - drawShift (Earth itself is drawn off the Earth-Moon barycentre): equal to the elements' state within 1 mm (the shift added and taken off again)
    for (std::size_t i = 0; i < 8; i++) { const orbit::State st = orbit::planetStateAt(i, jd); const orbit::Vec3 p = a.bodies[i].pos - a.bodies[i].drawShift; same = same && orbit::length(p - st.pos) < 1e-3 && a.bodies[i].vel.x == st.vel.x && (i == 2 || (p.x == st.pos.x && p.y == st.pos.y && p.z == st.pos.z)); }
    checkTrue("Simple: the planets start exactly at the orbital elements' state (Earth's barycentre within 1 mm)", same);
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
    const double refMs = double(timescale::unixMsOfTDB(orbit::kNBodyRefJD));   // 0.46.3: the reference epoch is 12:00 TDB, the lab's clock UT (69.184 s earlier)
    orbit::SolarSystem c;
    c.setRealPlanets(true);
    c.loadAt(int64_t(std::llround(refMs)));
    const orbit::NBodyState st0 = orbit::nbodyStartState(true);   // 0.32: the app's start has the Pluto system
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
    clock.stateAt((double(jump) - (orbit::kNBodyRefJD - 2440587.5) * 86400000.0) / 1000.0 + timescale::deltaTSeconds(jump / 86400000.0 + 2440587.5), x, v);   // the jump's date is UT: the clock's TDB is Delta T later (64.184 s in 2001, IERS)
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
    const atom::AtomParams base, app = atom::AtomParams::appDefaults();
    checkStr("Phase colours: Sign by default (the plain struct and the app's defaults)", base.phase + "/" + app.phase, "Sign/Sign");
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
    const atom::AtomParams base, app = atom::AtomParams::appDefaults();
    checkStr("Superposition: Off by default, second orbital 2p, mix 50 %", base.super + "/" + app.super + "/" + base.orb2 + "/" + std::to_string(int(base.mix)), "Off/Off/2p/50");
    checkTrue("Superposition: 'On' and 'Off' accepted, anything else refused", atom::AtomLab::validChoice("super", "On") && atom::AtomLab::validChoice("super", "Off") && !atom::AtomLab::validChoice("super", "on") && !atom::AtomLab::validChoice("super", ""));
    // 0.50.3 (outside review): the panel offers 5s to 6f since 0.46, so a saved 5d is kept (it was refused here and reset to 1s on the next start); was !validChoice("orb2", "5s")
    checkTrue("Second orbital and its orientation: the same choices as the first, bad values refused",
              atom::AtomLab::validChoice("orb2", "4f") && atom::AtomLab::validChoice("orb2", "6f") && !atom::AtomLab::validChoice("orb2", "7s") && atom::AtomLab::validChoice("orb", "5d") && !atom::AtomLab::validChoice("orb", "6g") && atom::AtomLab::validChoice("fAx2", "m-3") && !atom::AtomLab::validChoice("dAx2", "m+3") && atom::AtomLab::validChoice("pAx2", "y"));
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
    // 0.29: on by default again (with the accurate integrator it brings Mercury from 40,962 km to 24 km of JPL Horizons; DECISIONS.md). 0.10.3-0.28 it was off
    // (with the old integrator it moved every planet away from JPL), so this check changed on purpose twice.
    checkTrue("General relativity (1PN): on by default (it only acts in Real)", vs.relativity);
    vs.relativity = true; vs = orbit::ViewSettings{};                         // what the Defaults button does (putBackDefaults: app.set = ViewSettings{})
    vs.relativity = false; vs = orbit::ViewSettings{};
    checkTrue("General relativity (1PN): the Defaults button puts it back to on", vs.relativity);
    Prefs p; p.orbit = {{"physics", "Real"}, {"gr", "0"}};
    const Prefs q = loadPrefs(savePrefs(p));
    checkTrue("General relativity (1PN): goes through the settings file unchanged", q.orbit == p.orbit && q.orbit.at("gr") == "0");
    // the file keeps a saved gr value (0.29: ui/settings.cpp reads it only from a file that also has gr29, written since 0.29, so a pre-0.29 "off" does not override the new default)
    Prefs p1; p1.orbit = {{"physics", "Real"}, {"gr", "1"}};
    const Prefs q1 = loadPrefs(savePrefs(p1));
    checkTrue("General relativity (1PN): a saved gr=1 (old settings file) comes back as 1", q1.orbit == p1.orbit && q1.orbit.at("gr") == "1");
    // the system: a new SolarSystem has it off; the app passes the setting (on by default since 0.29)
    const double refMs = double(timescale::unixMsOfTDB(orbit::kNBodyRefJD));   // 0.46.3: the reference epoch is 12:00 TDB, the lab's clock UT (69.184 s earlier)
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
    orbit::SolarSystem sm;
    sm.loadAt(1790000000000LL);
    for (int k = 0; k < 40; k++) sm.step(0.25, -0.3);
    double mt = units::kSunInEarthMasses; orbit::Vec3 bary;
    for (const orbit::Body& b : sm.bodies) { bary = bary + b.pos * b.massEarths; mt += b.massEarths; }
    bary = bary * (1.0 / mt);
    const orbit::Vec3 bs = sm.barycentre();
    checkTrue("Simple: the barycentre is the mass-weighted mean of the Sun and the bodies, unchanged", bs.x == bary.x && bs.y == bary.y && bs.z == bary.z && sm.sunTrail.empty());
    // Real: the barycentre is where the integrator has it: minus the Sun's barycentric position (the Sun moves against the planets)
    const double refMs = double(timescale::unixMsOfTDB(orbit::kNBodyRefJD));   // 0.46.3: the reference epoch is 12:00 TDB, the lab's clock UT (69.184 s earlier)
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
    total += orbit::kRealGMPluto;                                                   // 0.32: the Pluto system is the tenth body
    for (const auto& bd : r.bodies) if (bd.name == "Pluto") sq = sq + bd.pos * orbit::kRealGMPluto;
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
    checkTrue("back to Simple: no Sun's path, and the simple barycentre again", r.sunTrail.empty() && !r.realPlanets());
}

#include "test_theme_style.inc"   // 0.14 stage 1: richer theme styles and Frutiger Aero
#include "test_look.inc"          // 0.15 stage 5i: the three look options (contrast by the WCAG formula, written out in the test)


// 0.55.7: the guard round the graphics card's light paths (app/gpu_guard.h), without graphics: the settings line, the decision at start, the marker file
// (a step named before it begins, read back by the next start), "Try again", the time limit's "slow", the log's order
static std::string readAll(const std::string& path) {
    std::string t;
    if (FILE* f = std::fopen(path.c_str(), "rb")) { char buf[4096]; std::size_t n; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) t.append(buf, n); std::fclose(f); }
    return t;
}
static bool exists(const std::string& path) { FILE* f = std::fopen(path.c_str(), "rb"); if (f) std::fclose(f); return f != nullptr; }
static void testGpuGuard() {
    using app::GpuGuardRecord;
    const GpuGuardRecord r{"off", "0.55.7", "ATI | AMD Radeon RX 9060 XT | 4.6.0 Core 26.10.1", "compile the light paths' shader"};
    const GpuGuardRecord back = app::gpuGuardParse(app::gpuGuardText(r));
    checkStr("guard: the settings line reads back (a '|' inside the driver's string becomes '/')", app::gpuGuardText(back),
             "off | 0.55.7 | ATI / AMD Radeon RX 9060 XT / 4.6.0 Core 26.10.1 | compile the light paths' shader");
    checkTrue("guard: a broken or unknown line is the empty record", app::gpuGuardParse("maybe | 1 | x | y").state.empty() && app::gpuGuardParse("off | 1 | x").state.empty() &&
              app::gpuGuardParse("").state.empty() && app::gpuGuardText(GpuGuardRecord{}).empty());
    // (0.55.9: the second field holds the light-path shader's id, render/kerr_compute.h kerrShaderId, in place of the app version)
    const std::string v = "shader 0123456789abcdef", d = "AMD / Radeon / 4.6 26.10.1";
    const GpuGuardRecord none, marker{"off", v, d, "check: dispatch 1"};
    auto dec = app::gpuGuardDecide(none, none, v, d);
    checkTrue("guard: never tried: allowed, guarded, nothing kept", dec.allowed && dec.guarded && dec.keep.state.empty());
    dec = app::gpuGuardDecide(none, marker, v, d);
    checkTrue("guard: a marker left behind (same version and driver): off, crashed, the step kept", !dec.allowed && dec.crashed && dec.keep == marker);
    dec = app::gpuGuardDecide(marker, none, v, d);
    checkTrue("guard: kept off, the same driver after a restart: stays off", !dec.allowed && dec.crashed);
    dec = app::gpuGuardDecide(marker, none, v, "AMD / Radeon / 4.6 26.11.2");
    checkTrue("guard: kept off, another driver: tried once more, guarded", dec.allowed && dec.guarded && dec.keep == marker);
    dec = app::gpuGuardDecide(marker, none, "shader fedcba9876543210", d);
    checkTrue("guard: kept off, the light-path shader's text changed: tried once more, guarded", dec.allowed && dec.guarded);
    // 0.55.9: a new app version with the same shader and driver: the same id (the app version is not kept), so it stays off
    dec = app::gpuGuardDecide(marker, none, v, d);
    checkTrue("guard: kept off, a new app version with the same shader and driver: stays off", !dec.allowed && dec.crashed && dec.keep.version == v);
    dec = app::gpuGuardDecide(GpuGuardRecord{"ok", v, d, ""}, none, v, d);
    checkTrue("guard: succeeded once with this version and driver: allowed, no markers", dec.allowed && !dec.guarded);
    dec = app::gpuGuardDecide(GpuGuardRecord{"ok", v, d, ""}, none, v, "other");
    checkTrue("guard: succeeded with another driver: guarded again", dec.allowed && dec.guarded);
    dec = app::gpuGuardDecide(GpuGuardRecord{"slow", v, d, "fence wait, case 1 edges"}, none, v, d);
    checkTrue("guard: a step that ran out of time (same driver): off, not called a crash", !dec.allowed && !dec.crashed);
    dec = app::gpuGuardDecide(GpuGuardRecord{"ok", v, d, ""}, marker, v, d);
    checkTrue("guard: a marker wins over a kept ok", !dec.allowed && dec.keep == marker);
    // the files: a folder of its own
#ifdef _WIN32
    const std::string dir = std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") + "\\pl_gpuguard_test\\";
    _mkdir(dir.c_str());
#else
    const std::string dir = "/tmp/pl_gpuguard_test_" + std::to_string(static_cast<long>(getpid())) + "/";
    mkdir(dir.c_str(), 0755);
#endif
    const std::string mk = dir + app::GpuGuard::kMarker, lg = dir + app::GpuGuard::kLog;
    std::remove(mk.c_str()); std::remove(lg.c_str());
    {   app::GpuGuard g;
        g.start(dir, v, d, "", {"head line 1", "head line 2"});
        checkTrue("guard files: a first start: allowed, guarded, no marker yet", g.allowed() && g.guarded() && !exists(mk));
        g.begin("compile the light paths' shader");
        const std::string m = readAll(mk);
        checkTrue("guard files: the marker names the version, the driver and the step before it begins", m.find("version=" + v + "\n") != std::string::npos &&
                  m.find("driver=" + d + "\n") != std::string::npos && m.find("step=compile the light paths' shader\n") != std::string::npos);
        g.end("compile the light paths' shader", "compiled");
        g.begin("check: dispatch 1");
        // (the process "dies" here: the object is dropped with the marker on disk)
    }
    {   app::GpuGuard g;
        g.start(dir, v, d, "", {});
        checkTrue("guard files: the next start finds the marker: off, the step named in the Settings line",
                  !g.allowed() && g.crashed() && g.offText().find("stopped the app last time") != std::string::npos && g.offText().find("'check: dispatch 1'") != std::string::npos);
        checkStr("guard files: the settings file keeps it", g.saved(), "off | " + v + " | " + d + " | check: dispatch 1");
        checkTrue("guard files: the marker stays until the settings file holds that", exists(mk));
        g.settingsWritten();
        checkTrue("guard files: then it is removed", !exists(mk));
        g.closing();
        const std::string saved = g.saved();
        app::GpuGuard h;                                   // a restart, same driver: stays off without a marker
        h.start(dir, v, d, saved, {});
        checkTrue("guard files: a restart with the same driver stays off", !h.allowed() && h.offText().find("check: dispatch 1") != std::string::npos);
        app::GpuGuard k;                                   // another driver: tried again
        k.start(dir, v, d + " new", saved, {});
        checkTrue("guard files: another driver string tries again (guarded)", k.allowed() && k.guarded() && k.offText().empty());
        h.tryAgain();
        checkTrue("guard files: Try again: allowed, guarded, nothing kept, a new generation", h.allowed() && h.guarded() && h.saved().empty() && h.generation() == 1 && h.offText().empty());
        h.begin("check: dispatch 1");
        h.finished(true);
        checkTrue("guard files: the whole path succeeded: the marker removed, ok kept, no markers any more", !exists(mk) && h.saved() == "ok | " + v + " | " + d + " | " && !h.guarded());
        h.begin("fill 2: something");
        checkTrue("guard files: after success a step writes no marker", !exists(mk));
        h.timedOut("fill 2: fence wait, pass B (not finished after 60 s)");
        checkTrue("guard files: a step out of time: off for this run, kept as slow with its step", !h.allowed() && !h.crashed() &&
                  h.saved() == "slow | " + v + " | " + d + " | fill 2: fence wait, pass B (not finished after 60 s)" && h.offText().find("did not finish a step in time") != std::string::npos);
        const std::vector<std::string> tail = h.logTail(4);
        checkTrue("guard files: the log's tail is its last lines", tail.size() == 4 && tail.back().find("did not finish within its time limit") != std::string::npos);
        const std::string log = readAll(lg);
        const std::size_t a = log.find("begin check: dispatch 1"), b2 = log.find("the whole path succeeded"), c = log.find("begin fill 2: something");
        checkTrue("guard files: the log has the steps in order (overwritten at each start)", log.find("head line 1") == std::string::npos && a != std::string::npos &&
                  b2 != std::string::npos && c != std::string::npos && a < b2 && b2 < c);
    }
    std::remove(mk.c_str()); std::remove(lg.c_str());
    app::Prefs p; p.gpuPaths = "off | 0.55.7 | x / y | compile";
    checkStr("guard: the settings file's line goes through the settings file unchanged", app::loadPrefs(app::savePrefs(p)).gpuPaths, p.gpuPaths);
    checkTrue("guard: no line when nothing is kept", app::savePrefs(app::Prefs{}).find("gpupaths") == std::string::npos);
}

int main() {
    testLookContrast();
    testRoundTrip();
    testBadLines();
    testOldProgressLines();
    testSettingsFrom018();
    testPrefsFuzz();
    testSettingsWrite();
    testThemes();
    testTextSizes();
    testPages();
    testFactsAndTabs();
    testShot();
    testFps();
    testWindow();
    testBench();
    testBenchAtomScenes();
    testBenchRiskScenes();
    testBenchSceneNumbers();
    testBenchBlackHole();
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
    testAppTexts();
    testThemeStyle();
    testEntities();
    testPowerRequest();
    testBenchFocusFlag();
    testDisplayScale();
    testShell();
    testGpuGuard();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
