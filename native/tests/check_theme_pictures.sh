#!/bin/sh
# 0.14 stage 1 test: the main screens of the REAL app (Linux build with -DPL_BUILD_APP=ON, headless on Mesa's llvmpipe) must draw exactly the pixels they drew
# before 0.14 for every theme that existed then, dark and light; Frutiger Aero must draw without a crash, with the fallback font when Segoe UI is missing.
#
#   check_theme_pictures.sh <PhysicsLab binary> <work folder> <tests/reference folder> <tools folder>
#
# Reference files (made by tools/theme_pictures.sh):
#   theme_pictures_0.13.sha256   the SHA-256 of the pixels of every picture the 0.13 code drew (Mesa 25.2.8 llvmpipe, LLVM 20.1.2).
#                                Five screens must still match it for all 12 theme variants (Orbit Lab, Atom Lab nucleus, Cloud 3D, chart, Settings).
#   theme_pictures_0.14_picker.sha256   the theme picker, which got its 12th card (Frutiger Aero) in 0.14 on purpose.
# A different Mesa may round differently: then regenerate both files from the OLD code first (see REPORT.md of 0.14 stage 1), never edit a hash by hand.
set -e
BIN="$1"; WORK="$2"; REF="$3"; TOOLS="$4"
rm -rf "$WORK"; mkdir -p "$WORK"
sh "$TOOLS/theme_pictures.sh" "$BIN" "$WORK" > "$WORK/run.txt"
fail=0
for ref in theme_pictures_0.13.sha256 theme_pictures_0.14_picker.sha256; do
    while read -r hash name; do
        [ -z "$name" ] && continue
        case "$ref:$name" in
            theme_pictures_0.13.sha256:themes_*) continue ;;        # the picker has its own file
            theme_pictures_0.14_picker.sha256:themes_*) ;;
            theme_pictures_0.14_picker.sha256:*) continue ;;
        esac
        got=$(grep "  $name\$" "$WORK/pictures.sha256" | cut -d' ' -f1)
        if [ "$got" != "$hash" ]; then echo "DIFFERENT PICTURE: $name"; fail=$((fail + 1)); fi
    done < "$REF/$ref"
done
# Frutiger Aero: draws, does not crash, and (no Segoe UI here) uses the default font
sh "$TOOLS/theme_pictures.sh" "$BIN" "$WORK/aero" aero > /dev/null
n=$(ls "$WORK"/aero/*_aero.bmp | wc -l)
[ "$n" = 6 ] || { echo "Aero: expected 6 pictures, got $n"; fail=$((fail + 1)); }
if grep -q "Font: .*segoeui" "$WORK/aero/log.txt"; then echo "Aero used a font file that should not exist here"; fail=$((fail + 1)); fi
grep -q "Font: " "$WORK/aero/log.txt" || { echo "Aero: no font line in the log"; fail=$((fail + 1)); }
if [ "$(grep -c '_aero$' "$WORK/aero/pictures.sha256")" != 6 ] || [ "$(grep '^' "$WORK/aero/pictures.sha256" | cut -d' ' -f1 | sort -u | wc -l)" != 6 ]; then
    echo "Aero: the six pictures are missing or not all different"; fail=$((fail + 1)); fi
# and the Aero pictures are not the dark theme's (the theme really did something)
if grep -q "$(grep ' orbit_dark$' "$WORK/pictures.sha256" | cut -d' ' -f1)" "$WORK/aero/pictures.sha256"; then echo "Aero looks like Dark"; fail=$((fail + 1)); fi
echo "$fail problems"
[ "$fail" = 0 ]
