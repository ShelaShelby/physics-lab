# Resume

## Current state
- 0.55.8 (16 Nov 2026): the spinning hole's rays on the processor in Mino time (blackhole/mino.h: u = 1/r, mu = cos theta, polynomial equations, followed to infinity; rays within sin^2 theta < 1e-3 of the axis keep KerrBL) and a tolerance that grows away from the shadow's edge (spin_sky.h SpinFill): the moving table x3.7, the fine table x4.7 on this box; Real's saved hash changed on purpose (within one level of 255 of 0.55.7's picture); the benchmark's Black Hole scene goes by the run's clock (its last movement is counted) and reports the sharp picture's time after a stop. Graphics-card light paths unchanged (off on the owner's Radeon); small version.
- Last full UI run: 0.55.0 (the commits "native 0.55.0"), 33 of 33 after fixing ui_look (check_5t solid) and ui_popups (check_5k windows) and re-running those two.
- Next: the rest of PLAN_BLACKHOLE.md step 4 (quality levels High and Ultra), then step 5, moving cameras; "Show special radii" is a ROADMAP line. Element jumps: paused (ROADMAP.md, "Waiting for the owner").
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
