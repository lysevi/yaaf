INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Examples: ")

function(ADD_EXAMPLE name src)
	MESSAGE(STATUS " +" ${name})

	SET(trgt example_${name})
        add_executable(${trgt} ${src})
        target_link_libraries(${trgt} ${YAAF_LIBS})
        set_target_properties(${trgt} PROPERTIES FOLDER Examples)
		add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${trgt}) 
		cotire(${trgt})
endfunction(ADD_EXAMPLE)


ADD_EXAMPLE(pingpong ping_pong_example.cpp)

