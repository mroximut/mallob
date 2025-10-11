#!/bin/bash

BUILD_SUFFIX=""
APP_INCSAT=""
if [ "$inc" == "1" ] || [ "$INC" == "1" ] ; then
    BUILD_SUFFIX="_inc"
    APP_INCSAT="-DMALLOB_APP_INCSAT=1"
fi
BUILD_DIR="build${BUILD_SUFFIX}"

rm ./${BUILD_DIR}/*mallob* 2>/dev/null || true
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ "$SERVER" == "1" ] ; then
    echo "Building for server..."
    CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 ${APP_INCSAT} \
    -DMALLOB_JEMALLOC_DIR=/nfs/home/omutlu/.user_spack/environments/myenv/.spack-env/view/lib -DMALLOB_USE_JEMALLOC=1 \
    -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"${BUILD_DIR}/\" ..
    make -j32
    cd ..
else 
    CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 ${APP_INCSAT} \
    -DMALLOB_USE_JEMALLOC=1 -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"${BUILD_DIR}/\" ..
    make -j16
    cd ..
fi
