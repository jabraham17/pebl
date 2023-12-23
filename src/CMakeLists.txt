set(SOURCE_DIR
    "${PROJECT_SOURCE_DIR}/src"
    CACHE INTERNAL "")

#
# build core
#
if(${PEBL_SHARED_MODE})
  add_library(core SHARED)
else()
  add_library(core STATIC)
endif()

target_include_directories(core PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                        "${SOURCE_DIR}")
install(TARGETS core DESTINATION lib)

function(add_sources srcs directory_name)
  target_sources(core PRIVATE ${srcs})
endfunction()

# build library
add_subdirectory("${SOURCE_DIR}/common")
add_subdirectory("${SOURCE_DIR}/context")
add_subdirectory("${SOURCE_DIR}/ast")
add_subdirectory("${SOURCE_DIR}/parser")

#
# build codegen
#
if(${PEBL_SHARED_MODE})
  add_library(codegen SHARED)
else()
  add_library(codegen STATIC)
endif()
target_include_directories(codegen PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                           "${SOURCE_DIR}")
install(TARGETS codegen DESTINATION lib)

# link in core and llvm
target_link_libraries(codegen PRIVATE ${llvm_libs} core)

function(add_sources srcs directory_name)
  target_sources(codegen PRIVATE ${srcs})
endfunction()

add_subdirectory("${SOURCE_DIR}/codegen")

#
# build tools
#
add_subdirectory("${SOURCE_DIR}/tool/lexer")
add_subdirectory("${SOURCE_DIR}/tool/parser")
add_subdirectory("${SOURCE_DIR}/tool/peblc")
add_subdirectory("${SOURCE_DIR}/tool/driver")
