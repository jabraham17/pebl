add_executable(lexer main.c)
target_include_directories(lexer PRIVATE "${PROJECT_SOURCE_DIR}/include" "${SOURCE_DIR}")
target_link_libraries(lexer PRIVATE core)
install(TARGETS lexer DESTINATION bin)
