# Bundled fonts

Two fonts are built into PhysicsLab.exe for the Settings → Font choices of the web version
(`PL_FONTS` in index.html). Both are free to ship under the SIL Open Font License 1.1 (the
licence texts are next to them):

| Choice | Font | Files |
| --- | --- | --- |
| Easy to read | Atkinson Hyperlegible (Braille Institute of America) | `AtkinsonHyperlegible-{Regular,Bold,Italic}-{latin,latin-ext}.ttf` |
| Rounded | Nunito (The Nunito Project Authors) | `Nunito-{Regular,Bold,Italic}-{latin,latin-ext}.ttf` |

They come from the npm packages `@fontsource/atkinson-hyperlegible` and `@fontsource/nunito`
(version 5.3.0), as WOFF files split by alphabet, and were turned into TrueType with fontTools
(only the container changes, not the font). The other choices use fonts every Windows PC has,
so they are not shipped: Theme default = Segoe UI, Serif = Georgia (also the Paper theme's book
font), Monospace = Consolas (also the Monochrome theme's font).
