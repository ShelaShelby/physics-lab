// HTML character references (0.15 stage 2). The About page is HTML text (app_text.inc), and the two small HTML drawers (html_view.cpp, rich_text.cpp) did not
// decode its entities, so "&rsquo;" showed as typed. No graphics here, so it is unit-tested (tests/test_entities.inc).
#pragma once

#include <string>

namespace app {

// UTF-8 bytes of one code point (cp above 0x10FFFF or in the surrogate range D800-DFFF gives an empty string).
std::string utf8FromCodepoint(unsigned cp);

// Replaces the references in `text` with their characters, in ONE pass (a decoded "&" starts nothing new):
//   named   "&rsquo;"  every name of the HTML Standard's list that ends in ";" (2,125 of them, html_entities_data.inc), case-sensitive;
//   numeric "&#8217;" and "&#x2019;" / "&#X2019;" (a code point from 1 to 0x10FFFF, not a surrogate).
// Anything else stays as typed, nothing is lost: an unknown name, a reference without its ";" (HTML would decode a few legacy ones such as "&amp"; this does not),
// "&#0;", an out-of-range or surrogate number, a lone "&". Tags and all other text are not touched.
std::string decodeHtmlEntities(const std::string& text);

}  // namespace app
