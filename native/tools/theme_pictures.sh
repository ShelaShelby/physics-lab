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
THEMES="${*:-auto auto_light light dark black midnight solar paper sage contrast monochrome}"
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd); BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")
cd "$OUT"
export SDL_VIDEODRIVER=offscreen PL_WINDOW=1000x640 PL_MOTION=on PL_FIXED_DT=0.016 PL_DYK_FIRST=3 PL_DATE=2026-10-05
# 0.15 stage 4.1: the themes are drawn in parallel (one background job per theme, at most $JOBS at a time; each with its own HOME and log, so they cannot collide). The pictures are the
# same as one after the other: every start is independent and the app draws the same pixels twice on the same Mesa. log.txt is the last theme's log (as before).
JOBS="${PL_JOBS:-$(nproc 2>/dev/null || echo 2)}"
shot() {   # theme name env...
    th="$1"; name="$2"; shift 2
    H="$OUT/home_$th"; mkdir -p "$H"
    light=""; t="$th"
    if [ "$th" = auto_light ]; then t=auto; light=1; fi
    rm -rf "$H/.local" "$H/.config"        # no saved settings from the last picture
    if [ -n "$light" ]; then
        env HOME="$H" PL_LIGHT=1 PL_THEME="$t" "$@" PL_EXIT_AFTER_FRAMES=40 PL_SCREENSHOT="$OUT/${name}_${th}.bmp" "$BIN" > "$OUT/log_$th.txt" 2>&1
    else
        env HOME="$H" PL_THEME="$t" "$@" PL_EXIT_AFTER_FRAMES=40 PL_SCREENSHOT="$OUT/${name}_${th}.bmp" "$BIN" > "$OUT/log_$th.txt" 2>&1
    fi
}
theme_job() {
    th="$1"
    shot "$th" orbit
    shot "$th" nucleus  PL_ATOM=1 PL_ATOM_DO=nucleus
    shot "$th" cloud3d  PL_ATOM=1 "PL_ATOM_SET=Z=26;view=Cloud 3D;orb=4f"
    shot "$th" chart    PL_ATOM=1 "PL_ATOM_SET=Z=92;view=Chart of nuclides"
    shot "$th" settings PL_OPEN=settings
    shot "$th" themes   PL_OPEN=themes
}
n=0
for th in $THEMES; do
    theme_job "$th" &
    n=$((n + 1))
    if [ "$n" -ge "$JOBS" ]; then wait; n=0; fi
done
wait
for th in $THEMES; do last="$th"; done
cp "$OUT/log_$last.txt" "$OUT/log.txt"
(cd "$OUT" && for f in *.bmp; do printf '%s  %s\n' "$(tail -c +55 "$f" | sha256sum | cut -d' ' -f1)" "${f%.bmp}"; done > pictures.sha256)
echo "wrote $(wc -l < "$OUT/pictures.sha256") pictures to $OUT"
