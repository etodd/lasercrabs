# CMake entry point
cmake_minimum_required (VERSION 2.6)
project (deceiver)

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
	external/assimp/include
)

if (WIN32)
	set(IMPORT_INCLUDES ${IMPORT_INCLUDES} external/dirent)
endif(WIN32)

set(ALL_INCLUDES
	external
	src
	${ZLIB_INCLUDE_DIR}
)

if (WIN32)
	set(ALL_INCLUDES ${ALL_INCLUDES} external/dirent)
endif(WIN32)

set(SERVER_CLIENT_INCLUDES
	${ALL_INCLUDES}
	external/bullet/src
)

string(REPLACE ";" "," ALL_INCLUDES_COMMA "${ALL_INCLUDES}")
configure_file(.includes.in .includes)
configure_file(.exrc.in ${CMAKE_CURRENT_SOURCE_DIR}/.exrc)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/lvl)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/str)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/shader)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/air)

if (APPLE)
	set(ENV{PKG_CONFIG_PATH} ${CMAKE_SOURCE_DIR}/pkgconfig) # so pkg-config can find our custom gl.pc
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-register -Wno-deprecated-declarations")
	set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 -g")
	set(CMAKE_CXX_FLAGS_RELEASE "-DAK_OPTIMIZED=1")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set(CMAKE_C_FLAGS "/Zi /MP")
	set(CMAKE_C_FLAGS_DEBUG "-DDEBUG=1 /MTd /Od")
	set(CMAKE_C_FLAGS_RELEASE "/MT")
	set(CMAKE_CXX_FLAGS "-wd4351 -wd4530 -wd4577 -DWIN64=1 /EHsc /Zi /MP")
	set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 /MTd /Od")
	set(CMAKE_CXX_FLAGS_RELEASE "-DAK_OPTIMIZED=1 /MT")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (PLAYSTATION)
	set(SDL_LIBS "")
elseif (CLIENT)
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
	set(LIBC ON CACHE BOOL "" FORCE)
	add_subdirectory(external/sdl)

	set(SDL_LIBS
		SDL2-static
		SDL2main
		GLEW_static
	)
endif()
 
## Wwise
add_subdirectory(external/wwise)
cmake_policy(SET CMP0008 OLD)

## Bullet
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
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

## lodepng
if (CLIENT)
add_library(lodepng STATIC external/lodepng/lodepng.cpp)
endif()

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
	external/recast/Recast/Source/RecastAssert.cpp
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
	external/recast/Detour/Source/DetourAssert.cpp
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

add_library(cJSON STATIC
	external/cjson/cJSON.h
	external/cjson/cJSON.c
	external/cjson/cJSON_Utils.c
	external/cjson/cJSON_Utils.h
)

## Source

set(SRC 
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
	src/data/priority_queue.h
	src/render/render.h
	src/render/render.cpp
	src/render/views.h
	src/render/views.cpp
	src/render/particles.h
	src/render/particles.cpp
	src/render/skinned_model.h
	src/render/skinned_model.cpp
	src/render/glvm.h
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
	src/platform/util.h
	src/platform/sock.h
	src/platform/sock.cpp
	src/game/master.h
	src/game/master.cpp
	src/sync.h
	src/types.h
	src/vi_assert.h
	src/noise.h
	src/noise.cpp
	src/lmath.h
	src/lmath.cpp
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
	src/net.h
	src/net.cpp
	src/net_serialize.h
	src/net_serialize.cpp
	src/physics.h
	src/physics.cpp
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
	src/game/player.h
	src/game/player.cpp
	src/game/team.h
	src/game/team.cpp
	src/game/overworld.h
	src/game/overworld.cpp
	src/game/ai_player.h
	src/game/ai_player.cpp
	src/game/drone.h
	src/game/drone.cpp
	src/game/walker.h
	src/game/walker.cpp
	src/game/constants.h
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
	src/game/parkour.h
	src/game/parkour.cpp
	src/game/sudoku.h
	src/game/sudoku.cpp
	external/sdl_controllers/gamecontrollerdb.txt
	assets/shader/particle_standard.glsl
	assets/shader/particle_eased.glsl
	assets/shader/particle_textured.glsl
	assets/shader/particle_spark.glsl
	assets/shader/particle_rain.glsl
	assets/shader/armature.glsl
	assets/shader/composite.glsl
	assets/shader/flat.glsl
	assets/shader/nav_dots.glsl
	assets/shader/flat_texture.glsl
	assets/shader/fresnel.glsl
	assets/shader/water.glsl
	assets/shader/point_light.glsl
	assets/shader/global_light.glsl
	assets/shader/spot_light.glsl
	assets/shader/standard.glsl
	assets/shader/standard_flat.glsl
	assets/shader/standard_instanced.glsl
	assets/shader/flat_instanced.glsl
	assets/shader/culled.glsl
	assets/shader/ui.glsl
	assets/shader/ui_texture.glsl
	assets/shader/debug_depth.glsl
	assets/shader/ssao.glsl
	assets/shader/ssao_blur.glsl
	assets/shader/ssao_downsample.glsl
	assets/shader/downsample.glsl
	assets/shader/blit.glsl
	assets/shader/blit_depth.glsl
	assets/shader/scan_lines.glsl
	assets/shader/bloom_downsample.glsl
	assets/shader/blur.glsl
	assets/shader/skybox.glsl
	assets/shader/underwater.glsl
	assets/shader/sky_decal.glsl
	assets/shader/clouds.glsl
	assets/script/blend_to_fbx.py
	assets/script/blend_to_lvl.py
	assets/script/ttf_to_fbx.py
	assets/str/en.json
)

