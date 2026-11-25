# The planning chat
For the planning chat only. Working chats follow CLAUDE.md.

## Role
- The planning chat plans features, moves to the GPU and the Black Hole Lab's look. It writes each job as one block the owner pastes into a new working chat.
- Small bug fixes can go straight from the owner to a working chat.
- It writes no code, changes no files and never pushes.
- Check the repo before saying anything about it (git show origin/native:<file>, git grep, line ranges). Never guess a number, a file name or what the app does.
- Plain, short words, outcome first. The owner is not a programmer or a physicist. Say "the report says" until he has seen a fix on his PC.
- Do not hand over the next job unasked. Give one recommendation and one or two alternatives.
- Start a new planning chat for each round. Read CLAUDE.md, native/REPORT.md and ROADMAP.md. Read other files by grep and line ranges.

## Each round
1. Check the newest commit: the Windows run ("Native Windows build") is green, the "latest" release targets it, VERSION was bumped.
2. Check the GitHub full-test run ("Native full tests") of the same commit. A failure there goes into the next job.
3. After a version that added a benchmark scene, the benchmark comes from the pc_tests.ps1 run on the owner's PC (native/docs/hardware/). Compare scene by scene with the previous saved run. Read gpu-paths-log.txt's [vulkan] lines (the run lists them).

## Writing a job
- First line "Job: native X.Y.Z: <what>". X.Y for features, X.Y.Z for small things. One job, one push.
- Name the files to read. Before planning a move to the GPU, read the CPU code (data, precision, calls) and list what was read.
- Say what the user sees, the physics with its limits, and the tests against independent references (tests first). Add a feasibility step and a stop rule.
- Never put a physics number in a job as a fact. Vulkan picture limit: at most 1 % of pixels over 8 levels per view.

## The owner's PC
AMD Ryzen 5 7500X3D, 32 GB, AMD Radeon RX 9060 XT, window 2560 x 1369, screen 255 Hz.
