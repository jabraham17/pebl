
add_executable(parser
  main.c
  scope.c
)
target_include_directories(parser PRIVATE "${PROJECT_SOURCE_DIR}/include" "${SOURCE_DIR}")
target_link_libraries(parser PRIVATE core)
install(TARGETS parser DESTINATION bin)
