#include "nubase.h"

#include <algorithm>
#include <vector>

namespace atom {

#include "nubase_data.inc"

static constexpr int kRows = int(sizeof kNubase / sizeof kNubase[0]);

const NubaseRow* nubase(int Z, int N) {
    const auto it = std::lower_bound(kNubase, kNubase + kRows, std::make_pair(Z, N), [](const NubaseRow& r, const std::pair<int, int>& k) { return r.Z != k.first ? r.Z < k.first : r.N < k.second; });
    return it != kNubase + kRows && it->Z == Z && it->N == N ? it : nullptr;
}
int nubaseCount() { return kRows; }

// "9/2-*" -> "9/2−" (a real minus sign), "(7/2-)" kept in brackets, "4+#" -> "4+". The marks * and # are NUBASE's: directly measured, from systematics.
SpinText nubaseSpin(const NubaseRow& r) {
    SpinText out;
    std::string j = r.jpi;
    if (j.empty()) return out;
    const bool star = j.find('*') != std::string::npos, hash = j.find('#') != std::string::npos, tent = j.find('(') != std::string::npos;
    std::string t;
    for (const char c : j) {
        if (c == '*' || c == '#') continue;
        if (c == '-') t += "−"; else t += c;
    }
    out.text = t;
    out.how = hash ? 2 : tent ? 3 : star ? 1 : 4;
    return out;
}

}  // namespace atom
