# Decisions (made by the owner)

- The native app is the main version; Windows only; Orbit Lab and Atom Lab only.
- Versions are labelled alpha until the 1.0 beta, then plain release.
- Real planets start from the JPL Horizons state of 1 January 2026 12:00 TDB (the data, tool and .inc files of 0.10.3 stage 1). (3 Oct 2026)
- The relativity tick box in Real defaults to OFF: measured against JPL (0.10.3 stage 1 report) the Sun-potential GR term moves every planet's position away from JPL (Mercury, worst case 1800-2100: 70,044 km with it, 29,230 km without; it gets the perihelion turn right but the motion along the orbit slightly wrong). The box stays and its note says so. (3 Oct 2026)
- Real planet physics stays an option (default Simple) until the owner has tried it on the PC (the list is in OWNER-TODO.md); the default is decided after that. (3 Oct 2026)
- The Moon in Real mode will come from ELP2000-82B, checked against Horizons (3 Oct 2026).
- Stay on OpenGL 3.3; a newer version only for a speed job later, with a fallback.
- Electron cloud defaults: style Glow, density High.
- "Reset to today" keeps the zoom, the Moons/Dwarfs/Comets/Belts switches and Time speed; it resets the selection and Follow.
- Theme picker is a 3-column grid with the description as a tooltip.
- The Milky Way band is smooth and labelled as simplified.
- Code signing comes later; for now the release text explains the Windows warning.
- No connectors, plugins or extra skills for now (they cost usage).
- Coding sessions run on Sonnet 5.5; the strongest model is used only in a fresh short planning chat (for example the 1.0 design plan).
- More electrons: the first level (shells filling with simplified screening, Slater's rules) needs the owner's decision before it is planned.
