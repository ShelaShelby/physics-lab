// Texts the native app adds to the 🕰 history page (the web's own entries and intro are not touched).
#pragma once

namespace app {

constexpr const char* kHistoryNativeHeading = "Physics Lab (native app)";
constexpr const char* kHistoryWebHeading = "The web app's history";
// Directly above the web's entries
constexpr const char* kHistoryWebNote =
    "Native app: the entries below are the web app's history, word for word. Some of them are about the Black Hole Lab, the Spacetime Lab, "
    "the Slingshot game and the Games tab. The native app does not have those: it has the Orbit Lab and the Atom Lab.";

}  // namespace app
