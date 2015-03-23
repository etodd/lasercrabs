# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (grepr)
set(AMD64 ON)

find_package(OpenGL REQUIRED)

## GLEW
add_subdirectory(external/glew)
add_definitions(-DGLEW_STATIC)
## GLFW
add_subdirectory(external/glfw)
## GLM
add_subdirectory(external/glm)
## libevent
add_subdirectory(external/libevent)
## bullet
set(USE_GLUT OFF)
set(BUILD_BULLET2_DEMOS OFF)
set(BUILD_BULLET3 OFF)
add_subdirectory(external/bullet)
include(${CMAKE_CURRENT_BINARY_DIR}/external/bullet/BulletConfig.cmake)
add_definitions(${BULLET_DEFINITIONS})

## AssImp

set(ASSIMP_LIB_INSTALL_DIR "lib")
set(LIBASSIMP_COMPONENT libassimp)
set(BUILD_STATIC_LIB ON)

# Compile without boost
include_directories(external/assimp/code/BoostWorkaround)
add_definitions(-DASSIMP_BUILD_BOOST_WORKAROUND)

# Search for external dependencies, and build them from source if not found
# Search for zlib
include(CheckIncludeFile)
include(CheckTypeSize)
include(CheckFunctionExists)
# compile from sources
add_subdirectory(external/assimp/contrib/zlib)
set(ZLIB_FOUND 1)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/contrib/zlib ${CMAKE_CURRENT_BINARY_DIR}/external/assimp/contrib/zlib)
INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})

set(GIT_COMMIT_HASH 0)
configure_file(
  external/assimp/revision.h.in
  ${CMAKE_SOURCE_DIR}/external/assimp/code/revision.h
)
add_subdirectory(external/assimp/code)

## lodepng
add_library(lodepng STATIC external/lodepng/lodepng.cpp)

## Includes
 
set(ALL_INCLUDES
	external/glew/include
	external/glfw/include
	external/glm
	external/assimp/include
	external/lodepng
	${LIBEVENT_INCLUDE_DIRS}
	external/bullet/src
	src
)
string(REPLACE ";" "," ALL_INCLUDES_COMMA "${ALL_INCLUDES}")
include_directories(${ALL_INCLUDES})
configure_file(.includes.in .includes)
configure_file(.exrc.in ${CMAKE_CURRENT_SOURCE_DIR}/.exrc)

## Source

set(shaders
	shaders/StandardShading.fragmentshader
	shaders/StandardShading.vertexshader
)

source_group(shaders REGULAR_EXPRESSION ".*/.*shader$")

add_executable(grepr
	src/main.cpp
	src/controls.cpp
	src/objloader.cpp
	src/shader.cpp
	src/texture.cpp
	src/physics.cpp

	${shaders}
)

## Libraries

target_link_libraries(grepr
	${OPENGL_LIBRARIES}
	${GLFW_LIBRARIES}
	glfw
	GLEW_static
	${LIBEVENT_LIBRARIES}
	BulletDynamics
	BulletCollision
	BulletSoftBody
	LinearMath
	lodepng
)

add_executable(import
	src/import.cpp
)

target_link_libraries(import
	assimp
)
