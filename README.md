# Pebl

![pebl icon](highlight/pebl/icons/pebl.svg)

## Build

Pebl uses CMake to build the compiler. Example commands for building for Linux and Windows are in `scripts/build.sh` and `scripts/build.cmd`, respectively. Note that if a proper LLVM install is not avaiblae in `PATH`, it needs to be set with the `LLVM_DIR` environment variable to `LLVM_INSTALL_DIR/lib/cmake/llvm`.

## Requirements

Pebl requires LLVM 17 to build. When building on Windows, using `clang-cl` is required.
