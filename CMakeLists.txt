# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (yearning)

## Includes

set(ZLIB_INCLUDE_DIR
	${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/contrib/zlib
	${CMAKE_CURRENT_BINARY_DIR}/external/assimp/contrib/zlib
)

set(IMPORT_INCLUDES
	external
	${SDL2_BINARY_DIR}/include
	external/bullet/src
	src
)

if (WIN32)
	set(IMPORT_INCLUDES ${IMPORT_INCLUDES} external/dirent)
endif(WIN32)

set(ALL_INCLUDES
	external
	${SDL2_BINARY_DIR}/include
	external/bullet/src
	src
)

string(REPLACE ";" "," ALL_INCLUDES_COMMA "${ALL_INCLUDES}")
configure_file(.includes.in .includes)
configure_file(.exrc.in ${CMAKE_CURRENT_SOURCE_DIR}/.exrc)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/lvl)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/str)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/dl)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/shader)

if (APPLE)
	set(ENV{PKG_CONFIG_PATH} ${CMAKE_SOURCE_DIR}/pkgconfig) # so pkg-config can find our custom gl.pc
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DAK_OPTIMIZED=1")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-register -Wno-deprecated-declarations")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set(CMAKE_CXX_FLAGS "-wd4351 -wd4530 -wd4577 -DWIN64=1 /EHsc /MP")
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
 
## Wwise
add_subdirectory(external/wwise)
cmake_policy(SET CMP0008 OLD)

## cJSON
add_subdirectory(external/cjson)

## Bullet
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

include_directories()

## AssImp

set(ASSIMP_LIB_INSTALL_DIR "lib")
set(LIBASSIMP_COMPONENT libassimp)
set(BUILD_STATIC_LIB ON)

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

# Compile without boost
target_compile_definitions(assimp PRIVATE
	-DASSIMP_BUILD_BOOST_WORKAROUND
	-DASSIMP_BUILD_NO_EXPORT
	-DASSIMP_BUILD_NO_3DS_IMPORTER
	-DASSIMP_BUILD_NO_AC_IMPORTER
	-DASSIMP_BUILD_NO_ASE_IMPORTER
	-DASSIMP_BUILD_NO_B3D_IMPORTER
	-DASSIMP_BUILD_NO_BLEND_IMPORTER
	-DASSIMP_BUILD_NO_BVH_IMPORTER
	-DASSIMP_BUILD_NO_COB_IMPORTER
	-DASSIMP_BUILD_NO_COLLADA_IMPORTER
	-DASSIMP_BUILD_NO_CSM_IMPORTER
	-DASSIMP_BUILD_NO_DXF_IMPORTER
	-DASSIMP_BUILD_NO_OBJ_IMPORTER
	-DASSIMP_BUILD_NO_STL_IMPORTER
	-DASSIMP_BUILD_NO_PLY_IMPORTER
	-DASSIMP_BUILD_NO_HMP_IMPORTER
	-DASSIMP_BUILD_NO_IFC_IMPORTER
	-DASSIMP_BUILD_NO_X_IMPORTER
	-DASSIMP_BUILD_NO_MDL_IMPORTER
	-DASSIMP_BUILD_NO_MD3_IMPORTER
	-DASSIMP_BUILD_NO_MD2_IMPORTER
	-DASSIMP_BUILD_NO_SMD_IMPORTER
	-DASSIMP_BUILD_NO_MDC_IMPORTER
	-DASSIMP_BUILD_NO_LWO_IMPORTER
	-DASSIMP_BUILD_NO_LWS_IMPORTER
	-DASSIMP_BUILD_NO_NFF_IMPORTER
	-DASSIMP_BUILD_NO_RAW_IMPORTER
	-DASSIMP_BUILD_NO_OFF_IMPORTER
	-DASSIMP_BUILD_NO_IRRMESH_IMPORTER
	-DASSIMP_BUILD_NO_IRR_IMPORTER
	-DASSIMP_BUILD_NO_Q3D_IMPORTER
	-DASSIMP_BUILD_NO_TERRAGEN_IMPORTER
	-DASSIMP_BUILD_NO_3D_IMPORTER
	-DASSIMP_BUILD_NO_OGRE_IMPORTER
	-DASSIMP_BUILD_NO_MS3D_IMPORTER
	-DASSIMP_BUILD_NO_Q3BSP_IMPORTER
	-DASSIMP_BUILD_NO_NDO_IMPORTER
	-DASSIMP_BUILD_NO_XGL_IMPORTER
	-DASSIMP_BUILD_NO_C4D_IMPORTER
)

target_include_directories(assimp PRIVATE
	external/assimp/include
	external/assimp/code/BoostWorkaround
	${ZLIB_INCLUDE_DIR}
)

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

target_include_directories(recast PRIVATE external/recast/Recast/Include)

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
	external/recast/DetourTileCache/Include/DetourTileCache.h
	external/recast/DetourTileCache/Include/DetourTileCacheBuilder.h
	external/recast/DetourTileCache/Source/DetourTileCache.cpp
	external/recast/DetourTileCache/Source/DetourTileCacheBuilder.cpp
)

target_include_directories(detour PUBLIC
	external/recast/Detour/Include
	external/recast/DetourTileCache/Include
)

## FastLZ
add_library(fastlz STATIC
	external/fastlz/fastlz.c
	external/fastlz/fastlz.h
)

## Mersenne Twister
add_library(mersenne STATIC
	external/mersenne/mersenne-twister.cpp
	external/mersenne/mersenne-twister.h
)

