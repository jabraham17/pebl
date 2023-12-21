#!/usr/bin/bash
set -e

RUNTIME_DIR=runtime
LIBRARY_DIR=library

TEMP_DIR=.temp
rm -rf $TEMP_DIR
mkdir -p $TEMP_DIR

# build library
(set -x &&./build/bin/compiler $LIBRARY_DIR/io.pebl -output $TEMP_DIR/io.ll) && \
(set -x && $LLVM/opt -passes='mem2reg' -S $TEMP_DIR/io.ll -o $TEMP_DIR/io-opt.ll) && \
(set -x && $LLVM/llc -relocation-model=pic -filetype=obj $TEMP_DIR/io-opt.ll -o $TEMP_DIR/io.o) &
(set -x &&./build/bin/compiler $LIBRARY_DIR/memory.pebl -output $TEMP_DIR/memory.ll) && \
(set -x && $LLVM/opt -passes='mem2reg' -S $TEMP_DIR/memory.ll -o $TEMP_DIR/memory-opt.ll) && \
(set -x && $LLVM/llc -relocation-model=pic -filetype=obj $TEMP_DIR/memory-opt.ll -o $TEMP_DIR/memory.o) &

# build runtime
(set -x && $LLVM/clang -g -c $RUNTIME_DIR/io.c -o $TEMP_DIR/runtime_io.o) &
(set -x && $LLVM/clang -g -c $RUNTIME_DIR/main.c -o $TEMP_DIR/runtime_main.o) &
(set -x && $LLVM/clang -g -c $RUNTIME_DIR/memory.c -o $TEMP_DIR/runtime_memory.o) &
(set -x && $LLVM/clang -g -c $RUNTIME_DIR/runtime.c -o $TEMP_DIR/runtime_runtime.o) &


# compile, optimize, and assemble
(set -x &&./build/bin/compiler foo.pebl -output $TEMP_DIR/foo.ll) && \
(set -x && $LLVM/opt -passes='mem2reg' -S $TEMP_DIR/foo.ll -o $TEMP_DIR/foo-opt.ll) && \
(set -x && $LLVM/llc -relocation-model=pic -filetype=obj $TEMP_DIR/foo-opt.ll -o $TEMP_DIR/foo.o) &


# wait for runtime and link
wait
(set -x && $LLVM/clang -g $TEMP_DIR/foo.o $TEMP_DIR/io.o $TEMP_DIR/memory.o $TEMP_DIR/runtime_io.o $TEMP_DIR/runtime_main.o $TEMP_DIR/runtime_memory.o $TEMP_DIR/runtime_runtime.o -o a.out)
