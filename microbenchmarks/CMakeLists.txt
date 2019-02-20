INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Microbenchmarks")

FILE(GLOB HDRS "*.h")
FILE(GLOB SRC "*.cpp")

add_executable(microbenchmarks ${HDRS} ${SRC})
TARGET_LINK_LIBRARIES(microbenchmarks benchmark ${YAAF_LIBRARIES})
TARGET_LINK_DIRECTORIES(microbenchmarks PUBLIC ${YAAF_LIBRARY_DIR})
target_include_directories(microbenchmarks PUBLIC ${YAAF_INCLUDE_DIR})

set_target_properties(microbenchmarks PROPERTIES FOLDER Benchmarks)
add_test(microbenchmarks ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/microbenchmarks) 
cotire(microbenchmarks)