# stolen from https://cheind.wordpress.com/2009/12/10/reducing-compilation-time-unity-builds/
function(enable_unity_build UB_SUFFIX SOURCE_VARIABLE_NAME)
	set(files ${${SOURCE_VARIABLE_NAME}})
	# Generate a unique filename for the unity build translation unit
	set(unit_build_file ${CMAKE_CURRENT_BINARY_DIR}/ub_${UB_SUFFIX}.cpp)
	# Exclude all translation units from compilation
	set_source_files_properties(${files} PROPERTIES HEADER_FILE_ONLY true)
	# Open the ub file
	FILE(WRITE ${unit_build_file} "// Unity Build generated by CMake\n")
	# Add include statement for each translation unit
	foreach(source_file ${files} )
	  FILE( APPEND ${unit_build_file} "#include <${CMAKE_CURRENT_SOURCE_DIR}/${source_file}>\n")
	endforeach(source_file)
	# Complement list of translation units with the name of ub
	set(${SOURCE_VARIABLE_NAME} ${${SOURCE_VARIABLE_NAME}} ${unit_build_file} PARENT_SCOPE)  
endfunction(enable_unity_build)

if (SERVER)
	# server
	set(SRC_SERVER
		${SRC}
		src/platform/server.cpp
	)
	#enable_unity_build(deceiversrv SRC_SERVER)
	add_executable(deceiversrv ${SRC_SERVER})

	# master
	set(SRC_MASTER
		src/data/array.h
		src/data/pin_array.h
		src/data/priority_queue.h
		src/net_serialize.h
		src/net_serialize.cpp
		src/vi_assert.h
		src/types.h
		src/platform/sock.h
		src/platform/sock.cpp
		src/platform/util.h
		src/game/master.h
		src/game/master.cpp
		src/game/constants.h
		src/asset/level.h
		src/platform/master_server.cpp
	)
	#enable_unity_build(deceivermaster SRC_MASTER)
	add_executable(deceivermaster
		${SRC_MASTER}
	)

	target_include_directories(deceiversrv PRIVATE
		${SERVER_CLIENT_INCLUDES}
		external/wwise
	)

	target_include_directories(deceivermaster PRIVATE
		${ALL_INCLUDES}
	)

	target_link_libraries(deceiversrv
		BulletDynamics
		BulletCollision
		BulletSoftBody
		LinearMath
		recast
		detour
		fastlz
		cJSON
		mersenne
		zlibstatic
	)

	if (APPLE)
	elseif (WIN32)
	else()
		target_link_libraries(deceiversrv "-lpthread")
	endif()

	target_compile_definitions(deceiversrv PRIVATE -DSERVER=1)
	target_compile_definitions(deceivermaster PRIVATE -DMASTER_SERVER=1)

	target_link_libraries(deceivermaster
		zlibstatic
		cJSON
	)

endif()

