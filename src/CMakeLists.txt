
set(SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")
add_library(core SHARED)
target_include_directories(core PRIVATE "${PROJECT_SOURCE_DIR}/include" "${SOURCE_DIR}")
install(TARGETS core DESTINATION lib)

function(add_sources srcs directory_name)
  target_sources(core
    PRIVATE
    ${srcs})
endfunction()

# build library
add_subdirectory("${SOURCE_DIR}/common")
add_subdirectory("${SOURCE_DIR}/context")
add_subdirectory("${SOURCE_DIR}/ast")
add_subdirectory("${SOURCE_DIR}/parser")

add_library(codegen SHARED)
target_include_directories(codegen PRIVATE "${PROJECT_SOURCE_DIR}/include" "${SOURCE_DIR}")
install(TARGETS codegen DESTINATION lib)
target_link_libraries(codegen PRIVATE ${llvm_libs})
target_link_libraries(codegen PRIVATE core)

function(add_sources srcs directory_name)
  target_sources(codegen
    PRIVATE
    ${srcs})
endfunction()

add_subdirectory("${SOURCE_DIR}/codegen")

# build tools
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tool/lexer")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tool/parser")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tool/compiler")
