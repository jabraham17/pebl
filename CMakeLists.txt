cmake_minimum_required(VERSION 3.21.1)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

option(PEBL_SHARED_MODE "" ON)

if(PEBL_SHARED_MODE)
  message(STATUS "Building shared")

  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/compiler")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  set(BUILD_SHARED_LIBS ON)
else()
  message(STATUS "Building static")
  set(BUILD_SHARED_LIBS OFF)
  set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_SHARED_LIBRARY_ENABLE_EXPORTS TRUE)

if(PEBL_ASAN)
  message(STATUS "building asan")
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

add_compile_options(-Wall -Wextra)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

# set compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Pebl LANGUAGES C CXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
enable_language(Pebl)
message(STATUS "Using peblc: ${CMAKE_Pebl_COMPILER}")

# add linker flags to go faster
message(STATUS "Using compiler from ${CMAKE_C_COMPILER_ID}")
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_link_options(-fuse-ld=mold)
  message(STATUS "Using mold linker")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # sadly, not all versions of gcc accept fuse=mold
  add_link_options(-fuse-ld=gold)
  message(STATUS "Using gold linker")
endif()

# specific to system
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core analysis)
message(STATUS "Components mapped to libnames: ${llvm_libs}")

add_subdirectory("${PROJECT_SOURCE_DIR}/src")

add_subdirectory("${PROJECT_SOURCE_DIR}/runtime")
add_subdirectory("${PROJECT_SOURCE_DIR}/stdlib")
