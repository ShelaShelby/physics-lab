# Physics Lab: rules for working sessions
These are the rules in force. DECISIONS.md is a record of the owner's decisions; sessions do not have to read it.

## Talking with the owner
- Write in chat only when you need the owner's answer, and once at the end. The final message starts with the outcome.

## The app
- native/ is the app: Windows, C++17, SDL3, Vulkan, Dear ImGui. Four labs: Orbit, Atom, Molecule, Black Hole. It stays C++. No Rust in this repo.
- There is no other version of the app. Never mention, copy from or compare with one.
- Every simulation runs on the GPU. The CPU version stays as the exact reference the tests check the GPU against. It is also the fallback where the GPU or driver cannot run it (the guard in app/gpu_guard). A feature is not finished while its heavy repeated work runs only on the CPU. New drawing is on the GPU from the start.
- Target: the screen's refresh rate in every scene on the owner's PC (255 Hz). A job that adds something drawn adds a benchmark scene to the Hard list (app/bench_scenes.cpp). The scene has a stable id, moves as a user does, and uses the hardest view the controls reach. A feature with a wait (a picture made off the frame, a table, a cloud filling) adds a response line (ui/benchmark.cpp waitDefs).
- Windows is stricter than Linux: include every header you use, expect CRLF, check CreateProcess. AMD drivers are stricter than Mesa: initialise every variable, no implicit int/float conversion in shaders, use only features the device reports, exact barriers.

## Accuracy
- The app is for university and PhD-level learners. Every value, formula and calculation is correct and traceable.
- Take values from named sources (NIST/CODATA, NASA/JPL, NUBASE, CCCBDB, PubChem ...). Put the source in a code comment and in About, "Where the facts come from".
- Never invent or guess a number, reference value or test expectation. If a source cannot be reached, say so and leave the value out.
- Tests check against independent references: published data, textbook formulas, conservation laws. For new physics or logic, write the tests first, and they must fail on the old build.
- Say the limits in the app text (one electron, non-relativistic, test particles, fixed shapes ...).
- A doubtful value is reported in REPORT.md with the source of the correct value. It is never hidden or silently changed.
- Never loosen or delete a physics test to make it pass. When a deliberate change breaks a test, fix the test's reference and say so in REPORT.md.

## Sources
- This repo is public. Papers, preprints, scans, extracted paper text, saved web pages and downloads of unclear terms go only to the private repo ShelaShelby/physics-lab-sources. Attach it with add_repo (push access) and push to its `main`. Same relative path under `data/` (native/data/x/y.pdf is data/x/y.pdf there).
- This repo keeps the small extract a test reads (`<name>.extract.txt`: exact lines, reference, URL, date, sha256), one line per saved file in native/data/SOURCES.md (path, reference, URL, date, sha256, users), and the per-folder README.txt.
- After saving or reading a source, run `python3 native/tools/check_sources.py <sources checkout>`. Check every number against the saved original before using it.
- A helper agent may search, build, summarise or fetch. Check what it reports against the file, log or saved source before using it.

## Build and test
- First build in a fresh session (about 6 min): `apt-get update && apt-get install -y --no-install-recommends cmake ninja-build ccache libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxtst-dev libxrender-dev libxinerama-dev libxkbcommon-dev libwayland-dev glslang-tools libdrm-dev libgbm-dev libasound2-dev libdecor-0-dev mesa-vulkan-drivers libvulkan1 vulkan-tools vulkan-validationlayers`
- Full build: `cmake -S native -B /home/user/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON && ninja -C /home/user/build`. Unit tests only: `-DPL_BUILD_APP=OFF`.
- Run tests from the repository root.
- Save usage: send build and test output to a file and read only its end; read code with grep and line ranges; never re-run a passing suite.
- Every job runs `check.sh job`: quick plus ui_smoke (the app starts, each lab draws, the app ends cleanly), about 5 minutes together.
- The full suite runs on GitHub after the push (workflow "Native full tests"). Its failures become a later job and do not block the current one. Run `check.sh full` by hand only when the owner asks.
- The ui_* groups are in native/CMakeLists.txt. native/tests/TESTS.md lists every UI part, its time and why it is kept. A changed UI check is also run by hand: `python3 native/tests/<script.py> /home/user/build/PhysicsLab /home/user/build/one [part]`.
- A new UI check only for a new screen or a bug the owner has seen. It goes into an existing script, its ui_* group and TESTS.md, and the full suite stays under 10 minutes. If not, an older check goes.
- run_group.py stops a UI part after 5 minutes. A ui_* test stops after 15 minutes.
- Any single command that runs longer than 15 minutes is stopped, and the reason is written in REPORT.md.
- A failing check in a ui_* test the job runs is fixed, or its test is corrected with the reason in the test and in REPORT.md. Never loosened.
- When a feature is removed, its tests go too.
- A problem that fails 3 times is skipped and written in REPORT.md and ROADMAP.md.

## Writing
- Developer files (code comments, REPORT.md, plan files, CLAUDE.md, ROADMAP.md) use standard terms: GPU, CPU, shader, build machine. Short sentences, one fact each. No chains of semicolons and colons.
- Text users see in the app keeps its plain words.
- Do not rewrite old comments in bulk. Fix them only in files the job already touches.

## Docs each job updates
- native/REPORT.md, this job only, at most 25 lines, four headings: "What changed" (3 to 6 bullets), "Tests run" (with results), "Not done" (and why), "References changed on purpose" (tests whose expected values changed, with the reason).
- A changelog.cpp entry when the app changes, and VERSION in native/CMakeLists.txt.
- ROADMAP.md (its rules are at its top). README.md, PLANNER.md and plan files when the job makes a line wrong. DECISIONS.md when the owner decides something.

## Push and done
- Fresh session: `git checkout -B native origin/native`.
- One commit and one push per job, at the end, after the tests and docs are final. Commit name "native X.Y.Z".
- Push with `git push origin HEAD:refs/heads/native`, then verify with `git ls-remote origin refs/heads/native`.
- Done when the Windows run ("Native Windows build") of that commit is green and the "latest" release targets it. The final chat message gives the Windows run, the release and the GitHub full-test run.