## SHA1
add_library(sha1 STATIC
	external/sha1/sha1.cpp
	external/sha1/sha1.h
)

## Source

add_executable(yearning
	CMakeLists.txt
	src/data/array.h
	src/data/pin_array.h
	src/data/import_common.h
	src/data/import_common.cpp
	src/data/entity.h
	src/data/entity.cpp
	src/data/components.h
	src/data/components.cpp
	src/data/animator.h
	src/data/animator.cpp
	src/data/ragdoll.h
	src/data/ragdoll.cpp
	src/data/behavior.h
	src/data/behavior.cpp
	src/data/priority_queue.h
	src/render/glvm.h
	src/render/glvm.cpp
	src/render/render.h
	src/render/render.cpp
	src/render/views.h
	src/render/views.cpp
	src/render/particles.h
	src/render/particles.cpp
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
	src/asset/soundbank.h
	src/asset/Wwise_IDs.h
	src/asset/string.h
	src/asset/dialogue.h
	src/platform/sdl.cpp
	src/platform/util.h
	src/sync.h
	src/types.h
	src/vi_assert.h
	src/physics.h
	src/noise.h
	src/noise.cpp
	src/lmath.h
	src/input.h
	src/input.cpp
	src/strings.h
	src/strings.cpp
	src/common.h
	src/common.cpp
	src/console.h
	src/console.cpp
	src/load.h
	src/load.cpp
	src/settings.h
	src/physics.cpp
	src/lmath.cpp
	src/ai.h
	src/ai.cpp
	src/ai_worker.cpp
	src/loop.h
	src/ease.h
	src/game/component_ids.cpp
	src/game/entities.h
	src/game/entities.cpp
	src/game/minion.h
	src/game/minion.cpp
	src/game/parkour.h
	src/game/parkour.cpp
	src/game/player.h
	src/game/player.cpp
	src/game/team.h
	src/game/team.cpp
	src/game/ai_player.h
	src/game/ai_player.cpp
	src/game/awk.h
	src/game/awk.cpp
	src/game/walker.h
	src/game/walker.cpp
	src/game/game.h
	src/game/game.cpp
	src/game/audio.h
	src/game/audio.cpp
	src/game/menu.h
	src/game/menu.cpp
	src/game/scripts.h
	src/game/scripts.cpp
	src/game/usernames.h
	src/game/usernames.cpp
	external/sdl_controllers/gamecontrollerdb.txt
	assets/shader/standard_particle.glsl
	assets/shader/standard_particle_textured.glsl
	assets/shader/spark.glsl
	assets/shader/spark_teleport.glsl
	assets/shader/armature.glsl
	assets/shader/composite.glsl
	assets/shader/flat.glsl
	assets/shader/flat_texture.glsl
	assets/shader/point_light.glsl
	assets/shader/global_light.glsl
	assets/shader/spot_light.glsl
	assets/shader/standard.glsl
	assets/shader/culled.glsl
	assets/shader/standard_instanced.glsl
	assets/shader/ui.glsl
	assets/shader/ui_texture.glsl
	assets/shader/debug_depth.glsl
	assets/shader/ssao.glsl
	assets/shader/ssao_blur.glsl
	assets/shader/ssao_downsample.glsl
	assets/shader/edge_detect.glsl
	assets/shader/bloom_downsample.glsl
	assets/shader/blur.glsl
	assets/shader/skybox.glsl
	assets/shader/sky_decal.glsl
	assets/script/blend_to_fbx.py
	assets/script/blend_to_lvl.py
	assets/script/text_to_speech.py
	assets/script/ttf_to_fbx.py
	assets/str/ui_en.json
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/external/sdl_controllers/gamecontrollerdb.txt ${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt
	MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/external/sdl_controllers/gamecontrollerdb.txt
)

target_include_directories(yearning PRIVATE ${ALL_INCLUDES})

if(WIN32)
	set_property(TARGET yearning APPEND PROPERTY LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
	set_property(TARGET yearning APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_CONSOLE>)
	set_property(TARGET yearning APPEND PROPERTY LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS")
	set_property(TARGET yearning APPEND PROPERTY LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
	set_property(TARGET yearning APPEND PROPERTY LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
endif(WIN32)

## Libraries

target_link_libraries(yearning
	${OPENGL_LIBRARIES}
	SDL2-static
	SDL2main
	GLEW_static
	BulletDynamics
	BulletCollision
	BulletSoftBody
	LinearMath
	lodepng
	recast
	detour
	fastlz
	cJSON
	mersenne
	wwise_integration
	sha1
)

if(APPLE)
target_link_libraries(yearning "-framework AudioToolbox" "-framework CoreAudio")
endif()

add_dependencies(yearning assets)

add_executable(import
	src/data/import_common.h
	src/data/import_common.cpp
	src/render/glvm.h
	src/render/glvm.cpp
	src/data/array.h
	src/types.h
	src/lmath.h
	src/lmath.cpp
	src/import.cpp
)

target_include_directories(import PRIVATE ${IMPORT_INCLUDES})

target_link_libraries(import
	${OPENGL_LIBRARIES}
	SDL2-static
	GLEW_static
	assimp
	recast
	detour
	fastlz
	cJSON
	mersenne
	sha1
)

add_custom_target(
	assets ALL
	COMMAND $<TARGET_FILE:import>
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(assets import)
add_dependencies(yearning assets)
