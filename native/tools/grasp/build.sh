#!/bin/sh
# Builds GRASP2018 (C. Froese Fischer, G. Gaigalas, P. Joensson, J. Bieron, Comput. Phys. Commun. 237, 184 (2019); MIT licence)
# at a pinned commit, OUTSIDE the repository. Usage: tools/grasp/build.sh [dir]   (default $HOME/grasp; binaries in <dir>/bin)
# Needs: apt-get install -y gfortran cmake libblas-dev liblapack-dev libopenmpi-dev openmpi-bin   (about 3 min on 4 cores; 0.47.4: with MPI it also builds the *_mpi programs GRASP_NP uses)
set -e
MP=$(cd "$(dirname "$0")" && pwd)/multipoles
DIR=${1:-$HOME/grasp}
COMMIT=9006157730a82ac839f2b4ff4e938bcba63a539e
case "$(cd "$(dirname "$DIR")" 2>/dev/null && pwd)/" in
  "$(cd "$(dirname "$0")/../../.." && pwd)"/*) echo "build.sh: refusing to build inside the repository" >&2; exit 1;;
esac
[ -d "$DIR/.git" ] || git clone -q https://github.com/compas/grasp "$DIR"
cd "$DIR"
git checkout -q "$COMMIT"
mkdir -p build && cd build
# gfortran >= 10 refuses lib9290's argument mismatches without -fallow-argument-mismatch
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_Fortran_FLAGS="-fallow-argument-mismatch -O2" .. > cmake.log
make -j"$(nproc)" install > make.log 2>&1 || { tail -20 make.log; exit 1; }
# rmultipole (tools/grasp/multipoles/rmultipole.f90, ours) against GRASP's libraries, with rhfs's input routines (gethfd, getmixblock, setdbg)
W="$DIR/build/rmultipole"; mkdir -p "$W"
cp "$DIR"/src/appl/rhfs90/gethfd*.f90 "$DIR"/src/appl/rhfs90/getmixblock*.f90 "$DIR"/src/appl/rhfs90/setdbg*.f90 "$DIR"/src/appl/rhfs90/opt6_C.f90 "$W"/
M="$DIR/build/modules"
(cd "$W" && gfortran -O2 -fallow-argument-mismatch -fno-automatic -I"$M"/mod -I"$M"/9290 -I"$M"/rang90 -I"$M"/mcp90 -J. opt6_C.f90 setdbg_I.f90 setdbg.f90 \
  gethfd_I.f90 gethfd.f90 getmixblock_I.f90 getmixblock.f90 "$MP"/rmultipole.f90 -L"$DIR"/build/lib -lrang90 -lmcp90 -l9290 -lmod -llapack -lblas -o "$DIR"/bin/rmultipole)
echo "GRASP2018 $COMMIT built, with rmultipole: $DIR/bin"
