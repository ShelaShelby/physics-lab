// 0.15 stage 2, C1: HTML entities in the About page (kAboutHtml has 254 of them; the page showed "&rsquo;" as typed). WRITTEN BEFORE the code (compile-red first).
// References: the code points are those of the WHATWG HTML Standard's named character references (entities.json, fetched 2026-10-05 from the w3c/html mirror;
// the standard's own host was not reachable), typed here by value, not computed by the code under test. Numeric references: HTML "numeric character reference".
#include "app/html_entities.h"

static void testEntities() {
    // every named entity that kAboutHtml uses, with the character it must become (UTF-8 bytes written out)
    const struct { const char* ent; const char* utf8; unsigned cp; } used[] = {
        {"&rsquo;", "\xE2\x80\x99", 0x2019}, {"&minus;", "\xE2\x88\x92", 0x2212}, {"&rdquo;", "\xE2\x80\x9D", 0x201D}, {"&ldquo;", "\xE2\x80\x9C", 0x201C},
        {"&sup2;", "\xC2\xB2", 0xB2}, {"&le;", "\xE2\x89\xA4", 0x2264}, {"&ndash;", "\xE2\x80\x93", 0x2013}, {"&radic;", "\xE2\x88\x9A", 0x221A},
        {"&amp;", "&", 0x26}, {"&times;", "\xC3\x97", 0xD7}, {"&middot;", "\xC2\xB7", 0xB7}, {"&alpha;", "\xCE\xB1", 0x3B1}, {"&hellip;", "\xE2\x80\xA6", 0x2026},
        {"&gt;", ">", 0x3E}, {"&beta;", "\xCE\xB2", 0x3B2}, {"&prime;", "\xE2\x80\xB2", 0x2032}, {"&pi;", "\xCF\x80", 0x3C0}, {"&frac12;", "\xC2\xBD", 0xBD},
        {"&delta;", "\xCE\xB4", 0x3B4}, {"&Sigma;", "\xCE\xA3", 0x3A3}, {"&sup3;", "\xC2\xB3", 0xB3}, {"&rang;", "\xE2\x9F\xA9", 0x27E9}, {"&lang;", "\xE2\x9F\xA8", 0x27E8},
        {"&iacute;", "\xC3\xAD", 0xED}, {"&eacute;", "\xC3\xA9", 0xE9}, {"&copy;", "\xC2\xA9", 0xA9}, {"&Aring;", "\xC3\x85", 0xC5}};
    for (const auto& u : used) {
        checkStr(std::string("entity ") + u.ent, app::decodeHtmlEntities(std::string("a") + u.ent + "b"), std::string("a") + u.utf8 + "b");
        checkStr(std::string("code point of ") + u.ent, app::decodeHtmlEntities(std::string(u.ent)), app::utf8FromCodepoint(u.cp));
    }
    // numeric references: decimal, hexadecimal with x or X
    checkStr("&#8217;", app::decodeHtmlEntities("&#8217;"), "\xE2\x80\x99");
    checkStr("&#x2019;", app::decodeHtmlEntities("&#x2019;"), "\xE2\x80\x99");
    checkStr("&#X2019;", app::decodeHtmlEntities("&#X2019;"), "\xE2\x80\x99");
    checkStr("&#65;", app::decodeHtmlEntities("&#65;"), "A");
    checkStr("&#x1F4A1; (4-byte UTF-8, the bulb)", app::decodeHtmlEntities("&#x1F4A1;"), "\xF0\x9F\x92\xA1");
    // unknown or malformed ones stay as text, nothing is lost
    for (const char* keep : {"&bogus;", "&rsquo", "&;", "&#;", "&#x;", "&#xZZ;", "&#12a;", "&#0;", "&#x110000;", "&#xD800;", "&", "A & B", "&&", "AT&T;x", "&amp", "x&"})
        checkStr(std::string("kept as text: ") + keep, app::decodeHtmlEntities(keep), keep);
    // one pass only: a decoded & does not start another entity
    checkStr("&amp;lt; is &lt; (no double decoding)", app::decodeHtmlEntities("&amp;lt;"), "&lt;");
    checkStr("&amp;#65; is &#65;", app::decodeHtmlEntities("&amp;#65;"), "&#65;");
    // neighbours and text around
    checkStr("two in a row", app::decodeHtmlEntities("&ldquo;Hi&rdquo;&nbsp;&ndash;&mdash;"), "\xE2\x80\x9CHi\xE2\x80\x9D\xC2\xA0\xE2\x80\x93\xE2\x80\x94");
    checkStr("plain text is untouched", app::decodeHtmlEntities("Sun's radius is 695,700 km <b>bold</b>"), "Sun's radius is 695,700 km <b>bold</b>");
    // the table has more than the 26 that the About page uses, including entities with TWO code points (WHATWG: &NotEqualTilde; = U+2242 U+0338) and case-sensitive names
    checkStr("&NotEqualTilde; (two code points)", app::decodeHtmlEntities("&NotEqualTilde;"), "\xE2\x89\x82\xCC\xB8");
    checkStr("&Sigma; and &sigma; differ", app::decodeHtmlEntities("&Sigma;&sigma;"), "\xCE\xA3\xCF\x83");
    checkStr("&lang; is U+27E8 (HTML5), not the old U+2329", app::decodeHtmlEntities("&lang;"), "\xE2\x9F\xA8");
    checkStr("&sup1; &frac14; &deg; &plusmn;", app::decodeHtmlEntities("&sup1;&frac14;&deg;&plusmn;"), "\xC2\xB9\xC2\xBC\xC2\xB0\xC2\xB1");
    // the real page: after decoding, no entity of the kinds used is left and the page keeps its tags
    const std::string page = app::kAboutHtml;
    int found = 0;
    for (std::size_t i = page.find('&'); i != std::string::npos; i = page.find('&', i + 1)) {
        const std::size_t e = page.find(';', i);
        if (e == std::string::npos || e - i > 10) continue;
        const std::string name = page.substr(i, e - i + 1);
        bool word = name.size() > 2;
        for (std::size_t k = 1; k + 1 < name.size(); k++) if (!std::isalnum(static_cast<unsigned char>(name[k]))) word = false;
        if (!word) continue;
        found++;
        const std::string d = app::decodeHtmlEntities(name);
        checkTrue("About entity " + name + " becomes a character", d != name && !d.empty() && d.find('&') == (name == "&amp;" ? 0u : std::string::npos));
    }
    checkTrue("kAboutHtml has 254 entities (98+21+20+20+12+12+11+9+9+8+4+4+3+3+3+2+2+2+2+2+1+1+1+1+1+1+1 counted by hand from the 27 names; stage 1 said 236 because its pattern missed names with digits and capitals)", found == 254);
    const std::string decoded = app::decodeHtmlEntities(page);
    checkTrue("decoded About has no &rsquo; left", decoded.find("&rsquo;") == std::string::npos && decoded.find("&ndash;") == std::string::npos && decoded.find("&ldquo;") == std::string::npos);
    checkTrue("decoded About keeps its tags", decoded.find("<div class=\"lrnCard\">") == 0);
    checkTrue("decoding only shortens the page", decoded.size() < page.size());
}
