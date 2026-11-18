# Resume

## Current state
- 0.63.0 (18 Nov 2026): Vulkan, step 5 of 5 (native/PLAN_VULKAN.md): Vulkan the default, OpenGL the backup; the benchmark on Vulkan (timestamp queries, report names the API and the guards' states); new Hard scene atom.slice_super_drag; one guarded step open at a time in the whole process (the guard's wrong-program fix); steps 1-3 on dynamic rendering and synchronization2. OpenGL's removal waits for the owner.
- Last full UI run: 0.63.0 (the commit "native 0.63.0"), on Vulkan; see native/REPORT.md for the checks left failing (check_6b benchscenes film_tilt, benchwaits, all_light).
- Next: the owner's Vulkan benchmark (Hard) on his PC; then all the speed work on Vulkan, slowest scene first (DECISIONS 18 Nov 2026); OpenGL removed when the owner decides. Element jumps: paused (ROADMAP.md, "Waiting for the owner").
- Workflow (details in CLAUDE.md): `native/tools/check.sh quick` for every job; `check.sh full` (about 35 min) only at a new version X.Y or when the owner asks.

## How work flows
- Three roles: a planning chat (the lead: architecture, design, what comes next) writes each job; it writes no code and does not push. An Opus session builds the job and leads its Haiku helpers (CLAUDE.md, "Two models"); Haiku helps.
- The owner pastes the written job into a new Claude Code session (branch box `native`).
- Three repos: ShelaShelby/physics-lab (public: the app; one branch `native`, history starts at 15 Nov 2026), ShelaShelby/physics-lab-archive (private: the full history before 15 Nov 2026, read-only for jobs) and ShelaShelby/physics-lab-sources (private: papers, scans, saved pages; a session that saves or reads a source attaches it with add_repo and pushes to its `main`; CLAUDE.md, Sources).
- The session works on its own side branch and pushes to `native` with `git push origin HEAD:refs/heads/native` (check with `git ls-remote origin refs/heads/native`). It is a plain push, never a PR.
- Quiet chat: the session writes in chat only when it cannot go on without the owner's answer, and once at the end (the outcome first). Everything else goes in native/REPORT.md.
- Each push is checked by the planning chat in a lean way: fresh clone, build, only the changed screens and REPORT.md (not every test program each time). After that come the GitHub build and the release tagged `latest`. The release is a Pre-release while the stage is alpha or beta.
- GitHub Actions builds the Windows zip only on pushes that change native/** or the workflow.

## Rules that matter most
- The accuracy and honesty rules in CLAUDE.md.
- Opus leads the session; Haiku helpers do routine work, spot-checked by Opus (CLAUDE.md, "Two models").
- Quiet chat (CLAUDE.md, first line). One task per session.

## Keeping this file up to date
- "Current state" holds only the newest version: each job rewrites its line (older versions are in changelog.cpp and git).
