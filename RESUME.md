# Resume

## Current state
- 0.60.0 (18 Nov 2026): Vulkan, step 2 of 5 (native/PLAN_VULKAN.md): on Vulkan (test) the Orbit, Atom and Molecule Labs draw their pictures: Renderer2D's recording is shared, Renderer2D::endVulkan (render/renderer2d_vk.cpp) draws it through render/vk_draw2d.* with shaders.h's own text as SPIR-V (glslang -R at build time), 4x MSAA, Save image; card programs take the processor's path (step 3); the Black Hole Lab shows a note (step 4); start-up check "renderer2d check". OpenGL unchanged bit for bit.
- Last full UI run: 0.60.0 (the commit "native 0.60.0"), 32 of 34; ui_popups (check_6b benchwaits; benchscenes blackhole.all_light) and ui_molecule (check_molecule card) left, as in 0.59.0 (ROADMAP).
- Next: the owner tries Vulkan (test) on his PC (REPORT.md); then Vulkan step 3, the card programs (ROADMAP "Whole app"); the owner's 0.58.0 benchmark and the Hard-list scenes below his 255 Hz, slowest first; then the moving gas (PLAN_MOVINGGAS.md). Element jumps: paused (ROADMAP.md, "Waiting for the owner").
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
