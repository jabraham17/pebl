#!/usr/bin/env bash

set -e
set -x

mkdir -p build
cmake -S . -B build -G Ninja \
  -DCMAKE_INSTALL_PREFIX=build \
  -DCMAKE_BUILD_TYPE=Debug
# cmake --build build --target install
ninja -C build install
