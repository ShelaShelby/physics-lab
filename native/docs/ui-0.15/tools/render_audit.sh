#!/bin/sh
# usage: render.sh <outdir> <WxH> <textsize> <theme> [screen ...]   (probe build; writes <screen>.bmp and <screen>.probe)
BIN=${PL_PROBE_BIN:?set PL_PROBE_BIN to the probe build of PhysicsLab (see README.md)}
OUT=$1; WIN=$2; TS=$3; TH=$4; shift 4
mkdir -p "$OUT/home"; cd "$OUT"
export HOME="$OUT/home" SDL_VIDEODRIVER=offscreen PL_WINDOW=$WIN PL_MOTION=on PL_FIXED_DT=0.016 PL_DYK_FIRST=3 PL_DATE=2026-10-05 PL_THEME=$TH PL_TEXTSIZE=$TS
run() { n=$1$SFX; shift; rm -rf "$HOME/.local" "$HOME/.config"
  env "$@" PL_PROBE="$OUT/$n.probe" PL_EXIT_AFTER_FRAMES=40 PL_SCREENSHOT="$OUT/$n.bmp" "$BIN" > "$OUT/$n.log" 2>&1; }
for s in "$@"; do case $s in
 orbit) run orbit X=1;;
 orbit_sel) run orbit_sel PL_SELECT=Earth;;
 atom_orbit) run atom_orbit PL_ATOM=1;;
 nucleus) run nucleus PL_ATOM=1 PL_ATOM_DO=nucleus;;
 cloud) run cloud PL_ATOM=1 "PL_ATOM_SET=Z=26;view=Cloud 3D;orb=4f";;
 slice) run slice PL_ATOM=1 "PL_ATOM_SET=Z=1;view=Slice;orb=3d";;
 chart) run chart PL_ATOM=1 "PL_ATOM_SET=Z=92;view=Chart of nuclides";;
 landscape) run landscape PL_ATOM=1 PL_ATOM_TILT=1 "PL_ATOM_SET=Z=92;view=Chart of nuclides";;
 learn) run learn PL_PANE=1;;
 ach) run ach PL_PANE=2;;
 settings) run settings PL_OPEN=settings;;
 themes) run themes PL_OPEN=themes;;
 about) run about PL_OPEN=about;;
 history) run history PL_OPEN=history;;
 lablog) run lablog PL_OPEN=log;;
 guide_orbit) run guide_orbit PL_LEARN=1;;
 guide_atom) run guide_atom PL_ATOM=1 PL_ATOM_LEARN=1;;
esac; done
