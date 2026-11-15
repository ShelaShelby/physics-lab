// 0.15 stage 5c (bug fix A): where the picture may draw a label. With the new UI the bars and the Controls panel float over the full-window picture; a distance-ring label at a far zoom-out
// ("6,500,000,000 AU · 103,000 light years") used to be drawn over the breadcrumb in the top bar. A label is drawn only when its whole rectangle is clear of the top bar band, the toolbar
// band at the bottom and the Controls panel on the right. All 0 (the old layout): every label is drawn as before. Points (picture units), y down.
#pragma once

namespace orbit {

struct LabelInsets { double top = 0, bottom = 0, right = 0; };     // the bands the bars and the Controls panel cover, from the window's top, bottom and right edge
struct LabelBox { double x0, y0, x1, y1; };

// The rectangle of one line of text: `x` is where it is anchored (align 0 = its left end, 0.5 = its middle), `baseline` the text's baseline, `width` its width and `size` its font size.
// The line reaches 0.95 size above the baseline and 0.3 size below it (a safe box for ascenders and descenders).
inline LabelBox labelBox(double x, double baseline, double width, double size, double align) {
    return {x - width * align, baseline - 0.95 * size, x - width * align + width, baseline + 0.3 * size};
}

// True when the box lies clear of the covered bands; a band of 0 covers nothing, so with no insets every label is drawn as before (also one half off the window).
inline bool labelClear(const LabelBox& b, double winW, double winH, const LabelInsets& in) {
    return (in.top <= 0 || b.y0 >= in.top) && (in.bottom <= 0 || b.y1 <= winH - in.bottom) && (in.right <= 0 || b.x1 <= winW - in.right);   // (a band of 0: nothing is covered there)
}

}  // namespace orbit
