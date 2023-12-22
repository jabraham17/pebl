add_executable(compiler main.c)
target_include_directories(compiler PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                            "${SOURCE_DIR}")
target_link_libraries(compiler PRIVATE core codegen)
install(TARGETS compiler DESTINATION bin)
