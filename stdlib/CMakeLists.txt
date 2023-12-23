set(SRCS io.pebl memory.pebl)
add_library(pebl_stdlib STATIC)
target_sources(pebl_stdlib PRIVATE ${SRCS})
install(TARGETS pebl_stdlib DESTINATION lib)
