// Turning numbers into the words shown on screen, matching the web version's wording.
// Metric only for now (the metric/imperial switch comes later).
#pragma once

#include <cstdint>
#include <string>

namespace fmt {

std::string fixed(double x, int decimals);          // like JavaScript x.toFixed(decimals)
std::string commas(double x);                       // rounded to a whole number, with thousands commas: 1,234,567
std::string plainNumber(double x);                  // shortest form, like JavaScript String(x): 2439.7, 11.1
std::string superscript(int n);                     // 30 -> ³⁰, -5 -> ⁻⁵
std::string precision(double x, int digits);        // like JavaScript +x.toPrecision(digits), then shortest form

std::string km(double km);                          // "6,371 km"
std::string kmPlain(double km);                     // "11.1 km" (as written in the data)
std::string millionKm(double mkm);                  // "150 million km"
std::string kms(double kmPerS, int decimals = 1);   // "29.8 km/s"
std::string celsius(double c);                      // "15 °C"
std::string au(double x);                           // "1.00 AU" / "30.1 AU"
std::string years(double y);                        // "88 days" / "1.88 years" / "165 years"
std::string kg(double kg);                          // "1.3×10²² kg"

std::string dateUTC(int64_t unixMs);                // "1 Jan 2026" (Greenwich time)
std::string simDate(int64_t epochMs, double years); // the date after `years` of simulated time (plain years when far away)
int yearUTC(int64_t unixMs);

}  // namespace fmt
