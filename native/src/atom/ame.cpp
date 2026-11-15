#include "ame.h"

#include <algorithm>

namespace atom {

#include "ame_data.inc"

static constexpr int kRows = int(sizeof kAme / sizeof kAme[0]);

const AmeRow* ame(int Z, int N) {
    const auto it = std::lower_bound(kAme, kAme + kRows, std::make_pair(Z, N), [](const AmeRow& r, const std::pair<int, int>& k) { return r.Z != k.first ? r.Z < k.first : r.N < k.second; });
    return it != kAme + kRows && it->Z == Z && it->N == N ? it : nullptr;
}
int ameCount() { return kRows; }
int ameMeasuredCount() { static const int n = [] { int k = 0; for (const AmeRow& r : kAme) if (!r.est) k++; return k; }(); return n; }
std::vector<const AmeRow*> ameHighestN(int k) {
    std::vector<const AmeRow*> v;
    for (const AmeRow& r : kAme) if (!r.est) v.push_back(&r);
    std::sort(v.begin(), v.end(), [](const AmeRow* a, const AmeRow* b) { return a->bindPerA != b->bindPerA ? a->bindPerA > b->bindPerA : (a->Z != b->Z ? a->Z < b->Z : a->N < b->N); });
    if (int(v.size()) > k) v.resize(std::size_t(k));
    return v;
}
const AmeRow& ameHighest() { static const AmeRow* r = ameHighestN(1)[0]; return *r; }
int ameHeaviestMeasuredZ() { static const int z = [] { int m = 0; for (const AmeRow& r : kAme) if (!r.est) m = std::max<int>(m, r.Z); return m; }(); return z; }

}  // namespace atom
