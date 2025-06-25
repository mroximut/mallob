cd lib/2ls
./build.sh
cd ../..
rm ./build_2ls/*mallob*
cd build_2ls
CC=$(which mpicc) CXX=$(which mpicxx) cmake -DCMAKE_BUILD_TYPE=RELEASE -DMALLOB_APP_SAT=1 -DMALLOB_APP_2LS=1 \
-DMALLOB_USE_JEMALLOC=1 -DMALLOB_LOG_VERBOSITY=4 -DMALLOB_ASSERT=1 -DMALLOB_SUBPROC_DISPATCH_PATH=\"build_2ls/\" ..
make -j16
cd ..
