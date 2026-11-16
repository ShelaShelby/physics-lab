# Resume

## Current state
- 0.55.7 (16 Nov 2026): a guard round the graphics card's light paths (app/gpu_guard: a marker file per step beside the settings file until the path has worked once for the app version and driver; a marker left behind, or a step over 10 s, switches the path off for that pair, kept in the settings file, Settings, Performance names the step with Try again); the start-up check a step per frame, only from the view's third frame; the first fill piece by piece from 64 rays; the log gpu-paths-log.txt beside the settings file (Settings opens its folder; the benchmark report ends with its last 40 lines). Owner's Radeon crash in 0.55.6: cause not yet known, waiting for his log; small version.
- Last full UI run: 0.55.0 (the commits "native 0.55.0"), 33 of 33 after fixing ui_look (check_5t solid) and ui_popups (check_5k windows) and re-running those two.
- Next: the rest of PLAN_BLACKHOLE.md step 4 (quality levels High and Ultra, a faster fine table), then step 5, moving cameras; "Show special radii" is a ROADMAP line. Element jumps: paused (ROADMAP.md, "Waiting for the owner").
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
