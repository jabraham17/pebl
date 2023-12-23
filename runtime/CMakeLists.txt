
# 
# build the runtime
# 
set(SRCS io.c memory.c runtime.c)
add_library(pebl_runtime STATIC)
target_sources(pebl_runtime PRIVATE ${SRCS})
install(TARGETS pebl_runtime DESTINATION lib)

# 
# build startup
# 
add_library(pebl_start STATIC)
target_sources(pebl_start PRIVATE start/main.c)
install(TARGETS pebl_start DESTINATION lib)
