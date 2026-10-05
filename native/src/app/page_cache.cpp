#include "app/page_cache.h"

#include "app/theme_style.h"

namespace app {

bool pageNeedsCache(const PageStyle& page) {
    if (page.kind == 0 || page.stops < 2) return false;                        // no gradient under the textures
    return pageFxTotal(page.fx) > 0.0f || page.fx.streak > 0.0f;               // the same test drawPageFx uses to return at once
}

}  // namespace app
