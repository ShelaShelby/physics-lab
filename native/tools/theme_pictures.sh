#!/bin/sh
# 0.14 stage 1: pictures of the main screens of the REAL app (built with -DPL_BUILD_APP=ON), drawn headless by Mesa's llvmpipe
# (SDL_VIDEODRIVER=offscreen, no window and no graphics card), for every theme, dark and light.
#
#   theme_pictures.sh <PhysicsLab binary> <output folder> [theme ...]
#
# Writes <screen>_<theme>.bmp for 6 screens (Orbit Lab, Atom Lab nucleus, Cloud 3D, chart of nuclides, Settings, theme picker)
# and a manifest <output folder>/pictures.sha256 (one line per picture: the SHA-256 of its pixels, then its name).
# "auto" is Auto with the device in dark mode, "auto_light" is Auto with the device in light mode.
# The app only draws the same pixels twice on the same Mesa; the manifest in tests/reference was made on Mesa 25.2.8 llvmpipe (LLVM 20.1.2).
# Not a speed test, and not Windows.
set -e
BIN="$1"; OUT="$2"; shift 2
THEMES="${*:-auto auto_light light dark black midnight solar paper sage contrast synthwave monochrome}"
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd); BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")
H="$OUT/home"; mkdir -p "$H"
cd "$OUT"
export HOME="$H" SDL_VIDEODRIVER=offscreen PL_WINDOW=1000x640 PL_MOTION=on PL_FIXED_DT=0.016 PL_DYK_FIRST=3 PL_DATE=2026-10-05
shot() {   # name theme env...
    name="$1"; theme="$2"; shift 2
    light=""; t="$theme"
    if [ "$theme" = auto_light ]; then t=auto; light=1; fi
    rm -rf "$H/.local" "$H/.config"        # no saved settings from the last picture
    if [ -n "$light" ]; then
        env PL_LIGHT=1 PL_THEME="$t" "$@" PL_EXIT_AFTER_FRAMES=40 PL_SCREENSHOT="$OUT/${name}_${theme}.bmp" "$BIN" > "$OUT/log.txt" 2>&1
    else
        env PL_THEME="$t" "$@" PL_EXIT_AFTER_FRAMES=40 PL_SCREENSHOT="$OUT/${name}_${theme}.bmp" "$BIN" > "$OUT/log.txt" 2>&1
    fi
}
for th in $THEMES; do
    shot orbit    "$th"
    shot nucleus  "$th" PL_ATOM=1 PL_ATOM_DO=nucleus
    shot cloud3d  "$th" PL_ATOM=1 "PL_ATOM_SET=Z=26;view=Cloud 3D;orb=4f"
    shot chart    "$th" PL_ATOM=1 "PL_ATOM_SET=Z=92;view=Chart of nuclides"
    shot settings "$th" PL_OPEN=settings
    shot themes   "$th" PL_OPEN=themes
done
(cd "$OUT" && for f in *.bmp; do printf '%s  %s\n' "$(tail -c +55 "$f" | sha256sum | cut -d' ' -f1)" "${f%.bmp}"; done > pictures.sha256)
echo "wrote $(wc -l < "$OUT/pictures.sha256") pictures to $OUT"
