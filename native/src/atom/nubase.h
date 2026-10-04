// What NUBASE2020 says about each ground state that the lab's own table (atom_data.inc) does not keep (0.11 stage 2): the spin and parity, and what KIND of half-life the
// nuclide has (stable, a measured value, only a limit, only an estimate from systematics, none). Written by tools/make_nubase_data.py from data/nubase/nubase_4.mas20.txt;
// the source and its limits are in data/nubase/README.txt. The half-lives and decay modes themselves are in atom_core.h (nucCode, decInfo).
#pragma once

#include <string>

namespace atom {

struct NubaseRow {
    int Z, N;
    const char* jpi;      // spin and parity as NUBASE writes it ("9/2-*", "(7/2-)", "4+#"); empty = none given
    char kind;            // S stable ("stbl"), M a plain half-life, A approximate (~), B only a limit (> <), E an estimate from systematics (#), P "p-unst", '-' no half-life field
    const char* lim;      // S: the lower limit the file gives (">140Py"), else empty; A and B: the half-life text with its unit (">4.6 zs")
    const char* modes;    // S: the decay-mode entries marked with a question mark ("2B+ ?"), else empty
};

const NubaseRow* nubase(int Z, int N);       // nullptr when NUBASE2020 has no ground state there
int nubaseCount();                           // 3,557

// the spin and parity written for people: "9/2⁻", and how sure it is. kind: 0 none given, 1 directly measured (*), 2 estimate from systematics (#), 3 tentative (in brackets), 4 as evaluated (no mark)
struct SpinText { std::string text; int how = 0; };
SpinText nubaseSpin(const NubaseRow& r);

}  // namespace atom
