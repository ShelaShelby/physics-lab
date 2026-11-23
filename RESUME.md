# Resume

## Current state
- 0.75.1 (23 Nov 2026): Saturn's rings each at their own viewing slant (orbit/ring_draw.h ringMu: the camera is at one height h over the plane, mu = h / distance; the card's kRingsFS takes the means of Cassini's measurements over each band at every pixel's mu, the processor the same at its triangles' corners; bands no longer carry one opacity and light); the benchmark's card times are tagged with their frame (frames left out of the count are left out of them); the Orbit Lab's rest line names the part of its picture key that changed when it is drawn anew at rest (not reproduced on the box: the owner's PC still did in 0.75.0). 0.75.0: the 3D pinhole camera (orbit/camera.h, kFieldOfViewDeg 45), real depth, benchmark scene orbit.flyby.
- Last full UI run: 0.69.0 (the commit "native 0.69.0"); failing in it: ui_search (check_5l labels) and ui_atom_layout (check_5h cards: the radial card read before the card's outline came), both fixed and re-run; ui_blackhole smooth (the card count, ROADMAP: the one-in-two limit; its sky-guard checks fixed and re-run); ui_popups orbitkeep (rest.orbit at 2560 x 1369 drawn anew: fails on 0.68.1 too, the owner's bug, ROADMAP) and benchwaits (passes alone, on 0.68.1 too: the full run's load).
- Next steps: the owner's next benchmark (whenever a version adds a scene) names why rest.orbit is drawn anew (ROADMAP **fix**); ROADMAP's next Alpha Centauri line; the moving gas waits for the owner's new model; the Black Hole Lab's look and speed work resume when the owner says.
- Workflow (details in CLAUDE.md): `native/tools/check.sh quick` for every job; every job: `check.sh quick` plus the ui_* tests of the screens it changes; `check.sh full` (about 35 min) only when the owner asks. One push per job; PLANNER.md and this file kept true by every job.

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
