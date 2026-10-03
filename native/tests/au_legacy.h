// 0.10.5 stage 4: the LEGACY elements start (nbodyElementsStartState: the app's orbital elements put into barycentric form with the real GMs) as it was BEFORE the lab changed
// from the web's au (149.6e9 m) to the IAU au (149,597,870,700 m).
//
// Why this and not a rescaling of the saved numbers (nbody's "saved from 0.10.0" positions, jpl's "0.10.2" Real values): those were made by integrating that start with the REAL
// GM values (kRealGM), which do not scale with the au. With s = au_IAU / 149.6e9 the start state changes like this: positions x s, but velocities x s^(-1/2) (the elements' velocity
// is sqrt(kGMSun a) / r ~ s, then it is multiplied by sqrt((GM_sun_real + GM_i) / kGMSun) ~ s^(-3/2)). Because the real GMs stay, the integrated orbit is not the old one scaled, so the saved
// end states cannot be rescaled. But the OLD START can be rebuilt exactly (x / s, v x s^(1/2)), and the integrator then has to give the saved numbers at the old tolerance: that keeps
// "the integrator did not change" as strict as before. The saved values themselves are unchanged.
#pragma once

#include <cmath>

#include "orbit/nbody.h"
#include "orbit/units.h"

inline orbit::NBodyState legacyStartWithWebAu() {
    const double s = units::kAU / 149.6e9;
    orbit::NBodyState st = orbit::nbodyElementsStartState();
    for (int i = 0; i < orbit::kNBodies; i++) {
        st.x[i] = st.x[i] * (1.0 / s);
        st.v[i] = st.v[i] * std::sqrt(s);
    }
    return st;
}
