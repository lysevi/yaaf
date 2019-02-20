INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Examples: ")

function(ADD_EXAMPLE name src)
	MESSAGE(STATUS " +" ${name})

	SET(trgt example_${name})
    add_executable(${trgt} ${src})
    TARGET_LINK_LIBRARIES(${trgt} ${YAAF_LIBRARIES})
	TARGET_LINK_DIRECTORIES(${trgt} PUBLIC ${YAAF_LIBRARY_DIR})
	target_include_directories(${trgt} PUBLIC ${YAAF_INCLUDE_DIR})

    set_target_properties(${trgt} PROPERTIES FOLDER Examples)
	add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${trgt}) 
	cotire(${trgt})
endfunction(ADD_EXAMPLE)


ADD_EXAMPLE(pingpong ping_pong_example.cpp)
if(yaaf_ENABLE_NETWORK)
ADD_EXAMPLE(network network_example.cpp)
endif(yaaf_ENABLE_NETWORK)

