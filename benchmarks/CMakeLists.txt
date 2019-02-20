INCLUDE_DIRECTORIES(${yaaf_INCLUDE_DIR})

MESSAGE(STATUS "Benchmarks: ")

function(ADD_BENCHARK name src)
		MESSAGE(STATUS " +" ${name})

		SET(trgt benchmark_${name})
        add_executable(${trgt} ${src})
        TARGET_LINK_LIBRARIES(${trgt} ${YAAF_LIBRARIES})
		TARGET_LINK_DIRECTORIES(${trgt} PUBLIC ${YAAF_LIBRARY_DIR})
		target_include_directories(${trgt} PUBLIC ${YAAF_INCLUDE_DIR})

        set_target_properties(${trgt} PROPERTIES FOLDER Benchmarks)

		add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${trgt}) 
		cotire(${trgt})
endfunction(ADD_BENCHARK)


ADD_BENCHARK(pingpong pingpong_b.cpp)
ADD_BENCHARK(exchange exchange_b.cpp)
