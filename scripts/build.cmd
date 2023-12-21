
mkdir build-win
cmake -S . -B build-win -G Ninja -DCMAKE_INSTALL_PREFIX=build-win -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build-win --target install
