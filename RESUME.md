# Resume

## Current state
- 0.54.4 (15 Nov 2026): housekeeping, nothing changed in the app. The repo is public now: the saved papers, scans, preprints, saved web pages and unclear-licence downloads (541 files, 116 MB) moved to the private repo ShelaShelby/physics-lab-sources (data/<same path>); here stay the extracts the tests read (26 files), native/data/SOURCES.md and tools/check_sources.py (run by check.sh quick; needs PL_SOURCES_DIR). Published as one fresh commit in the new public repo (15 Nov 2026); the history is in the private archive. UI suite not run.
- Last full UI run: 0.54.0 (the commit "native 0.54.0"), 31 of 31 passed. Next full run at 0.55.
- Next: the lab itself, 0.55.0 (PLAN_BLACKHOLE.md step 1, the screen: a fourth tab, the non-spinning hole in front of our sky, presets, the Orbit Lab's link), built on src/blackhole/. Element jumps: paused (ROADMAP.md, "Waiting for the owner").
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
