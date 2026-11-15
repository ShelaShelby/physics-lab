// The colours of the markers the Orbit Lab picture draws that are not a body's own colour. The picture (orbit_view.cpp) and the new UI's Legend (ui/shell_orbit.cpp) both read
// them from here, so the Legend shows the colours that are really drawn (0.15 stage 4).
#pragma once

namespace orbit::marks {

constexpr const char* kHighlight = "#ff2bd6";            // the "look here" ring round the selected thing: nothing else in the picture is this colour
constexpr const char* kMoon = "#a4a4a4";                 // a moon
constexpr const char* kProbeRing = "#ffd27a";            // a spacecraft's ring (and its path)
constexpr const char* kObject = "#8fd0ff";               // a far-away object that is not a star (ring and dot)
constexpr int kHabitable[3] = {70, 180, 100};            // the habitable zone's band (bytes; drawn faint, 13 % opaque)
constexpr int kIonTail[3] = {140, 205, 255};             // a comet's gas tail (bytes)
constexpr int kDustTail[3] = {255, 232, 180};            // a comet's dust tail (bytes)
constexpr const char* kAxisX = "#e5484d";                // the axes gizmo
constexpr const char* kAxisY = "#30a46c";
constexpr const char* kAxisZ = "#3e63dd";

}  // namespace orbit::marks
