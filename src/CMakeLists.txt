set(SOURCE_DIR
    "${PROJECT_SOURCE_DIR}/src"
    CACHE INTERNAL "")

#
# build core and peblc.so
#
if(${PEBL_SHARED_MODE})
  add_library(core SHARED)
else()
  add_library(core STATIC)
endif()

target_include_directories(core PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                        "${SOURCE_DIR}")
install(TARGETS core DESTINATION lib/compiler)

function(add_sources srcs directory_name)
  target_sources(core PRIVATE ${srcs})
endfunction()

# add libs for pebl
target_link_libraries(core PRIVATE pebl_stdlib pebl_runtime)

# build library
add_subdirectory("${SOURCE_DIR}/common")
add_subdirectory("${SOURCE_DIR}/context")
add_subdirectory("${SOURCE_DIR}/ast")
add_subdirectory("${SOURCE_DIR}/parser")

#
# build shared object for peblc
#
add_library(libpeblc SHARED $<TARGET_OBJECTS:core>)
set_target_properties(libpeblc PROPERTIES OUTPUT_NAME peblc)
install(TARGETS libpeblc DESTINATION lib/compiler)
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
install(TARGETS codegen DESTINATION lib/compiler)

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
