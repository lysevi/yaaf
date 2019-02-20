INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Unittests")

FILE(GLOB HDRS "*.h")
FILE(GLOB SRC "*.cpp")

add_executable(yaaf_unittests ${HDRS} ${SRC})
TARGET_LINK_LIBRARIES(yaaf_unittests ${YAAF_LIBRARIES})
TARGET_LINK_DIRECTORIES(yaaf_unittests PUBLIC ${YAAF_LIBRARY_DIR})
target_include_directories(yaaf PUBLIC ${YAAF_INCLUDE_DIR})

add_test(yaaf_unittests ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/unittests) 
set_target_properties(yaaf_unittests PROPERTIES FOLDER Tests)
cotire(yaaf_unittests)
