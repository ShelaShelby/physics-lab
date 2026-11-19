// 0.66.0: the Density surface's triangles far to near (molecule_view.cpp): a radix sort of their depths, 4 passes of 8 bits over the float's bits made
// monotonic. Stable from the triangles' own order, so it gives the one order by (depth, triangle number); std::sort (until 0.66.0) took about 0.8 ms a
// frame for benzene's 14,128 triangles on the session box and left equal depths in any order. Header only (the app and tests/test_app.cpp use it).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace molecule {

// order: the indexes 0 .. z.size() - 1 by z ascending (equal z: the smaller index first; -0 before +0); key, key2, tmp: the caller's buffers, kept between frames
inline void depthOrder(const std::vector<float>& z, std::vector<int>& order, std::vector<std::uint32_t>& key, std::vector<std::uint32_t>& key2, std::vector<int>& tmp) {
    const std::size_t n = z.size();
    order.resize(n); key.resize(n); key2.resize(n); tmp.resize(n);
    for (std::size_t t = 0; t < n; t++) {
        std::uint32_t u = 0;
        std::memcpy(&u, &z[t], 4);
        key[t] = (u & 0x80000000u) ? ~u : (u | 0x80000000u);   // (negative floats reversed, the positive ones above them)
        order[t] = static_cast<int>(t);
    }
    for (int pass = 0; pass < 4; pass++) {
        const int sh = 8 * pass;
        std::size_t cnt[257] = {};
        for (std::size_t t = 0; t < n; t++) cnt[((key[t] >> sh) & 0xffu) + 1]++;
        for (int k = 0; k < 256; k++) cnt[k + 1] += cnt[k];
        for (std::size_t t = 0; t < n; t++) { const std::size_t at = cnt[(key[t] >> sh) & 0xffu]++; tmp[at] = order[t]; key2[at] = key[t]; }
        order.swap(tmp); key.swap(key2);
    }
}

}  // namespace molecule
