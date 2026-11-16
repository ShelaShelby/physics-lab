# Resume

## Current state
- 0.56.0 (16 Nov 2026): the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms in single precision (render/shaders.h kKerrSkyFS, render/kerr_sky.h, blackhole/sky_card.h; its own guard gpusky=, steps in rising order, a start-up check against kerr_closed.h in double); the exposure found on the card in the same frame (spin 0 too); a lower resolution while moving when the pass is slow; the stars still from the processor (they trail while moving; ROADMAP); the processor's tables stay as reference and fallback (Settings, Performance switch); old forms of the gpupaths= line read as off; spin 0's moving disc table 3x faster.
- Last full UI run: 0.56.0 (the commit "native 0.56.0"), 33 of 33, nothing to fix.
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
