// 0.82.0: the camera's feel, shared by the Orbit Lab (orbit_view.cpp, ui/orbit_panel.cpp) and the Black Hole Lab (blackhole/lab.cpp).
// One place for the turn per point of a drag, a released turn's momentum and the wheel's eased steps. The numbers are 0.74.0's (DECISIONS.md, the Orbit
// Lab's camera). They are not physics: they set how the camera handles.
#pragma once

#include <algorithm>
#include <cmath>

namespace orbit {

constexpr double kTurnPerPoint = 0.01;        // rad a drag turns the view per point the pointer moves (both directions)
constexpr double kMomentumFade = 7.5;         // a released turn's speed falls by e^-7.5 a second (5 % left after 0.4 s); it stops below kMomentumStop
constexpr double kMomentumStop = 0.05;        // rad/s
constexpr double kMomentumMax = 2.5;          // rad/s: a fast flick coasts at most kMomentumMax / kMomentumFade = 0.33 rad
constexpr double kWheelZoomSeconds = 0.15;    // a wheel notch's or a key's eased step

// the Orbit Lab's drag (OrbitView::drag): the yaw and pitch go up by this for a drag of dx, dy points (y down). The Black Hole Lab's picture is turned half a
// turn from the Orbit Lab's for the same yaw and pitch (blackhole/lab.h), so it takes the negatives: the same way on the screen.
inline void dragAngles(double dx, double dy, double& dYaw, double& dPitch) { dYaw = dx * kTurnPerPoint; dPitch = dy * kTurnPerPoint; }

// eased in and out over s = 0..1 (smoothstep): 0 at 0, 1 at 1, flat at both ends
inline double ease01(double s) { s = std::clamp(s, 0.0, 1.0); return s * s * (3 - 2 * s); }

// while pressed: the drag's speed (rad/s), smoothed over a few frames from the turn made since the last tick (a held, still pointer brings it to 0)
inline void momentumFollow(double& vYaw, double& vPitch, double turnYaw, double turnPitch, double dt) {
    if (dt > 0) { vYaw = 0.5 * vYaw + 0.5 * turnYaw / dt; vPitch = 0.5 * vPitch + 0.5 * turnPitch / dt; }
}
// the press ends: the speed is kept, at most kMomentumMax; none with Reduce motion or below kMomentumStop
inline void momentumRelease(double& vYaw, double& vPitch, bool reduceMotion) {
    const double v = std::hypot(vYaw, vPitch);
    if (reduceMotion || v < kMomentumStop) { vYaw = vPitch = 0; return; }
    if (v > kMomentumMax) { vYaw *= kMomentumMax / v; vPitch *= kMomentumMax / v; }
}
// after a coasting step: the speed fades by e^(-kMomentumFade dt); it ends exactly (0 below kMomentumStop)
inline void momentumFade(double& vYaw, double& vPitch, double dt) {
    const double f = std::exp(-kMomentumFade * dt);
    vYaw *= f; vPitch *= f;
    if (std::hypot(vYaw, vPitch) < kMomentumStop) vYaw = vPitch = 0;
}

// 0.82.0, the Black Hole Lab: the turn a fading speed v makes over a step dt is v times this, the exact integral (1 - e^(-k dt)) / k, so a flick coasts at most
// kMomentumMax / kMomentumFade at any frame rate. (The Orbit Lab steps v dt before the fade, as in 0.74.0: its flick coasts up to x / (1 - e^-x) times that,
// x = kMomentumFade dt: 1.5 % more at 255 frames a second, 6 % at 60. Kept: REPORT 0.82.0.)
inline double momentumCoastSeconds(double dt) { return (1 - std::exp(-kMomentumFade * dt)) / kMomentumFade; }

}  // namespace orbit
