set(CMAKE_Pebl_COMPILE_OBJECT
    "<CMAKE_Pebl_COMPILER> -o <OBJECT> -c <SOURCE>"
)

set(CMAKE_Pebl_LINK_EXECUTABLE
    "<CMAKE_Pebl_COMPILER> -o <TARGET> <OBJECTS>"
)

set(CMAKE_Pebl_ARCHIVE_CREATE "<CMAKE_AR> rcs <TARGET> <OBJECTS>")
set(CMAKE_Pebl_ARCHIVE_FINISH "")

set(CMAKE_Pebl_INFORMATION_LOADED 1)
