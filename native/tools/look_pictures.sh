#!/bin/sh
# 0.14 stage 2: the 6 main screens plus the Orbit Lab guide window, for the themes given, to LOOK at (not a test). The real app, headless (SDL offscreen, Mesa llvmpipe).
#   look_pictures.sh <PhysicsLab binary> <output folder> theme ...
set -e
BIN="$1"; OUT="$2"; shift 2
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd); BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")
H="$OUT/home"; mkdir -p "$H"; cd "$OUT"
export HOME="$H" SDL_VIDEODRIVER=offscreen PL_WINDOW=1000x640 PL_MOTION=on PL_FIXED_DT=0.016 PL_DYK_FIRST=3 PL_DATE=2026-10-05
shot() { name="$1"; theme="$2"; shift 2; rm -rf "$H/.local" "$H/.config"; env PL_THEME="$theme" "$@" PL_EXIT_AFTER_FRAMES=40 PL_SCREENSHOT="$OUT/${name}_${theme}.bmp" "$BIN" > "$OUT/log_${name}_${theme}.txt" 2>&1; }
for th in "$@"; do
    shot orbit "$th"
    shot nucleus "$th" PL_ATOM=1 PL_ATOM_DO=nucleus
    shot cloud3d "$th" PL_ATOM=1 "PL_ATOM_SET=Z=26;view=Cloud 3D;orb=4f"
    shot chart "$th" PL_ATOM=1 "PL_ATOM_SET=Z=92;view=Chart of nuclides"
    shot settings "$th" PL_OPEN=settings
    shot themes "$th" PL_OPEN=themes
    shot guide "$th" PL_LEARN=1
done
