// The display scale and what follows from it (0.15 stage 2, C4). Windows' scale (100 %, 125 %, 150 % ...) is what SDL calls the display content scale (1.0, 1.25, 1.5).
// Until 0.14 the app read it once, from the PRIMARY monitor, at start. Now it is read again when SDL says the window's display scale changed (the window moved to another
// monitor, or the user changed Windows' scale while the app ran). Plain logic, no graphics: unit-tested with a stand-in for the events (tests/test_stage2.inc).
#pragma once

namespace app {

constexpr int kMinWindowWu = 880, kMinWindowHu = 560;     // the smallest window, in units (1 unit = 1 px at 100 %): DESIGN.md section 5
constexpr float kPanelWidthU = 360.0f;                     // the side column's width, in units (it goes away in stages 4 and 5)

// A scale the system gave: 0, negative or not-a-number is read as 1; anything else is kept within 0.5 .. 4.
float sanitizeDisplayScale(float s);
// The smallest window size in window pixels at this scale (880 x 560 units, rounded).
void minimumWindowSize(float scale, int& w, int& h);
float panelWidthFor(float scale);
// A text size in pixels: max(floor, round(base units * scale * Text size factor)) (DESIGN.md section 1). Double arithmetic so 19.5 stays 19.5.
int scaledTextPx(double baseUnits, int floorPx, double scale, double textFactor);

// Remembers the scale in use; update() takes a new reading and says whether the scale changed (by more than 0.001 after sanitising).
class DisplayScale {
public:
    explicit DisplayScale(float first) : scale_(sanitizeDisplayScale(first)) {}
    float scale() const { return scale_; }
    int changes() const { return changes_; }
    bool update(float reading);
    void minimumWindow(int& w, int& h) const { minimumWindowSize(scale_, w, h); }

private:
    float scale_;
    int changes_ = 0;
};

}  // namespace app
