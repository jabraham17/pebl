
set( CMAKE_Pebl_COMPILE_OBJECT
    "<CMAKE_Pebl_COMPILER> -o <OBJECT> -c <SOURCE> --runtime $ENV{PEBL_SOURCE}/runtime --standard-library $ENV{PEBL_SOURCE}/library"
)

set( CMAKE_Pebl_LINK_EXECUTABLE 
    "<CMAKE_Pebl_COMPILER> -o <TARGET> <OBJECTS> --runtime $ENV{PEBL_SOURCE}/runtime --standard-library $ENV{PEBL_SOURCE}/library"
)

set( CMAKE_Pebl_INFORMATION_LOADED 1 )
