# Resume

## Current state
- 0.68.0 (20 Nov 2026): speed batch 5: on Vulkan the Orbit Lab's trails, orbits and distance rings (kLinesCS/kLinesVS) and the Molecule Lab's Density surface (kSurfaceCS/kSurfaceVS, sorted on the card) drawn by the card, each checked against the processor's at start; the crash-safe helper makes every guarded program's pipelines and first small run (sky, lens, light paths: pipelines only); the Black Hole Lab's stars not moved (they need the processor's tables: ROADMAP).
- Last full UI run: 0.68.0 (its fixes were not re-run as a whole suite).
- Failing now: ui_molecule (check_molecule card), ui_blackhole (smooth), ui_popups (benchscenes film_tilt/all_light, benchwaits), all three also failing in 0.67.0; check.sh quick was 17 of 18 at the 0.68.0 push (the 18th was stopped, not failed). The next job starts by making these pass or by writing in ROADMAP.md why they cannot on this box.
- Next: the owner's Vulkan benchmark (Hard) on his PC; then all the speed work on Vulkan, slowest scene first (DECISIONS 18 Nov 2026); OpenGL removed when the owner decides. Element jumps: paused (ROADMAP.md, "Waiting for the owner").
- Workflow (details in CLAUDE.md): `native/tools/check.sh quick` plus the ui_* groups of the changed screens for every job; `check.sh full` (35-45 min) only when the owner asks.

## How work flows
- Three roles: a planning chat (the lead: architecture, design, what comes next) writes each job; it writes no code and does not push. An Opus session builds the job and leads its Haiku helpers (CLAUDE.md, "Two models"); Haiku helps.
- The owner pastes the written job into a new Claude Code session (branch box `native`).
- Three repos: ShelaShelby/physics-lab (public: the app; one branch `native`, history starts at 15 Nov 2026), ShelaShelby/physics-lab-archive (private: the full history before 15 Nov 2026, read-only for jobs) and ShelaShelby/physics-lab-sources (private: papers, scans, saved pages; a session that saves or reads a source attaches it with add_repo and pushes to its `main`; CLAUDE.md, Sources).
- ONE push per job, at the end, once tests and docs are final: no checkpoint pushes, no doc-only pushes afterwards (owner decision 20 Nov 2026; CLAUDE.md, "Pushes").
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
