# Physics Lab native — project rules
- The native app (native/) is the main version from 0.9 on. index.html is a frozen reference for the web version (sha256 starts e5501387ae64). Never modify it. It is only needed to regenerate data with the scripts in native/tools and native/tests/reference.
- New features and changes do not need a web equivalent. If a change makes a "word for word" test fail on purpose, update that test's saved reference and say so in REPORT.md. Never loosen a test just to make it pass.
- Physics tests (orbit_tests, atom_tests) must keep passing unless the physics is changed on purpose, and then explain why.
- Windows only. Black Hole Lab, Spacetime Lab and the Slingshot game are not part of the native app.
- No GPU and no Windows PC in the cloud session: never claim speed or Windows behaviour as tested. Say what was and was not tested in REPORT.md.
- Commit and push to `native` after each part with "native X.Y stage N". Keep REPORT.md, README and the 🕰 history up to date.

- Cost rules (the owner is out of credit and runs on weekly plan limits): keep REPORT.md under 80 lines, covering only this version (what changed, what was tested, what was not tested); do not copy old text forward, old reports are in git history.
- Read only CLAUDE.md, the latest REPORT.md and the files you will change. Do not explore the whole repo.
- Take only the screenshots needed to check the change, and look at each once.
- Run only the tests that matter while working, and the full test suite once before each commit. Do not rebuild everything after every small edit.
- Do not start helper agents (subagents) unless the task says so.
- If the task turns out much bigger than described, stop and ask before continuing.

- Accuracy bar: this app is meant to be accurate enough for university students and PhD-level learners. Every value, formula and calculation must be correct and traceable. Take values from named sources (NIST/CODATA, NASA/JPL, NUBASE and so on) and put the source in a code comment. Never invent or guess a number, a reference value or a test expectation: if a source cannot be reached, say so and leave it out. Tests should check against independent references (known values, conservation laws, published data), not only against our own earlier output.
- Be honest about limits: if a model is simplified (one electron, non-relativistic, test particles, approximate elements), the app text must say so. Never present an approximation as exact. If you find an error or doubtful value in existing code or text, do not hide it or silently change it: report it in REPORT.md with the source for the correct value.
- Plans: see ROADMAP.md at the repo root (what is done and what is planned). Keep it up to date; do not promise anything that is not on it.
- Every source used for a value, formula or method must also be listed in the About page, 'Where the facts come from'.
