# Physics Lab: project rules
Quiet chat (owner decision 14 Nov 2026): do not report progress to the owner. Write in chat only (1) when you cannot go on without the owner's answer, and (2) one final message that starts with the outcome. Everything else goes in REPORT.md. Every message costs the owner's usage.
This also covers the app's own 'say what you are doing' reminders: answer them with at most three words (for example 'Fix 4 of 9'), never a sentence.

## Project
- native/ is the app (Windows C++): four labs, Orbit Lab, Atom Lab, Molecule Lab, Black Hole Lab. (The folder and the branch keep the name "native".)
- The app is C++ and stays C++. No Rust code in this repo (owner decision 18 Oct 2026; Rust is for separate apps).
- There is no other version of Physics Lab: never mention, copy from or compare with one. Tests compare the app with independent references only (published data, textbook formulas, conservation laws). When a deliberate change breaks a test, fix the test's reference and say so in REPORT.md. Never loosen or delete a physics test to make it pass.
- Physics tests (ctest names orbit_physics, atom_physics, molecule_logic and the other *_physics / *_logic / *_validation tests; the programs are orbit_tests, atom_tests ...) must keep passing unless the physics changes on purpose; then explain why.
- If the job is much bigger than described, say so in REPORT.md and keep going unless the job says to stop.

## Graphics and speed (owner decision 16 Nov 2026)
Every simulation is built to run on the graphics card; the processor's version stays as the exact reference the tests check the card against and takes over where the card or driver cannot (the guard of 0.55.7). A feature is not finished while its heavy repeated work runs only on the processor. A job that adds something drawn adds a benchmark scene for it; the target is the screen's refresh rate on the owner's PC in every scene (the owner sends the benchmark after each X.Y; the planning chat checks it). A new scene goes to the Hard list (app/bench_scenes.cpp) with a stable id, moves as a user does, and uses the hardest view a user can reach with the controls (zooms tried, REPORT.md); it moves to All scenes once the owner's run shows it far above his screen's rate. A feature with a wait (a picture made off the frame, a table, a cloud filling) adds a response line (ui/benchmark.cpp waitDefs).
Owner decision 17 Nov 2026: no scene below the screen's refresh rate is acceptable (his PC: 255 Hz). Owner decision 17 Nov 2026 (evening): the Black Hole Lab's look comes first, then the speed work (moving heavy repeated work to the graphics card, slowest Hard-list scene first, until every scene reaches the 255 Hz); every new drawing is on the graphics card from the start (the processor as reference and fallback).

## Two models (owner decision 4 Nov 2026)
Three roles (owner decision 14 Nov 2026): a planning chat leads (architecture, design, what comes next) and writes each job, but writes no code and does not push; the Opus session builds the job and leads its Haiku helpers as below.
The session's own model (Opus) is the lead developer; it uses Haiku helper agents (Agent tool, model "haiku") for the routine work below. Opus decides what goes to Haiku and may keep anything.
- Opus: architecture and design decisions; tricky bugs; everything hard, including UI and the look; the physics: sources, formulas, numbers, test references and expectations; the final review of every diff; what is committed and pushed; talking with the owner; anything that is not clearly routine.
- Haiku: codebase search and "where is X"; summarizing files, logs and test output; building and running tests and reporting the failing names with their last lines; boilerplate and mechanical edits that Opus has specified exactly; lookups in docs; fetching a page and pulling a table out of it (saved as received, into the sources repo: see Sources); taking the screenshots Opus asks for; first drafts of doc text (changelog, REPORT.md) for Opus to rewrite.
- Haiku never: chooses a source or a test's expected value, changes physics data or a physics test, decides a pairing or an exception, commits or pushes, or answers the owner.
- Opus spot-checks Haiku's results, because a helper can misreport: every number or quote Haiku reports from a source is checked by Opus against the saved original (in the sources repo, see Sources) before it is used; for a search or summary Opus opens at least one cited file and line per claim it relies on; for a test run Opus reads the failing test's own output itself. A claim that cannot be checked is not used.
- Give Haiku a short, exact task (what, where, what to report, in how many lines) and have it write long output to a file; one Haiku task at a time unless the tasks are independent.

