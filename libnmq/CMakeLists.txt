include(GenerateExportHeader)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
INCLUDE_DIRECTORIES(${NMQ_INCLUDE_DIR})

FILE(GLOB GLOB_HDRS "*.h")
FILE(GLOB NET_HDRS "network/*.h")
FILE(GLOB SRLZ_HDRS "serialization/*.h")
FILE(GLOB UTILS_HDRS "utils/*.h")
FILE(GLOB UTILS_ASYNC_HDRS "utils/async/*.h")
FILE(GLOB LOCKFREE_HDRS "lockfree/*.h")


FILE(GLOB GLOB_SRC "*.cpp")
FILE(GLOB NET_SRC "network/*.cpp")
FILE(GLOB SRLZ_SRC "serialization/*.cpp")
FILE(GLOB UTILS_SRC "utils/*.cpp")
FILE(GLOB UTILS_ASYNC_SRC "utils/async/*.cpp")
FILE(GLOB LOCKFREE_SRC "lockfree/*.cpp")


source_group(utils FILES ${UTILS_HDRS} ${UTILS_SRC})
source_group(utils\\async FILES ${UTILS_ASYNC_HDRS} ${UTILS_ASYNC_SRC})
source_group(nmq FILES ${GLOB_HDRS} ${GLOB_SRC})
source_group(nmq\\network FILES ${NET_HDRS} ${NET_SRC})
source_group(nmq\\serialization  FILES ${SRLZ_HDRS} ${SRLZ_SRC})
source_group(nmq\\lockfree FILES ${LOCKFREE_HDRS} ${LOCKFREE_SRC})

SET(SRC ${GLOB_HDRS} ${GLOB_SRC} 
	${NET_HDRS} ${NET_SRC}
	${LOCKFREE_HDRS} ${LOCKFREE_SRC}
	${UTILS_HDRS}  ${UTILS_SRC} 
	${SRLZ_HDRS} ${SRLZ_SRC}
	${UTILS_ASYNC_HDRS}  ${UTILS_ASYNC_SRC})

SET(NMQ_LIBS nmq ${Boost_LIBRARIES} ${JEMALLOC_LIBS} CACHE STRING "nmq libs.")

add_library(nmq SHARED ${SRC})
TARGET_LINK_LIBRARIES(nmq ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_THREAD_LIBS_INIT})
cotire(nmq)

IF(WIN32)
 target_link_libraries(nmq wsock32 ws2_32)
endif()

GENERATE_EXPORT_HEADER(nmq
    BASE_NAME nmq
    EXPORT_MACRO_NAME EXPORT
    EXPORT_FILE_NAME exports.h
    STATIC_DEFINE SHARED_EXPORTS_BUILT_AS_STATIC)
