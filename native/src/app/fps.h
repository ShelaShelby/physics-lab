// The frames-per-second counter's text: the web's  Math.round(frames * 1000 / ms) + ' fps'.
#pragma once
#include <string>
namespace app {
std::string fpsText(double fps);   // "64 fps"; "" before the first measurement (fps < 0)
}
