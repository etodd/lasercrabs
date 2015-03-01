# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (grepr)

find_package(OpenGL REQUIRED)

## GLEW
add_subdirectory(external/glew)
## GLFW
add_subdirectory(external/glfw)
## GLM
add_subdirectory(external/glm)

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

## Includes
 
include_directories(
	external/glew/include
	external/glfw/include
	external/glm
	external/assimp/include
	src
)

## Source

set(shaders
	shaders/StandardShading.fragmentshader
	shaders/StandardShading.vertexshader
)

add_definitions(-DGLEW_STATIC)

add_executable(grepr
	src/main.cpp
	src/controls.cpp
	src/objloader.cpp
	src/shader.cpp
	src/texture.cpp
	src/vboindexer.cpp

	${shaders}
)

source_group(shaders REGULAR_EXPRESSION ".*/.*shader$")
file(COPY ${shaders} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

## Libraries

target_link_libraries(grepr
	${OPENGL_LIBRARIES}
	${GLFW_LIBRARIES}
	glfw
	assimp
	GLEW_static
)
