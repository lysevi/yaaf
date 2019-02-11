INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Benchmarks: ")

function(ADD_BENCHARK name src)
		MESSAGE(STATUS " +" ${name})

		SET(trgt benchmark_${name})
        add_executable(${trgt} ${src})
        target_link_libraries(${trgt} ${YAAF_LIBS})
        set_target_properties(${trgt} PROPERTIES FOLDER Benchmarks)
		add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${trgt}) 
		cotire(${trgt})
endfunction(ADD_BENCHARK)


ADD_BENCHARK(pingpong pingpong_b.cpp)
