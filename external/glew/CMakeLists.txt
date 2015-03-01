project(GLEW)
cmake_minimum_required(VERSION 2.4)

if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(GLEW_VERSION "1.11.0")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(GLEW_LIB_NAME glew32)
else(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(GLEW_LIB_NAME GLEW)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

#
# All platforms need OpenGL
#

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
else(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	include(FindPkgConfig)
	pkg_check_modules( OpenGL REQUIRED gl )
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

#
# Linux needs X11
#

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(X11 REQUIRED)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(CMAKE_C_FLAGS "${CFLAGS} ${CMAKE_C_FLAGS} -DGLEW_BUILD -DGLEW_NO_GLU -O2 -Wall" )
else(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(CMAKE_C_FLAGS "${CFLAGS} ${CMAKE_C_FLAGS} -DGLEW_BUILD -DGLEW_NO_GLU -O2 -Wall -W" )
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

include_directories( ${PROJECT_SOURCE_DIR}/include )

add_library(GLEW_static STATIC src/glew.c )
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set_target_properties(GLEW_static PROPERTIES OUTPUT_NAME ${GLEW_LIB_NAME})
else(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set_target_properties(GLEW_static PROPERTIES OUTPUT_NAME ${GLEW_LIB_NAME} PREFIX lib)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
