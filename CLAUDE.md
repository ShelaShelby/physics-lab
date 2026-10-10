# Physics Lab native: project rules
Work silently: no text between tool calls, only the final message (5 lines max).

## Project
- native/ is the main app (Windows C++: Orbit Lab + Atom Lab). Black Hole Lab, Spacetime Lab and the Slingshot game are not part of it.
- native/archive/web/index.html is the frozen web reference. Never modify it. It is only needed to regenerate data with the scripts in native/tools and native/tests/reference.
- New features need no web equivalent. If a "word for word" test fails on purpose, update its saved reference and say so in REPORT.md. Never loosen or delete a test to make it pass.
- Physics tests (orbit_tests, atom_tests) must keep passing unless the physics changes on purpose; then explain why.
- The cloud has no GPU, no Windows and no real mouse: never claim speed or Windows behaviour as tested. REPORT.md says what was and was not tested.
- No helper agents unless the task says so. If the task is much bigger than described, stop and ask.

## Cost rules (the owner runs on weekly plan limits)
- Read only CLAUDE.md, the latest REPORT.md and the files you will change. Do not explore the whole repo.
- Send build and test output to a file; read only its last 20 lines or the failing test names. Read code with grep plus line ranges, never whole large files.
- At most 4 cropped screenshots, each looked at once, only of what changed.
- Small job: target 25 minutes, hard stop 40. If not green by then, push what is green and report what is left. Never skip tests, pixel checks or the Windows run to save time.
- Batch the owner's bugs into one job.

## Docs
- Per job: REPORT.md, changelog.cpp (the 🕰 history), one line in RESUME.md 'Current state'. DECISIONS.md only if the owner decided something.
- REPORT.md: at most 40 lines, this version only (what changed, what was tested, what was not). Do not copy old text forward; old reports are in git history.
- ROADMAP.md, DECISIONS.md, OWNER-TODO.md: read only when the task says so. Things only the owner can do (e.g. a test on a real PC) go into OWNER-TODO.md. Promise nothing that is not on the roadmap.
- native/PROGRESS.md must not exist.

## Accuracy
- The app must be accurate enough for university and PhD-level learners. Every value, formula and calculation must be correct and traceable. Take values from named sources (NIST/CODATA, NASA/JPL, NUBASE and so on) and put the source in a code comment. Every source is also listed in the About page, 'Where the facts come from'.
- Never invent or guess a number, reference value or test expectation. If a source cannot be reached, say so and leave it out.
- Tests check against independent references (known values, conservation laws, published data), not only our own earlier output. For new physics or logic, write the tests first.
- Be honest about limits: if a model is simplified (one electron, non-relativistic, test particles, approximate elements), the app text must say so. If you find an error or doubtful value, do not hide or silently change it: report it in REPORT.md with the source for the correct value.

## Build and test
- Fresh session, first build (about 6 min): `apt-get update && apt-get install -y --no-install-recommends cmake ninja-build ccache libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxtst-dev libxrender-dev libxinerama-dev libxkbcommon-dev libwayland-dev libegl-dev libgl-dev libgl1-mesa-dri libegl-mesa0 glslang-tools libdrm-dev libgbm-dev libasound2-dev libdecor-0-dev`
- Full build: `cmake -S native -B /home/user/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON && ninja -C /home/user/build`
- Unit tests only (no SDL, about 1 min): same with `-DPL_BUILD_APP=OFF`.
- While working: `native/tools/check.sh quick` (about 1 min, no app start) plus the single test for what you changed: `ctest --test-dir /home/user/build -R '^name$' --output-on-failure` (names: `ctest -N`).
- `check.sh full` only at the end of a stage, when the owner asks, exactly once. Never re-run a passing suite; if one test fails, fix it and re-run that test only.
- New tests: at most one per fix, 3-4 themes, one size. The failing-before run is the proof; break-on-purpose only for new test logic, with the fastest test that catches it (never the full suite).
- Picture renders for looking: only the screens this job changed, one look each.
- Long waits: one blocking wait (`timeout 119 sh -c 'until grep -q ... ; do sleep 10; done'`), no status polling.
- Picture tests set LP_NUM_THREADS=1 and a shared Mesa shader cache themselves (CMake / check.sh); they need no settings.

## Push and done
- Fresh session: `git checkout -B native origin/native`. Commit as "native X.Y stage N".
- Push only with `git push origin HEAD:refs/heads/native`, then verify with `git ls-remote`.
- Done only when the Windows run (5-8 min) is green AND the "latest" release is published and targets the head commit.
