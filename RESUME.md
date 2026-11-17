# Resume

## Current state
- 0.56.4 (17 Nov 2026): moving gas, step 1 (PLAN_MOVINGGAS.md section 7), physics only, no change on screen: src/blackhole/gas.h (the clocks, the clump of Broderick & Loeb 2006, the SKH06 pattern, the integrator with t; moved from tools/movinggas, which calls it), the azimuth and light travel time at each disc crossing in closed form (kerr_closed.h, every radial case), stored in the processor's spin table; tests in blackhole_tests (testGas, testClosedTime).
- Last full UI run: 0.56.0 (the commit "native 0.56.0"), 33 of 33, nothing to fix. (0.56.1 to 0.56.4: not run, small versions.)
- Next: the moving gas (owner's choice of 17 Nov 2026), PLAN_MOVINGGAS.md section 7, step 2 (the card, the time control and clocks, the labels, the benchmark scene); then the rest of PLAN_BLACKHOLE.md step 4 (quality levels) and step 5, moving cameras. Element jumps: paused (ROADMAP.md, "Waiting for the owner").
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
