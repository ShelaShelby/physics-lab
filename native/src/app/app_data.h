// The web app's app-wide texts and data (Settings, themes, achievements ...), generated from
// index.html by tools/make_app_text.js into app_text.inc. This header gives the C++ side its names.
#pragma once

#include <cstddef>
#include <set>
#include <string>

namespace app {

#include "app_text.inc"

// The name a theme card shows: "???" only while the theme is secret and still locked; once it is unlocked, its real name.
inline std::string themeShownName(const ThemeDef& t, const std::set<std::string>& unlocked) {
    return t.secret && !unlocked.count(t.id) ? "???" : t.name;
}

template <class T, std::size_t N> constexpr std::size_t count(const T (&)[N]) { return N; }

}  // namespace app
