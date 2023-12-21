# Pebl

![pebl icon](highlight/pebl/icons/pebl.svg)

## Build

Pebl uses CMake to build the compiler. Example commands for building for Linux and Windows are in `scripts/build.sh` and `scripts/build.cmd`, respectively. Note that if a proper LLVM install is not avaiblae in `PATH`, it needs to be set with the `LLVM_DIR` environment variable to `LLVM_INSTALL_DIR/lib/cmake/llvm`.

## Requirements

Pebl requires LLVM 17 to build. When building on Windows, using `clang-cl` is required.

## New revisions

This compiler is bootstrapped, meaning that the compiler for pebl is written in pebl. To enable this, a previous binary MUST always be kept around. To create and push a new revision of the compiler, run the following commands in a x86 Linux environment.

```bash
python3 scripts/make_revision.py
git push origin --tags
```

This creates a new revision in a git tag, which is pushed to GitHub for backup. This revision contains a fully built compiler and a tar file of the built compiler. When running the compiler, you may nee to set `LD_LIBRARY_PATH`
