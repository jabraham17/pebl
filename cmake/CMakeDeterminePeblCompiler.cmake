find_program(
  CMAKE_Pebl_COMPILER
  NAMES "pebl"
  HINTS "${CMAKE_SOURCE_DIR}"
  DOC "Pebl compiler")

set(CMAKE_Pebl_SOURCE_FILE_EXTENSIONS pebl)

set(CMAKE_Pebl_OUTPUT_EXTENSION .o)
set(CMAKE_Pebl_COMPILER_ENV_VAR "")

configure_file(${CMAKE_CURRENT_LIST_DIR}/CMakePeblCompiler.cmake.in
               ${CMAKE_PLATFORM_INFO_DIR}/CMakePeblCompiler.cmake)
