// A small rich-text layout for the Learn guides, drawn with Dear ImGui.
// It understands the few HTML pieces the web guides use: paragraphs (<p class="lrnP">, muted with
// "lrnMuted"), bold <b>, italic <i>, <small>, <br>, and fact cards (<div class="lrnCard"> with a
// <div class="lrnBig"> and a <div class="lrnSm">). Text is wrapped word by word to the window width.
#pragma once

#include <string>

namespace ui {

// Draws `html` at the cursor, as wide as the space left in the window.
void richText(const std::string& html);

// A heading line (bold, a little larger).
void richHeading(const std::string& text);

}  // namespace ui
