# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (mkzebra)

## Includes

set(ZLIB_INCLUDE_DIR
	${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/contrib/zlib
	${CMAKE_CURRENT_BINARY_DIR}/external/assimp/contrib/zlib
)
 
include(src/game/CMakeLists.txt)

set(ALL_INCLUDES
	external/glew/include
	${SDL2_BINARY_DIR}/include
	external/sdl/include
	external/assimp/include
	external/lodepng
	external/bullet/src
	external/recast/Recast/Include
	external/recast/Detour/Include
	external/cjson
	external/mersenne
	src
	external/assimp/code/BoostWorkaround
	${ZLIB_INCLUDE_DIR}
	${GAME_INCLUDE_PATHS}
)

if (WIN32)
	set(ALL_INCLUDES ${ALL_INCLUDES} external/dirent)
endif(WIN32)

string(REPLACE ";" "," ALL_INCLUDES_COMMA "${ALL_INCLUDES}")
include_directories(${ALL_INCLUDES})
configure_file(.includes.in .includes)
configure_file(.exrc.in ${CMAKE_CURRENT_SOURCE_DIR}/.exrc)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/lvl)

if (APPLE)
	set(ENV{PKG_CONFIG_PATH} ${CMAKE_SOURCE_DIR}/pkgconfig) # so pkg-config can find our custom gl.pc
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DAK_OPTIMIZED=1")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-register -Wno-deprecated-declarations")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set(CMAKE_CXX_FLAGS "-wd4351 -wd4530 -DWIN64=1")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(OpenGL REQUIRED)

## GLEW
add_subdirectory(external/glew)
add_definitions(-DGLEW_STATIC)

## SDL
if (WIN32)
	set(HAVE_XINPUT_H 1)
	link_directories($ENV{DXSDK_DIR}Lib\\x64)
endif (WIN32)
set(DIRECTX ON CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(DISKAUDIO OFF CACHE BOOL "" FORCE)
set(DUMMYAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_POWER ON CACHE BOOL "" FORCE)
set(SDL_THREADS ON CACHE BOOL "" FORCE)
set(SDL_FILE ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(external/sdl)

## cJSON
add_subdirectory(external/cjson)

## bullet
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "" FORCE)
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
	external/recast/Recast/Include/RecastAlloc.h
	external/recast/Recast/Include/Recast.h
	external/recast/Recast/Include/RecastAssert.h
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

## Detour
add_library(detour STATIC
	external/recast/Detour/Include/DetourAlloc.h
	external/recast/Detour/Include/DetourAssert.h
	external/recast/Detour/Include/DetourCommon.h
	external/recast/Detour/Include/DetourMath.h
	external/recast/Detour/Include/DetourNavMesh.h
	external/recast/Detour/Include/DetourNavMeshBuilder.h
	external/recast/Detour/Include/DetourNavMeshQuery.h
	external/recast/Detour/Include/DetourNode.h
	external/recast/Detour/Include/DetourStatus.h
	external/recast/Detour/Source/DetourAlloc.cpp
	external/recast/Detour/Source/DetourCommon.cpp
	external/recast/Detour/Source/DetourNavMesh.cpp
	external/recast/Detour/Source/DetourNavMeshBuilder.cpp
	external/recast/Detour/Source/DetourNavMeshQuery.cpp
	external/recast/Detour/Source/DetourNode.cpp
)

## Mersenne Twister
add_library(mersenne STATIC
	external/mersenne/mersenne-twister.cpp
	external/mersenne/mersenne-twister.h
)

## Source

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/game/CMakeLists.txt)
	set(template_files
		src/template/CMakeLists.txt
		src/template/game.cpp
		src/template/game.h
	)
	file(COPY ${template_files} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/src/game/)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/external/sdl_controllers/gamecontrollerdb.txt ${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt
	MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/external/sdl_controllers/gamecontrollerdb.txt
)

add_executable(mkzebra
	CMakeLists.txt
	src/data/array.h
	src/data/intrusive_list.h
	src/data/pin_array.h
	src/data/import_common.h
	src/data/import_common.cpp
	src/data/entity.h
	src/data/entity.cpp
	src/data/components.h
	src/data/components.cpp
	src/data/animator.h
	src/data/animator.cpp
	src/render/render.h
	src/render/render.cpp
	src/render/views.h
	src/render/views.cpp
	src/render/skinned_model.h
	src/render/skinned_model.cpp
	src/render/ui.h
	src/render/ui.cpp
	src/asset/lookup.h
	src/asset/animation.h
	src/asset/armature.h
	src/asset/font.h
	src/asset/mesh.h
	src/asset/shader.h
	src/asset/level.h
	src/asset/texture.h
	src/asset/values.cpp
	src/platform/gl.h
	src/platform/win_mac_linux.cpp
	src/sync.h
	src/types.h
	src/vi_assert.h
	src/physics.h
	src/lmath.h
	src/input.h
	src/common.h
	src/common.cpp
	src/console.h
	src/console.cpp
	src/load.h
	src/load.cpp
	src/physics.cpp
	src/lmath.cpp
	src/ai.h
	src/ai.cpp
	${GAME_SRC_FILES}
	external/sdl_controllers/gamecontrollerdb.txt
	assets/armature.glsl
	assets/composite.glsl
	assets/flat.glsl
	assets/flat_texture.glsl
	assets/point_light.glsl
	assets/spot_light.glsl
	assets/standard.glsl
	assets/ui.glsl
	assets/ui_texture.glsl
	assets/buffer_debug.glsl
	assets/skybox.glsl
	assets/ssao.glsl
	assets/ssao_blur.glsl
	assets/ssao_downsample.glsl
	assets/blend_to_fbx.py
	assets/blend_to_lvl.py
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
	SDL2-static
	SDL2main
	GLEW_static
	BulletDynamics
	BulletCollision
	BulletSoftBody
	LinearMath
	lodepng
	detour
	cJSON
	mersenne
	${GAME_LIBRARIES}
)

if(APPLE)
target_link_libraries(mkzebra "-framework AudioToolbox" "-framework CoreAudio")
endif()

add_dependencies(mkzebra assets)

add_executable(import
	src/data/import_common.h
	src/data/import_common.cpp
	src/data/array.h
	src/types.h
	src/lmath.h
	src/lmath.cpp
	src/import.cpp
)

target_link_libraries(import
	${OPENGL_LIBRARIES}
	SDL2-static
	SDL2main
	GLEW_static
	assimp
	recast
	detour
	cJSON
)

add_custom_target(
	assets ALL
	COMMAND $<TARGET_FILE:import>
	DEPENDS import
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(mkzebra assets)
