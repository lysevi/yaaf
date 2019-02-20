cmake_minimum_required (VERSION 3.1)
set(PROJECT "yaaf")
project (${PROJECT} VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF(NOT CMAKE_BUILD_TYPE)
        message(STATUS "CMAKE_BUILD_TYPE is not set, set to default = RELWITHDEBINFO")
        SET(CMAKE_BUILD_TYPE "RELWITHDEBINFO")
ENDIF()

MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

option(yaaf_ENABLE_TESTS "Enable testing of the yaaf." ON)
option(yaaf_ENABLE_DOUBLECHECKS "More checks in code." OFF)
option(yaaf_ENABLE_NETWORK "Enable network." OFF)
option(yaaf_ASIO_DEBUG "Enable asio debug flag." OFF)
option(yaaf_ENABLE_BENCHMARKS "Enable build benchmarks." ON)
option(yaaf_ASAN_UBSAN "clang asan" OFF)
option(yaaf_MSAN "clang msan" OFF)

if(yaaf_ASAN_UBSAN AND yaaf_MSAN)
  message(FATAL_ERROR "Sanitizers cannot be enabled simultaneously.")
endif()

MESSAGE(STATUS "Options: ")
MESSAGE(STATUS "yaaf_ENABLE_TESTS - " ${yaaf_ENABLE_TESTS})
MESSAGE(STATUS "yaaf_ENABLE_DOUBLECHECKS - " ${yaaf_ENABLE_DOUBLECHECKS})
MESSAGE(STATUS "yaaf_ENABLE_NETWORK - " ${yaaf_ENABLE_NETWORK})
MESSAGE(STATUS "yaaf_ASIO_DEBUG - " ${yaaf_ASIO_DEBUG})
MESSAGE(STATUS "yaaf_ENABLE_BENCHMARKS - " ${yaaf_ENABLE_BENCHMARKS})
MESSAGE(STATUS "yaaf_ASAN_UBSAN - " ${yaaf_ASAN_UBSAN})
MESSAGE(STATUS "yaaf_MSAN - " ${yaaf_MSAN})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cotire/CMake")

include(AddCXXCompilerFlag)
include(CompillerSettings)
include(cotire)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

IF(yaaf_ASIO_DEBUG)
 add_definitions(-DBOOST_ASIO_ENABLE_HANDLER_TRACKING)
ENDIF()

IF(yaaf_ENABLE_DOUBLECHECKS)
 add_definitions(-DDOUBLE_CHECKS)
ENDIF()


########## BOOST
if(yaaf_ENABLE_NETWORK)
add_definitions(-DYAAF_NETWORK_ENABLED)

add_subdirectory(extern/dialler)
endif(yaaf_ENABLE_NETWORK)
##### THREADS
find_package (Threads)

##### MISC
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(yaaf_ENABLE_TESTS)
   include_directories(extern/Catch2/single_include/catch2/)
endif(yaaf_ENABLE_TESTS)

#########


if(yaaf_ENABLE_NETWORK)
	SET(YAAF_LIBRARY_DIR ${DIALLER_LIBRARY_DIR}  )
	SET(YAAF_INCLUDE_DIR ${DIALLER_INCLUDE_DIR}  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR} )
	SET(YAAF_PRIVATE_LIBRARIES ${DIALLER_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
else()
	SET(YAAF_LIBRARY_DIR)
	SET(YAAF_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR} )
	SET(YAAF_PRIVATE_LIBRARIES  ${CMAKE_THREAD_LIBS_INIT})	
endif(yaaf_ENABLE_NETWORK)

add_subdirectory(libyaaf)
add_subdirectory(examples)

if (yaaf_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(unittests)
endif()

if(yaaf_ENABLE_BENCHMARKS)
   set(BENCHMARK_ENABLE_TESTING OFF)
   add_subdirectory(extern/benchmark)
   include_directories(extern/benchmark/include)
   set_target_properties(benchmark PROPERTIES FOLDER Extern)
   add_subdirectory (microbenchmarks)
   
   SET(CXXOPTS_BUILD_TESTS OFF)
   SET(CXXOPTS_BUILD_EXAMPLES OFF)
   add_subdirectory(extern/cxxopts)
   include_directories(extern/cxxopts/include)

   add_subdirectory(benchmarks)
endif(yaaf_ENABLE_BENCHMARKS)

