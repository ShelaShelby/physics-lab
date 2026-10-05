# Bundled fonts

Two fonts are built into PhysicsLab.exe for the Settings → Font choices of the web version
(`PL_FONTS` in index.html). Both are free to ship under the SIL Open Font License 1.1 (the
licence texts are next to them):

| Choice | Font | Files |
| --- | --- | --- |
| Easy to read | Atkinson Hyperlegible (Braille Institute of America) | `AtkinsonHyperlegible-{Regular,Bold,Italic}-{latin,latin-ext}.ttf` |
| Rounded | Nunito (The Nunito Project Authors) | `Nunito-{Regular,Bold,Italic}-{latin,latin-ext}.ttf` |

0.14 stage 2 adds one more, for the titles of the two Y2K themes only (the body text keeps its normal font):

| Use | Font | Files |
| --- | --- | --- |
| Window titles and the app name in Y2K Ice / Y2K Night | Michroma (The Michroma Project Authors), a wide extended face | `Michroma-Regular-latin.ttf`, licence `OFL-Michroma.txt` |

Michroma is SIL Open Font License 1.1, copied unchanged from the Google Fonts repository (`ofl/michroma/Michroma-Regular.ttf` and `ofl/michroma/OFL.txt`,
github.com/google/fonts, fetched 5 Oct 2026). It is not split or converted; the file is the whole font.

The first two come from the npm packages `@fontsource/atkinson-hyperlegible` and `@fontsource/nunito`
(version 5.3.0), as WOFF files split by alphabet, and were turned into TrueType with fontTools
(only the container changes, not the font). The other choices use fonts every Windows PC has,
so they are not shipped: Theme default = Segoe UI, Serif = Georgia (also the Paper theme's book
font), Monospace = Consolas (also the Monochrome theme's font).
