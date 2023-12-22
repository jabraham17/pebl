set(SRCS codegen-llvm.c cg-operator.c cg-helpers.c cg-expr.c
         compiler-builtins.c)
add_sources("${SRCS}" "${CMAKE_CURRENT_SOURCE_DIR}")
