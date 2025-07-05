#!/bin/bash

if [ ! -d "lib/2ls" ]; then
  cd lib
  git clone git@github.com:mroximut/2ls.git --branch app_for_mallob
  cd ..
fi
cd lib/2ls
bash ./build.sh
cd ../..
mkdir -p build_2ls
rm ./build_2ls/*mallob*
cd build_2ls

if [ $SERVER == "1" ] ; then
echo "Building for server..."
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 -DMALLOB_APP_2LS=1 \
-DMALLOB_JEMALLOC_DIR=/nfs/home/omutlu/.user_spack/environments/myenv/.spack-env/view/lib -DMALLOB_USE_JEMALLOC=1 \
-DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"build_2ls/\" ..
make -j32
cd ..
else        
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 -DMALLOB_APP_2LS=1 \
-DMALLOB_USE_JEMALLOC=1 -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"build_2ls/\" ..
make -j16
cd ..
fi
