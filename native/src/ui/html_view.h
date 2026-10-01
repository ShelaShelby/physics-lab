// Draws the HTML pieces of the Atom Lab guides with Dear ImGui: paragraphs (lrnP, lrnMuted), cards
// (lrnCard with lrnBig / lrnSm), fact boxes (lrnFacts / lrnF), log-scale bar rows (lrnRow), stacked bars
// (lrnStack), lists (lrnUl), and inline <b>, <i>, <small>, <sub>, <br> and coloured dots (lrnDot).
// "[[chips]]" in the HTML calls `chips` there, so a row of buttons can sit inside the text.
#pragma once

#include <functional>
#include <string>

namespace ui {

void htmlView(const std::string& html, const std::function<void()>& chips = nullptr);
// What a click on an action word does (the guide's "monochrome" word calls it with "monoKey").
void setHtmlAction(std::function<void(const std::string&)> f);

}  // namespace ui
