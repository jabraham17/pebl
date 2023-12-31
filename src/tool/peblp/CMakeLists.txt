add_executable(peblp preprocess.pebl)
target_link_libraries(peblp PRIVATE pebl_stdlib pebl_runtime)
install(TARGETS peblp DESTINATION bin)
