// A small rich-text layout for the Learn guides, drawn with Dear ImGui.
// "lrnMuted"), bold <b>, italic <i>, <small>, <br>, and fact cards (<div class="lrnCard"> with a
// <div class="lrnBig"> and a <div class="lrnSm">). Text is wrapped word by word to the window width.
#pragma once

#include <string>

namespace ui {

// Draws `html` at the cursor, as wide as the space left in the window.
// style); size: the text size relative to the normal font; gap: space after each block, in text heights.
void richText(const std::string& html, bool mutedBase = false, float size = 1.0f, float gap = 0.55f);

// A heading line (bold, a little larger).
void richHeading(const std::string& text);

}  // namespace ui
