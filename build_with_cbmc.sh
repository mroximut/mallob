#!/bin/bash

BUILD_SUFFIX=""
APP_INCSAT=""

# also change in lib/cbmc/build_with_mallob.sh : to build_2ls_inc
# also change in lib/cbmc/src/solvers/Makefile : to build_inc

if [ "$inc" = "1" ] || [ "$INC" = "1" ] ; then
    BUILD_SUFFIX="_inc"
    APP_INCSAT="-DMALLOB_APP_INCSAT=1"
fi
BUILD_DIR="build_cbmc${BUILD_SUFFIX}"

if [ ! -d "lib/cbmc" ]; then
  cd lib
  git clone git@github.com:mroximut/cbmc.git --branch app_for_mallob
  cd ..
fi

cd lib/cbmc
bash ./build_with_mallob.sh
cd ../..

mkdir -p "${BUILD_DIR}"
rm ./"${BUILD_DIR}"/*mallob* 2>/dev/null || true
cd "${BUILD_DIR}"

if [ "$SERVER" == "1" ] ; then
echo "Building for server..."
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 ${APP_INCSAT} -DMALLOB_APP_CBMC=1 \
-DMALLOB_JEMALLOC_DIR=/nfs/home/omutlu/.user_spack/environments/myenv/.spack-env/view/lib -DMALLOB_USE_JEMALLOC=1 \
-DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH="${BUILD_DIR}/" ..
make -j32
cd ..
else        
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 ${APP_INCSAT} -DMALLOB_APP_CBMC=1 \
-DMALLOB_USE_JEMALLOC=1 -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH="${BUILD_DIR}/" ..
make -j16
cd ..
fi
