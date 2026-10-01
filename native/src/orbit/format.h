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

// ---- units on screen: metric or imperial (the Settings switch). Only the words change;
// every number inside the app stays in SI units. These follow the switch: ----
void setImperial(bool on);
bool imperial();
constexpr double kMilesPerKm = 0.621371192;
std::string km(double km);                          // "6,371 km" / "3,959 mi"
std::string kmPlain(double km);                     // "11.1 km" (as written in the data) / "6.9 mi"
std::string millionKm(double mkm);                  // "150 million km" / "93 million mi"
std::string kms(double kmPerS, int decimals = 1);   // "29.8 km/s" / "18.5 mi/s"
std::string kmsNumber(double kmPerS, int decimals = 1);   // the number only, in the same unit
std::string celsius(double c);                      // "15 °C" / "59 °F"
// A sentence with km, km/h, km/s or °C in it (the data's notes), in miles / mph / mi/s / °F when imperial is on
std::string unitText(const std::string& text);

std::string au(double x);                           // "1.00 AU" / "30.1 AU"
std::string years(double y);                        // "88 days" / "1.88 years" / "165 years"
std::string kg(double kg);                          // "1.3×10²² kg"

// ---- JavaScript's own number wording (the Atom Lab text uses these all over) ----
std::string jsString(double x);                      // String(x): 0.000054, 5.4e-7, 1e+21
std::string jsToPrecision(double x, int digits);     // x.toPrecision(d): "4.47", "5.4e-5"
std::string jsToExponential(double x, int digits);   // x.toExponential(d): "1e+18"
std::string jsLocale(double x, int maxFrac = 3);      // x.toLocaleString('en'): commas, at most 3 decimals: "5,700", "0.054"

std::string dateUTC(int64_t unixMs);                // "1 Jan 2026" (Greenwich time)
std::string simDate(int64_t epochMs, double years); // the date after `years` of simulated time (plain years when far away)
int yearUTC(int64_t unixMs);

}  // namespace fmt
