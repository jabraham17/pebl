add_executable(peblc main.c)
target_include_directories(peblc PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                            "${SOURCE_DIR}")
target_link_libraries(peblc PRIVATE core codegen)
install(TARGETS peblc DESTINATION bin)
