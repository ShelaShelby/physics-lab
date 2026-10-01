# Physics Lab native — project rules
- The native app (native/) is the main version from 0.9 on. index.html is a frozen reference for the web version (sha256 starts e5501387ae64). Never modify it. It is only needed to regenerate data with the scripts in native/tools and native/tests/reference.
- New features and changes do not need a web equivalent. If a change makes a "word for word" test fail on purpose, update that test's saved reference and say so in REPORT.md. Never loosen a test just to make it pass.
- Physics tests (orbit_tests, atom_tests) must keep passing unless the physics is changed on purpose, and then explain why.
- Windows only. Black Hole Lab, Spacetime Lab and the Slingshot game are not part of the native app.
- No GPU and no Windows PC in the cloud session: never claim speed or Windows behaviour as tested. Say what was and was not tested in REPORT.md.
- Commit and push to `native` after each part with "native X.Y stage N". Keep REPORT.md, README and the 🕰 history up to date.
