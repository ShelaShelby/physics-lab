#!/bin/sh
# check.sh quick|fix '<regex>'|full [build dir]   (0.15 stage 4.1; fix: 0.22)
#   quick: the unit test programs, the shader check and the source scan: about 1-3 min, no app start. EVERY job runs this (CLAUDE.md). All modes first run the guard against
#          papers / saved pages in this public repo and tools/check_sources.py (skipped without PL_SOURCES_DIR, the checkout of ShelaShelby/physics-lab-sources).
#   fix  : quick + the picture / UI tests whose names match the regex, e.g.  check.sh fix 'ui_atom|ui_look' (optional: re-running the failures of a full run)
#   full : quick + every picture / UI test, all in parallel (one process per core), about 35 min on 4 cores (0.38.1). Only at a batch checkpoint: a new version X.Y or when the owner asks.
# A single test: ctest --test-dir <build dir> -R '^name$' --output-on-failure   (names: ctest -N)
set -e
mode=${1:-quick}
if [ "$mode" = fix ]; then pat=${2:?usage: check.sh fix '<regex>' [build dir]}; b=${3:-$(dirname "$0")/../../build}; else b=${2:-$(dirname "$0")/../../build}; fi
[ -d "$b" ] || b=/home/user/build
# 0.54.4: this repo is public. No paper, scan, preprint, saved web page or paper text may be under native/ (they live in the private repo ShelaShelby/physics-lab-sources,
# native/data/SOURCES.md); the guard fails when one is found (a .txt counts as paper text when pdftotext is named in its first two lines). The provenance check runs with PL_SOURCES_DIR (the checkout of that repo) and is skipped without it.
native=$(cd "$(dirname "$0")/.." && pwd)
stray=$(find "$native" -type f \( -name '*.pdf' -o -name '*.html' -o -name '*.htm' -o -name '*.wikitext' -o -name '*.tar.gz' -o -name '*.txt.gz' \) 2>/dev/null)
stray="$stray$(find "$native/data" -type f -name '*.txt' ! -name '*.extract.txt' ! -name 'README*' 2>/dev/null | while read -r f; do if head -n 2 "$f" | grep -qi 'pdftotext'; then echo "$f"; fi; done)"
if [ -n "$stray" ]; then
  echo "check.sh: FAILED: papers, scans, saved web pages (.pdf .html .htm .wikitext .tar.gz .txt.gz) and extracted paper text must not be in this public repo:"
  echo "$stray" | sed 's/^/  /'
  echo "They belong in the private repo ShelaShelby/physics-lab-sources (attach it with add_repo, push to its main, same relative path under data/); this repo gets only the extract a test needs and a line in native/data/SOURCES.md (see CLAUDE.md, Sources)."
  exit 1
fi
python3 "$native/tools/check_sources.py" || exit 1
n=$(nproc 2>/dev/null || echo 2)
mkdir -p "$b/mesa_cache"
export PL_JOBS=1   # ctest already runs one test per core; a single test run by hand uses every core instead
case $mode in
  quick) exec ctest --test-dir "$b" -L quick -j"$n" --output-on-failure --timeout 600 ;;
  fix)   ctest --test-dir "$b" -L quick -j"$n" --output-on-failure --timeout 600 && exec ctest --test-dir "$b" -R "$pat" -LE quick -j"$n" --output-on-failure --timeout 3000 ;;
  full)  exec ctest --test-dir "$b" -j"$n" --output-on-failure --timeout 3000 ;;
  *) echo "usage: check.sh quick|fix <regex>|full [build dir]"; exit 2 ;;
esac
