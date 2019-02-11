INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Microbenchmarks")

FILE(GLOB HDRS "*.h")
FILE(GLOB SRC "*.cpp")

add_executable(microbenchmarks ${HDRS} ${SRC})
TARGET_LINK_LIBRARIES(microbenchmarks ${YAAF_LIBS} benchmark)
target_link_libraries(microbenchmarks benchmark)
set_target_properties(microbenchmarks PROPERTIES FOLDER Benchmarks)
add_test(microbenchmarks ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/microbenchmarks) 
cotire(microbenchmarks)
