# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (mkzebra)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets)

if (APPLE)
	set(ENV{PKG_CONFIG_PATH} ${CMAKE_SOURCE_DIR}/pkgconfig) # so pkg-config can find our custom gl.pc
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-register -Wno-deprecated-declarations")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set(CMAKE_CXX_FLAGS "-wd4351 -wd4530")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(OpenGL REQUIRED)

## GLEW
add_subdirectory(external/glew)
add_definitions(-DGLEW_STATIC)
## GLFW
set(GLFW_INSTALL OFF)
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
add_definitions(-DASSIMP_BUILD_NO_EXPORT)
add_definitions(-DASSIMP_BUILD_NO_3DS_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_AC_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_ASE_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_B3D_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_BLEND_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_BVH_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_COB_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_COLLADA_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_CSM_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_DXF_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_OBJ_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_STL_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_PLY_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_HMP_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_IFC_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_X_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_MDL_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_MD3_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_MD2_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_SMD_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_MDC_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_LWO_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_LWS_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_NFF_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_RAW_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_OFF_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_IRRMESH_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_IRR_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_Q3D_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_TERRAGEN_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_3D_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_OGRE_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_MS3D_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_Q3BSP_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_NDO_IMPORTER)
add_definitions(-DASSIMP_BUILD_NO_XGL_IMPORTER)

# Search for external dependencies, and build them from source if not found
# Search for zlib
include(CheckIncludeFile)
include(CheckTypeSize)
include(CheckFunctionExists)
set(AMD64 OFF)
# compile from sources
add_subdirectory(external/assimp/contrib/zlib)
set(ZLIB_FOUND 1)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIR
	${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/contrib/zlib
	${CMAKE_CURRENT_BINARY_DIR}/external/assimp/contrib/zlib
)
include_directories(${ZLIB_INCLUDE_DIR})
set_target_properties(zlibstatic PROPERTIES LINKER_LANGUAGE CXX)

set(GIT_COMMIT_HASH 0)
configure_file(
  external/assimp/revision.h.in
  ${CMAKE_SOURCE_DIR}/external/assimp/code/revision.h
)
add_subdirectory(external/assimp/code)

## lodepng
add_library(lodepng STATIC external/lodepng/lodepng.cpp)

## Recast
add_library(recast STATIC
	external/recast/Recast/Source/Recast.cpp
	external/recast/Recast/Source/RecastAlloc.cpp
	external/recast/Recast/Source/RecastArea.cpp
	external/recast/Recast/Source/RecastContour.cpp
	external/recast/Recast/Source/RecastFilter.cpp
	external/recast/Recast/Source/RecastLayers.cpp
	external/recast/Recast/Source/RecastMesh.cpp
	external/recast/Recast/Source/RecastMeshDetail.cpp
	external/recast/Recast/Source/RecastRasterization.cpp
	external/recast/Recast/Source/RecastRegion.cpp
)

## Includes
 
set(ALL_INCLUDES
	external/glew/include
	external/glfw/include
	external/assimp/include
	external/lodepng
	external/bullet/src
	external/recast/Recast/Include
	src
)

if (WIN32)
	set(ALL_INCLUDES ${ALL_INCLUDES} external/dirent)
endif(WIN32)

string(REPLACE ";" "," ALL_INCLUDES_COMMA "${ALL_INCLUDES}")
include_directories(${ALL_INCLUDES})
configure_file(.includes.in .includes)
configure_file(.exrc.in ${CMAKE_CURRENT_SOURCE_DIR}/.exrc)

## Source

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/game/CMakeLists.txt)
	set(template_files
		src/template/CMakeLists.txt
		src/template/game.cpp
		src/template/game.h
	)
	file(COPY ${template_files} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/src/game/)
endif()

include(src/game/CMakeLists.txt)

add_executable(mkzebra
	src/data/array.h
	src/data/mesh.h
	src/data/entity.h
	src/data/entity.cpp
	src/data/components.h
	src/data/components.cpp
	src/render/render.h
	src/render/render.cpp
	src/render/view.h
	src/render/view.cpp
	src/render/armature.h
	src/render/armature.cpp
	src/render/ui.h
	src/render/ui.cpp
	src/asset.h
	src/sync.h
	src/types.h
	src/vi_assert.h
	src/physics.h
	src/lmath.h
	src/common.h
	src/common.cpp
	src/console.h
	src/console.cpp
	src/load.h
	src/load.cpp
	src/physics.cpp
	src/asset.cpp
	src/lmath.cpp
	src/main.h
	src/main.cpp
	${GAME_SRC_FILES}
)

if(WIN32)
	set_property(TARGET mkzebra APPEND PROPERTY LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
	set_property(TARGET mkzebra APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_CONSOLE>)
	set_property(TARGET mkzebra APPEND PROPERTY LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS")
	set_property(TARGET mkzebra APPEND PROPERTY LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
	set_property(TARGET mkzebra APPEND PROPERTY LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
endif(WIN32)

## Libraries

target_link_libraries(mkzebra
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
	src/data/array.h
	src/types.h
	src/lmath.h
	src/lmath.cpp
	src/import.cpp
)

target_link_libraries(import
	${OPENGL_LIBRARIES}
	${GLFW_LIBRARIES}
	glfw
	GLEW_static
	assimp
	recast
)

add_custom_target(
	assets ALL
	COMMAND $<TARGET_FILE:import>
	DEPENDS import
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(mkzebra assets)
