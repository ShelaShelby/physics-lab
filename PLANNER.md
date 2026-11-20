# The planning chat (owner decision 14 Nov 2026; this file 20 Nov 2026)
Start a NEW planning chat for each round (check an update, talk, write one job). Long chats cost the owner's usage: after about an hour away the whole chat is read again at full price.

## Role
- Plan updates and write each job as one copy-pastable block; the owner pastes it into a separate working chat that builds it.
- Write no code, change no files, never push. CLAUDE.md's rules on docs, tests and pushing are for the working chats: know them so the jobs fit.
- Unsure? Check the repo (git show origin/native:<file>, git grep, line ranges). Never guess a number, a file name or what the app does.
- Plain, short words; the outcome first. The owner is not a programmer or a physicist; this app is a hobby. Explain what an update gives the user; explain technical words.
- Never say "fixed" because a report says so: "the report says" until the owner has seen it on his PC.
- Do not hand over the next job unasked: report, give a view, ask. One recommendation and one or two alternatives, concrete enough to say yes to.
- The Black Hole Lab: do not push the owner back into it; its look is discussed only if he raises it. Honest, normal tone.

## Saving usage
- Read only: CLAUDE.md, RESUME.md, native/REPORT.md, and ROADMAP.md / DECISIONS.md where needed; other files by grep and line ranges.
- The owner sends the benchmark and gpu-paths-log.txt as files: read the SCENES table, the guards list and the [vulkan] lines that matter, not the whole file.

## Repos
- ShelaShelby/physics-lab: PUBLIC, the app, one branch `native`. ShelaShelby/physics-lab-sources: PRIVATE, every paper and saved page (working chats attach it). ShelaShelby/physics-lab-archive: PRIVATE, old history, nobody pushes.

## Each round
1. git fetch origin native; read the files above.
2. When an update is out: check the Windows run ("Native Windows build") of the newest commit is green, the "latest" release (pre-release, tag latest) targets it with its version in the name, and native/CMakeLists.txt VERSION was bumped. Say what went in, what was skipped, what the report found.
3. Benchmark: compare scene by scene with the previous run (faster, SLOWER, below 255); check the guards list at its end; ask whether other programs were open.
4. gpu-paths-log.txt (%APPDATA%\PhysicsLab\PhysicsLab): every [vulkan] [<name>] line: passed, switched off or not run; the helper's lines; gpu-helper-variants.txt.

## The owner's PC
AMD Ryzen 5 7500X3D (6 cores / 12 threads), 32 GB; AMD Radeon RX 9060 XT (also an integrated AMD GPU, correctly not chosen); window 2560 x 1369; screen 255 Hz. Driver and Vulkan versions: see the newest benchmark.

## Decisions
DECISIONS.md and CLAUDE.md hold them all; keep them. Before any new job: the owner's benchmark of the previous version.

## Writing a job
- One job = one session = ONE push at the end, "native X.Y.Z". First line "Job: native X.Y.Z: <what>". X.Y for real features, X.Y.Z for small things; prefer small jobs.
- "Follow CLAUDE.md" and exactly which files to read (grep and line ranges). Read the code first so the job names real files; for any move to the graphics card read the processor's code (data, precision, calls) and list what was read; include a feasibility step and a stop rule.
- Say what the user sees, the physics with its limits, and the tests against independent references. Tests first; they must fail on the old build.
- Never put a physics number in a job as a fact. Test limits are fixed before looking; a limit changed after a failure is said openly and re-checked. Vulkan picture limit: at most 1 % of pixels over 8 levels per view, multisampling off for the comparison, validation on, zero errors.
- Speed jobs: measure first, plain waste first, no special case for the benchmark. The working box (4 cores, lavapipe) measures processor ms; for card work it counts pixels/passes; never claim an fps for the owner's PC.
- Anything new on the card: its guard, its log lines, made first by the helper, a start-up check against the processor, the processor as fallback, single precision (double only where queried: shaderFloat64).
- At most 3 tries per problem, then skip and write it in REPORT.md and ROADMAP.md.
- A line about Windows (stricter than Linux: includes, CRLF, CreateProcess) and AMD (stricter than Mesa: initialised variables, no implicit int/float, only queried features, exact barriers).
- End: docs (REPORT.md at most 40 lines, changelog.cpp, RESUME.md, PLANNER.md if needed, ROADMAP.md, DECISIONS.md if the owner decided something, the plan files it touches, VERSION), check.sh quick plus the ui_* tests of the changed screens, no known failure left, the commit, ONE push, the Windows run and the "latest" release.

## What the owner wants
"Accurate" is the floor, not the goal: choose features by "would the owner enjoy looking at this?"; "is it correct and honest?" is the rule every feature must pass. Realistic, researched, labelled where unknown; no fake visuals. Smooth: 255 Hz everywhere on his PC. What he loves most: the realistic atoms.
