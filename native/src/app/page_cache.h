// The page effects cache (0.15 stage 2, C2). The clouds, bubbles, grain, grid, halftone and streak of a page style are STATIC (no shipped theme has them since 0.15 stage 5m): they only change with the window size, the
// theme and the text size. Until 0.14 they were rebuilt as thousands of vertices every frame (about 0.7 ms of CPU on the owner's PC); now the
// page (gradient + textures) is drawn ONCE into a texture (ui/page_cache.cpp) and each docked panel draws a piece of it. This file is the plain logic (when to use it, when to
// rebuild), unit-tested; the graphics are in ui/page_cache.cpp.
#pragma once

#include <string>

#include "app/theme_types.h"

namespace app {

// The cache is used when the page has textures to draw (clouds, bubbles, grain, halftone, grid, streak) over a gradient. No theme has textures now, so it is not used. A page with no gradient under the textures is not cached (the texture would carry transparent pixels).
bool pageNeedsCache(const PageStyle& page);

struct PageCacheKey {
    std::string theme;       // the theme id
    int w = 0, h = 0;        // the window, in framebuffer pixels
    int fontPx100 = 0;       // the text size in hundredths of a pixel (the textures are sized in text units: it covers the display scale and the Text size)
    bool operator==(const PageCacheKey& o) const { return theme == o.theme && w == o.w && h == o.h && fontPx100 == o.fontPx100; }
};

// Says whether the texture must be rebuilt for this frame's key (the first time, and whenever the key differs from the one it was built for).
class PageCacheTracker {
public:
    bool update(const PageCacheKey& k) { if (built_ && k == key_) return false; key_ = k; built_ = true; rebuilds_++; return true; }
    void invalidate() { built_ = false; }            // the GL texture is gone (a lost context, a failed build): rebuild at the next use
    int rebuilds() const { return rebuilds_; }
private:
    PageCacheKey key_;
    bool built_ = false;
    int rebuilds_ = 0;
};

}  // namespace app