## Cost rules (the owner runs on weekly plan limits)
- Normal jobs: read only CLAUDE.md, the latest native/REPORT.md and the files you will change. Do not explore the whole repo. A job that says "audit" may read anything.
- Send build and test output to a file; read only its last 20 lines or the failing test names. Read code with grep plus line ranges, never whole large files.
- Screenshots: at most 4 cropped looks for a fix job, each looked at once, only of what changed. A look or design job sets its own number.
- No time limit (owner decision 18 Oct 2026). If the same problem fails 3 times, skip it, write it in REPORT.md and go on. Never skip the quick tests or the Windows run to save time (the full UI suite runs only when the owner asks, see Build and test).
- Batch the owner's bugs into one job.

## Docs
- Per job: native/REPORT.md, a changelog.cpp entry when the app changes (the history), and the "Current state" line in RESUME.md (only the newest version's lines; older ones are in changelog.cpp and git). DECISIONS.md only if the owner decided something.
- REPORT.md: at most 40 lines, this job only (what changed and what was tested). Old reports are in git history.
- Keep native/README.md, RESUME.md, ROADMAP.md and DECISIONS.md true and short: when a job changes what the app is or does, fix the lines that are now wrong. Promise nothing that is not on the roadmap.
- ROADMAP.md: follow the rules at its top. A job that does a roadmap item deletes that line in the same commit (a partly done item is rewritten to what is left); new ideas and found problems go in as one line under their lab.
- An audit job writes its findings in REPORT.md (or a file the job names) and moves open items to ROADMAP.md; no lasting AUDIT.md.

## Sources (owner decision 15 Nov 2026)
- This repo is PUBLIC. Papers, preprints, journal scans, extracted paper text, saved web pages and downloads of unclear terms go only to the private repo ShelaShelby/physics-lab-sources (attach it with add_repo, push access; push to its `main`; same relative path under `data/`, so native/data/x/y.pdf is data/x/y.pdf there). Never put one under native/: `check.sh quick` fails on a .pdf/.html/.htm/.wikitext/.tar.gz/.txt.gz or paper text.
- This repo keeps: the small extract a test needs (`<name>.extract.txt` next to where the file was: exact lines, reference, URL, date, sha256, place; the test reads it), one line per saved file in native/data/SOURCES.md (path, reference, URL, date, sha256, users), and the per-folder README.txt. Stays too: own outputs, small typed tables with their reference, raw datasets of US government sources or with a stated licence; unclear terms and no test reads it: it moves; a test reads it: it stays and is listed in REPORT.md for the owner.
- A session that saves or reads a source attaches the sources repo, adds the file and its SOURCES.md line, and runs `python3 native/tools/check_sources.py <checkout>` (or PL_SOURCES_DIR=<checkout> native/tools/check.sh quick). The tools in native/tools that read a source take PL_SOURCES_DIR (tools/pl_sources.py says what to do when it is absent). A number is still checked against the saved original before it is used.

## Accuracy
- The app must be accurate enough for university and PhD-level learners. Every value, formula and calculation must be correct and traceable. Take values from named sources (NIST/CODATA, NASA/JPL, NUBASE, CCCBDB, PubChem and so on) and put the source in a code comment. Every source is also listed in the About page, 'Where the facts come from'.
- Never invent or guess a number, reference value or test expectation. If a source cannot be reached, say so and leave it out.
- Tests check against independent references (known values, conservation laws, published data), not only the app's own earlier output. For new physics or logic, write the tests first.
- Be honest about limits: if a model is simplified (one electron, non-relativistic, test particles, approximate elements, fixed equilibrium molecule shapes), the app text must say so. If you find an error or doubtful value, do not hide or silently change it: report it in REPORT.md with the source for the correct value.

## Build and test
- Fresh session, first build (about 6 min): `apt-get update && apt-get install -y --no-install-recommends cmake ninja-build ccache libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxtst-dev libxrender-dev libxinerama-dev libxkbcommon-dev libwayland-dev libegl-dev libgl-dev libgl1-mesa-dri libegl-mesa0 glslang-tools libdrm-dev libgbm-dev libasound2-dev libdecor-0-dev mesa-vulkan-drivers libvulkan1 vulkan-tools vulkan-validationlayers`
- Full build: `cmake -S native -B /home/user/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON && ninja -C /home/user/build`
- Unit tests only (no SDL, about 1 min): same with `-DPL_BUILD_APP=OFF`.
- Run tests from the repository root (some tests write files into the current folder).
- Which tests before the push (owner decisions of 6 Nov and 20 Nov 2026):
  - every job: `native/tools/check.sh quick` (the physics and logic programs, about 1–3 min) plus the ui_* tests of the screens the job changes (find each check's group in native/CMakeLists.txt). A job that adds or changes a UI check also runs that one check by hand: `python3 native/tests/<check_script.py> /home/user/build/PhysicsLab /home/user/build/one [part]` (seconds to a few minutes).
  - the full suite (`native/tools/check.sh full`, about 35 min) runs only when the owner asks ("run the full tests"). RESUME.md keeps the line "Last full UI run" (version and commit, and the tests that failed in it); REPORT.md of a job says that the UI suite was not run.
  - the UI suite is 16 tests, one per screen (ui_shell, ui_controls, ui_look, ui_pages, ui_orbit, ui_atom, ui_molecule ...; `ctest --test-dir /home/user/build -N`). Each runs its check scripts in turn (tests/run_group.py) and names the check that failed: fix it and re-run that check by hand, then its ui_* test once.
  - No known failure carries over to the next version: a failing check in a ui_* test the job runs gets its cause fixed, or its test corrected with the reason written in the test and in REPORT.md. Never loosened.
  - Never re-run a passing suite.
- Writing UI tests (every app start costs 10–30 s of CPU, and the suite once grew to an hour):
  - add a part to the check script of that screen and add it to that screen's ui_* group in native/CMakeLists.txt; no new file and no new ctest test per update;
  - a behaviour test (clicks, scrolling, layout) uses ONE theme and only the window sizes where the result can differ (usually 880x560 and 1280x800); a look test (colours, contrast) covers the themes at ONE size;
  - at most one new test per fix; the failing-before run is the proof. Break on purpose only for new test logic, with the fastest test that catches it;
  - when a feature is removed, its tests go with it (git keeps them).
- Picture renders for looking: only the screens this job changed, one look each.
- Long waits: run the suite in the background and use one blocking wait of up to 10 min at a time (`timeout 590 sh -c 'until grep -q ... ; do sleep 20; done'`), no status polling.
- Every test that is not labelled quick gets the label "pictures", LP_NUM_THREADS=1 and a shared Mesa shader cache from CMake automatically; a new test needs no settings.

## Pushes (owner decisions 20 Nov 2026)
- Exactly one push per job, at the end, after the tests and the docs are final, with the commit named "native X.Y.Z". Never a checkpoint push. No doc-only push after the final push. A fix found after testing goes into that same push.
- Push only with `git push origin HEAD:refs/heads/native`, then verify with `git ls-remote`.
- REPORT.md cannot hold the Windows result (it comes after the push): the final chat message gives it.

## Planning rules (owner decisions 20 Nov 2026)
- The planning chat reads the processor's code (data, precision, calls) before it plans any move to the graphics card, and names in the job the files and lines it read.
- Before any new job, the owner's benchmark of the previous version is in.
- Who keeps each plan file true: the job that changes a plan's area updates it; the planning chat names the plan files in the job.

## Done
- Fresh session: `git checkout -B native origin/native`. Commit as "native X.Y.Z" (the version, e.g. "native 0.46.1"; no "stage" names: owner decision 8 Nov 2026).
- Done when the Windows run for the newest commit that changed native/ or the workflow is green, and the "latest" release targets that commit. (A push that only changes root docs starts no Windows run and needs none.)
