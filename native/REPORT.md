# 0.54.4 (no version change): the clean tree published to the new public repo ShelaShelby/physics-lab

No app change; no build or tests run here (the tree passed `check.sh quick` as 0.54.4; only the docs below differ from the archive's `native` tip 79c3670). UI suite not run.

## Checked before publishing
- Verified: archive repo private; ShelaShelby/physics-lab public, no branches/commits, push access; tip of archive `native` = "native 0.54.4".
- Export of that tip with `git archive` (no .git, no history, no other branch or tag): 1065 files.
- Guard of check.sh (no .pdf .html .htm .wikitext .tar.gz .txt.gz under native/): 0 found. Paper-text .txt guard: not run separately (check.sh not run).
- `check_sources.py`: runs, skipped (no sources checkout).
- Secrets: 0 tokens/keys/private-key blocks, 0 URLs with tokens, 0 passwords, 0 home-folder paths with a person's name (`/home/user/...` only, in CLAUDE.md and tests), no owner e-mail or name.
- E-mail addresses: 4 found, all third-party public contacts, **kept** (saved as received or a licence text that must stay verbatim; none is the owner's): Jon.D.Giorgini@jpl.nasa.gov in the header of 24 JPL Horizons files (native/data/jpl/...); rasmus@notion.se and a debian address in native/assets/fonts/OFL-Inter.txt (licence text); colour-developers@colour-science.org in 2 colour-science_v0.4.7 files (native/data/stars, native/data/planets). The owner may decide to remove them; editing the Horizons files changes their sha256.

## Docs (only wrong lines)
RESUME.md (current state, "How work flows": three repos), DECISIONS.md (the 15 Nov decision), ROADMAP.md (deleted the "owner decides" line about old commits). native/README.md: nothing wrong. No changelog entry.

## Workflow
.github/workflows/native-windows.yml: unchanged. It names no repo (uses GITHUB_REPOSITORY); `permissions: contents: write`; the "latest" release step tolerates a failed `gh release delete`, and `gh release create latest --target <sha>` makes the tag on the first run.

## Open for the owner (from the 0.54.4 report, unchanged)
NUBASE file, NIST ASD, CIE tables, `visscher_table4.extract.txt`, `gaia_sky.jpg` (CC BY-SA 3.0 IGO): licence questions, still not decided.
