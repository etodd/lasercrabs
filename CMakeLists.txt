# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (grepr)
set(AMD64 ON)

if (APPLE)
	set(ENV{PKG_CONFIG_PATH} ${CMAKE_SOURCE_DIR}/pkgconfig) # so pkg-config can find our custom gl.pc
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-register -Wno-deprecated-declarations")
endif()

find_package(OpenGL REQUIRED)

## GLEW
add_subdirectory(external/glew)
add_definitions(-DGLEW_STATIC)
## GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)
include_directories(${GLFW_BINARY_DIR}/src)

## bullet
set(USE_GLUT OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
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
include_directories(${ZLIB_INCLUDE_DIR})

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
	external/assimp/include
	external/lodepng
	external/bullet/src
	src
)
string(REPLACE ";" "," ALL_INCLUDES_COMMA "${ALL_INCLUDES}")
include_directories(${ALL_INCLUDES})
configure_file(.includes.in .includes)
configure_file(.exrc.in ${CMAKE_CURRENT_SOURCE_DIR}/.exrc)

set(asset_prefix "../")

## Models
set(models "")
set(model_files "\t\"\",\n")
set(i 1)
file(GLOB files "assets/*.mdl")
foreach(file ${files})
	get_filename_component(name ${file} NAME_WE)
	set(model_files "${model_files}\t\"${asset_prefix}assets/${name}.mdl\",\n")
	set(models "${models}\t\tASSET(${name}, ${i})\n")
	math(EXPR i "${i} + 1")
endforeach()
set(model_count ${i})

## Textures
set(textures "")
set(texture_files  "\t\"\",\n")
set(i 1)
file(GLOB files "assets/*.png")
foreach(file ${files})
	get_filename_component(name ${file} NAME_WE)
	set(texture_files "${texture_files}\t\"${asset_prefix}assets/${name}.png\",\n")
	set(textures "${textures}\t\tASSET(${name}, ${i})\n")
	math(EXPR i "${i} + 1")
endforeach()
set(texture_count ${i})

## Shaders
set(shaders "")
set(shader_files "\t\"\",\n")
set(i 1)
file(GLOB files "shaders/*.glsl")
foreach(file ${files})
	get_filename_component(name ${file} NAME_WE)
	set(shader_files "${shader_files}\t\"${asset_prefix}shaders/${name}.glsl\",\n")
	set(shaders "${shaders}\t\tASSET(${name}, ${i})\n")
	math(EXPR i "${i} + 1")
endforeach()
set(shader_count ${i})

configure_file(src/asset.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/asset.h)
configure_file(src/asset.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/src/asset.cpp)

## Source

source_group(h REGULAR_EXPRESSION ".*\\.h$")
source_group(cpp REGULAR_EXPRESSION ".*\\.cpp$")

add_executable(grepr
	src/data/array.h
	src/data/mesh.h
	src/data/entity.h
	src/data/entity.cpp
	src/render/render.h
	src/render/view.h
	src/render/render.cpp
	src/render/view.cpp
	src/asset.h
	src/sync.h
	src/types.h
	src/vi_assert.h
	src/physics.h
	src/lmath.h
	src/load.h
	src/exec.h
	src/controls.h
	src/controls.cpp
	src/load.cpp
	src/physics.cpp
	src/asset.cpp
	src/lmath.cpp
	src/main.cpp
)

## Libraries

target_link_libraries(grepr
	${OPENGL_LIBRARIES}
	${GLFW_LIBRARIES}
	glfw
	GLEW_static
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

set_target_properties(uninstall PROPERTIES EXCLUDE_FROM_ALL TRUE)
