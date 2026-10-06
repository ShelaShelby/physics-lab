#!/bin/sh
# check.sh quick|full [build dir]   (0.15 stage 4.1)
#   quick: the unit test programs, the shader check and the source scan: about a minute, no app start.
#   full : quick + every picture / UI test, all in parallel (one process per core). Run it ONCE, at the end of a stage.
# A single test: ctest --test-dir <build dir> -R '^name$' --output-on-failure   (names: ctest -N)
set -e
mode=${1:-quick}
b=${2:-$(dirname "$0")/../../build}
[ -d "$b" ] || b=/home/user/build
n=$(nproc 2>/dev/null || echo 2)
mkdir -p "$b/mesa_cache"
export PL_JOBS=1   # ctest already runs one test per core; a single test run by hand uses every core instead
case $mode in
  quick) exec ctest --test-dir "$b" -L quick -j"$n" --output-on-failure --timeout 600 ;;
  full)  exec ctest --test-dir "$b" -j"$n" --output-on-failure --timeout 3000 ;;
  *) echo "usage: check.sh quick|full [build dir]"; exit 2 ;;
esac
