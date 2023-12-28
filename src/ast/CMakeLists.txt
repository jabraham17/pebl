set(SRCS ast.c location.c parse-checks.c scope-resolve.c Type.c
         parse-checks.pebl)
add_sources("${SRCS}" "${CMAKE_CURRENT_SOURCE_DIR}")
