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
- Plans and state: ROADMAP.md (what is done and planned), RESUME.md (current state and how work flows), DECISIONS.md (what the owner decided), OWNER-TODO.md (things only the owner can do). Read them only when the task says so, except that you update RESUME.md 'Current state' at the end of each version. Add anything the owner must do (for example a test on a real PC) to OWNER-TODO.md. Do not promise anything that is not on the roadmap.
- Tests first: for new physics or logic, write the tests from independent references first, then the code. If a test has to change, say why in REPORT.md.
- Every source used for a value, formula or method must also be listed in the About page, 'Where the facts come from'.

## Workflow rules (0.15 stage 4.1; they apply to every stage)
Why: stages used to take 20-30 minutes and had grown to hours: the cost was running the picture tests (each starts the real app under software OpenGL) many times. So:
- Build (fresh session; if CMake stops on "XCURSOR" install the X11 headers first: `apt-get install -y --no-install-recommends libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libegl-dev libgl-dev libgl1-mesa-dri libegl-mesa0 glslang-tools libdrm-dev libgbm-dev libasound2-dev libdecor-0-dev`; about 6 minutes the first time; ccache is used when installed: `apt-get install -y ccache`):
  `cmake -S native -B /home/user/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON && ninja -C /home/user/build`
  Unit tests only (no SDL, about 1 minute): `cmake -S native -B /home/user/build -G Ninja -DPL_BUILD_APP=OFF && ninja -C /home/user/build`
- Tests: `native/tools/check.sh quick` (unit tests, shaders, source scan: about 1 minute, no app start) and `native/tools/check.sh full` (everything, in parallel). One test: `ctest --test-dir /home/user/build -R '^name$' --output-on-failure` (names: `ctest -N`). Labels: quick, pictures.
- While working: run only `check.sh quick` and the single test for what you changed.
- Perturbation proofs ("break it on purpose"): use the fastest test that catches the break (a unit test, or ONE picture test), never the full suite.
- Run `check.sh full` exactly ONCE, at the end, before pushing. Never re-run a passing suite; if one test fails, fix it and re-run that test only.
- Picture renders for LOOKING: only the screens this stage changed; one look each.
- Waiting for a long run: one long blocking wait (`timeout 119 sh -c 'until grep -q ... ; do sleep 10; done'` in a few calls at most), no polling loops of status.
- Push as soon as the final run passes.
- The owner's usage is limited: prefer smaller stages; stop and report if a stage grows.
- Time budget: About 70 minutes per job in two parts, one push per part. The times are a ceiling, not a target. Never skip tests, pixel checks or the Windows run to save time; if time runs out, push what is green and list what is left.
- Test environment: picture tests run one llvmpipe thread per app start (LP_NUM_THREADS=1) and a shared Mesa shader cache (set by CMake / check.sh); they never need their own settings.