if (CLIENT)
	# client
	if (PLAYSTATION)
		set(SRC_PLAYSTATION
			${SRC}
			src/platform/ps/glvm.cpp
			src/platform/ps/main.cpp
		)
		#enable_unity_build(deceiver SRC_PLAYSTATION)
		add_executable(deceiver ${SRC_PLAYSTATION})
	else()
		set(SRC_CLIENT
			${SRC}
			src/platform/glvm.cpp
			src/platform/sdl.cpp
		)
		#enable_unity_build(deceiver SRC_CLIENT)
		add_executable(deceiver ${SRC_CLIENT})
		target_include_directories(deceiver PRIVATE ${SDL2_BINARY_DIR}/include)
	endif()

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/external/sdl_controllers/gamecontrollerdb.txt ${CMAKE_CURRENT_BINARY_DIR}/gamecontrollerdb.txt
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/external/sdl_controllers/gamecontrollerdb.txt
	)

	target_include_directories(deceiver PRIVATE ${SERVER_CLIENT_INCLUDES})

	if(WIN32)
		set_property(TARGET deceiver APPEND PROPERTY LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
		set_property(TARGET deceiver APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_CONSOLE>)
		set_property(TARGET deceiver APPEND PROPERTY LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS")
		set_property(TARGET deceiver APPEND PROPERTY LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
		set_property(TARGET deceiver APPEND PROPERTY LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
	endif(WIN32)

	## Libraries

	target_link_libraries(deceiver
		${OPENGL_LIBRARIES}
		${SDL_LIBS}
		BulletDynamics
		BulletCollision
		BulletSoftBody
		LinearMath
		recast
		detour
		fastlz
		cJSON
		mersenne
		lodepng
		wwise_integration
		zlibstatic
	)

	if (APPLE)
		target_link_libraries(deceiver "-framework AudioToolbox" "-framework CoreAudio")
	elseif (WIN32)
	else()
		target_link_libraries(deceiver "-lpthread")
	endif()
endif()

if (NOT PLAYSTATION)
	## AssImp

	set(ASSIMP_LIB_INSTALL_DIR "lib")
	set(LIBASSIMP_COMPONENT libassimp)
	set(BUILD_STATIC_LIB ON)
	set(ASSIMP_BUILD_3DS_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_3D_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_AC_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_ASE_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_ASSBIN_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_ASSXML_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_B3D_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_BLEND_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_BVH_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_COB_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_COLLADA_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_CSM_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_DXF_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_HMP_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_IFC_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_IRR_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_LWO_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_LWS_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MD2_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MD3_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MD5_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MDC_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MDL_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MS3D_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_NDO_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_NFF_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_OBJ_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_OFF_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_OGRE_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_OPENGEX_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_PLY_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_Q3BSP_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_Q3D_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_RAW_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_SMD_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_STL_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_TERRAGEN_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_XGL_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_X_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_AMF_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_IRRMESH_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_SIB_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_X3D_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_GLTF_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_3MF_IMPORTER OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_MMD_IMPORTER OFF CACHE BOOL "" FORCE)

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
	configure_file(
		external/assimp/include/assimp/config.h.in
		${CMAKE_CURRENT_BINARY_DIR}/external/assimp/include/assimp/config.h
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
		-DASSIMP_BUILD_NO_AMF_IMPORTER
		-DASSIMP_BUILD_NO_SIB_IMPORTER
		-DASSIMP_BUILD_NO_X3D_IMPORTER
		-DASSIMP_BUILD_NO_GLTF_IMPORTER
		-DASSIMP_BUILD_NO_3MF_IMPORTER
		-DASSIMP_BUILD_NO_MMD_IMPORTER
	)

	target_include_directories(assimp PRIVATE
		external/assimp/include
		external/assimp/code/BoostWorkaround
		${ZLIB_INCLUDE_DIR}
	)

	if (APPLE)
		set(GLVM_IMPORT src/platform/glvm.cpp)
	elseif (WIN32)
		set(GLVM_IMPORT src/platform/glvm.cpp)
	else()
		set(GLVM_IMPORT "")
	endif()

	set(SRC_IMPORT
		src/game/constants.h
		src/data/import_common.h
		src/data/import_common.cpp
		src/render/glvm.h
		src/data/array.h
		src/types.h
		src/lmath.h
		src/lmath.cpp
		src/import.cpp
		${GLVM_IMPORT}
	)
	#enable_unity_build(import SRC_IMPORT)
	add_executable(import ${SRC_IMPORT})

	target_include_directories(import PRIVATE ${IMPORT_INCLUDES})

	target_link_libraries(import
		${OPENGL_LIBRARIES}
		${SDL_LIBS}
		assimp
		recast
		detour
		fastlz
		cJSON
		mersenne
	)

	add_custom_target(
		assets ALL
		COMMAND $<TARGET_FILE:import>
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	add_dependencies(assets import)
	if (CLIENT)
		add_dependencies(deceiver assets)
	endif()
	if (SERVER)
		add_dependencies(deceiversrv assets)
	endif()
endif()
