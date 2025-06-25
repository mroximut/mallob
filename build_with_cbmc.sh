cd lib/cbmc
./build_with_mallob.sh
cd ../..
rm ./build_cbmc/*mallob*
cd build_cbmc
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 -DMALLOB_APP_CBMC=1 \
-DMALLOB_USE_JEMALLOC=1 -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"build_cbmc/\" ..
make -j16
cd ..
