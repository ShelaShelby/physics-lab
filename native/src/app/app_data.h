// The web app's app-wide texts and data (Settings, themes, achievements ...), generated from
// index.html by tools/make_app_text.js into app_text.inc. This header gives the C++ side its names.
#pragma once

#include <cstddef>

namespace app {

#include "app_text.inc"

template <class T, std::size_t N> constexpr std::size_t count(const T (&)[N]) { return N; }

}  // namespace app
