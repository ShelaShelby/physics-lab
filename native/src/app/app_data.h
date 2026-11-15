// This header gives the C++ side its names.
#pragma once

#include <cstddef>
#include <string>

namespace app {

#include "app_text.inc"

template <class T, std::size_t N> constexpr std::size_t count(const T (&)[N]) { return N; }

}  // namespace app
