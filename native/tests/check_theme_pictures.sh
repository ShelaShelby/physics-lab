#!/bin/sh
# 0.14 stage 1 test, changed in stage 2: the main screens of the REAL app (Linux build with -DPL_BUILD_APP=ON, headless on Mesa's llvmpipe) must draw exactly the pixels they
# drew before 0.14 for every theme that existed then, dark and light, EXCEPT what stage 2 changed on purpose; Frutiger Aero and the two Y2K themes must draw without a crash.
#
#   check_theme_pictures.sh <PhysicsLab binary> <work folder> <tests/reference folder> <tools folder>
#
# Reference files (made by tools/theme_pictures.sh):
#   theme_pictures_0.13.sha256   the SHA-256 of the pixels of every picture the 0.13 code drew (Mesa 25.2.8 llvmpipe, LLVM 20.1.2). The five screens (Orbit Lab, Atom Lab
#                                nucleus, Cloud 3D, chart, Settings) must still match it for the 10 variants that were not changed: auto, black, midnight, solar, paper,
#                                sage, contrast, dark, synthwave, monochrome = 50 pictures, 0 differing pixels.
#   theme_pictures_0.14s2_changed.sha256   what stage 2 changed ON PURPOSE, made by the stage 2 code after looking at the pictures (a regression guard, not an independent
#                                reference): the five screens of Light and of Auto in light mode (Light's accent text was 4.24:1 on its page, now 4.57:1: a darker blue, the same
#                                hue), and the theme picker in all 12 variants (it got two new cards, Y2K Ice and Y2K Night).
# A different Mesa may round differently: then regenerate both files from the OLD code first (see REPORT.md of 0.14 stage 1), never edit a hash by hand.
set -e
BIN="$1"; WORK="$2"; REF="$3"; TOOLS="$4"
rm -rf "$WORK"; mkdir -p "$WORK"
sh "$TOOLS/theme_pictures.sh" "$BIN" "$WORK" > "$WORK/run.txt"
fail=0
for ref in theme_pictures_0.13.sha256 theme_pictures_0.14s2_changed.sha256; do
    while read -r hash name; do
        [ -z "$name" ] && continue
        case "$ref:$name" in
            theme_pictures_0.13.sha256:themes_*|theme_pictures_0.13.sha256:*_light) continue ;;   # the picker and Light have their own file
            theme_pictures_0.14s2_changed.sha256:themes_*|theme_pictures_0.14s2_changed.sha256:*_light) ;;
            theme_pictures_0.14s2_changed.sha256:*) continue ;;
        esac
        got=$(grep "  $name\$" "$WORK/pictures.sha256" | cut -d' ' -f1)
        if [ "$got" != "$hash" ]; then echo "DIFFERENT PICTURE: $name"; fail=$((fail + 1)); fi
    done < "$REF/$ref"
done
# the three native themes (Frutiger Aero, Y2K Ice, Y2K Night): draw, do not crash, use the default font here (no Segoe UI), six different pictures, and are not Dark
for th in aero y2kice y2knight; do
    sh "$TOOLS/theme_pictures.sh" "$BIN" "$WORK/$th" "$th" > /dev/null
    n=$(ls "$WORK"/$th/*_$th.bmp | wc -l)
    [ "$n" = 6 ] || { echo "$th: expected 6 pictures, got $n"; fail=$((fail + 1)); }
    if grep -q "Font: .*segoeui" "$WORK/$th/log.txt"; then echo "$th used a font file that should not exist here"; fail=$((fail + 1)); fi
    grep -q "Font: " "$WORK/$th/log.txt" || { echo "$th: no font line in the log"; fail=$((fail + 1)); }
    if [ "$(grep -c "_$th\$" "$WORK/$th/pictures.sha256")" != 6 ] || [ "$(grep '^' "$WORK/$th/pictures.sha256" | cut -d' ' -f1 | sort -u | wc -l)" != 6 ]; then
        echo "$th: the six pictures are missing or not all different"; fail=$((fail + 1)); fi
    # and the pictures are not the dark theme's (the theme really did something)
    if grep -q "$(grep ' orbit_dark$' "$WORK/pictures.sha256" | cut -d' ' -f1)" "$WORK/$th/pictures.sha256"; then echo "$th looks like Dark"; fail=$((fail + 1)); fi
done
echo "$fail problems"
[ "$fail" = 0 ]
