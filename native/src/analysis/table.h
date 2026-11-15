// The Data panel's tables (0.51): a table of numbers with a unit and a source on every column, the axis transforms (1/x, ln x ...), the text a table is
// exported as (CSV for a spreadsheet, tab-separated for the clipboard) and read back from (the user's own data, typed or pasted). No graphics: ui/data_panel.cpp
// draws it; tests/test_analysis.cpp checks it.
//
// Units are written the way the calculator reads them ("au", "cm^-1", "km/s"), so a value or a fitted parameter goes into the calculator with its unit.
#pragma once

#include <string>
#include <vector>

#include "analysis/fit.h"

namespace analysis {

struct Column {
    std::string name;                 // "a", "Period", "1/n₁² − 1/n₂²"
    std::string unit;                 // as the calculator reads it ("au", "cm^-1"); "" = a plain number
    std::string source;               // where these values come from, short ("NASA/JPL Keplerian elements"; "computed: 1/λ")
    std::vector<double> v;            // one per row; NaN = no value
    std::vector<double> sigma;        // its 1-sigma uncertainty, one per row (NaN = none for that row); empty = none at all
    std::vector<std::string> text;    // a text column (names, transitions): then v is empty
    bool isText() const { return !text.empty(); }
    bool hasSigma() const;            // at least one row has an uncertainty
};

// The axis transforms: a fit of y against 1/n² is a straight line for the Rydberg formula, ln y against x for an exponential ...
enum class Transform { None, Inverse, InverseSquare, Square, Cube, Sqrt, Ln, Log10, Count };   // Cube: 0.51.1 (T² against a³)
const char* transformName(Transform t);               // "x", "1/x", "1/x²", "x²", "x³", "√x", "ln x", "log₁₀ x" (for a menu)
const char* transformId(Transform t);                 // "none", "inv" ... (the settings)
Transform transformById(const std::string& id);
bool applyTransform(Transform t, double v, double s, double& out, double& outSigma);   // false where it is not defined (1/0, ln of x <= 0); s may be NaN
std::string axisLabel(const std::string& name, const std::string& unit, Transform t);  // "1/n²", "ln(a / au)", "a² (au^2)"
std::string transformedUnit(const std::string& unit, Transform t);                    // calculator form; "" for ln and log10 (and plain numbers)

// Units of a power product: unitPower("au", 3, 2) = "au^(3/2)"; "" stays "". A simple unit with a power ("cm^-1") has its power multiplied ("cm^-1" squared: "cm^-2").
std::string unitPower(const std::string& unit, int num, int den = 1);
// A fitted parameter's unit, in the columns' units: y^yPow x^xPow, written for the calculator ("yr/au", "1/au", "cm^-1") ("" = a plain number)
std::string paramUnit(const std::string& yUnit, const std::string& xUnit, int yPow, int xPow);
bool unitKnown(const std::string& unit);              // the calculator reads it ("" is a plain number: known)

// A worked analysis a table offers (the professor's exercise): which columns, transforms and model, and what the answer should be.
struct Preset {
    std::string label;                // "Kepler's third law"
    int x = -1, y = -1;               // column indexes
    Transform tx = Transform::None, ty = Transform::None;
    Model model = Model::Line;
    bool fit = true;
    bool useSigma = true;             // weight by the uncertainties (where the table has them)
    std::string group;                // only the rows of this group ("" = all)
    std::string expect;               // what a textbook says the answer is, with its source
    int hold = -1;                    // 0.51.1: a parameter held at holdValue (not fitted), -1: none
    double holdValue = 0;
    bool logX = false, logY = false;  // 0.51.1: logarithmic plot axes (the drawing only: the fit is of the values as they are)
};

struct Table {
    std::string id, title;
    std::string about;                // what it is and its limits
    std::vector<std::string> sources; // every source, written out
    std::vector<Column> cols;
    int group = -1;                   // a column that splits the rows into groups (the planet of a moon, l of a level), -1: none
    std::vector<Preset> presets;
    std::size_t rows() const;
    int find(const std::string& name) const;   // a column by name, -1: none
};
std::vector<std::string> groupValues(const Table& t);                // the distinct values of the group column, in order of appearance
std::string cellText(const Table& t, int col, std::size_t row);      // a cell as shown and exported ("" when empty)
std::string numberText(double v, int digits = 10);                    // 10 significant digits (the CSV: 15), plain ASCII ("1.495978707e+11"); "" for NaN

// ---- text out ----
// CSV (RFC 4180: commas, quotes doubled) with '#' comment lines first: the title, what it is, every source, each column's unit and source; then the header
// "name [unit]" and "σ name [unit]" for an uncertainty column, then the rows. extra: more comment lines (a fit's results), each without the '#'.
std::string toCsv(const Table& t, const std::string& appVersion, const std::vector<std::string>& extra = {});
std::string toTsv(const Table& t);                                   // the same table tab-separated, for pasting into a spreadsheet (no comment lines)

// ---- text in: the user's own data ----
// Rows of numbers separated by commas, tabs, semicolons or spaces; '#' starts a comment line. An optional first line names the columns: "t (s), x (m), σx (m)" or
// "t [s]"; a column whose name starts with σ, "sigma", "err", "unc" or "±" is the uncertainty of the column before it. Empty cells and "NaN" are no value.
// The units are checked with the calculator; one it cannot read is kept as text with a note. False (and why) when there is nothing to use.
bool parseText(const std::string& text, Table& out, std::string& why, std::vector<std::string>& notes);

}  // namespace analysis
