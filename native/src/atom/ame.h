// Binding energy per nucleon from AME2020 (0.11 stage 4), for the chart's second height. Written by tools/make_ame_data.py from data/ame/mass.mas20.txt; the source and its limits are in data/ame/README.txt.
// The lab's own binding table (kBeaLines, whole keV per nucleon, used by massBinding) holds the same nuclides; the tests check the two agree to 1 keV. This table is unrounded and says which values are
// estimates (the file marks them with "#": the evaluators' extrapolation, not a measurement), which the lab's table does not.
#pragma once

#include <vector>

namespace atom {

struct AmeRow { short Z, N; double bindPerA, unc; int est; };   // keV per nucleon, uncertainty keV, est = 1: an estimate (non-experimental)

const AmeRow* ame(int Z, int N);                // nullptr when AME2020 has no such nuclide
int ameCount();                                 // 3,557
int ameMeasuredCount();                         // values from measured masses (2,549)
const AmeRow& ameHighest();                     // the measured row with the largest binding energy per nucleon: read from the table, never typed
std::vector<const AmeRow*> ameHighestN(int k);  // the k highest measured rows, highest first
int ameHeaviestMeasuredZ();                     // the largest Z that has a measured value (110)

}  // namespace atom
