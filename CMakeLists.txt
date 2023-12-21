cmake_minimum_required(VERSION 3.21.1)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

# shared libs
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

# set compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Pebl)

# specific to system
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs core analysis)
message(STATUS "Components mapped to libnames: ${llvm_libs}")

add_subdirectory(${PROJECT_SOURCE_DIR}/src)

