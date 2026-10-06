// 0.15 stage 3: the reading view. In the new UI, About, History, the lab changelogs and the guides open in a centred readable column (about 70 characters wide) over the dimmed
// picture, with a table of contents (a rail on the left in a wide window, else a "Contents" menu) and a close button; Esc closes it. With the new UI off they open in the floating
// window they always had (pageBegin does exactly what the old code did).
#pragma once

#include <string>

namespace ui {

struct AppState;

void setReadingMode(bool on);                                // the new UI is on (set by ui/shell.cpp every frame)
// Opens the page window: `title` ends with "###id". open: cleared when the page is closed. In the reading view the window is the column; else the floating window as before.
bool pageBegin(const std::string& title, bool* open);
void pageEnd();                                              // ImGui::End() and, in the reading view, what goes with it
void noteHeading(int level, const std::string& text);        // a heading the contents list shows (level 1..3); only counts inside the reading view
bool readingShown();                                         // a page was shown in the reading view this frame
void closePages(AppState& app);                              // closes every page (About, History, changelog, guides)
bool pagesOpen(const AppState& app);                         // any of them is open
float readingBodyFont();                                     // the font size a page's text is laid out in (the Body size's 16/14), in the reading view; 0 otherwise
float readingFloorBody();                                    // the smallest sizes in px the pages may draw text at in the reading view (Body, Caption); 0 outside it
float readingFloorCaption();
void readingLongestLine(int chars, bool body);                // the pages report the characters on each line they lay out (the reading view keeps the longest of the Body text and of the Caption text, PL_LOG_SHELL)
void readingNote(const std::string& line);                   // what the reading view measured (PL_LOG_SHELL)

}  // namespace ui
