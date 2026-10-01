// A small rich-text layout for the Learn guides, drawn with Dear ImGui.
// It understands the few HTML pieces the web guides use: paragraphs (<p class="lrnP">, muted with
// "lrnMuted"), bold <b>, italic <i>, <small>, <br>, and fact cards (<div class="lrnCard"> with a
// <div class="lrnBig"> and a <div class="lrnSm">). Text is wrapped word by word to the window width.
#pragma once

#include <string>

namespace ui {

// Draws `html` at the cursor, as wide as the space left in the window.
// mutedBase: plain text in the muted colour and bold text in the normal one (the web's small "info"
// style); size: the text size relative to the normal font; gap: space after each block, in text heights.
void richText(const std::string& html, bool mutedBase = false, float size = 1.0f, float gap = 0.55f);

// A heading line (bold, a little larger).
void richHeading(const std::string& text);

}  // namespace ui
