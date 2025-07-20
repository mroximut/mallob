#!/bin/bash

if [ ! -d "lib/cbmc" ]; then
  cd lib
  git clone git@github.com:mroximut/cbmc.git --branch app_for_mallob
  cd ..
fi

cd lib/cbmc
bash ./build_with_mallob.sh
cd ../..
mkdir -p build_cbmc
rm ./build_cbmc/*mallob*
cd build_cbmc

if [ "$SERVER" == "1" ] ; then
echo "Building for server..."
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 -DMALLOB_APP_CBMC=1 \
-DMALLOB_JEMALLOC_DIR=/nfs/home/omutlu/.user_spack/environments/myenv/.spack-env/view/lib -DMALLOB_USE_JEMALLOC=1 \
-DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"build_cbmc/\" ..
make -j32
cd ..
else        
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 -DMALLOB_APP_CBMC=1 \
-DMALLOB_USE_JEMALLOC=1 -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"build_cbmc/\" ..
make -j16
cd ..
fi
