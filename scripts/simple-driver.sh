#!/usr/bin/bash
set -e

RUNTIME_DIR=runtime

TEMP_DIR=.temp
rm -rf $TEMP_DIR
mkdir -p $TEMP_DIR

# compile, optimize, and assemble
(set -x &&./build/bin/compiler foo.pebl -output $TEMP_DIR/foo.ll) && \
(set -x && $LLVM/opt -passes='mem2reg' -S $TEMP_DIR/foo.ll -o $TEMP_DIR/foo-opt.ll) && \
(set -x && $LLVM/llc -relocation-model=pic -filetype=obj $TEMP_DIR/foo-opt.ll -o $TEMP_DIR/foo.o) &

(set -x &&./build/bin/compiler foo2.pebl -output $TEMP_DIR/foo2.ll) && \
(set -x && $LLVM/opt -passes='mem2reg' -S $TEMP_DIR/foo2.ll -o $TEMP_DIR/foo2-opt.ll) && \
(set -x && $LLVM/llc -relocation-model=pic -filetype=obj $TEMP_DIR/foo2-opt.ll -o $TEMP_DIR/foo2.o) &

# build runtime
(set -x && $LLVM/clang -g -c $RUNTIME_DIR/runtime.c -o $TEMP_DIR/runtime.o) &


# wait for runtime and link
wait
(set -x && $LLVM/clang -g $TEMP_DIR/foo.o $TEMP_DIR/foo2.o $TEMP_DIR/runtime.o -o a.out)
