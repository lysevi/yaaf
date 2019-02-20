include(GenerateExportHeader)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h)


FILE(GLOB GLOB_HDRS "*.h")
FILE(GLOB SRLZ_HDRS "serialization/*.h")
FILE(GLOB UTILS_HDRS "utils/*.h")
FILE(GLOB UTILS_ASYNC_HDRS "utils/async/*.h")

FILE(GLOB GLOB_SRC "*.cpp")
FILE(GLOB SRLZ_SRC "serialization/*.cpp")
FILE(GLOB UTILS_SRC "utils/*.cpp")
FILE(GLOB UTILS_ASYNC_SRC "utils/async/*.cpp")

source_group(\\ FILES ${GLOB_HDRS} ${GLOB_SRC})
source_group(utils FILES ${UTILS_HDRS} ${UTILS_SRC})
source_group(utils\\async FILES ${UTILS_ASYNC_HDRS} ${UTILS_ASYNC_SRC})
if(yaaf_ENABLE_NETWORK)

endif()
source_group(serialization  FILES ${SRLZ_HDRS} ${SRLZ_SRC})

SET(SRC ${GLOB_HDRS} ${GLOB_SRC} 
	${NET_HDRS} ${NET_SRC}
	${UTILS_HDRS}  ${UTILS_SRC} 
	${SRLZ_HDRS} ${SRLZ_SRC}
	${UTILS_ASYNC_HDRS}  ${UTILS_ASYNC_SRC})


if(yaaf_ENABLE_NETWORK)
	FILE(GLOB NET_HDRS "network/*.h")
	FILE(GLOB NET_SRC "network/*.cpp")

	SET(SRC ${SRC} ${NET_HDRS} ${NET_SRC})
	SET(yaaf_INCLUDE_DIR ${yaaf_INCLUDE_DIR} ${DIALLER_INCLUDE_DIR})
	source_group(network FILES ${NET_HDRS} ${NET_SRC})
endif()


add_library(yaaf SHARED ${SRC})
TARGET_LINK_LIBRARIES(yaaf ${YAAF_PRIVATE_LIBRARIES})
target_link_directories(yaaf PUBLIC ${YAAF_LIBRARY_DIR})
target_include_directories(yaaf PUBLIC ${YAAF_INCLUDE_DIR})
cotire(yaaf)

SET(YAAF_LIBRARIES yaaf ${YAAF_PRIVATE_LIBRARIES} CACHE STRING "yaaf libs")

GENERATE_EXPORT_HEADER(yaaf
    BASE_NAME yaaf
    EXPORT_MACRO_NAME EXPORT
    EXPORT_FILE_NAME exports.h
    STATIC_DEFINE SHARED_EXPORTS_BUILT_AS_STATIC)
