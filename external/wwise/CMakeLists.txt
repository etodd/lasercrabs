add_library(wwise_integration STATIC
	CMakeLists.txt
	AK/StreamManager/AkBuddyBlockPool.h
	AK/StreamManager/AkDefaultIOHookBlocking.h
	AK/StreamManager/AkDefaultIOHookBlocking.cpp
	AK/StreamManager/AkDeviceBase.h
	AK/StreamManager/AkDeviceBase.cpp
	AK/StreamManager/AkDeviceBlocking.h
	AK/StreamManager/AkDeviceBlocking.cpp
	AK/StreamManager/AkDeviceDeferredLinedUp.h
	AK/StreamManager/AkDeviceDeferredLinedUp.cpp
	AK/StreamManager/AkIOMemMgr.h
	AK/StreamManager/AkIOMemMgr.cpp
	AK/StreamManager/AkStmDeferredLinedUpBase.h
	AK/StreamManager/AkStmDeferredLinedUpBase.inl
	AK/StreamManager/AkStmMemView.h
	AK/StreamManager/AkStreamingDefaults.h
	AK/StreamManager/AkStreamMgr.h
	AK/StreamManager/AkStreamMgr.cpp
	AK/StreamManager/AkTransferDeferred.h
	AK/StreamManager/AkTransferDeferred.cpp
	AK/StreamManager/AkIOThread.h
	AK/StreamManager/AkIOThread.cpp
	AK/StreamManager/AkFileLocationBase.h
	AK/StreamManager/AkFileLocationBase.cpp
	AK/StreamManager/AkFileHelpers.h
	AK/StreamManager/AkMultipleFileLocation.h
	AK/StreamManager/AkMultipleFileLocation.inl
	AK/StreamManager/AkPlatformStreamingDefaults.h
)

target_include_directories(wwise_integration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

cmake_policy(SET CMP0008 OLD)

if (WIN32)
	target_link_libraries(wwise_integration
		ws2_32
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Win/Debug/AkMemoryMgr
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Win/Release/AkMemoryMgr
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Win/Debug/AkMusicEngine
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Win/Release/AkMusicEngine
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Win/Debug/AkSoundEngine
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Win/Release/AkSoundEngine
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Win/Debug/AkStreamMgr
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Win/Release/AkStreamMgr
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Win/Debug/AkVorbisDecoder
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Win/Release/AkVorbisDecoder
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Win/Debug/CommunicationCentral
	)
elseif(APPLE)
	target_link_libraries(wwise_integration
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Debug/libAkMemoryMgr.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Release/libAkMemoryMgr.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Debug/libAkMusicEngine.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Release/libAkMusicEngine.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Debug/libAkSoundEngine.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Release/libAkSoundEngine.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Debug/libAkStreamMgr.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Release/libAkStreamMgr.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Debug/libAkVorbisDecoder.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Release/libAkVorbisDecoder.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Mac/Debug/libCommunicationCentral.a
	)
else() # Linux
	target_link_libraries(wwise_integration
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Debug/libAkMemoryMgr.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Release/libAkMemoryMgr.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Debug/libAkMusicEngine.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Release/libAkMusicEngine.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Debug/libCommunicationCentral.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Debug/libAkSoundEngine.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Release/libAkSoundEngine.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Debug/libAkStreamMgr.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Release/libAkStreamMgr.a
		debug ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Debug/libAkVorbisDecoder.a
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/Linux/Release/libAkVorbisDecoder.a
	)
endif()
