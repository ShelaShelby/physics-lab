# Native Physics Lab 0.9.6 (alpha) — report

Atom Lab: quarks readable on light themes. Drawing only: no physics, sizes, positions or texts changed. Older reports are in git history. `index.html` was not touched.

## What changed (`atom/atom_view.cpp`, new `atom/quark_light.h`)
- **Where the backdrops were.** Both dark backdrops (a dark disc behind a whole nucleus in the Cinematic quark view, and one behind each nucleon) are drawn **only for Cinematic quarks**, not for Classic or Accurate (those had none). Cinematic is the default quark style.
- **Light themes (Light, Paper: the only two with a light page; Monochrome also counts as light):** no backdrop. The quarks, their halos, the gluon field, the gluon passing a colour, the sea pairs and the quark points are drawn with ordinary alpha blending instead of added-up light, in colours mixed 35 % towards black (`kLightQuarkDark`; the same idea as the cloud glow in `cloud_light.h`). The "u / d" letters use the theme's text colour there (they were white on the backdrop). The Accurate style on a light page gets the darker colours too (its blending was already ordinary).
- **Dark themes: not changed, on purpose, including the backdrop.** The task says both "remove both backdrops" and "dark themes: keep the glow exactly as it looks today". I kept the second: on a dark page the backdrop also hides the nucleon's own pink/grey disc behind the glow, so removing it would change today's look. If you want it gone on dark themes too, it is the two `!css.light` / `add` conditions in the code. Dark colours and blending are the same numbers as before (the colour constants moved to `quark_light.h`).
- **No "Quark backdrop" setting added:** every light theme looked acceptable, so, as the task says, there is no setting.
- Test hook only: `PL_ATOM_FOCUS` is now read before `PL_ATOM_DO` in `main.cpp`, so `PL_ATOM_DO=nucleon` can dive into a nucleon.
- Version 0.9.6, 🕰 history entry, README.

## Tests (`app_tests` 1,180 checks; `orbit_tests` 1,369 and `atom_tests` 17,818 unchanged; all pass; no saved reference changed)
- New: for every theme with a light page (Light, Paper, Monochrome), each of the three darkened colour charges is darker than the page and card and has a WCAG contrast ratio of at least 3:1 against both (and better than the undarkened colour); the three colour charges stay more than 80 apart in RGB. These are numbers; **they do not prove how it looks.**

## Looked at (Linux, Xvfb, software OpenGL; Helium-4, one nucleon, "+ Sea" detail)
Six images in two groups plus one that was wasted (the first two attempts were not at the quark zoom, so I fixed the test hook).
- **Paper:** Cinematic: soft red, green and blue halos and bright coloured quarks with letters, no dark patch; the three colours are clearly different, in spite of Paper's red-brown accent. Accurate: small, pale points and rings; readable but faint. Classic: yellow and teal balls with grey tubes; fine.
- **Light:** Cinematic: good, like Paper. Accurate: faint but readable (the colour dots are small).
- **Dark:** Cinematic, Accurate, Classic: as before (the Cinematic backdrop is still there). The dark pictures were taken after the change but not compared with older pictures pixel by pixel (the flicker depends on the clock).
- Not looked at: Monochrome (greys only: the three colour charges differ only in brightness there, as before), a second Paper size, other nuclei, and the final refactor (colour constants moved to a header) was checked by the build and the tests only, not by a new picture.

## Not tested
- No Windows PC and no GPU: nothing about speed or the real-screen look.
